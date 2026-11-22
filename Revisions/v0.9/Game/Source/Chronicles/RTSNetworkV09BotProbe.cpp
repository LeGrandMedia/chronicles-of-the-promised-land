#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSWorld.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

#if !UE_BUILD_SHIPPING
namespace {
int32 ProbeMap(){static const int32 Value=[](){int32 Map=2;FParse::Value(FCommandLine::Get(),TEXT("RTSNetMap="),Map);return FMath::Clamp(Map,0,RTS::MapCount()-1);}();return Value;}
struct FMixedLANProbe {
 int32 LobbyStage=0,Frames=0,HumanWorker=0,BotWorker=0;
 FVector2D OwnStart=FVector2D::ZeroVector;
 bool bReported=false,bCorrectRoster=false,bHumanMoved=false,bBotDeposit=false,bBotBuilt=false,bBotTrained=false;
 bool bEnemyPrivate=true,bUnauthorizedMoveRejected=true,bBotIsServerOwned=false;
};

bool VerifyBotRoster(const FRTSPlayerView& Player) {
 return Player.Slot==2&&Player.Team==1&&Player.bBot&&Player.bReady&&Player.bConnected&&
  Player.BotDifficulty==2&&Player.Faction==TEXT("coriantumr");
}

void AfterSnapshot(ARTSController* C,TFunction<void(ARTSController&)> Action) {
 TWeakObjectPtr<ARTSController> Local(C);
 C->GetWorldTimerManager().SetTimerForNextTick([Local,Action=MoveTemp(Action)](){
  if(auto* PC=Local.Get())Action(*PC);
 });
}
}
#endif

