#include "RTSWorld.h"
#include "RTSController.h"

void ARTSWorld::RefreshObstacles() const {
 if(ObstacleCount==Entities.Num())return;Obstacles.Reset();
 for(const auto& KV:Entities){const auto& Def=D(*KV.Value);if(Def.bBuilding||Def.Id==TEXT("tree")||Def.Id==TEXT("ore"))Obstacles.Add(KV.Value);}
 ObstacleCount=Entities.Num();
}
bool ARTSWorld::LOS(FVector2D A,FVector2D B,int32 IgnoreA,int32 IgnoreB) const {
 float Dist=FVector2D::Distance(A,B);int Steps=FMath::CeilToInt(Dist/1.5f);for(int I=1;I<Steps;I++) if(RTS::RockAt(FMath::Lerp(A,B,float(I)/Steps),Map))return false;
 FVector2D AB=B-A;double Len=AB.SizeSquared();if(Len<.01)return true;
 bool Tower=false;auto Source=Entities.Find(IgnoreA);if(Source)Tower=D(**Source).Id==TEXT("tower_of_watchmen");
 RefreshObstacles();for(const auto& Item:Obstacles){auto& E=*Item;if(E.Id==IgnoreA||E.Id==IgnoreB||!D(E).bBuilding||E.Progress<.2f)continue;if(Tower&&D(E).Id==TEXT("works_of_defense"))continue;double T=FVector2D::DotProduct(E.Pos-A,AB)/Len;if(T<.02||T>.98)continue;if(FVector2D::DistSquared(A+AB*T,E.Pos)<FMath::Square(D(E).Radius*.7f))return false;}return true;
}
bool ARTSWorld::VisiblePoint(int O,FVector2D P) const {
 if(!bFogOfWar&&Players.IsValidIndex(O))return true;const bool Shared=SharesEnemyVision(O);for(auto& KV:Entities){auto& E=*KV.Value;if((!Allied(O,E.Owner)&&!Allied(O,E.DisguisedOwner)&&!(Shared&&E.Owner>=0))||E.Carrier||E.HP<=0)continue;float Sight=EffectiveSight(E);if(FVector2D::DistSquared(P,E.Pos)<=FMath::Square(Sight)&&LOS(E.Pos,P,E.Id))return true;}return false;
}
bool ARTSWorld::SharesEnemyVision(int32 O) const {return Players.IsValidIndex(O)&&Players[O].Faction==TEXT("gadianton")&&Players[O].Favor>50&&Has(O,TEXT("network_of_secrets"));}
bool ARTSWorld::IsStealthed(const FRTSEntity& Target) const {
 auto& TD=D(Target);bool Hidden=(TD.Id==TEXT("gadianton_robber")||TD.Id==TEXT("kishkumen")||(TD.Id==TEXT("wilderness_spy")&&Players.IsValidIndex(Target.Owner)&&Players[Target.Owner].Faction==TEXT("gadianton")))&&Time-Target.LastMove>=3&&Time>=Target.RevealedUntil;
 if(TD.Id==TEXT("secret_murderer")&&Time>=Target.RevealedUntil&&Time-Target.LastDamage>=3)Hidden=true;
 if(TD.Kind==TEXT("lawyer")&&Target.Order!=TEXT("convert")&&Time-Target.LastMove>=3&&Time>=Target.RevealedUntil)Hidden=true;
 if(TD.bBuilding&&Target.Progress>=1&&Players.IsValidIndex(Target.Owner)&&Players[Target.Owner].Faction==TEXT("gadianton")&&(TD.Id==TEXT("seat_of_the_people")||TD.Id==TEXT("encampment_of_war"))&&Time-Target.LastDamage>=5&&Time>=Target.RevealedUntil)Hidden=true;
 if(TD.Id==TEXT("teancum")&&Target.BuffUntil>Time&&Time>=Target.RevealedUntil)Hidden=true;
 return Hidden;
}
bool ARTSWorld::CanSee(int O,const FRTSEntity& Target) const {
 if(Allied(O,Target.Owner)||Allied(O,Target.DisguisedOwner))return true;if(Target.Carrier)return false;
 const bool Shared=SharesEnemyVision(O);if(Shared&&Target.Owner>=0)return true;const bool Hidden=IsStealthed(Target);if(!bFogOfWar&&!Hidden&&Players.IsValidIndex(O))return true;
 for(auto& KV:Entities){auto& E=*KV.Value;if((!Allied(O,E.Owner)&&!Allied(O,E.DisguisedOwner)&&!(Shared&&E.Owner>=0))||E.Carrier||E.HP<=0)continue;auto& SD=D(E);float Sight=EffectiveSight(E);float Distance=FVector2D::Distance(E.Pos,Target.Pos);if(Distance>Sight+D(Target).Radius||!LOS(E.Pos,Target.Pos,E.Id,Target.Id))continue;if(!Hidden)return true;float Detect=SD.Id==TEXT("wilderness_spy")?6:(SD.Id==TEXT("tower_of_watchmen")||SD.Id==TEXT("cement_tower"))?8:0;if(Distance<=Detect)return true;}return false;
}
bool ARTSWorld::Walkable(FVector2D P,float R,int32 Ignore,bool Ship) const {
 if(P.X<R||P.Y<R||P.X>RTS::MapSize-R||P.Y>RTS::MapSize-R)return false;
 if(Ship){if(Map!=2||P.X<72+R||P.X>88-R)return false;}
 else if(RTS::WaterAt(P,Map)||RTS::RockAt(P,Map))return false;
 RefreshObstacles();for(const auto& Item:Obstacles){auto& E=*Item;auto& Def=D(E);if(E.Id==Ignore||E.Carrier)continue;if(Ship&&Def.Id==TEXT("place_of_many_waters"))continue;auto Mover=Entities.Find(Ignore);if(E.bGate&&E.Progress>=1&&Mover&&Allied((*Mover)->Owner,E.Owner))continue;if(FVector2D::DistSquared(P,E.Pos)<FMath::Square(R+Def.Radius*.88f))return false;}return true;
}
bool ARTSWorld::ExploredPoint(int OwnerSlot,FVector2D Point) const {
 if(!Players.IsValidIndex(OwnerSlot)||Point.X<0||Point.Y<0||Point.X>RTS::MapSize||Point.Y>RTS::MapSize)return false;
 int X=FMath::Clamp(int(Point.X/2.5),0,63),Y=FMath::Clamp(int(Point.Y/2.5),0,63);
 return (Players[OwnerSlot].Explored.IsValidIndex(Y*64+X)&&Players[OwnerSlot].Explored[Y*64+X]>0)||VisiblePoint(OwnerSlot,Point);
}
FVector2D ARTSWorld::WorkPosition(const FRTSEntity& E,const FRTSEntity& Site) const {
 // Stable, distributed approaches keep gatherers from all chasing the same edge.
 const bool Builder=E.Order==TEXT("build")||E.Order==TEXT("repair");int Rank=0,Assigned=0;for(auto& KV:Entities)if(KV.Value->WorkTarget==Site.Id||((KV.Value->Order==TEXT("build")||KV.Value->Order==TEXT("repair"))&&KV.Value->Target==Site.Id)){Assigned++;if(KV.Key<E.Id)Rank++;}
 const FString SiteId=D(Site).Id;const int Slots=Builder?FMath::Max(10,Assigned):SiteId==TEXT("hill_of_ore")?6:(SiteId==TEXT("fields_of_grain")||SiteId==TEXT("gathering_of_flocks"))?10:FMath::Max(8,FMath::FloorToInt(2*PI*(D(Site).Radius+.7f)/(D(E).Radius*2+.25f)));
 const float Radius=D(Site).Radius+.7f+(Rank/Slots)*(D(E).Radius*2+.3f);
 for(int Attempt=0;Attempt<Slots*3;Attempt++){
  const float A=2*PI*((Rank%Slots)+Attempt)/Slots;const float R=Radius+(Attempt/Slots)*.8f;
  const FVector2D At=Site.Pos+FVector2D(FMath::Cos(A),FMath::Sin(A))*R;
  if(Walkable(At,D(E).Radius,E.Id,D(E).Kind==TEXT("ship")))return At;
 }
 return Site.Pos+(E.Pos-Site.Pos).GetSafeNormal()*(D(Site).Radius+.7f);
}
void ARTSWorld::Route(FRTSEntity& E,FVector2D Target) {
 E.Path.Empty();E.LastPath=Time;E.Destination=Target;bool AlliedGate=false;for(const auto& KV:Entities)if(KV.Value->bGate&&KV.Value->Progress>=1&&Allied(E.Owner,KV.Value->Owner)){AlliedGate=true;break;}const int N=AlliedGate?160:80;const float Cell=AlliedGate?1.f:2.f;
 static TMap<ARTSWorld*,int32> NavigationMaps;
 if(NavigationLand.Num()!=6400||NavigationMaps.FindRef(this)!=Map||NavigationCount!=Entities.Num()||Time<NavigationTime||Time-NavigationTime>.5f){
  NavigationLand.SetNumUninitialized(6400);NavigationWater.SetNumUninitialized(6400);
  for(int I=0;I<6400;I++){FVector2D P((I%80+.5)*2,(I/80+.5)*2);NavigationLand[I]=Walkable(P,.5f,0,false);NavigationWater[I]=Walkable(P,1.05f,0,true);}NavigationTime=Time;NavigationCount=Entities.Num();NavigationMaps.Add(this,Map);
 }
 // Large beasts need their own clearance grid. Sharing the worker grid sends
 // them into gaps that their collision radius can never pass through.
 struct FClearanceGrid {TArray<uint8> Cells;uint32 Layout=0;float Updated=-100;};
 static TMap<ARTSWorld*,TMap<int32,FClearanceGrid>> ClearanceGrids;
 bool Ship=D(E).Kind==TEXT("ship");float Radius=D(E).Radius;const TArray<uint8>* GridPtr=Ship?&NavigationWater:&NavigationLand;
 if(AlliedGate||(!Ship&&Radius>.5f)||(Ship&&Radius>1.05f)){
  RefreshObstacles();uint32 Layout=GetTypeHash(Map);for(const auto& Item:Obstacles){Layout=HashCombine(Layout,GetTypeHash(Item->Id));Layout=HashCombine(Layout,GetTypeHash(Item->Pos));Layout=HashCombine(Layout,GetTypeHash(D(*Item).Radius));Layout=HashCombine(Layout,GetTypeHash(Item->bGate));Layout=HashCombine(Layout,GetTypeHash(Item->Owner));}
  int32 RadiusKey=FMath::CeilToInt(Radius*1000);int32 CacheKey=(RadiusKey*2+(Ship?1:0))*8+(AlliedGate?E.Owner+1:0);auto& Grid=ClearanceGrids.FindOrAdd(this).FindOrAdd(CacheKey);
  if(Grid.Cells.Num()!=N*N||Grid.Layout!=Layout||Grid.Updated!=NavigationTime){Grid.Cells.SetNumUninitialized(N*N);float Clearance=RadiusKey/1000.f;for(int I=0;I<N*N;I++)Grid.Cells[I]=Walkable(FVector2D((I%N+.5)*Cell,(I/N+.5)*Cell),Clearance,AlliedGate?E.Id:0,Ship);Grid.Layout=Layout;Grid.Updated=NavigationTime;}GridPtr=&Grid.Cells;
 }
 auto Key=[&](FVector2D P){return FMath::Clamp(FMath::FloorToInt(P.Y/Cell),0,N-1)*N+FMath::Clamp(FMath::FloorToInt(P.X/Cell),0,N-1);};
 auto Pos=[&](int K){return FVector2D((K%N+.5)*Cell,(K/N+.5)*Cell);};
 int Start=Key(E.Pos),Goal=Key(Target);TArray<int> Parent;Parent.Init(-2,N*N);TArray<int> Open;Open.Reserve(N*N);Open.Add(Start);Parent[Start]=-1;int Best=Start;double BestDist=FVector2D::DistSquared(E.Pos,Target);
 for(int Head=0;Head<Open.Num();Head++) {int K=Open[Head];double Dist=FVector2D::DistSquared(Pos(K),Target);if(Dist<BestDist){BestDist=Dist;Best=K;}if(K==Goal){Best=K;break;}
  const int DX[]={1,-1,0,0,1,-1,1,-1},DY[]={0,0,1,-1,1,1,-1,-1};
  const auto& Grid=*GridPtr;
  for(int I=0;I<8;I++){int X=K%N+DX[I],Y=K/N+DY[I];if(X<0||Y<0||X>=N||Y>=N)continue;int J=Y*N+X;if(Parent[J]!=-2||!Grid[J])continue;
   if(I>=4&&(!Grid[(K/N)*N+X]||!Grid[Y*N+K%N]))continue;Parent[J]=K;Open.Add(J);}
 }
 TArray<FVector2D> Reverse;for(int K=Best;K>=0&&K!=Start;K=Parent[K])Reverse.Add(Pos(K));for(int I=Reverse.Num()-1;I>=0;I--)E.Path.Add(Reverse[I]);if(Best==Goal&&Walkable(Target,D(E).Radius,E.Id,Ship))E.Path.Add(Target);
}
bool ARTSWorld::Move(FRTSEntity& E,FVector2D Target,float Stop,float DT) {
 if(E.Carrier||!E.Evolution.IsEmpty())return false;
 // Placement or completion can surround a builder or gatherer. Walk outward
 // without teleporting, ignoring only the enclosing owned structure.
 FRTSEntity* Foundation=nullptr;float Penetration=0;for(auto& KV:Entities){auto& Site=*KV.Value;if(Site.Owner!=E.Owner||!D(Site).bBuilding||(Site.bGate&&Site.Progress>=1&&Allied(E.Owner,Site.Owner)))continue;float Depth=D(Site).Radius*.88f+D(E).Radius+.03f-FVector2D::Distance(E.Pos,Site.Pos);if(Depth>Penetration){Penetration=Depth;Foundation=&Site;}}
 if(Foundation){
  const float Clearance=D(*Foundation).Radius*.88f+D(E).Radius+.03f;FVector2D Away=E.Pos-Foundation->Pos;float Dist=Away.Size();
  if(Dist<Clearance){if(Dist<.001f){float A=E.Id*2.39996f;Away=FVector2D(FMath::Cos(A),FMath::Sin(A));}else Away/=Dist;
   float Travel=FMath::Min(D(E).Speed*(E.bDrunkenAura?.8f:1.f)*DT,Clearance-Dist);for(int I=0;I<5;I++){float A=I==0?0:(I+1)/2*PI/8*(I%2?1:-1);FVector2D Step(Away.X*FMath::Cos(A)-Away.Y*FMath::Sin(A),Away.X*FMath::Sin(A)+Away.Y*FMath::Cos(A));FVector2D At=E.Pos+Step*Travel;
    if(FVector2D::Distance(At,Foundation->Pos)>Dist&&Walkable(At,D(E).Radius,Foundation->Id,D(E).Kind==TEXT("ship"))){E.Pos=At;E.Heading=FMath::Atan2(Step.Y,Step.X);E.LastMove=Time;break;}
   }E.Path.Empty();E.LastPath=-100;return false;
  }
 }
 float Distance=FVector2D::Distance(E.Pos,Target);if(Distance<=Stop+.08f){E.Path.Empty();E.StuckTime=0;return true;}
 bool Ship=D(E).Kind==TEXT("ship");FVector2D Delta=Target-E.Pos;Delta.Normalize();FVector2D Desired=Target-Delta*FMath::Max(0.f,Stop-.1f);
 if(FVector2D::DistSquared(E.LastProgressPos,E.Pos)>.04f){E.LastProgressPos=E.Pos;E.StuckTime=0;}else E.StuckTime+=DT;
 if((E.Path.Num()==0||FVector2D::DistSquared(E.Destination,Desired)>1||E.StuckTime>.9f)&&Time-E.LastPath>.45f){Route(E,Desired);E.StuckTime=0;}
 // Skip visible intermediate waypoints, retaining terrain clearance at unit radius.
 if(E.Path.Num()>1){int Furthest=0;for(int I=1;I<FMath::Min(E.Path.Num(),8);I++){FVector2D To=E.Path[I]-E.Pos;int Steps=FMath::Max(1,FMath::CeilToInt(To.Size()/.4f));bool Clear=true;for(int J=1;J<=Steps;J++)if(!Walkable(E.Pos+To*(float(J)/Steps),D(E).Radius,E.Id,Ship)){Clear=false;break;}if(!Clear)break;Furthest=I;}if(Furthest)E.Path.RemoveAt(0,Furthest);}
 FVector2D Next=E.Path.Num()?E.Path[0]:Desired;float Speed=D(E).Speed*(E.bDrunkenAura?.8f:1.f);if(D(E).Id==TEXT("kings_chariot")&&E.BuffUntil>Time)Speed*=1.5f;
 FVector2D V=Next-E.Pos;float Len=V.Size();if(Len<.15f&&E.Path.Num()){E.Path.RemoveAt(0);return false;}V.Normalize();float Travel=FMath::Min(Len,Speed*DT);FVector2D NewPos=E.Pos+V*Travel;
 if(!Walkable(NewPos,D(E).Radius,E.Id,Ship)){
  bool Found=false;for(int I=0;I<4;I++){float A=(I/2+1)*PI/4*(I%2?1:-1);FVector2D Turn(V.X*FMath::Cos(A)-V.Y*FMath::Sin(A),V.X*FMath::Sin(A)+V.Y*FMath::Cos(A));FVector2D Candidate=E.Pos+Turn*Travel;if(Walkable(Candidate,D(E).Radius,E.Id,Ship)){NewPos=Candidate;V=Turn;Found=true;break;}}if(!Found){E.Path.Empty();NewPos=E.Pos;}
 }
 if(!NewPos.Equals(E.Pos,.0001f)){E.Pos=NewPos;E.Heading=FMath::Atan2(V.Y,V.X);E.LastMove=Time;}
 // Soft separation never pushes a unit through terrain or a structure.
 FVector2D Separation=FVector2D::ZeroVector;for(auto& KV:Entities){auto& Other=*KV.Value;if(Other.Id==E.Id||Other.Carrier||D(Other).bBuilding||Other.Owner<0||FVector2D::DistSquared(E.Pos,Other.Pos)>4)continue;float R=(D(E).Radius+D(Other).Radius)*.9f;FVector2D Away=E.Pos-Other.Pos;float L=Away.Size();if(L<R){if(L<.001f){float A=E.Id*2.39996f;Away=FVector2D(FMath::Cos(A),FMath::Sin(A));L=0;}else Away/=L;Separation+=Away*(R-L);}}
 if(!Separation.IsNearlyZero()){float Limit=FMath::Min(.12f,D(E).Speed*DT*.3f);auto Push=E.Pos+Separation.GetSafeNormal()*FMath::Min(float(Separation.Size()),Limit);if(Walkable(Push,D(E).Radius,E.Id,Ship))E.Pos=Push;}
 return FVector2D::Distance(E.Pos,Target)<=Stop+.15f;
}
