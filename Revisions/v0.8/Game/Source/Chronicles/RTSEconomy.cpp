#include "RTSWorld.h"
#include "RTSController.h"

static float CargoTotal(const FRTSEntity& E){return E.Cargo.X+E.Cargo.Y+E.Cargo.Z;}
float ARTSWorld::EconomicBonus(int32 OwnerSlot,const FString& Key) const {
 if(!Players.IsValidIndex(OwnerSlot))return 0;float Bonus=0;
 for(const auto& Def:FRTSCatalog::Get().Defs)if(Has(OwnerSlot,Def.Id)){const float* Value=Def.EconomyBonuses.Find(Key);if(Value)Bonus+=*Value;}return Bonus;
}
void ARTSWorld::EmitResources(FRTSEntity& E,FVector Gain,float Spirit){
 if(Gain.IsNearlyZero()&&FMath::IsNearlyZero(Spirit))return;
 if(Time-E.ResourceEventTime<.75f){E.ResourceEvent+=Gain;E.SpiritEvent+=Spirit;return;}
 E.ResourceEvent=Gain;E.ResourceEventTime=Time;E.SpiritEvent=Spirit;E.CementEvent=0;
}
bool ARTSWorld::CanAssignWork(const FRTSEntity& Worker,const FRTSEntity& Site) const {
 const auto& Id=D(Site).Id;
 const bool Gad=Players.IsValidIndex(Worker.Owner)&&Players[Worker.Owner].Faction==TEXT("gadianton");
 if(Id==TEXT("crate"))return CarryCapacity(Worker)>0&&Site.ResourceRemaining>0;
 if(!RTS::IsWorker(D(Worker)))return false;
 if(Gad){bool ForeignFood=Site.Owner>=0&&Site.Owner!=Worker.Owner&&Site.Progress>=1&&(Id==TEXT("fields_of_grain")||Id==TEXT("gathering_of_flocks"));if(Id!=TEXT("tree")&&Id!=TEXT("ore")&&Id!=TEXT("hill_of_ore")&&Id!=TEXT("berries")&&Id!=TEXT("wild_game")&&!ForeignFood)return false;}
 const bool Natural=Site.Owner<0&&(Id==TEXT("tree")||Id==TEXT("ore")||Id==TEXT("berries")||Id==TEXT("wild_game"))&&Site.ResourceRemaining>0;
 const bool Building=(Allied(Worker.Owner,Site.Owner)||(Gad&&Site.Owner>=0&&Site.Owner!=Worker.Owner))&&Site.Progress>=1&&(Id==TEXT("fields_of_grain")||Id==TEXT("gathering_of_flocks")||Id==TEXT("hill_of_ore"));
 if(!Natural&&!Building)return false;
 if(Building){int Assigned=0;for(auto& KV:Entities)if(KV.Key!=Worker.Id&&KV.Value->WorkTarget==Site.Id)Assigned++;if(Assigned>=(Id==TEXT("hill_of_ore")?6:10))return false;}
 return true;
}
void ARTSWorld::AutoGather(FRTSEntity& Worker,const FRTSEntity& Site){
 if(!Worker.PendingOrders.IsEmpty()||!CanAssignWork(Worker,Site))return;
 ResetOrder(Worker,TEXT("gather"));Worker.Target=Worker.WorkTarget=Site.Id;Worker.Destination=Site.Pos;
}
void ARTSWorld::ProcessQueuedOrder(FRTSEntity& E){
 if(E.Order!=TEXT("idle")||E.PendingOrders.IsEmpty()||!Players.IsValidIndex(E.Owner))return;
 auto C=Players[E.Owner].Controller.Get();if(!C)return;
 const FRTSQueuedOrder Q=E.PendingOrders[0];E.PendingOrders.RemoveAt(0);
 const bool WasDispatching=bDispatchingQueuedOrder;bDispatchingQueuedOrder=true;
 Command(C,Q.Action,{E.Id},Q.Point,Q.Target,Q.Type,false);bDispatchingQueuedOrder=WasDispatching;
}
void ARTSWorld::EmitTrickleResources(FRTSEntity& E,FVector Gain,float Spirit,float Cement){
 E.ResourcePending+=Gain;E.SpiritPending+=Spirit;E.CementPending+=Cement;
 // Each resource earns its own ten-unit notification; unrelated small incomes
 // stay pending until that same channel reaches its threshold.
 auto TakeBatches=[](auto& Pending)->float{const double Magnitude=FMath::Abs(double(Pending));const double Ready=FMath::FloorToDouble((Magnitude+.0001)/10.0)*10.0;if(Ready<=0)return 0;const double Batch=Pending<0?-Ready:Ready;Pending-=Batch;if(FMath::Abs(double(Pending))<.0001)Pending=0;return float(Batch);};
 const FVector ReadyGain(TakeBatches(E.ResourcePending.X),TakeBatches(E.ResourcePending.Y),TakeBatches(E.ResourcePending.Z));const float ReadySpirit=TakeBatches(E.SpiritPending),ReadyCement=TakeBatches(E.CementPending);
 if(ReadyGain.IsNearlyZero()&&FMath::IsNearlyZero(ReadySpirit)&&FMath::IsNearlyZero(ReadyCement))return;
 const bool Accumulate=Time-E.ResourceEventTime<.75f;const float CementBatch=ReadyCement+(Accumulate?E.CementEvent:0);
 if(!ReadyGain.IsNearlyZero()||!FMath::IsNearlyZero(ReadySpirit))EmitResources(E,ReadyGain,ReadySpirit);
 else if(!Accumulate){E.ResourceEvent=FVector::ZeroVector;E.SpiritEvent=0;E.ResourceEventTime=Time;}
 E.CementEvent=CementBatch;
}
void ARTSWorld::EconomyAuras(float DT){
 for(auto& KV:Entities){auto& E=*KV.Value;E.ResourceRate=FVector::ZeroVector;E.CementRate=0;if(!Players.IsValidIndex(E.Owner)||E.Carrier||E.HP<=0)continue;auto Id=D(E).Id;
  if(D(E).Kind==TEXT("sheep")){if(Players[E.Owner].Faction==TEXT("gadianton"))continue;auto Home=Find(E.HomeBuilding);if(E.HomeBuilding&&(!Home||Home->Owner!=E.Owner||Home->Progress<1||D(*Home).Id!=TEXT("gathering_of_flocks")))continue;E.ResourceRate.X=.3f;}
  if(RTS::IsJaredite(Players[E.Owner].Faction)&&BeastTier(E)>0){float Bonus=1+EconomicBonus(E.Owner,TEXT("beast_income"));E.ResourceRate+=FVector(0,Id==TEXT("curelom")?.4f:0,Id==TEXT("cumelom")?.3f:0)*Bonus;if(Id==TEXT("elephant"))E.CementRate=.8f*Bonus;}
  if(Id==TEXT("winery")&&E.Progress>=1&&Players[E.Owner].Faction==TEXT("laban")&&Players[E.Owner].Age>=3&&Has(E.Owner,TEXT("winery_abundance")))E.ResourceRate.X=3.f;
  FVector Gain=E.ResourceRate*DT;Players[E.Owner].Resources+=Gain;Players[E.Owner].Cement+=E.CementRate*DT;EmitTrickleResources(E,Gain,0,E.CementRate*DT);
  if(BeastTier(E)>0)for(auto& FK:Entities){auto& F=*FK.Value;if(F.Owner==E.Owner&&F.Progress>=1&&D(F).Id==TEXT("gathering_of_flocks")&&FVector2D::Distance(E.Pos,F.Pos)<=D(F).Radius+8){E.HP=FMath::Min(E.MaxHP,E.HP+4*DT);break;}}
 }
}
float ARTSWorld::CarryCapacity(const FRTSEntity& E) const {
 auto& Def=D(E);float Cap=Def.Id==TEXT("gadianton_robber")||Def.Id==TEXT("corrupt_judge")?80:Def.Id==TEXT("secret_murderer")?40:Def.Kind==TEXT("ammonite")?20:RTS::IsWorker(Def)?10:0;
 for(auto& KV:Entities){auto& Other=*KV.Value;float Distance=FVector2D::Distance(E.Pos,Other.Pos);if(E.Owner==Other.Owner&&RTS::IsWorker(Def)&&Has(E.Owner,TEXT("seven_years_of_provisions"))&&D(Other).Id==TEXT("storehouse_of_provisions")&&Other.Progress>=1&&Distance<=12){Cap+=10;break;}}
 if(Def.Id==TEXT("gadianton_robber")||Def.Id==TEXT("secret_murderer")||Def.Id==TEXT("corrupt_judge"))for(auto& KV:Entities){auto& H=*KV.Value;if(Allied(E.Owner,H.Owner)&&D(H).Id==TEXT("gadianton")&&H.BuffUntil>Time&&FVector2D::Distance(E.Pos,H.Pos)<=10){Cap+=40;break;}}
 return Cap*(1+EconomicBonus(E.Owner,TEXT("carry"))+EconomicBonus(E.Owner,TEXT("carry_capacity")));
}
bool ARTSWorld::CanUseDropoff(const FRTSEntity& E,const FRTSEntity& Site) const {
 if(Site.Progress<1||!Players.IsValidIndex(Site.Owner))return false;auto Id=D(Site).Id;bool Raider=CarryCapacity(E)>20&&!RTS::IsWorker(D(E));
 if(Site.Owner==E.Owner)return Id==TEXT("seat_of_the_people")||Id==TEXT("storehouse_of_provisions")||(Raider&&Id==TEXT("encampment_of_war"));
 return RTS::IsWorker(D(E))&&Id==TEXT("storehouse_of_provisions")&&!Allied(E.Owner,Site.Owner)&&Players[Site.Owner].Faction==TEXT("gadianton")&&Has(Site.Owner,TEXT("signs_and_symbols"));
}
int32 ARTSWorld::Dropoff(const FRTSEntity& E) const {int32 Result=0;float Dist=1e9;for(auto& KV:Entities){auto& B=*KV.Value;if(!CanUseDropoff(E,B))continue;float N=FVector2D::DistSquared(E.Pos,B.Pos);if(N<Dist||(FMath::IsNearlyEqual(N,Dist)&&B.Id<Result)){Dist=N;Result=B.Id;}}return Result;}
FVector ARTSWorld::DepositResources(FRTSEntity& Worker,const FRTSEntity& Site,FVector Gain){
 if(!CanUseDropoff(Worker,Site))return FVector::ZeroVector;
 if(Site.Owner!=Worker.Owner){Players[Site.Owner].Resources+=Gain;EmitResources(Worker,Gain);NotifyRobbery(Worker.Owner,Site.Owner);return Gain;}
 FRTSEntity* Camper=nullptr;for(auto& KV:Entities){auto& Thief=*KV.Value;if((D(Site).Id!=TEXT("seat_of_the_people")&&D(Site).Id!=TEXT("storehouse_of_provisions"))||Thief.Owner<0||Allied(Worker.Owner,Thief.Owner)||Thief.Carrier||D(Thief).bBuilding||Players[Thief.Owner].Faction!=TEXT("gadianton")||Players[Thief.Owner].Favor<=70||!Has(Thief.Owner,TEXT("compromised_storehouses"))||!IsStealthed(Thief)||FVector2D::Distance(Thief.Pos,Site.Pos)>D(Site).Radius+4||!LOS(Thief.Pos,Site.Pos,Thief.Id,Site.Id))continue;if(!Camper||Thief.Id<Camper->Id)Camper=&Thief;}
 FVector Retained=Gain;if(Camper){FVector Stolen=Gain*(Players[Camper->Owner].Favor>=100?1.f:.5f);Retained-=Stolen;Players[Camper->Owner].Resources+=Stolen;EmitResources(*Camper,Stolen);NotifyRobbery(Worker.Owner,Camper->Owner);}
 Players[Worker.Owner].Resources+=Retained;EmitResources(Worker,Retained);return Retained;
}
void ARTSWorld::DropCargo(FRTSEntity& E) {if(CargoTotal(E)<=.001f)return;auto C=Spawn(TEXT("crate"),-1,E.Pos);if(C){C->Cargo=E.Cargo;C->Expiry=Time+30;C->ResourceRemaining=CargoTotal(E);C->HP=C->MaxHP=C->ResourceRemaining;}E.Cargo=FVector::ZeroVector;E.FoodCredit=0;E.bIntercepted=false;}
void ARTSWorld::Divert(int32 Site,int R,float Gross,float& Retained,bool& Intercepted) {
 Retained=Gross;if(Intercepted)return;FRTSEntity* Raider=nullptr;
 for(auto& KV:Entities){auto& E=*KV.Value;if(E.Order==TEXT("raid")&&E.Target==Site&&E.Channel>=3){if(!Raider||E.Id<Raider->Id)Raider=&E;}}
 if(Raider){float Suppressed=Gross*.25f,Available=FMath::Max(0.f,CarryCapacity(*Raider)-CargoTotal(*Raider)),Mult=PlunderMultiplier(Raider->Owner)*(1+EconomicBonus(Raider->Owner,TEXT("raid_yield"))),Taken=FMath::Min(Gross*.3f,Available/FMath::Max(1.f,Mult)),Stolen=Taken*Mult;Raider->Cargo[R]+=Stolen;FVector Gain=FVector::ZeroVector;Gain[R]=Stolen;EmitResources(*Raider,Gain);Retained=Gross-Suppressed-Taken;Intercepted=true;if(Taken>0){auto Victim=Find(Site);if(Victim)NotifyRobbery(Victim->Owner,Raider->Owner);}return;}
 // Giddianhi's temporary suppression never fabricates cargo.
 auto Target=Find(Site);if(Target&&Target->SuppressedUntil>Time)Retained=Gross*.8f;
}
void ARTSWorld::DepleteResource(int32 ResourceId){
 auto Node=Find(ResourceId);if(!Node)return;const FString NodeType=D(*Node).Id;int R=NodeType==TEXT("tree")?1:NodeType==TEXT("ore")||NodeType==TEXT("hill_of_ore")?2:0;
 TSet<int32> Removed;Removed.Add(ResourceId);for(auto& KV:Entities)if(D(*KV.Value).Id==TEXT("hill_of_ore")&&KV.Value->WorkTarget==ResourceId)Removed.Add(KV.Key);
 for(int32 Id:Removed)Entities.Remove(Id);ObstacleCount=-1;NavigationCount=-1;NavigationTime=-100;Obstacles.Empty();
 for(auto& KV:Entities){auto& Worker=*KV.Value;if(!RTS::IsWorker(D(Worker))||!Removed.Contains(Worker.WorkTarget))continue;int Replacement=0;float Best=1e9;
  if(Worker.PendingOrders.IsEmpty())for(auto& CandidateKV:Entities){auto& Candidate=*CandidateKV.Value;auto Id=D(Candidate).Id;if((R==0&&Id!=TEXT("berries")&&Id!=TEXT("wild_game"))||(R==1&&Id!=TEXT("tree"))||(R==2&&Id!=TEXT("ore"))||!CanAssignWork(Worker,Candidate)||!CanSee(Worker.Owner,Candidate))continue;float Dist=FVector2D::DistSquared(Worker.Pos,Candidate.Pos);if(Dist<Best){Best=Dist;Replacement=Candidate.Id;}}
  Worker.WorkTarget=Worker.Target=Replacement;Worker.Path.Empty();if(CargoTotal(Worker)>0)Worker.DropTarget=Dropoff(Worker);if(!Replacement&&!Worker.DropTarget)ResetOrder(Worker,TEXT("idle"));
 }Population();
}
void ARTSWorld::Economy(FRTSEntity& E,float DT) {
 auto& Def=D(E);
 auto RaidSite=[&](const FRTSEntity& Site){auto Id=D(Site).Id;return Site.Owner>=0&&!Allied(E.Owner,Site.Owner)&&!Site.Carrier&&Site.HP>0&&Site.Progress>=1&&((RTS::IsWorker(D(Site))&&CargoTotal(Site)>.001f)||Id==TEXT("seat_of_the_people")||Id==TEXT("storehouse_of_provisions")||Id==TEXT("fields_of_grain")||Id==TEXT("gathering_of_flocks")||Id==TEXT("hill_of_ore")||Id==TEXT("winery"));};
 if(E.Order==TEXT("raid_move")){FRTSEntity* Site=nullptr;float Best=1e9;for(auto& KV:Entities){auto& Candidate=*KV.Value;float Dist=FVector2D::DistSquared(Candidate.Pos,E.Pos);if(!RaidSite(Candidate)||Dist>FMath::Square(Def.Sight+D(Candidate).Radius)||!LOS(E.Pos,Candidate.Pos,E.Id,Candidate.Id)||!CanSee(E.Owner,Candidate))continue;if(Dist<Best||(Dist==Best&&Site&&Candidate.Id<Site->Id)){Site=&Candidate;Best=Dist;}}
  if(Site){E.Target=E.RaidSource=Site->Id;E.Order=TEXT("raid");E.Channel=0;E.Path.Empty();}else{if(Move(E,E.RaidDestination,.35f,DT)){if(CargoTotal(E)>0){E.Order=TEXT("raid_return");E.DropTarget=Dropoff(E);}else{ResetOrder(E,TEXT("idle"));E.bRaidMove=false;}}return;}
 }
 if(E.Order==TEXT("raid")){
  auto T=Find(E.Target);bool Eligible=T&&RaidSite(*T);
  if(!Eligible){E.Channel=0;E.Target=0;E.RaidSource=0;E.Path.Empty();if(CargoTotal(E)>.001f){E.Order=TEXT("raid_return");E.DropTarget=Find(E.TradeB)&&CanUseDropoff(E,*Find(E.TradeB))?E.TradeB:Dropoff(E);}else if(E.bRaidMove)E.Order=TEXT("raid_move");else ResetOrder(E,TEXT("idle"));return;}
  if(CargoTotal(E)>=CarryCapacity(E)-.001f){E.RaidSource=T->Id;E.Order=TEXT("raid_return");E.DropTarget=Find(E.TradeB)&&CanUseDropoff(E,*Find(E.TradeB))?E.TradeB:Dropoff(E);E.Path.Empty();return;}
  float Range=D(*T).Radius+1.5f;if(FVector2D::Distance(E.Pos,T->Pos)>Range){E.Channel=0;Move(E,T->Pos,Range-.2f,DT);return;}
  if(Time-E.LastDamage<.2f||!LOS(E.Pos,T->Pos,E.Id,T->Id)){E.Channel=0;return;}E.Channel+=DT;E.RevealedUntil=Time+5;
  if(E.Channel>=3){
   float Mult=FMath::Max(1.f,PlunderMultiplier(E.Owner)*(1+EconomicBonus(E.Owner,TEXT("raid_yield"))));float Budget=FMath::Min(FMath::Max(0.f,3*DT),FMath::Max(0.f,CarryCapacity(E)-CargoTotal(E))/Mult);FVector BaseTaken=FVector::ZeroVector;
   const bool WorkerVictim=RTS::IsWorker(D(*T));FVector& Stock=WorkerVictim?T->Cargo:Players[T->Owner].Resources;
   // Divide the stock theft evenly among available resources. When one stock
   // empties, redistribute its unused share without favoring the first index.
   for(int Pass=0;Pass<3&&Budget>.00001f;Pass++){int Available=0;for(int R=0;R<3;R++)if(Stock[R]-BaseTaken[R]>.00001f)Available++;if(!Available)break;float Share=Budget/Available,Spent=0;
    for(int R=0;R<3;R++){float AvailableStock=FMath::Max(0.f,float(Stock[R]-BaseTaken[R]));float Take=FMath::Min(AvailableStock,Share);BaseTaken[R]+=Take;Spent+=Take;}Budget=FMath::Max(0.f,Budget-Spent);
   }
   FVector Taken=BaseTaken*Mult;Stock-=BaseTaken;E.Cargo+=Taken;E.FoodCredit=0;E.bIntercepted=true;if(WorkerVictim)T->FoodCredit=FMath::Max(0.f,T->FoodCredit-float(BaseTaken.X));
   if(!Taken.IsNearlyZero()){EmitResources(E,Taken);NotifyRobbery(T->Owner,E.Owner);}else if(CargoTotal(E)>0){E.Order=TEXT("raid_return");E.DropTarget=Dropoff(E);}
  }
  return;
 }
 if(E.Order==TEXT("galavant")){
  if(Def.Kind!=TEXT("trader")||E.TradeRoute.IsEmpty()){ResetOrder(E,TEXT("idle"));return;}E.TradeRouteIndex=FMath::Clamp(E.TradeRouteIndex,0,E.TradeRoute.Num()-1);
  // A route click can land on a farm, market or resource body. Visit its
  // walkable edge instead of waiting forever for an unreachable center.
  FVector2D Waypoint=E.TradeRoute[E.TradeRouteIndex];if(!Walkable(Waypoint,Def.Radius,E.Id,false)){bool Found=false;for(int Ring=1;Ring<=16&&!Found;Ring++){float Best=1e9;FVector2D Approach=Waypoint;for(int I=0;I<32;I++){float A=I*PI/16;FVector2D At=Waypoint+FVector2D(FMath::Cos(A),FMath::Sin(A))*(Ring*.5f);if(!Walkable(At,Def.Radius,E.Id,false))continue;float Distance=FVector2D::DistSquared(E.Pos,At);if(Distance<Best){Best=Distance;Approach=At;Found=true;}}if(Found)Waypoint=Approach;}}
  if(Move(E,Waypoint,.35f,DT)){E.TradeRouteIndex=(E.TradeRouteIndex+1)%E.TradeRoute.Num();E.Path.Empty();}
  FVector Gain=FVector::ZeroVector;for(auto& KV:Entities){auto& Laborer=*KV.Value;if(Laborer.Owner!=E.Owner||Laborer.Carrier||!RTS::IsWorker(D(Laborer))||FVector2D::Distance(E.Pos,Laborer.Pos)>4||!LOS(E.Pos,Laborer.Pos,E.Id,Laborer.Id))continue;auto Last=TraderVisitAt.Find(Laborer.Id);if(Last&&Time-*Last<30)continue;TraderVisitAt.Add(Laborer.Id,Time);Gain.Z+=2;}
  if(!Gain.IsNearlyZero()){Gain*=1+EconomicBonus(E.Owner,TEXT("trade_income"));Players[E.Owner].Resources+=Gain;EmitResources(E,Gain);}return;
 }
 if(E.Order==TEXT("trade")){auto T=Find(E.Target);if(Def.Kind==TEXT("trader")&&T&&T->Owner==E.Owner&&RTS::IsWorker(D(*T))){E.TradeRoute={T->Pos};E.TradeRouteIndex=0;E.Order=TEXT("galavant");}else ResetOrder(E,TEXT("idle"));return;}
 if(E.Order!=TEXT("gather")&&E.Order!=TEXT("deliver")&&E.Order!=TEXT("raid_return"))return;
 if(E.DropTarget||E.Order==TEXT("deliver")||E.Order==TEXT("raid_return")){
  auto Drop=Find(E.DropTarget);if(!Drop||!CanUseDropoff(E,*Drop)){E.DropTarget=Dropoff(E);Drop=Find(E.DropTarget);}if(!Drop)return;
  if(!Move(E,Drop->Pos,D(*Drop).Radius+.65f,DT))return;
  FVector Delivered=DepositResources(E,*Drop,E.Cargo);
  if(Drop->Owner==E.Owner&&Players[E.Owner].Faction==TEXT("nephites")&&E.FoodCredit>0){auto& P=Players[E.Owner];P.GatheredFood+=FMath::Min(float(Delivered.X),E.FoodCredit);P.FoodAwards.RemoveAll([&](float T){return Time-T>=60;});while(P.GatheredFood>=100){P.GatheredFood-=100;if(P.FoodAwards.Num()<3){P.FoodAwards.Add(Time);Favor(E.Owner,1);}}}
  bool Returning=E.Order==TEXT("raid_return");E.Cargo=FVector::ZeroVector;E.FoodCredit=0;E.bIntercepted=false;E.DropTarget=0;E.Path.Empty();
  if(E.bRaidMove&&Returning){E.Order=TEXT("raid_move");E.Target=0;E.RaidSource=0;}
  else if(E.RaidSource&&Find(E.RaidSource)&&RaidSite(*Find(E.RaidSource))){E.Order=TEXT("raid");E.Target=E.RaidSource;E.Channel=0;}
  else if(E.WorkTarget)E.Order=TEXT("gather");else ResetOrder(E,TEXT("idle"));return;
 }
 auto Site=Find(E.WorkTarget);if(!Site){
  int Replacement=0;if(E.PendingOrders.IsEmpty()){float Best=1e9;for(auto& KV:Entities){auto& Node=*KV.Value;auto Id=D(Node).Id;if(Node.ResourceRemaining<=0||!CanSee(E.Owner,Node)||!CanAssignWork(E,Node))continue;if((E.ResourceType==0&&Id!=TEXT("berries")&&Id!=TEXT("wild_game"))||(E.ResourceType==1&&Id!=TEXT("tree"))||(E.ResourceType==2&&Id!=TEXT("ore")))continue;float Dist=FVector2D::DistSquared(E.Pos,Node.Pos);if(Dist<Best){Best=Dist;Replacement=Node.Id;}}}
  E.WorkTarget=Replacement;if(CargoTotal(E)>0)E.DropTarget=Dropoff(E);else if(!Replacement)ResetOrder(E,TEXT("idle"));return;
 }
 auto SiteId=D(*Site).Id;int R=SiteId==TEXT("tree")?1:SiteId==TEXT("ore")||SiteId==TEXT("hill_of_ore")?2:0;
 if(SiteId==TEXT("crate")){
  if(!Move(E,Site->Pos,1.1f,DT))return;float Free=FMath::Max(0.f,CarryCapacity(E)-CargoTotal(E));for(int I=0;I<3;I++){float Take=FMath::Min(float(Site->Cargo[I]),Free);E.Cargo[I]+=Take;Site->Cargo[I]-=Take;Free-=Take;}Site->ResourceRemaining=CargoTotal(*Site);if(CargoTotal(*Site)<.001f)Entities.Remove(Site->Id);E.bIntercepted=true;E.WorkTarget=0;E.DropTarget=Dropoff(E);return;
 }
 const bool Gad=Players[E.Owner].Faction==TEXT("gadianton");const bool ForeignFood=Gad&&Site->Owner>=0&&Site->Owner!=E.Owner&&(SiteId==TEXT("fields_of_grain")||SiteId==TEXT("gathering_of_flocks"));
 if(!RTS::IsWorker(Def)||(Gad&&!ForeignFood&&SiteId!=TEXT("tree")&&SiteId!=TEXT("ore")&&SiteId!=TEXT("hill_of_ore")&&SiteId!=TEXT("berries")&&SiteId!=TEXT("wild_game"))||(Site->Owner>=0&&!Allied(E.Owner,Site->Owner)&&!ForeignFood)||(D(*Site).bBuilding&&Site->Progress<1)){ResetOrder(E,TEXT("idle"));return;}
 FRTSEntity* Deposit=SiteId==TEXT("hill_of_ore")?Find(Site->WorkTarget):Site;
 bool Renewable=SiteId==TEXT("fields_of_grain")||SiteId==TEXT("gathering_of_flocks");bool Natural=SiteId==TEXT("tree")||SiteId==TEXT("ore")||SiteId==TEXT("berries")||SiteId==TEXT("wild_game")||SiteId==TEXT("hill_of_ore");
 if(!Renewable&&!Natural){ResetOrder(E,TEXT("idle"));return;}
 if(!Renewable&&(!Deposit||Deposit->ResourceRemaining<=.001f)){DepleteResource(Deposit?Deposit->Id:Site->Id);return;}
 if(CargoTotal(E)>0&&E.ResourceType!=R){E.DropTarget=Dropoff(E);return;}E.ResourceType=R;
 if(CargoTotal(E)>=CarryCapacity(E)-.001f){E.DropTarget=Dropoff(E);return;}
 if(!Move(E,WorkPosition(E,*Site),.3f,DT))return;
 if(Def.Kind==TEXT("worker")||Def.Kind==TEXT("job")){FString Job=R==2?TEXT("worker_in_ore"):SiteId==TEXT("gathering_of_flocks")?TEXT("keeper_of_flocks"):R==0?TEXT("tiller_of_the_earth"):TEXT("industrious_laborer");E.Def=FRTSCatalog::Get().Index(Job);}
 auto& P=Players[E.Owner];float Factor=Gad?.35f:R==2&&P.Faction==TEXT("coriantumr")?1.15f:R==2&&P.Faction==TEXT("laban")?1.2f:1;
 float Bonuses=(R==0&&Has(E.Owner,TEXT("seeds_of_every_kind"))?.15f:0)+(Has(E.Owner,TEXT("exceedingly_industrious"))?.1f:0);float Rate=(R==0?1.2f:R==1?.7f:.6f)*Factor*(Def.Kind==TEXT("ammonite")?1.5f:1)*(1+Bonuses)*LaborerAuraMultiplier(E)*GatherMultiplier(E.Owner)*(SiteId==TEXT("hill_of_ore")?1.5f:1.f)*(1+EconomicBonus(E.Owner,R==0?TEXT("gather_food"):R==1?TEXT("gather_timber"):TEXT("gather_precious")));
 float Gross=FMath::Min(Rate*DT,FMath::Max(0.f,CarryCapacity(E)-CargoTotal(E)));if(!Renewable)Gross=FMath::Min(Gross,Deposit->ResourceRemaining);
 float Retained;bool Intercept=false;Divert(Site->Id,R,Gross,Retained,Intercept);E.Cargo[R]+=Retained;E.bIntercepted|=Intercept;if(R==0)E.FoodCredit+=Retained;if(ForeignFood&&!Allied(E.Owner,Site->Owner)&&Retained>0)NotifyRobbery(Site->Owner,E.Owner);if(!Renewable){Deposit->ResourceRemaining=FMath::Max(0.f,Deposit->ResourceRemaining-Gross);Deposit->HP=Deposit->ResourceRemaining;if(Deposit->ResourceRemaining<=.001f)DepleteResource(Deposit->Id);}
}
void ARTSWorld::Production(FRTSEntity& E,float DT) {
 auto& Def=D(E);if(!Def.bBuilding)return;
 if(E.Progress<1){TArray<int32> Builders;bool Jared=RTS::IsJaredite(Players[E.Owner].Faction);for(auto& KV:Entities){auto& B=*KV.Value;if(!CanBuildSite(B,E))continue;bool Near=Jared?FVector2D::Distance(B.Pos,E.Pos)<=Def.Radius+8:((B.Order==TEXT("build")&&B.Target==E.Id)&&(FVector2D::Distance(B.Pos,E.Pos)<=Def.Radius+1||FVector2D::Distance(B.Pos,WorkPosition(B,E))<=.8f));if(Near)Builders.Add(B.Id);}if(Builders.IsEmpty())return;
  Builders.Sort();float Rate=0;for(int I=0;I<Builders.Num();I++){int Tier=BeastTier(*Find(Builders[I]));Rate+=(Tier==3?2.f:Tier==2?1.5f:1.f)*LaborerAuraMultiplier(*Find(Builders[I]))/(1.f+.6f*I);}
  Rate*=1+EconomicBonus(E.Owner,TEXT("build_speed"));float Ratio=E.HP/E.MaxHP;E.Progress=FMath::Min(1.f,E.Progress+DT*Rate/FMath::Max(1.f,Def.Seconds));E.LastProgress=Time;E.MaxHP=MaxHP(E);E.HP=E.MaxHP*Ratio;if(E.Progress>=1){for(auto& KV:Entities){auto& B=*KV.Value;if(B.Order==TEXT("build")&&B.Target==E.Id){ResetOrder(B,TEXT("idle"));AutoGather(B,E);}}Population();}return;
 }
 if(E.Jobs.Num()==0||E.PauseUntil>Time)return;auto& Job=E.Jobs[0];auto& JD=FRTSCatalog::Get().Def(Job.Type);float Rate=1;
 if(!JD.bUpgrade&&Def.Id==TEXT("encampment_of_war"))for(auto& KV:Entities){auto& H=*KV.Value;if(H.Owner==E.Owner&&D(H).Id==TEXT("lamoni")&&H.BuffUntil>Time&&FVector2D::Distance(H.Pos,E.Pos)<=12){Rate=1.2f;break;}}
 if(!JD.bUpgrade)Rate*=1+EconomicBonus(E.Owner,TEXT("train_speed"));
 if(!JD.bUpgrade&&Players[E.Owner].Faction==TEXT("lamanites"))Rate*=1+FMath::Clamp(Players[E.Owner].Favor,0.f,100.f)*.005f;
 Job.Remaining=FMath::Max(0.f,Job.Remaining-DT*Rate);if(Job.Remaining>0)return;
 if(JD.Kind==TEXT("age_option")){CompleteAgeChoice(E.Owner,JD,E.Pos,E.Id);E.Jobs.RemoveAt(0);}
 else if(Job.Type==TEXT("gate_segment_upgrade")){E.bGate=true;E.Jobs.RemoveAt(0);NavigationCount=-1;NavigationTime=-100;}
 else if(JD.bUpgrade){auto& P=Players[E.Owner];if(Job.Type==TEXT("age2"))P.Age=2;else if(Job.Type==TEXT("age3"))P.Age=3;else P.Upgrades.AddUnique(Job.Type);E.Jobs.RemoveAt(0);Rescale(E.Owner);if(P.Controller.IsValid())P.Controller->ClientNotice(JD.Name+TEXT(" completed."));}
 else {FVector2D At;bool Found=false;for(int Ring=0;Ring<5&&!Found;Ring++)for(int I=0;I<24;I++){float A=I*PI/12;At=E.Pos+FVector2D(FMath::Cos(A),FMath::Sin(A))*(Def.Radius+JD.Radius+.5f+Ring*.75f);if(Walkable(At,JD.Radius,0,JD.Kind==TEXT("ship"))){Found=true;break;}}
  if(!Found)return;FString Type=Job.Type;E.Jobs.RemoveAt(0);auto Unit=Spawn(Type,E.Owner,At);if(Unit){if(JD.Kind==TEXT("sheep")||JD.Kind==TEXT("beast")){Unit->HomeBuilding=E.Id;Unit->ResourcePulseAt=Time;}ApplyRally(*Unit,E);}
 }Population();
}
