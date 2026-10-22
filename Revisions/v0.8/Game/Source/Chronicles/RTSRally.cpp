#include "RTSWorld.h"

void ARTSWorld::SetRally(FRTSEntity& Building,FVector2D Point,int32 Target) {
 if(!FRTSCatalog::Get().IsUnitProducer(D(Building)))return;
 Building.Rally=Point;Building.RallyPoint=Point;Building.bRallySet=true;Building.RallyTarget=Target;
}
void ARTSWorld::SetEconomicRally(FRTSEntity& Building,FVector2D Point,int32 Target){
 if(D(Building).Id!=TEXT("seat_of_the_people"))return;
 Building.bEconomicRallySet=true;Building.EconomicRallyPoint=Point;Building.EconomicRallyTarget=Target;
}
void ARTSWorld::ApplyRally(FRTSEntity& Unit,const FRTSEntity& Building) {
 ResetOrder(Unit,TEXT("idle"));
 if(D(Building).Id==TEXT("storehouse_of_provisions")&&Building.bRaidRallySet&&Players.IsValidIndex(Unit.Owner)&&Players[Unit.Owner].Faction==TEXT("gadianton")&&(D(Unit).Id==TEXT("gadianton_robber")||D(Unit).Id==TEXT("secret_murderer")||D(Unit).Id==TEXT("corrupt_judge"))){
  Unit.TradeB=Building.Id;Unit.RaidDestination=Unit.Rally=Unit.Destination=Building.RaidRallyPoint;
  auto Victim=Find(Building.RaidRallyTarget);if(Victim&&Victim->Owner>=0&&!Allied(Unit.Owner,Victim->Owner)&&Victim->Progress>=1&&!Victim->Carrier&&(!RTS::IsWorker(D(*Victim))||Victim->Cargo.X+Victim->Cargo.Y+Victim->Cargo.Z>.001f)&&CanSee(Unit.Owner,*Victim)){Unit.Order=TEXT("raid");Unit.Target=Unit.RaidSource=Victim->Id;}else{Unit.Order=TEXT("raid_move");Unit.bRaidMove=true;}return;
 }
 if(D(Unit).Kind==TEXT("trader")&&!Building.GalavantRoute.IsEmpty()){Unit.TradeRoute=Unit.GalavantRoute=Building.GalavantRoute;Unit.TradeRouteIndex=0;Unit.Order=TEXT("galavant");Unit.Rally=Unit.Destination=Unit.TradeRoute[0];return;}
 const bool Economic=RTS::IsWorker(D(Unit))||D(Unit).Kind==TEXT("beast")||D(Unit).Kind==TEXT("trader")||D(Unit).Kind==TEXT("sheep");
 const bool HQEconomic=D(Building).Id==TEXT("seat_of_the_people")&&Economic;
 if(HQEconomic?!Building.bEconomicRallySet:!Building.bRallySet)return;
 FVector2D Point=HQEconomic?Building.EconomicRallyPoint:Building.RallyPoint;int Target=HQEconomic?Building.EconomicRallyTarget:Building.RallyTarget;
 Unit.Order=TEXT("move");Unit.Rally=Unit.Destination=Point;
 auto Site=Find(Target);if(!Site||!CanAssignWork(Unit,*Site))return;
 Unit.Order=TEXT("gather");Unit.WorkTarget=Unit.Target=Site->Id;
}
