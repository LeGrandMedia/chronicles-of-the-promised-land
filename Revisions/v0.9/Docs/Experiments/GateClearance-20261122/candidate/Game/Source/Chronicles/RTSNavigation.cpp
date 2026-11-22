#include "RTSWorld.h"
#include "RTSController.h"

#include "RTSPassages.h"

namespace {
bool PortalBoxesOverlap(FVector2D Delta,FVector2D Axis,FVector2D Body,FVector2D Pier) {
 const FVector2D Side(-Axis.Y,Axis.X);
 if(FMath::Abs(Delta.X)>Pier.X+FMath::Abs(Axis.X)*Body.X+FMath::Abs(Side.X)*Body.Y)return false;
 if(FMath::Abs(Delta.Y)>Pier.Y+FMath::Abs(Axis.Y)*Body.X+FMath::Abs(Side.Y)*Body.Y)return false;
 if(FMath::Abs(FVector2D::DotProduct(Delta,Axis))>Body.X+FMath::Abs(Axis.X)*Pier.X+FMath::Abs(Axis.Y)*Pier.Y)return false;
 return FMath::Abs(FVector2D::DotProduct(Delta,Side))<=Body.Y+FMath::Abs(Side.X)*Pier.X+FMath::Abs(Side.Y)*Pier.Y;
}
}

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
bool ARTSWorld::Walkable(FVector2D P,float R,int32 Ignore,bool Ship,float Heading) const {
 if(!RTS::TerrainClear(P,R,Map,Ship))return false;
 const auto Mover=Entities.Find(Ignore);
 RefreshObstacles();for(const auto& Item:Obstacles){auto& E=*Item;auto& Def=D(E);if(E.Id==Ignore||E.Carrier)continue;if(Ship&&Def.Id==TEXT("place_of_many_waters"))continue;
  if(E.bGate&&E.Progress>=1&&Mover&&Allied((*Mover)->Owner,E.Owner)) {
   const auto& Body=D(**Mover);
   if(Def.GatePassage.X>0&&!Body.bBuilding&&!Ship) {
    const FVector2D Along(FMath::Cos(E.Heading),FMath::Sin(E.Heading)),Across(-Along.Y,Along.X),Relative=P-E.Pos;
    const FVector2D Local(FVector2D::DotProduct(Relative,Along),FVector2D::DotProduct(Relative,Across));
    const bool Oriented=Heading!=MAX_flt;
    const double Broad=Oriented?Body.PassageHalfSize.Size():FMath::Max(double(R),Body.PassageHalfSize.X);
    if(FMath::Abs(Local.X)<Def.GatePassage.Y+Broad&&FMath::Abs(Local.Y)<Def.GatePassage.Z+Broad) {
     uint8 Joined=0;
     for(const auto& Neighbor:Obstacles)if(Neighbor->Id!=E.Id)Joined|=RTS::AdjacentGateSide(E,*Neighbor,Def.Radius);
     const FVector2D Extent((Def.GatePassage.Y-Def.GatePassage.X)*.5,Def.GatePassage.Z);
     for(int Sign:{-1,1}) {
      if(Joined&(Sign<0?RTS::GateJoinNegative:RTS::GateJoinPositive))continue;
      const FVector2D Delta=Local-FVector2D(Sign*(Def.GatePassage.X+Def.GatePassage.Y)*.5,0);
      if(Oriented) {
       const double Angle=Heading-E.Heading;
       if(PortalBoxesOverlap(Delta,FVector2D(FMath::Cos(Angle),FMath::Sin(Angle)),FVector2D(Body.PassageHalfSize.Y,Body.PassageHalfSize.X),Extent))return false;
      } else {
       const double X=FMath::Max(0.,FMath::Abs(Delta.X)-Extent.X),Y=FMath::Max(0.,FMath::Abs(Delta.Y)-Extent.Y);
       if(X*X+Y*Y<Broad*Broad)return false;
      }
     }
    }
   }
   continue;
  }
  if(FVector2D::DistSquared(P,E.Pos)<FMath::Square(R+Def.Radius*.88f))return false;
  // A long cart's center can clear a wall while its body still clips the end.
  // Retain the gameplay circle and additionally test the authored wall envelope.
  if(Mover&&!Ship&&Heading!=MAX_flt&&Def.GatePassage.X>0&&!D(**Mover).bBuilding&&
   FVector2D::DistSquared(P,E.Pos)<FMath::Square(Def.Radius+.6+D(**Mover).PassageHalfSize.Size())) {
   const auto& Body=D(**Mover);const FVector2D Along(FMath::Cos(E.Heading),FMath::Sin(E.Heading)),Across(-Along.Y,Along.X),Relative=P-E.Pos;
   const FVector2D Local(FVector2D::DotProduct(Relative,Along),FVector2D::DotProduct(Relative,Across));
   const double Angle=Heading-E.Heading;
   if(PortalBoxesOverlap(Local,FVector2D(FMath::Cos(Angle),FMath::Sin(Angle)),FVector2D(Body.PassageHalfSize.Y,Body.PassageHalfSize.X),FVector2D(Def.Radius+.077,.46)))return false;
  }
 }return true;
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
 E.Path.Empty();E.PortalWaypoints=0;E.LastPath=Time;E.Destination=Target;bool AlliedGate=false;for(const auto& KV:Entities)if(KV.Value->bGate&&KV.Value->Progress>=1&&Allied(E.Owner,KV.Value->Owner)){AlliedGate=true;break;}const int N=AlliedGate?160:80;const float Cell=AlliedGate?1.f:2.f;
 // A clear gate may fall between grid centers, especially for large beasts.
 // Try verified crossings before the grid; collision and team rules stay in Walkable.
 if(AlliedGate&&D(E).Kind!=TEXT("ship")) {
  struct FGateRoute {FVector2D Entry,Exit;double Cost;int32 Id;};
  TArray<FGateRoute> GateRoutes;
  const double Radius=D(E).Radius;
  for(const auto& KV:Entities) {
   const auto& Gate=*KV.Value;
   if(!Gate.bGate||Gate.Progress<1||!Allied(E.Owner,Gate.Owner))continue;
   const FVector2D Along(FMath::Cos(Gate.Heading),FMath::Sin(Gate.Heading)),Across(-Along.Y,Along.X);
   const double From=FVector2D::DotProduct(E.Pos-Gate.Pos,Across),To=FVector2D::DotProduct(Target-Gate.Pos,Across);
   const double Approach=FMath::Max(D(Gate).Radius*.88+Radius+.15,D(E).PassageHalfSize.Y+D(Gate).GatePassage.Z+.30);
   const bool Inside=FMath::Abs(From)<Approach&&FMath::Abs(FVector2D::DotProduct(E.Pos-Gate.Pos,Along))+D(E).PassageHalfSize.X<=D(Gate).GatePassage.X+.001;
   if(!Inside&&(From*To>=0||FMath::Abs(From-To)<.01))continue;
   const FVector2D Entry=Inside?E.Pos:Gate.Pos+Across*(From<0?-Approach:Approach);
   const FVector2D Exit=Gate.Pos+Across*(To<0?-Approach:Approach);
   const double Cost=FVector2D::Distance(E.Pos,Entry)+2*Approach+FVector2D::Distance(Exit,Target);
   GateRoutes.Add({Entry,Exit,Cost,Gate.Id});
  }
  const auto Clear=[&](FVector2D From,FVector2D To) {
   if(From.Equals(To,.001))return Walkable(From,Radius,E.Id,false,E.Heading);
   const float Heading=FMath::Atan2(To.Y-From.Y,To.X-From.X);
   const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(From,To)/.2));
   for(int32 I=0;I<=Steps;++I)
    if(!Walkable(FMath::Lerp(From,To,double(I)/Steps),Radius,E.Id,false,Heading))return false;
   return true;
  };
  // Prefer the direct segment whenever its complete oriented body fits. This
  // also avoids attracting unrelated movement to a distant portal or imposing
  // a centerline detour on a body already moving safely parallel to the wall.
  if(Clear(E.Pos,Target)){E.Path.Add(Target);return;}
  GateRoutes.Sort([](const FGateRoute& A,const FGateRoute& B){return A.Cost==B.Cost?A.Id<B.Id:A.Cost<B.Cost;});
  // Bound the extra collision work when a fortress has many gates. The ordinary
  // route remains available if nearby crossings need a more complex approach.
  for(int32 I=0;I<FMath::Min(4,GateRoutes.Num());++I) {
   const auto& Candidate=GateRoutes[I];
   if(!Clear(E.Pos,Candidate.Entry)||!Clear(Candidate.Entry,Candidate.Exit)||!Clear(Candidate.Exit,Target))continue;
   E.Path={Candidate.Entry,Candidate.Exit,Target};E.PortalWaypoints=2;return;
  }
 }
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
  const double RouteRadius=AlliedGate&&D(E).PassageHalfSize.Y>1.3?FMath::Max(double(Radius),D(E).PassageHalfSize.Size()):double(Radius);int32 RadiusKey=FMath::CeilToInt(RouteRadius*1000);int32 CacheKey=(RadiusKey*2+(Ship?1:0))*8+(AlliedGate?E.Owner+1:0);if(AlliedGate)CacheKey=HashCombine(CacheKey,GetTypeHash(D(E).PassageHalfSize));auto& Grid=ClearanceGrids.FindOrAdd(this).FindOrAdd(CacheKey);
  if(Grid.Cells.Num()!=N*N||Grid.Layout!=Layout||Grid.Updated!=NavigationTime){Grid.Cells.SetNumUninitialized(N*N);float Clearance=RadiusKey/1000.f;for(int I=0;I<N*N;I++)Grid.Cells[I]=Walkable(FVector2D((I%N+.5)*Cell,(I/N+.5)*Cell),Clearance,AlliedGate?E.Id:0,Ship);Grid.Layout=Layout;Grid.Updated=NavigationTime;}GridPtr=&Grid.Cells;
 }
 auto Key=[&](FVector2D P){return FMath::Clamp(FMath::FloorToInt(P.Y/Cell),0,N-1)*N+FMath::Clamp(FMath::FloorToInt(P.X/Cell),0,N-1);};
 auto Pos=[&](int K){return FVector2D((K%N+.5)*Cell,(K/N+.5)*Cell);};
 int Start=Key(E.Pos),Goal=Key(Target);TArray<int> Parent;Parent.Init(-2,N*N);TArray<int> Open;Open.Reserve(N*N);Open.Add(Start);Parent[Start]=-1;int Best=Start;double BestDist=FVector2D::DistSquared(E.Pos,Target);
 // Smoothing may leave a long body safely beside a wall but inside the larger
 // all-heading grid envelope. Connect reachable clear cells instead of trapping
 // the next search in a blocked start cell. Every connection is body-checked.
 if(!(*GridPtr)[Start]) {
  TArray<int32> Seeds;const int32 Reach=FMath::CeilToInt((D(E).PassageHalfSize.Size()+2.)/Cell);
  for(int32 DY=-Reach;DY<=Reach;++DY)for(int32 DX=-Reach;DX<=Reach;++DX) {
   const int32 X=Start%N+DX,Y=Start/N+DY;if(X<0||Y<0||X>=N||Y>=N)continue;
   const int32 Index=Y*N+X;if(Index!=Start&&(*GridPtr)[Index])Seeds.Add(Index);
  }
  Seeds.Sort([&](int32 A,int32 B){return FVector2D::DistSquared(E.Pos,Pos(A))<FVector2D::DistSquared(E.Pos,Pos(B));});
  for(int32 Index:Seeds) {
   const FVector2D To=Pos(Index);const float Heading=FMath::Atan2(To.Y-E.Pos.Y,To.X-E.Pos.X);
   const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(E.Pos,To)/.2));bool Clear=true;
   for(int32 Step=0;Step<=Steps;++Step)if(!Walkable(FMath::Lerp(E.Pos,To,double(Step)/Steps),Radius,E.Id,Ship,Heading)){Clear=false;break;}
   if(Clear){Parent[Index]=-1;Open.Add(Index);if(Open.Num()>=9)break;}
  }
 }
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
   const float Travel=FMath::Min(D(E).Speed*(E.bDrunkenAura?.8f:1.f)*DT,Clearance-Dist);
   // Extending a wall can trap a builder against the previous segment. Try side steps
   // that still increase clearance from the enclosing structure.
   for(int32 I=0;I<9;++I){
    const float Angle=I==0?0:(I+1)/2*PI/8*(I%2?1:-1);
    const FVector2D Step(Away.X*FMath::Cos(Angle)-Away.Y*FMath::Sin(Angle),Away.X*FMath::Sin(Angle)+Away.Y*FMath::Cos(Angle));
    const FVector2D At=E.Pos+Step*Travel;
    if(FVector2D::Distance(At,Foundation->Pos)>Dist&&Walkable(At,D(E).Radius,Foundation->Id,D(E).Kind==TEXT("ship"))){E.Pos=At;E.Heading=FMath::Atan2(Step.Y,Step.X);E.LastMove=Time;break;}
   }
   E.Path.Empty();E.LastPath=-100;return false;
  }
 }
 float Distance=FVector2D::Distance(E.Pos,Target);if(Distance<=Stop+.08f){E.Path.Empty();E.StuckTime=0;return true;}
 bool Ship=D(E).Kind==TEXT("ship");FVector2D Delta=Target-E.Pos;Delta.Normalize();FVector2D Desired=Target-Delta*FMath::Max(0.f,Stop-.1f);
 if(FVector2D::DistSquared(E.LastProgressPos,E.Pos)>.04f){E.LastProgressPos=E.Pos;E.StuckTime=0;}else E.StuckTime+=DT;
 if((E.Path.Num()==0||FVector2D::DistSquared(E.Destination,Desired)>1||E.StuckTime>.9f)&&Time-E.LastPath>.45f){Route(E,Desired);E.StuckTime=0;}
 // Skip visible intermediate waypoints, retaining terrain clearance at unit radius.
 if(E.Path.Num()>1&&E.PortalWaypoints==0){int Furthest=0;for(int I=1;I<FMath::Min(E.Path.Num(),8);I++){FVector2D To=E.Path[I]-E.Pos;int Steps=FMath::Max(1,FMath::CeilToInt(To.Size()/.4f));bool Clear=true;for(int J=1;J<=Steps;J++)if(!Walkable(E.Pos+To*(float(J)/Steps),D(E).Radius,E.Id,Ship,FMath::Atan2(To.Y,To.X))){Clear=false;break;}if(!Clear)break;Furthest=I;}if(Furthest)E.Path.RemoveAt(0,Furthest);}
 FVector2D Next=E.Path.Num()?E.Path[0]:Desired;float Speed=D(E).Speed*(E.bDrunkenAura?.8f:1.f);if(D(E).Id==TEXT("kings_chariot")&&E.BuffUntil>Time)Speed*=1.5f;
 FVector2D V=Next-E.Pos;float Len=V.Size();if(Len<(E.PortalWaypoints>0?.001f:.15f)&&E.Path.Num()){E.Path.RemoveAt(0);E.PortalWaypoints=FMath::Max(0,E.PortalWaypoints-1);return false;}V.Normalize();float Travel=FMath::Min(Len,Speed*DT);FVector2D NewPos=E.Pos+V*Travel;
 if(!Walkable(NewPos,D(E).Radius,E.Id,Ship,FMath::Atan2(V.Y,V.X))){
  bool Found=false;for(int I=0;I<(E.PortalWaypoints==1?0:4);I++){float A=(I/2+1)*PI/4*(I%2?1:-1);FVector2D Turn(V.X*FMath::Cos(A)-V.Y*FMath::Sin(A),V.X*FMath::Sin(A)+V.Y*FMath::Cos(A));FVector2D Candidate=E.Pos+Turn*Travel;if(Walkable(Candidate,D(E).Radius,E.Id,Ship,FMath::Atan2(Turn.Y,Turn.X))){NewPos=Candidate;V=Turn;Found=true;break;}}if(!Found){E.Path.Empty();E.PortalWaypoints=0;NewPos=E.Pos;}
 }
 if(!NewPos.Equals(E.Pos,.0001f)){E.Pos=NewPos;E.Heading=FMath::Atan2(V.Y,V.X);E.LastMove=Time;}
 if(E.PortalWaypoints==1)return FVector2D::Distance(E.Pos,Target)<=Stop+.15f;
 // Soft separation never pushes a unit through terrain or a structure.
 FVector2D Separation=FVector2D::ZeroVector;for(auto& KV:Entities){auto& Other=*KV.Value;if(Other.Id==E.Id||Other.Carrier||D(Other).bBuilding||Other.Owner<0||FVector2D::DistSquared(E.Pos,Other.Pos)>4)continue;float R=(D(E).Radius+D(Other).Radius)*.9f;FVector2D Away=E.Pos-Other.Pos;float L=Away.Size();if(L<R){if(L<.001f){float A=E.Id*2.39996f;Away=FVector2D(FMath::Cos(A),FMath::Sin(A));L=0;}else Away/=L;Separation+=Away*(R-L);}}
 if(!Separation.IsNearlyZero()){float Limit=FMath::Min(.12f,D(E).Speed*DT*.3f);auto Push=E.Pos+Separation.GetSafeNormal()*FMath::Min(float(Separation.Size()),Limit);if(Walkable(Push,D(E).Radius,E.Id,Ship,E.Heading))E.Pos=Push;}
 return FVector2D::Distance(E.Pos,Target)<=Stop+.15f;
}
