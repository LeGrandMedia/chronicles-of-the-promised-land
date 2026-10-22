#include "RTSController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "RTSGameMode.h"
#include "RTSWorld.h"
bool RunRTSNetworkV04Probe(ARTSController* C);
bool RunRTSNetworkV05Probe(ARTSController* C);
bool RunRTSNetworkV07Probe(ARTSController* C);
bool RunRTSNetworkV08Probe(ARTSController* C);

// Opt-in integration driver. Uses the same RPCs as human input; no resource grants.
void RunRTSNetworkProbe(ARTSController* C) {
#if !UE_BUILD_SHIPPING
 FString Role;if(!FParse::Value(FCommandLine::Get(),TEXT("RTSNetTest="),Role)||!C->IsLocalController())return;int Expected=2;FParse::Value(FCommandLine::Get(),TEXT("RTSNetPlayers="),Expected);Expected=FMath::Clamp(Expected,2,4);
 struct FProbe{int Stage=0,Frames=0,Worker=0,QueueWorker=0,MaxVisibleEntities=0;FVector2D Start,FirstWaypoint,LastWaypoint;bool FirstWaypointSeen=false,QueuedPath=false,Moved=false,Trained=false,FailedMap=false,Restored=false,Built=false,Rallied=false,StressSeeded=false,V07SettingsSent=false;};
 const bool bStress=FParse::Param(FCommandLine::Get(),TEXT("RTSNetStress"));const bool V04=FParse::Param(FCommandLine::Get(),TEXT("RTSNetV04"));const bool V04Passed=RunRTSNetworkV04Probe(C);const bool V05Passed=RunRTSNetworkV05Probe(C);const bool V07=FParse::Param(FCommandLine::Get(),TEXT("RTSNetV07"));const bool V07Passed=RunRTSNetworkV07Probe(C);const bool V08=FParse::Param(FCommandLine::Get(),TEXT("RTSNetV08"));const bool V08Passed=RunRTSNetworkV08Probe(C);const bool Extended=V04||V07||V08;
 static TMap<ARTSController*,FProbe> Tests;auto& T=Tests.FindOrAdd(C);T.Frames++;T.MaxVisibleEntities=FMath::Max(T.MaxVisibleEntities,C->Entities.Num());const auto* Me=C->Me();if(!Me)return;
 if(T.Frames%40==0){UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_PROGRESS role=%s slot=%d phase=%d time=%.1f stage=%d entities=%d"),*Role,C->LocalSlot,C->Phase,C->MatchTime,T.Stage,C->Entities.Num());}
 if(C->Phase==0){if(T.Stage==0){T.Stage=1;const TCHAR* Mixed[]={TEXT("gadianton"),TEXT("lamanites"),TEXT("laban"),TEXT("coriantumr")};C->ServerLobby(Role,(V04||V08)?Mixed[FMath::Clamp(C->LocalSlot,0,3)]:C->LocalSlot%2?TEXT("lamanites"):TEXT("nephites"),C->LocalSlot%2,true);return;}if((V07||V08)&&T.Stage==1&&!Me->bReady&&Me->LobbyFruitfulness==2){C->ServerLobby(Role,Me->Faction,Me->Team,true);return;}if(Role==TEXT("host")&&C->Players.Num()>=Expected){bool All=true;for(auto& P:C->Players)All&=P.bReady;if(All){if((V07||V08)&&!T.V07SettingsSent){T.V07SettingsSent=true;C->ServerLobbySettings(1,1.f,2,true);return;}T.Stage=2;C->ServerStart(0);}}return;}
 if(C->Phase==1){
  T.Trained|=Me->Population>=7;T.FailedMap|=!Me->bLiahona;if(T.Stage>=5&&Me->bLiahona)T.Restored=true;
  if(T.QueueWorker)if(auto Q=C->FindEntity(T.QueueWorker)){if(FVector2D::Distance(Q->Pos,T.FirstWaypoint)<2)T.FirstWaypointSeen=true;if(T.FirstWaypointSeen&&FVector2D::Distance(Q->Pos,T.LastWaypoint)<2)T.QueuedPath=true;}
  if(T.Worker){auto W=C->FindEntity(T.Worker);if(W&&FVector2D::Distance(W->Pos,T.Start)>1)T.Moved=true;}
  for(auto& E:C->Entities)if(E.Owner==C->LocalSlot&&FRTSCatalog::Get().Def(E.Def).Id==TEXT("tents_and_habitations")&&E.Progress>=1)T.Built=true;
  for(const auto& E:C->Entities)if(E.Owner==C->LocalSlot&&FRTSCatalog::Get().Def(E.Def).Id==TEXT("seat_of_the_people")&&E.bRallySet)T.Rallied=true;
  // Keep the ordinary probe actions intact for the first thirty seconds. The
  // host then adds idle worker bodies for snapshot-size testing only.
  if(bStress&&Role==TEXT("host")&&C->MatchTime>=(Extended?80:30)&&!T.StressSeeded){
   T.StressSeeded=true;
   if(ARTSGameMode* GM=Cast<ARTSGameMode>(C->GetWorld()->GetAuthGameMode()))if(ARTSWorld* W=GM->Simulation){
    for(auto& P:W->Players)for(auto& KV:W->Entities)if(KV.Value->Owner==P.Slot&&FRTSCatalog::Get().Def(KV.Value->Def).bBuilding)KV.Value->Jobs.Empty();
    W->Population();
    const FVector2D Bases[]={FVector2D(24,24),FVector2D(136,136),FVector2D(24,136),FVector2D(136,24)};
    for(int32 Slot=0;Slot<W->Players.Num();++Slot){auto& P=W->Players[Slot];for(int32 Site=0;Site<W->Players.Num();++Site){FVector2D At=Bases[Site]+FVector2D(3,3);if(FRTSEntity* VisionWorker=W->Spawn(TEXT("industrious_laborer"),Slot,At)){VisionWorker->Order=TEXT("idle");VisionWorker->Rally=At;}}}
    W->Population();
    for(int32 Slot=0;Slot<W->Players.Num();++Slot){auto& P=W->Players[Slot];int32 Missing=FMath::Max(0,200-P.Population);for(int32 I=0;I<Missing;++I){FVector2D At(80.f+((I%20)-9.5f)*.25f,80.f+((I/20)-4.5f)*.25f);if(FRTSEntity* Unit=W->Spawn(TEXT("industrious_laborer"),Slot,At)){Unit->Order=TEXT("idle");Unit->Rally=At;}}}
    W->Population();
   }
  }
  if(T.Stage<3&&C->MatchTime>1){T.Stage=3;TArray<int32> Workers;int HQ=0,Food=0;FVector2D At;for(auto& E:C->Entities){auto Id=FRTSCatalog::Get().Def(E.Def).Id;if(E.Owner==C->LocalSlot&&Id==TEXT("industrious_laborer"))Workers.Add(E.Id);if(E.Owner==C->LocalSlot&&Id==TEXT("seat_of_the_people")){HQ=E.Id;At=E.Pos;}if(Id==TEXT("berries"))Food=E.Id;}
   if(Workers.Num()>=2){T.Worker=Workers[0];T.Start=C->FindEntity(T.Worker)->Pos;if(Me->Faction==TEXT("gadianton"))C->ServerOrder(TEXT("move"),{Workers[0]},At+FVector2D(7,7),0,TEXT(""));else C->ServerOrder(TEXT("gather"),{Workers[0]},FVector2D::ZeroVector,Food,TEXT(""));int Builder=Workers[1];if(RTS::IsJaredite(Me->Faction))for(const auto& E:C->Entities)if(E.Owner==C->LocalSlot&&FRTSCatalog::Get().Def(E.Def).Id==TEXT("curelom"))Builder=E.Id;C->ServerOrder(TEXT("build"),{Builder},At+FVector2D(7,0),0,TEXT("tents_and_habitations"));}if(Workers.Num()>=3){T.QueueWorker=Workers[2];const FVector2D Forward=(FVector2D(80,80)-At).GetSafeNormal(),Side(-Forward.Y,Forward.X);T.FirstWaypoint=At+Forward*12+Side*3;T.LastWaypoint=At+Forward*20+Side*7;C->ServerOrder(TEXT("move"),{T.QueueWorker},T.FirstWaypoint,0,TEXT(""));C->ServerOrder(TEXT("move"),{T.QueueWorker},T.LastWaypoint,0,TEXT(""),true);}if(HQ){C->ServerOrder(TEXT("rally"),{HQ},At+FVector2D(9,-5),0,TEXT(""));C->ServerOrder(TEXT("train"),{HQ},FVector2D::ZeroVector,0,TEXT("industrious_laborer"));}
  }
  if(T.Stage==3&&C->MatchTime>5){T.Stage=4;C->ServerLiahonaFailure();}
  if(T.Stage==4&&C->MatchTime>7){T.Stage=5;C->ServerChat(TEXT("sorry"));}
  if(T.Stage==5&&C->MatchTime>(Extended?(bStress?96:86):(bStress?46:26))&&Role!=TEXT("host")){T.Stage=6;C->ServerOrder(TEXT("resign"),{},FVector2D::ZeroVector,0,TEXT(""));}
 }
 if(C->Phase==2&&T.Stage<7){T.Stage=7;auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("role"),Role);R->SetNumberField(TEXT("slot"),C->LocalSlot);R->SetStringField(TEXT("faction"),Me->Faction);R->SetNumberField(TEXT("team"),Me->Team);R->SetNumberField(TEXT("players"),C->Players.Num());R->SetNumberField(TEXT("snapshots"),T.Frames);R->SetNumberField(TEXT("maximum_visible_entities"),T.MaxVisibleEntities);R->SetBoolField(TEXT("stress"),bStress);R->SetNumberField(TEXT("largest_snapshot_bytes"),C->LargestSnapshotBytes);R->SetNumberField(TEXT("largest_compressed_bytes"),C->LargestPackedBytes);R->SetBoolField(TEXT("movement"),T.Moved);R->SetBoolField(TEXT("training"),T.Trained);R->SetBoolField(TEXT("queued_waypoints"),T.QueuedPath);R->SetBoolField(TEXT("construction"),T.Built);R->SetBoolField(TEXT("rally_replicated"),T.Rallied);R->SetBoolField(TEXT("liahona_failed"),T.FailedMap);R->SetBoolField(TEXT("apology_restored"),T.Restored);R->SetNumberField(TEXT("winner"),C->Winner);R->SetBoolField(TEXT("v04_features"),V04Passed);R->SetBoolField(TEXT("v05_features"),V05Passed);R->SetBoolField(TEXT("v07_features"),V07Passed);R->SetBoolField(TEXT("v08_features"),V08Passed);bool Pass=V04Passed&&V05Passed&&V07Passed&&V08Passed&&T.Moved&&T.QueuedPath&&T.Trained&&T.Built&&T.Rallied&&T.FailedMap&&T.Restored&&C->Winner==0&&C->Players.Num()==Expected&&(!bStress||T.MaxVisibleEntities>=800);R->SetBoolField(TEXT("passed"),Pass);FString JSON;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/(TEXT("network-")+Role+TEXT(".json"))));UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_TEST role=%s passed=%d frames=%d max_visible=%d stress=%d"),*Role,Pass,T.Frames,T.MaxVisibleEntities,bStress);FTimerHandle ExitTimer;C->GetWorldTimerManager().SetTimer(ExitTimer,[Pass](){FPlatformMisc::RequestExitWithStatus(false,Pass?0:1);},Role==TEXT("host")?2.f:.5f,false);}
#endif
}
