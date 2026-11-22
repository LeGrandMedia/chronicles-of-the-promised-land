#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSWorld.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Four real controllers send the same reliable orders as ordinary players.
// The host seeds isolated idle fixtures only; production, movement, stealing,
// conversion, boarding, unloading, and visibility run through the simulation.
bool RunRTSNetworkV08Probe(ARTSController* C) {
 if(!FParse::Param(FCommandLine::Get(),TEXT("RTSNetV08")))return true;
 struct FState {
  bool Seeded=false,Sent=false,Route=false,NewbornRoute=false,FirstVisit=false,SecondVisit=false,ThirdVisit=false,EconomicRally=false,NewbornWorker=false;
  bool RaidSent=false,RaidRally=false,RaidNewborn=false,CargoLoaded=false,CargoNoDamage=false,CargoVictimChanged=false,GarrisonSent=false,Boarded=false,UnloadSent=false,Unloaded=false;
  bool BribeSent=false,BribeChannel=false,BribeVisible=false,BribeDone=false,BribeRecovery=false,BribeStealthRestored=false,DeathsSent=false,VisibleDeath=false,VisibleDeathNeverSnapshotted=true,HiddenDeathAbsent=true,HiddenDeathAudit=false,Private=true,PrivacyObserved=false,Aura=false,AuraSeen=false,FixtureVisible=false,Reported=false;
  int32 Market=0,Seat=0,Store=0,Hideout=0,Passenger=0,Trader=0,Worker=0,Lawyer=0,BribeTarget=0,RaidTarget=0;
  float WorkerStartDistance=0,TargetHP=0,AuraObservedRadius=-1;TSet<int32> Existing;TArray<FVector2D> ExpectedRoute;
 };
 static TMap<ARTSController*,FState> States;auto& S=States.FindOrAdd(C);const auto Me=C->Me();if(!Me)return false;
 FString Role;FParse::Value(FCommandLine::Get(),TEXT("RTSNetTest="),Role);
 // Keep both bodies and their fog-cell centers clear of map-zero's rock at
 // (80,105). The old y=100 tangent hid the aura cell from enemy observers.
 const auto& Cat=FRTSCatalog::Get();const FVector2D DeathCenter(80,90);
 if(C->Phase==1&&C->LocalSlot==0&&C->MatchTime>=12&&!S.Seeded){S.Seeded=true;
  if(auto GM=Cast<ARTSGameMode>(C->GetWorld()->GetAuthGameMode()))if(auto W=GM->Simulation){
   const auto Clear=[&](FVector2D At,float Radius){if(W->Walkable(At,Radius))return At;for(int Ring=1;Ring<18;++Ring)for(int I=0;I<32;++I){const float A=I*PI/16;const FVector2D P=At+FVector2D(FMath::Cos(A),FMath::Sin(A))*Ring;if(W->Walkable(P,Radius))return P;}return At;};
   TMap<int32,int32> Seats;TArray<int32> Sorted;W->Entities.GetKeys(Sorted);Sorted.Sort();for(int Id:Sorted)if(auto E=W->Find(Id))if(W->D(*E).Id==TEXT("seat_of_the_people")&&!Seats.Contains(E->Owner))Seats.Add(E->Owner,Id);
   for(const auto& Pair:Seats){auto Seat=W->Find(Pair.Value);if(!Seat||!W->Players.IsValidIndex(Pair.Key))continue;const int Slot=Pair.Key;auto& P=W->Players[Slot];P.Age=4;P.Resources+=FVector(1500);P.Cement+=200;
    const FVector2D Forward=(FVector2D(80,80)-Seat->Pos).GetSafeNormal(),Side(-Forward.Y,Forward.X);
    auto Market=W->Spawn(TEXT("place_of_trade"),Slot,Clear(Seat->Pos+Forward*32-Side*10,Cat.Def(TEXT("place_of_trade")).Radius));
    if(Market){Market->bRallySet=true;Market->RallyTarget=801;Market->RallyPoint=Clear(Market->Pos+Forward*6+Side*7,.45f);Market->EconomicRallyPoint=Clear(Market->Pos+Forward*6-Side*7,.45f);Market->RaidRallyPoint=Clear(Market->Pos+Forward*12,.45f);}
    auto EconomicSeat=W->Spawn(TEXT("seat_of_the_people"),Slot,Clear(Seat->Pos+Forward*33+Side*3,Cat.Def(TEXT("seat_of_the_people")).Radius),RTS::IsJaredite(P.Faction)?1:0);
    if(EconomicSeat){EconomicSeat->bRallySet=true;EconomicSeat->RallyTarget=802;EconomicSeat->EconomicRallyPoint=Clear(EconomicSeat->Pos+Forward*13,.45f);}
    for(int I=0;I<3;++I)W->Spawn(TEXT("tents_and_habitations"),Slot,Clear(Seat->Pos-Forward*5+Side*(9+6*I),Cat.Def(TEXT("tents_and_habitations")).Radius));
    if(auto Observer=W->Spawn(TEXT("industrious_laborer"),Slot,DeathCenter+FVector2D(Slot*.3f,0))){Observer->Order=TEXT("hold");Observer->DisarmedUntil=W->Time+1000;Observer->bRallySet=true;Observer->RallyTarget=814;}
    if(P.Faction==TEXT("gadianton")){
     auto Store=W->Spawn(TEXT("storehouse_of_provisions"),Slot,Clear(Seat->Pos+Forward*22-Side*25,Cat.Def(TEXT("storehouse_of_provisions")).Radius));
     if(Store){Store->bRallySet=true;Store->RallyTarget=803;auto Victim=W->Spawn(TEXT("industrious_laborer"),(Slot+1)%W->Players.Num(),Clear(Store->Pos+Forward*8,.45f));if(Victim){Victim->Order=TEXT("hold");Victim->Cargo=FVector(60,0,0);Victim->FoodCredit=0;Victim->bRallySet=true;Victim->RallyTarget=808;Store->RaidRallyPoint=Victim->Pos;Store->RaidRallyTarget=Victim->Id;}}
     auto Hideout=W->Spawn(TEXT("tents_and_habitations"),Slot,Clear(Seat->Pos+Forward*15-Side*8,Cat.Def(TEXT("tents_and_habitations")).Radius));
     if(Hideout){Hideout->bRallySet=true;Hideout->RallyTarget=804;if(auto Passenger=W->Spawn(TEXT("wilderness_spy"),Slot,Clear(Hideout->Pos+FVector2D(Cat.Def(TEXT("tents_and_habitations")).Radius+.6f,0),.45f))){Passenger->Order=TEXT("hold");Passenger->bRallySet=true;Passenger->RallyTarget=805;}}
     auto Lawyer=W->Spawn(TEXT("lawyer"),Slot,DeathCenter-FVector2D(5,0));auto Bribed=W->Spawn(TEXT("industrious_laborer"),(Slot+1)%W->Players.Num(),DeathCenter-FVector2D(3,0));
     if(Lawyer&&Bribed){Lawyer->Order=Bribed->Order=TEXT("hold");Lawyer->bRallySet=true;Lawyer->RallyTarget=811;Lawyer->RallyPoint=Bribed->Pos;Bribed->bRallySet=true;Bribed->RallyTarget=812;}
    }
    if(P.Faction==TEXT("laban"))if(auto Aura=W->Spawn(TEXT("drunken_concubine"),Slot,DeathCenter+FVector2D(4,0))){Aura->Order=TEXT("hold");Aura->bRallySet=true;Aura->RallyTarget=813;}
   }
   // Validate the fixture through real terrain, sight and fog-cell LOS. An
   // owner-only acknowledgment lets every client require this setup evidence.
   for(auto& KV:W->Entities){auto& Observer=*KV.Value;if(Observer.RallyTarget!=814)continue;bool Ready=W->Walkable(Observer.Pos,.45f)&&W->Walkable(DeathCenter,.45f),AuraPresent=false;
    const FVector2D DeathCell(FMath::FloorToDouble(DeathCenter.X/2.5)*2.5+1.25,FMath::FloorToDouble(DeathCenter.Y/2.5)*2.5+1.25);Ready&=W->LOS(Observer.Pos,DeathCell,Observer.Id);
    for(const auto& Other:W->Entities){const auto& Body=*Other.Value;if(Body.RallyTarget<811||Body.RallyTarget>813)continue;const FVector2D Cell(FMath::FloorToDouble(Body.Pos.X/2.5)*2.5+1.25,FMath::FloorToDouble(Body.Pos.Y/2.5)*2.5+1.25);
     Ready&=W->Walkable(Body.Pos,W->D(Body).Radius)&&W->Walkable(Cell,.01f)&&FVector2D::Distance(Observer.Pos,Cell)<W->EffectiveSight(Observer)&&W->LOS(Observer.Pos,Body.Pos,Observer.Id,Body.Id)&&W->LOS(Observer.Pos,Cell,Observer.Id);
     if(Body.RallyTarget==813){AuraPresent=true;Ready&=W->CanSee(Observer.Owner,Body);}
    }Ready&=AuraPresent;Observer.EconomicRallyPoint=FVector2D(88609,Ready?1:0);UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_V08_FIXTURE observer_slot=%d walkable_body_and_cell_los=%d"),Observer.Owner,Ready);
   }W->Population();
  }
 }
 if(C->Phase==1&&C->LocalSlot==0&&C->MatchTime>=48&&!S.DeathsSent){S.DeathsSent=true;
  if(auto GM=Cast<ARTSGameMode>(C->GetWorld()->GetAuthGameMode()))if(auto W=GM->Simulation){
   // Spawn and kill between snapshots. A client cannot infer this death from a
   // disappearing body: only the explicit reliable event can present it.
   if(auto Visible=W->Spawn(TEXT("giddianhi"),-1,DeathCenter)){W->Kill(Visible->Id);}
   bool Found=false;for(int Y=2;Y<158&&!Found;Y+=5)for(int X=2;X<158&&!Found;X+=5){const FVector2D At(X,Y);bool Hidden=true;for(const auto& P:W->Players)Hidden&=!W->VisiblePoint(P.Slot,At);if(!Hidden)continue;if(auto HiddenUnit=W->Spawn(TEXT("teancum"),-1,At)){W->Kill(HiddenUnit->Id);Found=true;}}
   // Owner-only metadata acknowledges that the server actually created an
   // unseen death. No hidden entity ID, position, or state is transmitted.
   for(auto& KV:W->Entities)if(KV.Value->RallyTarget==814)KV.Value->RallyPoint=FVector2D(88608,Found?1:0);
   UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_V08_HIDDEN_DEATH_FIXTURE created=%d"),Found);
  }
 }
 if(C->Phase==1&&C->MatchTime>12.25f&&C->MatchTime<79){
  for(const auto& E:C->Entities){const auto& Def=Cat.Def(E.Def);if(E.Owner==C->LocalSlot){if(E.RallyTarget==801)S.Market=E.Id;if(E.RallyTarget==802)S.Seat=E.Id;if(E.RallyTarget==803)S.Store=E.Id;if(E.RallyTarget==804)S.Hideout=E.Id;if(E.RallyTarget==805)S.Passenger=E.Id;if(E.RallyTarget==811)S.Lawyer=E.Id;
    if(E.RallyTarget==808&&E.Cargo.X<59)S.CargoVictimChanged=true;
    if(E.RallyTarget==814&&E.RallyPoint.Equals(FVector2D(88608,1),.01f))S.HiddenDeathAudit=true;
    if(E.RallyTarget==814&&E.EconomicRallyPoint.Equals(FVector2D(88609,1),.01f))S.FixtureVisible=true;
   }else S.Private&=E.Cargo.IsNearlyZero()&&E.GalavantRoute.IsEmpty()&&!E.bRaidRallySet&&E.RaidRallyTarget==0&&E.RaidRallyPoint.IsNearlyZero()&&E.PassengerIds.IsEmpty()&&E.PassengerDefs.IsEmpty();
   if(Def.Id==TEXT("drunken_concubine")&&FVector2D::Distance(E.Pos,DeathCenter)<8){S.AuraSeen=true;S.AuraObservedRadius=E.AuraRadius;S.Aura|=FMath::IsNearlyEqual(E.AuraRadius,5.f);}
   if(Def.Id==TEXT("lawyer")&&E.Owner==0&&FVector2D::Distance(E.Pos,DeathCenter)<10&&E.ConversionProgress>.02f&&E.ConversionProgress<.98f){S.BribeChannel=true;S.BribeVisible|=!E.bStealthed;}
   if(E.Owner==-1&&Def.Id==TEXT("giddianhi"))S.VisibleDeathNeverSnapshotted=false;
   if(E.Owner==-1&&Def.Id==TEXT("teancum"))S.HiddenDeathAbsent=false;
  }
  S.PrivacyObserved|=S.Route&&C->Entities.Num()>20;
  const auto Market=C->FindEntity(S.Market),Seat=C->FindEntity(S.Seat),Store=C->FindEntity(S.Store),Hideout=C->FindEntity(S.Hideout),Lawyer=C->FindEntity(S.Lawyer);
  if(Market&&Seat&&!S.Sent){S.Sent=true;for(const auto& E:C->Entities)S.Existing.Add(E.Id);S.ExpectedRoute={Market->RallyPoint,Market->EconomicRallyPoint,Market->RaidRallyPoint};
   for(int I=0;I<S.ExpectedRoute.Num();++I)C->ServerOrder(TEXT("market_galavant"),{Market->Id},S.ExpectedRoute[I],0,TEXT(""),I>0);
   if(Cat.Faction(Me->Faction).Units.Contains(TEXT("trader_in_precious_things")))C->ServerOrder(TEXT("train"),{Market->Id},FVector2D::ZeroVector,0,TEXT("trader_in_precious_things"));C->ServerOrder(TEXT("rally_economic"),{Seat->Id},Seat->EconomicRallyPoint,0,TEXT(""));C->ServerOrder(TEXT("train"),{Seat->Id},FVector2D::ZeroVector,0,TEXT("industrious_laborer"));
  }
  if(Market&&S.Sent){bool Same=Market->GalavantRoute.Num()==3;for(int I=0;I<Market->GalavantRoute.Num()&&I<S.ExpectedRoute.Num();++I)Same&=Market->GalavantRoute[I].Equals(S.ExpectedRoute[I],.01f);S.Route|=Same;}
  if(Seat)S.EconomicRally|=Seat->bEconomicRallySet;
  for(const auto& E:C->Entities)if(S.Sent&&E.Owner==C->LocalSlot&&!S.Existing.Contains(E.Id)){
   const auto& Def=Cat.Def(E.Def);
   if(Def.Kind==TEXT("trader")&&E.GalavantRoute.Num()==3&&E.Status==TEXT("galavant")){S.Trader=E.Id;S.NewbornRoute=true;}
   if(Def.Id==TEXT("industrious_laborer")&&Seat&&E.Status==TEXT("move")&&FVector2D::Distance(E.Pos,Seat->Pos)<8&&!S.Worker){S.Worker=E.Id;S.WorkerStartDistance=FVector2D::Distance(E.Pos,Seat->EconomicRallyPoint);}
   if(Def.Id==TEXT("gadianton_robber")&&Store&&(E.Status==TEXT("raid")||E.Status==TEXT("raid_move")||E.Status==TEXT("raid_return"))){S.RaidNewborn=true;S.CargoLoaded|=E.Cargo.X>0;}
  }
  if(auto Trader=C->FindEntity(S.Trader))if(S.ExpectedRoute.Num()==3){S.FirstVisit|=FVector2D::Distance(Trader->Pos,S.ExpectedRoute[0])<3;S.SecondVisit|=S.FirstVisit&&FVector2D::Distance(Trader->Pos,S.ExpectedRoute[1])<3;S.ThirdVisit|=S.SecondVisit&&FVector2D::Distance(Trader->Pos,S.ExpectedRoute[2])<3;}
  if(auto Worker=C->FindEntity(S.Worker))if(Seat)S.NewbornWorker|=S.WorkerStartDistance-FVector2D::Distance(Worker->Pos,Seat->EconomicRallyPoint)>2;
  if(Store&&!S.RaidSent){S.RaidSent=true;S.RaidTarget=Store->RaidRallyTarget;if(auto Target=C->FindEntity(S.RaidTarget))S.TargetHP=Target->HP;C->ServerOrder(TEXT("storehouse_raid"),{Store->Id},Store->RaidRallyPoint,Store->RaidRallyTarget,TEXT(""));C->ServerOrder(TEXT("train"),{Store->Id},FVector2D::ZeroVector,0,TEXT("gadianton_robber"));}
  if(Store)S.RaidRally|=Store->bRaidRallySet&&Store->RaidRallyTarget==S.RaidTarget;
  if(S.CargoLoaded&&S.TargetHP>0)if(auto Target=C->FindEntity(S.RaidTarget))S.CargoNoDamage|=FMath::IsNearlyEqual(Target->HP,S.TargetHP,.01f);
  if(Hideout&&S.Passenger&&!S.GarrisonSent){S.GarrisonSent=true;C->ServerOrder(TEXT("context"),{S.Passenger},Hideout->Pos,Hideout->Id,TEXT(""));}
  if(Hideout&&Hideout->PassengerIds.Contains(S.Passenger)&&!C->FindEntity(S.Passenger)){S.Boarded=true;if(!S.UnloadSent){S.UnloadSent=true;C->ServerOrder(TEXT("ability"),{Hideout->Id},Hideout->Pos,0,TEXT(""));}}
  if(S.Boarded&&Hideout&&Hideout->PassengerIds.IsEmpty())if(auto Passenger=C->FindEntity(S.Passenger))S.Unloaded|=FVector2D::Distance(Passenger->Pos,Hideout->Pos)>Cat.Def(Hideout->Def).Radius+.3f;
  if(Lawyer&&C->MatchTime>=66&&!S.BribeSent){for(const auto& E:C->Entities)if(E.Pos.Equals(Lawyer->RallyPoint,.1f)&&Cat.Def(E.Def).Id==TEXT("industrious_laborer")&&E.Owner!=C->LocalSlot){S.BribeTarget=E.Id;S.BribeSent=true;C->ServerOrder(TEXT("context"),{Lawyer->Id},E.Pos,E.Id,TEXT(""));break;}}
  if(Lawyer&&S.BribeSent)if(auto Target=C->FindEntity(S.BribeTarget))if(Target->Owner==C->LocalSlot&&Target->bConverted){S.BribeDone=true;S.BribeRecovery|=!Lawyer->bStealthed;S.BribeStealthRestored|=S.BribeRecovery&&Lawyer->bStealthed;}
  for(const auto& Death:C->DeathAnimations){const auto& E=Death.Entity;const FString Id=Cat.Def(E.Def).Id;if(E.Owner==-1&&Id==TEXT("giddianhi"))S.VisibleDeath|=E.Status==TEXT("death")&&E.HP==0&&E.Cargo.IsNearlyZero()&&E.GalavantRoute.IsEmpty()&&E.PassengerIds.IsEmpty();if(E.Owner==-1&&Id==TEXT("teancum"))S.HiddenDeathAbsent=false;}
 }
 const bool Gadianton=Me->Faction==TEXT("gadianton");
 const bool TrainsTrader=Cat.Faction(Me->Faction).Units.Contains(TEXT("trader_in_precious_things"));
 const bool EconomyPassed=S.Route&&(!TrainsTrader||(S.NewbornRoute&&S.ThirdVisit))&&S.EconomicRally&&S.NewbornWorker;
 const bool GadiantonPassed=!Gadianton||(S.RaidRally&&S.RaidNewborn&&S.CargoLoaded&&S.CargoNoDamage&&S.Boarded&&S.Unloaded&&S.BribeDone&&S.BribeRecovery&&S.BribeStealthRestored);
 const bool Passed=C->Players.Num()==4&&EconomyPassed&&GadiantonPassed&&(C->LocalSlot!=1||S.CargoVictimChanged)&&S.BribeChannel&&S.BribeVisible&&S.VisibleDeath&&S.VisibleDeathNeverSnapshotted&&S.HiddenDeathAudit&&S.HiddenDeathAbsent&&S.Private&&S.PrivacyObserved&&S.FixtureVisible&&S.Aura;
 if(C->Phase==2&&!S.Reported){S.Reported=true;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("role"),Role);R->SetNumberField(TEXT("slot"),C->LocalSlot);R->SetStringField(TEXT("faction"),Me->Faction);R->SetNumberField(TEXT("team"),Me->Team);R->SetBoolField(TEXT("route_rpc_round_trip"),S.Route);R->SetBoolField(TEXT("trader_training_supported"),TrainsTrader);R->SetBoolField(TEXT("newborn_trader_inherits_and_visits_three_points"),!TrainsTrader||(S.NewbornRoute&&S.ThirdVisit));R->SetBoolField(TEXT("economic_rally_newborn_moves"),S.EconomicRally&&S.NewbornWorker);R->SetBoolField(TEXT("gadianton_specific"),Gadianton);R->SetBoolField(TEXT("storehouse_raid_rally_newborn_and_real_cargo"),S.RaidRally&&S.RaidNewborn&&S.CargoLoaded&&S.CargoNoDamage);R->SetBoolField(TEXT("cargo_victim_observed_transfer"),S.CargoVictimChanged);R->SetBoolField(TEXT("hideout_real_rpc_board_and_exterior_unload"),S.Boarded&&S.Unloaded);R->SetBoolField(TEXT("bribe_channel_visible"),S.BribeChannel&&S.BribeVisible);R->SetBoolField(TEXT("bribe_owner_completion_recovery_and_stealth"),S.BribeDone&&S.BribeRecovery&&S.BribeStealthRestored);R->SetBoolField(TEXT("reliable_death_without_prior_snapshot"),S.VisibleDeath&&S.VisibleDeathNeverSnapshotted);R->SetBoolField(TEXT("hidden_death_server_fixture_confirmed"),S.HiddenDeathAudit);R->SetBoolField(TEXT("hidden_death_never_leaked"),S.HiddenDeathAudit&&S.HiddenDeathAbsent);R->SetBoolField(TEXT("remote_cargo_routes_garrison_privacy"),S.Private&&S.PrivacyObserved);R->SetBoolField(TEXT("fixture_walkable_body_and_fog_cell_los"),S.FixtureVisible);R->SetBoolField(TEXT("aura_entity_seen"),S.AuraSeen);R->SetNumberField(TEXT("observed_aura_radius"),S.AuraObservedRadius);R->SetBoolField(TEXT("public_innate_aura_radius"),S.Aura);R->SetBoolField(TEXT("passed"),Passed);FString JSON;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/FString::Printf(TEXT("network-v08-slot%d.json"),C->LocalSlot)));UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_V08 slot=%d passed=%d %s"),C->LocalSlot,Passed,*JSON);}
 return Passed;
}
