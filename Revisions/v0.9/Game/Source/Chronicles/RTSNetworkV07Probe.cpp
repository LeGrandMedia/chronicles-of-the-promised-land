#include "RTSNetworkV07Probe.h"
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

namespace {
 struct FGateCommandObservation {
  FVector Before=FVector::ZeroVector;
  float CementBefore=0,Time=0;
  bool GateBefore=false,UpgradeBefore=false;
  FString JobsBefore;
  TArray<TSharedPtr<FJsonValue>> JobDetailsBefore;
 };
 TMap<int32,FGateCommandObservation> GateCommandObservations;
 TArray<TSharedPtr<FJsonValue>> AuditVector(FVector Value){return {MakeShared<FJsonValueNumber>(Value.X),MakeShared<FJsonValueNumber>(Value.Y),MakeShared<FJsonValueNumber>(Value.Z)};}
 TArray<TSharedPtr<FJsonValue>> AuditJobs(const FRTSEntity& Wall){TArray<TSharedPtr<FJsonValue>> Jobs;for(const auto& J:Wall.Jobs){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),J.Type);O->SetNumberField(TEXT("remaining"),J.Remaining);O->SetNumberField(TEXT("total"),J.Total);O->SetNumberField(TEXT("population"),J.Pop);O->SetNumberField(TEXT("cement_paid"),J.CementPaid);O->SetArrayField(TEXT("resources_paid"),AuditVector(J.Paid));Jobs.Add(MakeShared<FJsonValueObject>(O));}return Jobs;}
 FString AuditJobSignature(const FRTSEntity& Wall){auto O=MakeShared<FJsonObject>();O->SetArrayField(TEXT("jobs"),AuditJobs(Wall));FString JSON;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&JSON));return JSON;}
 constexpr double GateAuditAcknowledgment=77007;
}
void RTSNetworkV07OrderAudit(ARTSController* C,ARTSWorld* W,const FString& Action,const TArray<int32>& Ids,const FString& Type,bool bAfter){
#if !UE_BUILD_SHIPPING
 if(!FParse::Param(FCommandLine::Get(),TEXT("RTSNetV07"))||!C||!W||!C->HasAuthority()||Action!=TEXT("research")||Type!=TEXT("gate_segment_upgrade")||Ids.Num()!=1||!W->Players.IsValidIndex(C->ServerSlot))return;
 auto Wall=W->Find(Ids[0]);if(!Wall||Wall->Owner!=C->ServerSlot||(Wall->RallyTarget!=714&&Wall->RallyTarget!=716))return;
 auto& Player=W->Players[C->ServerSlot];if(Wall->RallyTarget!=716&&FRTSCatalog::Get().Faction(Player.Faction).Upgrades.Contains(Type))return;
 if(!bAfter){FGateCommandObservation O;O.Before=Player.Resources;O.CementBefore=Player.Cement;O.Time=W->Time;O.GateBefore=Wall->bGate;O.UpgradeBefore=Player.Upgrades.Contains(Type);O.JobsBefore=AuditJobSignature(*Wall);O.JobDetailsBefore=AuditJobs(*Wall);GateCommandObservations.Add(Wall->Id,MoveTemp(O));return;}
 const auto Before=GateCommandObservations.Find(Wall->Id);if(!Before)return;
 const FVector ResourceDelta=Player.Resources-Before->Before,Abs=ResourceDelta.GetAbs();const float CementDelta=Player.Cement-Before->CementBefore;
 const bool ResourcesUnchanged=ResourceDelta.IsZero(),CementUnchanged=CementDelta==0,JobsUnchanged=Before->JobsBefore==AuditJobSignature(*Wall),GateUnchanged=Before->GateBefore==Wall->bGate,UpgradeUnchanged=Before->UpgradeBefore==Player.Upgrades.Contains(Type);
 const double Mutation=Abs.X+Abs.Y+Abs.Z+FMath::Abs(CementDelta)+(JobsUnchanged&&GateUnchanged&&UpgradeUnchanged?0:100000);
 // Acknowledgment is carried by this opt-in fixture's existing owner-only rally
 // metadata. It proves this exact remote RPC was observed on the server. Zero Y
 // means every resource, cement, job, gate and upgrade field was unchanged.
 Wall->RallyPoint=FVector2D(GateAuditAcknowledgment,Mutation);
 auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("slot"),C->ServerSlot);R->SetStringField(TEXT("faction"),Player.Faction);R->SetNumberField(TEXT("wall_id"),Wall->Id);R->SetNumberField(TEXT("simulation_time"),Before->Time);R->SetStringField(TEXT("action"),Action);R->SetStringField(TEXT("research"),Type);R->SetBoolField(TEXT("research_supported_by_faction"),FRTSCatalog::Get().Faction(Player.Faction).Upgrades.Contains(Type));R->SetBoolField(TEXT("invalid_non_wall_producer"),Wall->RallyTarget==716);R->SetStringField(TEXT("rejection_case"),Wall->RallyTarget==716?TEXT("non_wall_producer"):TEXT("unsupported_faction"));R->SetArrayField(TEXT("resources_before"),AuditVector(Before->Before));R->SetArrayField(TEXT("resources_after"),AuditVector(Player.Resources));R->SetArrayField(TEXT("resource_delta"),AuditVector(ResourceDelta));R->SetNumberField(TEXT("cement_before"),Before->CementBefore);R->SetNumberField(TEXT("cement_after"),Player.Cement);R->SetNumberField(TEXT("cement_delta"),CementDelta);R->SetArrayField(TEXT("jobs_before"),Before->JobDetailsBefore);R->SetArrayField(TEXT("jobs_after"),AuditJobs(*Wall));R->SetBoolField(TEXT("gate_before"),Before->GateBefore);R->SetBoolField(TEXT("gate_after"),Wall->bGate);R->SetBoolField(TEXT("upgrade_before"),Before->UpgradeBefore);R->SetBoolField(TEXT("upgrade_after"),Player.Upgrades.Contains(Type));R->SetBoolField(TEXT("exactly_zero_cost"),ResourcesUnchanged&&CementUnchanged);R->SetBoolField(TEXT("jobs_and_gate_unchanged"),JobsUnchanged&&GateUnchanged&&UpgradeUnchanged);R->SetStringField(TEXT("rejection_notice_evidence"),FString::Printf(TEXT("network-v07-slot%d.json: unsupported_gate_research_notice and unsupported_gate_research_rejected, observed by the actual remote client"),C->ServerSlot));R->SetBoolField(TEXT("passed"),ResourcesUnchanged&&CementUnchanged&&JobsUnchanged&&GateUnchanged&&UpgradeUnchanged);
 FString JSON;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/FString::Printf(TEXT("network-v07-audit-slot%d-wall%d.json"),C->ServerSlot,Wall->Id)));UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_V07_GATE_AUDIT %s"),*JSON);GateCommandObservations.Remove(Wall->Id);