void RunRTSNetworkV09BotProbe(ARTSController* C) {
#if !UE_BUILD_SHIPPING
 static const FString Role=[](){
  FString Value;FParse::Value(FCommandLine::Get(),TEXT("RTSNetV09Bots="),Value);
  return Value;
 }();
 if((Role!=TEXT("host")&&Role!=TEXT("guest"))||!C->IsLocalController()||!C->HasActorBegunPlay()||!C->Scene3D)return;
 const auto* Me=C->Me();
 if(!Me)return;
 static TMap<ARTSController*,FMixedLANProbe> Probes;
 auto& Probe=Probes.FindOrAdd(C);
 if(Probe.bReported)return;
 ++Probe.Frames;
 const bool bHost=Role==TEXT("host");
 const auto* Bot=C->Players.FindByPredicate([](const FRTSPlayerView& P){return P.bBot;});
 const auto* Guest=C->Players.FindByPredicate([](const FRTSPlayerView& P){return P.Slot==1;});
 const bool bExpectedSlots=C->Players.Num()==3&&Bot&&VerifyBotRoster(*Bot)&&Guest&&Guest->bConnected&&!Guest->bBot;

 if(C->Phase==0){
  if(bHost&&Probe.LobbyStage==0&&Guest&&Guest->bConnected){
   Probe.LobbyStage=1;
   AfterSnapshot(C,[](ARTSController& PC){
    PC.ServerLobby(TEXT("LAN Host"),TEXT("nephites"),0,true);
    PC.ServerOrder(TEXT("bot_add"),{},FVector2D::ZeroVector,0,TEXT("laban"));
    PC.ServerOrder(TEXT("bot_config"),{1,2},FVector2D::ZeroVector,2,TEXT("coriantumr"));
   });
  }
  if(!bHost&&Probe.LobbyStage==0&&bExpectedSlots){
   Probe.LobbyStage=1;
   AfterSnapshot(C,[](ARTSController& PC){
    PC.ServerOrder(TEXT("bot_remove"),{},FVector2D::ZeroVector,2,TEXT(""));
    PC.ServerOrder(TEXT("bot_config"),{0,0},FVector2D::ZeroVector,2,TEXT("gadianton"));
    PC.ServerLobby(TEXT("LAN Guest"),TEXT("lamanites"),1,true);
   });
  }
  if(bHost&&Probe.LobbyStage==1&&bExpectedSlots&&Guest->bReady&&Guest->Name==TEXT("LAN Guest")){
   Probe.LobbyStage=2;
   Probe.bCorrectRoster=true;
   AfterSnapshot(C,[](ARTSController& PC){PC.ServerStart(ProbeMap());});
  }
  return;
 }
 if(C->Phase!=1)return;
 Probe.bCorrectRoster|=bExpectedSlots;
 if(bHost){
  if(Bot)Probe.bEnemyPrivate&=Bot->Resources.IsZero()&&Bot->Cement==0&&Bot->Upgrades.IsEmpty()&&Bot->Population==0&&Bot->Capacity==0;
  Probe.bEnemyPrivate&=!C->Entities.ContainsByPredicate([](const FRTSEntityView& E){return E.Owner==2;});
  if(auto* Mode=C->GetWorld()->GetAuthGameMode<ARTSGameMode>()){
   auto* Sim=Mode->Simulation;
   if(Sim&&Sim->Players.IsValidIndex(2)){
    const auto& Player=Sim->Players[2];
    Probe.bBotIsServerOwned|=Player.bBot&&Player.bConnected&&!Player.Controller.IsValid()&&!Sim->bSandbox;
    for(const auto& Entry:Sim->Entities){
     const auto& E=*Entry.Value;
     if(E.Owner!=2)continue;
     const auto& Def=Sim->D(E);
     Probe.bBotDeposit|=RTS::IsWorker(Def)&&E.ResourceEventTime>=0;
     Probe.bBotBuilt|=Def.bBuilding&&Def.Id!=TEXT("seat_of_the_people")&&E.Progress>=1;
    }
    int32 Laborers=0;
    for(const auto& Entry:Sim->Entities)if(Entry.Value->Owner==2&&RTS::IsWorker(Sim->D(*Entry.Value)))++Laborers;
    Probe.bBotTrained|=Laborers>6;
   }
  }
 }else{
  int32 Laborers=0;
  const FVector2D ForbiddenDestination(128,128);
  for(const auto& Entity:C->Entities){
   const auto& Def=FRTSCatalog::Get().Def(Entity.Def);
   if(Entity.Owner==2){
    const bool bWorker=RTS::IsWorker(Def);
    Probe.bBotDeposit|=bWorker&&Entity.ResourceEventTime>=0&&Entity.ResourceEvent.SizeSquared()>0;
    Probe.bBotBuilt|=Def.bBuilding&&Def.Id!=TEXT("seat_of_the_people")&&Entity.Progress>=1;
    if(bWorker)++Laborers;
   }
  }
  Probe.bBotTrained|=Laborers>6;
  if(!Probe.HumanWorker&&C->MatchTime>=2){
   const auto* Human=C->Entities.FindByPredicate([&](const FRTSEntityView& E){
    return E.Owner==C->LocalSlot&&RTS::IsWorker(FRTSCatalog::Get().Def(E.Def));
   });
   const auto* Ally=C->Entities.FindByPredicate([](const FRTSEntityView& E){
    return E.Owner==2&&RTS::IsWorker(FRTSCatalog::Get().Def(E.Def))&&E.Status==TEXT("gather");
   });
   if(Human&&Ally){
    Probe.HumanWorker=Human->Id;Probe.BotWorker=Ally->Id;Probe.OwnStart=Human->Pos;
    const int32 HumanId=Human->Id,BotId=Ally->Id;
    AfterSnapshot(C,[HumanId,BotId,ForbiddenDestination](ARTSController& PC){
     PC.ServerOrder(TEXT("move"),{HumanId,BotId},ForbiddenDestination,0,TEXT(""));
    });
   }
  }
  if(const auto* Human=C->FindEntity(Probe.HumanWorker))Probe.bHumanMoved|=FVector2D::DistSquared(Probe.OwnStart,Human->Pos)>9;
  if(const auto* Ally=C->FindEntity(Probe.BotWorker)){
   Probe.bUnauthorizedMoveRejected&=Ally->Status!=TEXT("move")&&FVector2D::DistSquared(Ally->Pos,ForbiddenDestination)>FMath::Square(55.0);
  }
 }

 if(C->MatchTime<46)return;
 Probe.bReported=true;
 const bool bHostResult=Probe.bEnemyPrivate&&Probe.bBotIsServerOwned&&Probe.LobbyStage==2;
 const bool bGuestResult=Probe.HumanWorker>0&&Probe.BotWorker>0&&Probe.bHumanMoved&&Probe.bUnauthorizedMoveRejected&&
  C->DeliveredPackedSequence>0&&C->LargestPackedBytes>0;
 const bool bPassed=Probe.bCorrectRoster&&C->Map==ProbeMap()&&Probe.bBotDeposit&&Probe.bBotBuilt&&Probe.bBotTrained&&(bHost?bHostResult:bGuestResult);
 auto Result=MakeShared<FJsonObject>();
 Result->SetStringField(TEXT("role"),Role);
 Result->SetNumberField(TEXT("map"),C->Map);
 Result->SetStringField(TEXT("map_id"),RTS::MapInfo(C->Map).Id);
 uint32 Terrain=2166136261u;for(int Y=0;Y<64;++Y)for(int X=0;X<64;++X){const FVector2D At((X+.5)*2.5,(Y+.5)*2.5);Terrain=(Terrain^(int32(RTS::WaterAt(At,C->Map))+2*int32(RTS::RockAt(At,C->Map))))*16777619u;}
 Result->SetStringField(TEXT("terrain_signature"),FString::Printf(TEXT("%08x"),Terrain));
 Result->SetNumberField(TEXT("role_slot"),C->LocalSlot);
 Result->SetNumberField(TEXT("snapshots"),Probe.Frames);
 Result->SetNumberField(TEXT("largest_packed_snapshot_bytes"),C->LargestPackedBytes);
 Result->SetBoolField(TEXT("passed"),bPassed);
 Result->SetBoolField(TEXT("host_configured_roster_after_guest_commands"),Probe.bCorrectRoster);
 Result->SetBoolField(TEXT("bot_gathered"),Probe.bBotDeposit);
 Result->SetBoolField(TEXT("bot_built"),Probe.bBotBuilt);
 Result->SetBoolField(TEXT("bot_trained"),Probe.bBotTrained);
 if(bHost){
  Result->SetBoolField(TEXT("host_enemy_resources_and_units_filtered"),Probe.bEnemyPrivate);
  Result->SetBoolField(TEXT("bot_runs_on_authority_without_controller"),Probe.bBotIsServerOwned);
 }else{
  Result->SetBoolField(TEXT("remote_human_worker_moved"),Probe.bHumanMoved);
  Result->SetBoolField(TEXT("remote_human_could_not_order_allied_bot_worker"),Probe.BotWorker>0&&Probe.bUnauthorizedMoveRejected);
 }
 FString Json;
 FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");
 IFileManager::Get().MakeDirectory(*Dir,true);
 FFileHelper::SaveStringToFile(Json,*(Dir/(TEXT("network-v09-bots-")+Role+TEXT(".json"))));
 UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_V09_BOTS role=%s passed=%d %s"),*Role,bPassed,*Json);
 FTimerHandle ExitTimer;
 C->GetWorldTimerManager().SetTimer(ExitTimer,[bPassed](){FPlatformMisc::RequestExitWithStatus(false,bPassed?0:1);},bHost?2.5f:.25f,false);
#endif
}