#endif
}

// Only -RTSNetV07 enables these isolated bodies and structures. Ordinary probe
// workers and their starting resources/factions remain available to the base test.
bool RunRTSNetworkV07Probe(ARTSController* C) {
 if(!FParse::Param(FCommandLine::Get(),TEXT("RTSNetV07")))return true;
 struct FState {bool Seeded=false,Lobby=false,ResetSeen=false,ReadyAgain=false,Sent=false,Channel=false,Converted=false,FiveSeconds=false,ImmunitySent=false,ImmunityRejected=false,ImmunityStable=false,Foundation=false,Gate1Sent=false,Gate2Sent=false,PerWall=false,Gates=false,UnsupportedGateRejected=false,UnsupportedGateNoCharge=false,UnsupportedGateAudit=false,OwnRates=false,PublicStats=false,Privacy=false,Reported=false;
  int32 ConvertedId=0,ImmunityTarget=0,ImmunityOwner=-1,GateAuditWallId=0;float Started=-1,LastProgress=0,ConvertedAt=-1,ImmunityAt=-1,UnsupportedGateAt=-1;FVector UnsupportedGateResources=FVector::ZeroVector,UnsupportedResourcesAfterAudit=FVector::ZeroVector;FString UnsupportedGateNotice;double UnsupportedAuditMutation=-1;};
 static TMap<ARTSController*,FState> States;auto& S=States.FindOrAdd(C);const auto Me=C->Me();if(!Me)return false;
 const auto& Cat=FRTSCatalog::Get();const bool GateSupported=Cat.Faction(Me->Faction).Upgrades.Contains(TEXT("gate_segment_upgrade"));
 S.Lobby|=Me->LobbyResources==1&&Me->LobbyFruitfulness==2&&Me->LobbySpeed==1&&Me->bLobbyFog;
 if(C->Phase==0&&S.Lobby&&!Me->bReady)S.ResetSeen=true;
 S.ReadyAgain|=S.ResetSeen&&Me->bReady;
 if(C->Phase==1&&C->LocalSlot==0&&C->MatchTime>=10&&!S.Seeded){S.Seeded=true;
  if(auto GM=Cast<ARTSGameMode>(C->GetWorld()->GetAuthGameMode()))if(auto W=GM->Simulation){
   TArray<int32> Seats;for(const auto& KV:W->Entities)if(W->D(*KV.Value).Id==TEXT("seat_of_the_people"))Seats.Add(KV.Key);Seats.Sort();
   TSet<int32> SeededOwners;
   const auto Clear=[&](FVector2D Desired,float Radius){if(W->Walkable(Desired,Radius))return Desired;for(int32 Ring=1;Ring<=14;++Ring)for(int32 N=0;N<24;++N){const float A=N*PI/12;const FVector2D P=Desired+FVector2D(FMath::Cos(A),FMath::Sin(A))*Ring;if(W->Walkable(P,Radius))return P;}return Desired;};
   for(int32 SeatId:Seats)if(auto Seat=W->Find(SeatId)){const int32 Slot=Seat->Owner;if(!W->Players.IsValidIndex(Slot)||SeededOwners.Contains(Slot))continue;SeededOwners.Add(Slot);
    auto& P=W->Players[Slot];P.Resources+=FVector(400);P.Cement+=100;P.Age=FMath::Max(2,P.Age);
    const FVector2D Forward=(FVector2D(80,80)-Seat->Pos).GetSafeNormal(),Side(-Forward.Y,Forward.X);
    // The fixture Seat supplies capacity without satisfying the ordinary test's
    // completed-Tents requirement or replacing any initial worker/order.
    W->Spawn(TEXT("seat_of_the_people"),Slot,Clear(Seat->Pos+Forward*24-Side*18,Cat.Def(TEXT("seat_of_the_people")).Radius),RTS::IsJaredite(P.Faction)?1:0);
    FVector2D Anchor=Seat->Pos+Forward*22+Side*15;bool ClearChannel=false;
    for(int32 Ring=0;Ring<=12&&!ClearChannel;++Ring)for(int32 N=0;N<24&&!ClearChannel;++N){const float A=N*PI/12;const FVector2D Candidate=Anchor+FVector2D(FMath::Cos(A),FMath::Sin(A))*Ring;
     if(W->Walkable(Candidate,.5f)&&W->Walkable(Candidate+Forward*2,.5f)&&W->Walkable(Candidate+Forward*7,.5f)&&W->LOS(Candidate,Candidate+Forward*2)&&W->LOS(Candidate+Forward*7,Candidate+Forward*2)){Anchor=Candidate;ClearChannel=true;}}
    const FVector2D TargetAt=Anchor+Forward*2;
    auto Converter=W->Spawn(P.Faction==TEXT("nephites")?TEXT("teacher_of_the_word"):P.Faction==TEXT("laban")?TEXT("drunken_concubine"):P.Faction==TEXT("gadianton")?TEXT("lawyer"):TEXT("priestcrafter"),Slot,Anchor);
    const int32 Enemy=(Slot+1)%W->Players.Num();auto Target=W->Spawn(TEXT("cimeter_bearer"),Enemy,TargetAt);
    auto Counter=W->Spawn(TEXT("priestcrafter"),Enemy,Anchor+Forward*7);
    if(Converter){Converter->Order=TEXT("hold");Converter->bRallySet=true;Converter->RallyTarget=710;Converter->RallyPoint=TargetAt;}
    if(Target){Target->Order=TEXT("hold");Target->DisarmedUntil=W->Time+100;Target->bRallySet=true;Target->RallyTarget=711;Target->RallyPoint=Anchor;}
    if(Counter){Counter->Order=TEXT("hold");Counter->bRallySet=true;Counter->RallyTarget=712;Counter->RallyPoint=TargetAt;}
    W->Spawn(TEXT("sheep"),Slot,Anchor-Side*5);
    // Neutral tower cannot participate in fighting; its real public combat
    // stats still travel through authoritative remote snapshots.
    FVector2D TowerAt=Clear(Anchor+Side*8,Cat.Def(TEXT("cement_tower")).Radius);TowerAt.X=FMath::FloorToDouble(TowerAt.X/2.5)*2.5+1.25;TowerAt.Y=FMath::FloorToDouble(TowerAt.Y/2.5)*2.5+1.25;W->Spawn(TEXT("cement_tower"),-1,TowerAt);
    const TCHAR* WallType=RTS::IsJaredite(P.Faction)?TEXT("cement_wall"):TEXT("works_of_defense");
    for(int32 I=0;I<2;++I)if(auto Wall=W->Spawn(WallType,Slot,Clear(Seat->Pos+Side*(18+I*5)+Forward*8,Cat.Def(WallType).Radius),RTS::IsJaredite(P.Faction)?3:0)){Wall->bRallySet=true;Wall->RallyTarget=714+I;}
    // Every current faction supports gates. A non-wall remains a genuinely
    // invalid gate producer, exercising rejection without obsolete faction bans.
    if(auto Invalid=W->Spawn(TEXT("storehouse_of_provisions"),Slot,Clear(Seat->Pos-Side*12+Forward*17,Cat.Def(TEXT("storehouse_of_provisions")).Radius))){Invalid->bRallySet=true;Invalid->RallyTarget=716;}
    FVector2D First=Seat->Pos+Forward*10-Side*12;bool Found=false;const float Radius=Cat.Def(TEXT("tents_and_habitations")).Radius;
    for(int32 Ring=0;Ring<=12&&!Found;++Ring)for(int32 N=0;N<24&&!Found;++N){const float A=N*PI/12;const FVector2D Test=First+FVector2D(FMath::Cos(A),FMath::Sin(A))*Ring;bool Free=true;for(int32 I=0;I<3;++I)Free&=W->Walkable(Test+Side*(I*6),Radius);if(Free){First=Test;Found=true;}}
    if(Found)if(auto Builder=W->Spawn(RTS::IsJaredite(P.Faction)?TEXT("curelom"):TEXT("industrious_laborer"),Slot,Clear(First-Forward*5,RTS::IsJaredite(P.Faction)?1.05f:.45f))){Builder->Order=TEXT("hold");Builder->bRallySet=true;Builder->RallyTarget=713;Builder->RallyPoint=First;}
   }
   W->Population();
  }
 }
 // Only shorten work already accepted from a client's research RPC. Normal
 // Production performs the gate mutation and deducts the real research cost.
 if(C->Phase==1&&C->LocalSlot==0)if(auto GM=Cast<ARTSGameMode>(C->GetWorld()->GetAuthGameMode()))if(auto W=GM->Simulation)for(auto& KV:W->Entities)if(KV.Value->RallyTarget==714||KV.Value->RallyTarget==715)for(auto& Job:KV.Value->Jobs)if(Job.Type==TEXT("gate_segment_upgrade"))Job.Remaining=FMath::Min(Job.Remaining,.1f);
 if(C->Phase==1&&C->MatchTime>10.25f&&C->MatchTime<79){
  const FRTSEntityView *Converter=nullptr,*Counter=nullptr,*Builder=nullptr,*Wall1=nullptr,*Wall2=nullptr,*InvalidGate=nullptr,*Seat=nullptr;
  for(const auto& E:C->Entities)if(E.Owner==C->LocalSlot&&!E.bRemembered){if(E.RallyTarget==710)Converter=&E;if(E.RallyTarget==712)Counter=&E;if(E.RallyTarget==713)Builder=&E;if(E.RallyTarget==714)Wall1=&E;if(E.RallyTarget==715)Wall2=&E;if(E.RallyTarget==716)InvalidGate=&E;if(Cat.Def(E.Def).Id==TEXT("seat_of_the_people")&&(!Seat||E.Id<Seat->Id))Seat=&E;}
  const FVector2D Forward=Seat?(FVector2D(80,80)-Seat->Pos).GetSafeNormal():FVector2D(1,0),Side(-Forward.Y,Forward.X);
  if(Converter&&Builder&&C->MatchTime>=60&&!S.Sent){const FRTSEntityView* Target=nullptr;for(const auto& E:C->Entities)if(E.Pos.Equals(Converter->RallyPoint,.1f)&&Cat.Def(E.Def).Id==TEXT("cimeter_bearer"))Target=&E;
   if(Target){S.Sent=true;S.ConvertedId=Target->Id;C->ServerOrder(TEXT("context"),{Converter->Id},Target->Pos,Target->Id,TEXT(""));for(int32 I=0;I<3;++I)C->ServerOrder(TEXT("build"),{Builder->Id},Builder->RallyPoint+Side*(I*6),0,TEXT("tents_and_habitations"),I>0);}
  }
  if(Builder){int32 Found=0;for(int32 I=0;I<3;++I)for(const auto& E:C->Entities)if(E.Owner==C->LocalSlot&&Cat.Def(E.Def).Id==TEXT("tents_and_habitations")&&E.Progress<1&&E.Pos.Equals(Builder->RallyPoint+Side*(I*6),.2f)){++Found;break;}S.Foundation|=Found==3;}
  if(Converter&&S.Sent&&Converter->ConversionProgress>0){if(S.Started<0||Converter->ConversionProgress<S.LastProgress-.02f)S.Started=C->MatchTime-Converter->ConversionProgress*5;S.LastProgress=Converter->ConversionProgress;S.Channel|=Converter->ConversionProgress>.02f&&Converter->ConversionProgress<.95f;}
  if(S.ConvertedId)if(const auto Target=C->FindEntity(S.ConvertedId))if(Target->Owner==C->LocalSlot&&Target->bConverted&&!S.Converted){S.Converted=true;S.ConvertedAt=C->MatchTime;const float Duration=S.Started<0?0:C->MatchTime-S.Started;S.FiveSeconds=S.Channel&&Duration>=4.85f&&Duration<=5.8f;C->ServerOrder(TEXT("hold"),{S.ConvertedId},FVector2D::ZeroVector,0,TEXT(""));C->ServerOrder(TEXT("hold"),{Converter?Converter->Id:0},FVector2D::ZeroVector,0,TEXT(""));}
  if(Counter&&!S.ImmunitySent){const FRTSEntityView* Target=nullptr;for(const auto& E:C->Entities)if(E.Pos.Equals(Counter->RallyPoint,.1f)&&E.bConverted)Target=&E;
   if(Target){S.ImmunitySent=true;S.ImmunityTarget=Target->Id;S.ImmunityOwner=Target->Owner;S.ImmunityAt=C->MatchTime;C->ServerOrder(TEXT("convert"),{Counter->Id},Target->Pos,Target->Id,TEXT(""));}
  }
  if(S.ImmunitySent){S.ImmunityRejected|=C->Notice.Contains(TEXT("eligible enemy unit"));if(const auto Target=C->FindEntity(S.ImmunityTarget))S.ImmunityStable|=C->MatchTime-S.ImmunityAt>=5&&Target->bConverted&&Target->Owner==S.ImmunityOwner;}
  if(GateSupported){
   if(Wall1&&Wall2&&!S.Gate1Sent){S.Gate1Sent=true;C->ServerOrder(TEXT("research"),{Wall1->Id},FVector2D::ZeroVector,0,TEXT("gate_segment_upgrade"));}
   if(Wall1&&Wall2&&Wall1->bGate&&!Wall2->bGate){S.PerWall=true;if(!S.Gate2Sent){S.Gate2Sent=true;C->ServerOrder(TEXT("research"),{Wall2->Id},FVector2D::ZeroVector,0,TEXT("gate_segment_upgrade"));}}
   S.Gates|=Wall1&&Wall2&&Wall1->bGate&&Wall2->bGate&&!Me->Upgrades.Contains(TEXT("gate_segment_upgrade"));
   if(S.Gates&&InvalidGate&&!S.UnsupportedGateAudit&&S.UnsupportedGateAt<0){S.GateAuditWallId=InvalidGate->Id;S.UnsupportedGateAt=C->MatchTime;S.UnsupportedGateResources=Me->Resources;C->ServerOrder(TEXT("research"),{InvalidGate->Id},FVector2D::ZeroVector,0,TEXT("gate_segment_upgrade"));}
   if(S.UnsupportedGateAt>=0&&C->Notice.Contains(TEXT("unit is acquired elsewhere"))){S.UnsupportedGateRejected=true;S.UnsupportedGateNotice=C->Notice;}
   if(S.UnsupportedGateAt>=0&&InvalidGate&&InvalidGate->RallyPoint.X==GateAuditAcknowledgment){S.UnsupportedGateAudit=true;S.UnsupportedAuditMutation=InvalidGate->RallyPoint.Y;S.UnsupportedResourcesAfterAudit=Me->Resources;S.UnsupportedGateNoCharge|=S.UnsupportedAuditMutation==0&&!InvalidGate->bGate&&InvalidGate->Queue.IsEmpty()&&!Me->Upgrades.Contains(TEXT("gate_segment_upgrade"));}
  }else{
   // Unsupported civilizations must reject the real research RPC without
   // opening either wall, entering a queue, or charging a research cost.
   if(Wall1&&Wall2&&S.Foundation&&!S.Gate1Sent){S.Gate1Sent=true;S.GateAuditWallId=Wall1->Id;S.UnsupportedGateAt=C->MatchTime;S.UnsupportedGateResources=Me->Resources;C->ServerOrder(TEXT("research"),{Wall1->Id},FVector2D::ZeroVector,0,TEXT("gate_segment_upgrade"));}
   if(S.Gate1Sent&&C->Notice.Contains(TEXT("civilization cannot recruit or research"))){S.UnsupportedGateRejected=true;S.UnsupportedGateNotice=C->Notice;}
   if(S.Gate1Sent&&!S.UnsupportedGateAudit&&Wall1&&Wall1->RallyPoint.X==GateAuditAcknowledgment){S.UnsupportedGateAudit=true;S.UnsupportedAuditMutation=Wall1->RallyPoint.Y;S.UnsupportedResourcesAfterAudit=Me->Resources;}
   if(S.UnsupportedGateRejected&&S.UnsupportedGateAudit&&Wall1&&Wall2)S.UnsupportedGateNoCharge|=S.UnsupportedAuditMutation==0&&!Wall1->bGate&&!Wall2->bGate&&Wall1->Queue.IsEmpty()&&Wall2->Queue.IsEmpty()&&!Me->Upgrades.Contains(TEXT("gate_segment_upgrade"));
  }
  bool Private=true,EnemySeen=false,FoodRate=false,SpiritRate=false;
  for(const auto& E:C->Entities){const auto& Def=Cat.Def(E.Def);if(E.Owner==C->LocalSlot){FoodRate|=Def.Kind==TEXT("sheep")&&E.ResourceRate.X>.29f;SpiritRate|=RTS::IsReligious(Def)&&E.SpiritRate>.05f;}
   else{Private&=E.ResourceRate.IsNearlyZero()&&E.SpiritRate==0&&E.CementRate==0;if(E.Owner>=0&&C->Players.IsValidIndex(E.Owner)&&C->Players[E.Owner].Team!=Me->Team){EnemySeen=true;Private&=E.ResourceEvent.IsNearlyZero()&&E.SpiritEvent==0&&E.CementEvent==0;}}
   if(Def.Id==TEXT("cement_tower"))S.PublicStats|=FMath::IsNearlyEqual(E.EffectiveRange,E.EffectiveSight,.001f)&&E.EffectiveRange>=16&&FMath::IsNearlyEqual(E.MinimumRange,3.f)&&FMath::IsNearlyEqual(E.SplashRadius,2.f)&&FMath::IsNearlyEqual(E.EffectiveAttack,40.f)&&FMath::IsNearlyEqual(E.AttackRate,4.f)&&E.DamageModifiers.Contains(TEXT("0.25"));
  }
  for(const auto& P:C->Players)if(P.Team!=Me->Team)Private&=P.Resources.IsNearlyZero()&&P.Cement==0&&P.Upgrades.IsEmpty();
  S.OwnRates|=SpiritRate&&(FoodRate||Me->Faction==TEXT("gadianton"));S.Privacy|=EnemySeen&&Private;
 }
 const bool GatePassed=GateSupported?(S.PerWall&&S.Gates&&S.UnsupportedGateRejected&&S.UnsupportedGateNoCharge):(S.UnsupportedGateRejected&&S.UnsupportedGateNoCharge);
 const bool Passed=S.Lobby&&S.ResetSeen&&S.ReadyAgain&&S.Sent&&S.FiveSeconds&&S.Converted&&S.ImmunitySent&&S.ImmunityRejected&&S.ImmunityStable&&S.Foundation&&GatePassed&&S.OwnRates&&S.PublicStats&&S.Privacy;
 if(C->Phase==2&&!S.Reported){S.Reported=true;auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("slot"),C->LocalSlot);R->SetBoolField(TEXT("lobby_settings"),S.Lobby);R->SetBoolField(TEXT("readiness_reset_observed"),S.ResetSeen);R->SetBoolField(TEXT("ready_again"),S.ReadyAgain);R->SetBoolField(TEXT("conversion_context_rpc"),S.Sent);R->SetBoolField(TEXT("conversion_five_seconds"),S.FiveSeconds);R->SetBoolField(TEXT("converted_marker"),S.Converted);R->SetBoolField(TEXT("reconversion_rpc_rejected"),S.ImmunitySent&&S.ImmunityRejected);R->SetBoolField(TEXT("reconversion_owner_stable_after_five_seconds"),S.ImmunityStable);R->SetBoolField(TEXT("three_queued_foundations_replicate"),S.Foundation);R->SetBoolField(TEXT("gate_supported"),GateSupported);R->SetBoolField(TEXT("gate_research_is_per_wall"),S.PerWall&&S.Gates);R->SetBoolField(TEXT("unsupported_gate_research_rejected"),S.UnsupportedGateRejected);R->SetBoolField(TEXT("unsupported_gate_research_no_charge"),S.UnsupportedGateNoCharge);R->SetBoolField(TEXT("unsupported_gate_server_audit_received"),S.UnsupportedGateAudit);if(S.GateAuditWallId)R->SetStringField(TEXT("unsupported_gate_server_audit_file"),FString::Printf(TEXT("network-v07-audit-slot%d-wall%d.json"),C->LocalSlot,S.GateAuditWallId));R->SetNumberField(TEXT("unsupported_gate_server_audit_mutation"),S.UnsupportedAuditMutation);R->SetStringField(TEXT("unsupported_gate_research_notice"),S.UnsupportedGateNotice);R->SetArrayField(TEXT("client_resources_at_research_send"),AuditVector(S.UnsupportedGateResources));R->SetArrayField(TEXT("client_resources_when_audit_received"),AuditVector(S.UnsupportedResourcesAfterAudit));R->SetBoolField(TEXT("gate_path_passed"),GatePassed);R->SetBoolField(TEXT("owner_trickle_rates"),S.OwnRates);R->SetBoolField(TEXT("public_tower_stats"),S.PublicStats);R->SetBoolField(TEXT("financial_privacy"),S.Privacy);R->SetBoolField(TEXT("passed"),Passed);FString JSON;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/FString::Printf(TEXT("network-v07-slot%d.json"),C->LocalSlot)));UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_V07 slot=%d passed=%d %s"),C->LocalSlot,Passed,*JSON);}
 return Passed;
}
