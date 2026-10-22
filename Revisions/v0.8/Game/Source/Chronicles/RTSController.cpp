#include "RTSController.h"
#include "RTSHUD.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"
#include "Engine/NetDriver.h"
#include "RTSControls.h"
#include "RTSNames.h"
#include "RTSVisualAssets.h"
#include "RTSInputTests.h"

namespace { bool OpenIntoLobby=false; bool SoundCuesEnabled=true; bool OpeningHost=false; bool JoinFailureTestStarted=false; FString PendingConnectionError; }

ARTSController::ARTSController() {
 bShowMouseCursor=true;
 bEnableClickEvents=false;
 bEnableMouseOverEvents=false;
 PrimaryActorTick.bCanEverTick=true;
 DefaultMouseCursor=EMouseCursor::Default;
}

void ARTSController::BeginPlay() {
 Super::BeginPlay();
 if (!IsLocalController()) return;
 FRTSCatalog::Get();
 // Finish loose-art import before lobby readiness or any match snapshot can advance.
 FRTSVisualAssets::Get();
 PlayerName=FPlatformProcess::UserName();
 PlayerName=PlayerName.Left(24);
 if(PlayerName.IsEmpty()) PlayerName=TEXT("Player");
 bMenu=!(OpenIntoLobby || GetNetMode()==NM_Client || GetNetMode()==NM_ListenServer);
 bPreview=FParse::Param(FCommandLine::Get(),TEXT("RTSPreview"));
 bPreviewExit=FParse::Param(FCommandLine::Get(),TEXT("RTSPreviewExit"));
 bMenuShot=FParse::Param(FCommandLine::Get(),TEXT("RTSMenuShot"));
 bSoundEnabled=SoundCuesEnabled;
 bJoinFailureTest=FParse::Param(FCommandLine::Get(),TEXT("RTSJoinFailureTest"));
 if(OpeningHost && GetNetMode()==NM_Standalone && PendingConnectionError.IsEmpty()) PendingConnectionError=TEXT("Unable to host a LAN game. Close any other copy using port 7777 and try again.");
 OpeningHost=false;
 if(!PendingConnectionError.IsEmpty()) {
  ConnectionError=PendingConnectionError;PendingConnectionError.Empty();bMenu=true;
  UE_LOG(LogTemp,Display,TEXT("RTS_CONNECTION_RETURN_MENU"));
  if(bJoinFailureTest) {FailureTestReturnTime=GetWorld()->GetRealTimeSeconds();UE_LOG(LogTemp,Display,TEXT("RTS_JOIN_FAILURE_TEST_PASS returned_to_menu"));}
 }
 if(GEngine) {
  NetworkFailureHandle=GEngine->OnNetworkFailure().AddUObject(this,&ARTSController::HandleNetworkFailure);
  TravelFailureHandle=GEngine->OnTravelFailure().AddUObject(this,&ARTSController::HandleTravelFailure);
 }
 OpenIntoLobby=false;
 // Temporary capture forwards the initial press and keeps drag/release paired.
 // NoCapture suppresses ordinary button-down events in a game FSceneViewport.
 FInputModeGameAndUI InputMode;InputMode.SetHideCursorDuringCapture(false);InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
 SetInputMode(InputMode);
 if(GetWorld()->GetGameViewport()) {
  GetWorld()->GetGameViewport()->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
  GetWorld()->GetGameViewport()->SetHideCursorDuringCapture(false);
  GetWorld()->GetGameViewport()->SetMouseLockMode(EMouseLockMode::DoNotLock);
 }
}

void ARTSController::EndPlay(const EEndPlayReason::Type EndPlayReason) {
 if(GEngine) {GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);GEngine->OnTravelFailure().Remove(TravelFailureHandle);}
 Super::EndPlay(EndPlayReason);
}

void ARTSController::ConnectionFailed(const FString& Message) {
 if(!IsLocalController()||bReturningToMenu)return;
 bReturningToMenu=true;bConnecting=false;bMenu=true;bPauseMenu=false;bHelp=false;bChat=false;TextFocus.Empty();OpenIntoLobby=false;OpeningHost=false;
 PendingConnectionError=Message;ConnectionError=Message;
 UE_LOG(LogTemp,Display,TEXT("RTS_CONNECTION_FAILURE returned_to_menu"));
 const TWeakObjectPtr<ARTSController> Weak=this;
 GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([Weak](){
  if(Weak.IsValid()) {if(GEngine)GEngine->CancelPending(Weak->GetWorld());UGameplayStatics::OpenLevel(Weak.Get(),FName(TEXT("/Engine/Maps/Entry")),true);}
 }));
}

void ARTSController::HandleNetworkFailure(UWorld* FailedWorld,UNetDriver* Driver,ENetworkFailure::Type Failure,const FString& Error) {
 if(FailedWorld && FailedWorld!=GetWorld())return;
 // A remote commander's disconnect must not close the host's lobby or match.
 const bool Server=Driver&&Driver->GetNetMode()!=NM_Client;
 if(Server && HasAuthority() && !bConnecting && (Failure==ENetworkFailure::ConnectionLost||Failure==ENetworkFailure::ConnectionTimeout||Failure==ENetworkFailure::NetGuidMismatch||Failure==ENetworkFailure::NetChecksumMismatch))return;
 FString Message;
 if(Failure==ENetworkFailure::OutdatedClient||Failure==ENetworkFailure::OutdatedServer) Message=TEXT("These computers are running different game versions. Launch the same LAN build on both computers and try again.");
 else if(Failure==ENetworkFailure::ConnectionLost) Message=TEXT("The host has disconnected. You have returned to the main menu.");
 else if(Failure==ENetworkFailure::ConnectionTimeout||Failure==ENetworkFailure::PendingConnectionFailure) Message=TEXT("Unable to reach the host. Check the LAN address, that the host's lobby is open, and your firewall settings.");
 else if(Failure==ENetworkFailure::NetDriverCreateFailure||Failure==ENetworkFailure::NetDriverAlreadyExists) Message=TEXT("Unable to start the LAN connection. Close any other copy using this port and try again.");
 else Message=TEXT("The LAN connection ended or was refused. Check that the lobby is open and both computers are running the same game build.");
 ConnectionFailed(Message);
}
void ARTSController::HandleTravelFailure(UWorld* FailedWorld,ETravelFailure::Type Failure,const FString& Error) {
 if(FailedWorld && FailedWorld!=GetWorld())return;
 ConnectionFailed(TEXT("Unable to open the requested game. You have returned to the main menu. Please check the host address and try again."));
}

void ARTSController::SetupInputComponent() { Super::SetupInputComponent(); }

const FRTSPlayerView* ARTSController::Me() const {
 for(const FRTSPlayerView& P:Players) if(P.Slot==LocalSlot) return &P;
 return nullptr;
}
const FRTSEntityView* ARTSController::FindEntity(int32 Id) const {
 return Entities.FindByPredicate([Id](const FRTSEntityView& E){return E.Id==Id;});
}
bool ARTSController::IsHost() const { return LocalSlot==0 && HasAuthority(); }
void ARTSController::ShowNotice(const FString& Message) {
 Notice=RTSNames::ResolveText(Message);
 if(const auto* LocalPlayerInfo=Me())for(const auto& Def:FRTSCatalog::Get().Defs){const FString Display=FRTSCatalog::Get().DisplayName(Def,LocalPlayerInfo->Faction);if(Display!=Def.Name&&!Def.Name.IsEmpty())Notice.ReplaceInline(*Def.Name,*Display,ESearchCase::CaseSensitive);}
 NoticeUntil=GetWorld()?GetWorld()->GetRealTimeSeconds()+6.f:6.f;
}

void ARTSController::StateReceived() {
 if(!bIdentitySynced && Me()) {
  bIdentitySynced=true;
  const FRTSPlayerView* P=Me();
  LobbyTeam=P->Team;
  const auto& Fs=FRTSCatalog::Get().Factions;
  for(int32 I=0;I<Fs.Num();I++) if(Fs[I].Id==P->Faction) LobbyFaction=I;
 }
 if(Phase!=PreviousPhase) {
  if(Phase==1) {
   bAudioSnapshotSeen=false;AudioEntityState.Empty();GalavantRoutes.Empty();DeathAnimations.Empty();bWallAnchor=false;PlayCue(TEXT("complete"));
   bMenu=false;bPauseMenu=false;bBuildMenu=false;bHelp=false;
   Selection.Empty();PendingAction.Empty();PendingType.Empty();ActionPage=0;
   FocusHeadquarters();
  }
  if(Phase==0) { bPauseMenu=false; Selection.Empty(); }
  PreviousPhase=Phase;
 }
 Selection.RemoveAll([this](int32 Id){const auto* E=FindEntity(Id);return !E||E->bRemembered;});
 if(Me()) {
  if(!Me()->bLiahona) {bAwaitingLiahonaFailure=false;if(!bAwaitingLiahonaRestore)bLocalLiahona=false;}
  else if(!bAwaitingLiahonaFailure) {bLocalLiahona=true;bAwaitingLiahonaRestore=false;}
 }
 if(Phase==0){LobbyMap=Map;if(!Players.IsEmpty()){LobbyResources=Players[0].LobbyResources;LobbyFruitfulness=Players[0].LobbyFruitfulness;LobbySpeed=Players[0].LobbySpeed;bLobbyFog=Players[0].bLobbyFog;}}
 if(Phase==1 && IsLocalController()) {
  bool CombatCue=false,CompletionCue=false;TMap<int32,FVector2D> Current;
  for(const auto& E:Entities) {
   const float HealthFraction=E.HP/FMath::Max(1.f,E.MaxHP);
   Current.Add(E.Id,FVector2D(HealthFraction,E.Progress));
   const FVector2D* Old=AudioEntityState.Find(E.Id);
   if(bAudioSnapshotSeen && InWorld(WorldToScreen(E.Pos))) {
    if((E.bAttacking&&!RTS::IsWorker(FRTSCatalog::Get().Def(E.Def))) || (Old&&HealthFraction<Old->X-.000001f))CombatCue=true;
    if(E.Owner==LocalSlot && ((!Old&&!FRTSCatalog::Get().Def(E.Def).bBuilding)||(Old&&Old->Y<1&&E.Progress>=1)))CompletionCue=true;
   }
  }
  if(CombatCue)PlayCue(TEXT("combat"));
  if(CompletionCue)PlayCue(TEXT("complete"));
  AudioEntityState=MoveTemp(Current);bAudioSnapshotSeen=true;
 }
 for(const FRTSEntityView& E:Entities) if(!DisplayPositions.Contains(E.Id)) DisplayPositions.Add(E.Id,E.Pos);
}

FVector2D ARTSController::WorldToScreen(FVector2D Point) const {
 const float U=FMath::Min(ViewSize.X/1600.f,ViewSize.Y/900.f);
 const FVector2D D=RotateMap(Point-CameraCenter);
 return FVector2D(ViewSize.X*.5f+(D.X-D.Y)*9.f*Zoom*U,(ViewSize.Y-180.f*U)*.5f+(D.X+D.Y)*4.5f*Zoom*U);
}
FVector2D ARTSController::ScreenToWorld(FVector2D Point) const {
 const float U=FMath::Min(ViewSize.X/1600.f,ViewSize.Y/900.f);
 const float X=(Point.X-ViewSize.X*.5f)/(9.f*Zoom*U);
 const float Y=(Point.Y-(ViewSize.Y-180.f*U)*.5f)/(4.5f*Zoom*U);
 return CameraCenter+RotateMap(FVector2D((X+Y)*.5f,(Y-X)*.5f),true);
}
FVector2D ARTSController::VisualPosition(const FRTSEntityView& E) const {
 const FVector2D* P=DisplayPositions.Find(E.Id);return P?*P:E.Pos;
}
bool ARTSController::InWorld(FVector2D P) const {
 if(bMenu || bConnecting || !ConnectionError.IsEmpty() || Phase==0 || bHelp || bPauseMenu || bHotkeySettings || Phase==2) return false;
 const float U=FMath::Min(ViewSize.X/1600.f,ViewSize.Y/900.f);
 return P.Y>64*U && P.Y<ViewSize.Y-228*U;
}
int32 ARTSController::EntityUnder(FVector2D P,bool bContext) const {
 int32 Best=0;double Front=-1.e12;
 for(const auto& E:Entities) {
  if(E.bRemembered)continue;
  const double Depth=WorldToScreen(E.Pos).Y;
  if(EntityBounds(E,bContext).IsInsideOrOn(P) && Depth>=Front) {
   const float UI=FMath::Max(.25f,FMath::Min(ViewSize.X/1600.f,ViewSize.Y/900.f));
   if(!bContext&&!((E.Progress<1||E.bGate)&&FRTSCatalog::Get().Def(E.Def).bBuilding)&&!FRTSVisualAssets::Get().OpaqueAt(FRTSCatalog::Get().Def(E.Def).Id,P,WorldToScreen(VisualPosition(E))/UI,Zoom,UI,FactionFor(E.Owner),E.Id))continue;
   Front=Depth;Best=E.Id;
  }
 }
 return Best;
}
FBox2D ARTSController::EntityBounds(const FRTSEntityView& E,bool bContext) const {
 if(E.bRemembered)return FBox2D(EForceInit::ForceInit);
 const auto& D=FRTSCatalog::Get().Def(E.Def);const FVector2D S=WorldToScreen(VisualPosition(E));
 const float UI=FMath::Max(.25f,FMath::Min(ViewSize.X/1600.f,ViewSize.Y/900.f));
 if(D.bBuilding&&E.Progress<1){FBox2D Bounds(EForceInit::ForceInit);for(float X:{-1.f,1.f})for(float Y:{-1.f,1.f}){FVector2D Corner=WorldToScreen(E.Pos+FVector2D(X,Y)*FMath::Max(1.f,D.Radius));Bounds+=Corner+FVector2D(0,15)*Zoom*UI;Bounds+=Corner-FVector2D(0,12+26*E.Progress)*Zoom*UI;}return Bounds;}
 FBox2D AssetBounds;if(FRTSVisualAssets::Get().GetHitBounds(D.Id,S/UI,Zoom,UI,AssetBounds,bContext,FactionFor(E.Owner)))return AssetBounds;
 const float Scale=Zoom*UI*(D.bBuilding||D.Kind==TEXT("resource")?1.f:FRTSVisualAssets::WorldScale(D));
 FVector2D Min(-13,-30),Max(16,6);
 if(D.bBuilding){const float R=FMath::Max(22.f,D.Radius*8.f);Min=FVector2D(-R,-39);Max=FVector2D(R,17);if(bContext){Min-=FVector2D(11,5);Max+=FVector2D(11,8);}if(D.Id==TEXT("fields_of_grain")){Min=FVector2D(-R*.7f,-15);Max=FVector2D(R*.7f,13);if(bContext){Min=FVector2D(-R-14,-29);Max=FVector2D(R+14,26);}}}
 else if(D.Kind==TEXT("chariot")){Min=FVector2D(-34,-37);Max=FVector2D(35,12);}
 else if(D.Kind==TEXT("ship")){Min=FVector2D(-30,-49);Max=FVector2D(30,12);}
 else if(D.Kind==TEXT("beast")){Min=FVector2D(-22,-36);Max=FVector2D(36,10);}
 else if(D.Kind==TEXT("resource")){Min=FVector2D(-20,D.Id==TEXT("tree")?-49:-25);Max=FVector2D(20,7);if(bContext){Min-=FVector2D(13,8);Max+=FVector2D(13,12);}}
 return FBox2D(S+Min*Scale,S+Max*Scale);
}
void ARTSController::RefreshPointer() {float X,Y;if(GetMousePosition(X,Y))Mouse=FVector2D(X,Y);}

void ARTSController::PlayerTick(float Delta) {
 Super::PlayerTick(Delta);
 if(!IsLocalController()) return;
 float X,Y;if(GetMousePosition(X,Y)) Mouse=FVector2D(X,Y);
 if(FParse::Param(FCommandLine::Get(),TEXT("RTSInputTest"))) RunRTSInputProbe(this);
 const float RealTime=GetWorld()->GetRealTimeSeconds();
 if(bJoinFailureTest && !JoinFailureTestStarted && Phase==0 && Me()) {
  JoinFailureTestStarted=true;JoinAddress=TEXT("127.0.0.1:27991");UIAction(TEXT("join"));
  UE_LOG(LogTemp,Display,TEXT("RTS_JOIN_FAILURE_TEST_START"));
 }
 if(bJoinFailureTest && FailureTestReturnTime>=0 && RealTime-FailureTestReturnTime>3.f) ConsoleCommand(TEXT("quit"));
 if(bPreview && !bPreviewStarted && Phase==0 && Me()) {
  bPreviewStarted=true;bMenu=false;
  ServerLobby(TEXT("Preview"),TEXT("nephites"),0,true);ServerStart(0);
 }
 if((bPreview && Phase==1)||bMenuShot) {
  if(PreviewStart<0) PreviewStart=RealTime;
  if(!bScreenshotTaken && RealTime-PreviewStart>2.f) {
   if(bPreview) {FocusHeadquarters();Zoom=1.5f;for(const auto& E:Entities) if(E.Owner==LocalSlot&&FRTSCatalog::Get().Def(E.Def).Id==TEXT("seat_of_the_people")) {Selection={E.Id};break;}}
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(bMenuShot?TEXT("menu.png"):TEXT("gameplay.png"))),false,false);
   bScreenshotTaken=true;
  }
  if((bPreviewExit||bMenuShot)&&RealTime-PreviewStart>5.f) ConsoleCommand(TEXT("quit"));
 }
 for(const auto& E:Entities) {
  FVector2D& P=DisplayPositions.FindOrAdd(E.Id,E.Pos);
  if(FVector2D::Distance(P,E.Pos)>12) P=E.Pos;
  else P=FMath::Vector2DInterpTo(P,E.Pos,Delta,12.f);
 }
 if(Phase!=1 || bMenu || bChat || !TextFocus.IsEmpty() || bPauseMenu || bHelp || bHotkeySettings) {DeleteHeldSince=-1;return;}
 if(DeleteHeldSince>=0) {
  if(!IsInputKeyDown(EKeys::Delete)||DeleteSelection!=Selection)DeleteHeldSince=-1;
  else if(RealTime-DeleteHeldSince>=2.f){Issue(TEXT("delete"));DeleteHeldSince=-1;}
 }
 FVector2D Pan=FVector2D::ZeroVector;
 const bool bScreenFocus=GetWorld()->GetGameViewport() && GetWorld()->GetGameViewport()->Viewport && GetWorld()->GetGameViewport()->Viewport->HasFocus();
 const bool PanCtrl=IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl),PanShift=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift),PanAlt=IsInputKeyDown(EKeys::LeftAlt)||IsInputKeyDown(EKeys::RightAlt);
 const auto PanKey=[&](FKey Key){return IsInputKeyDown(Key)&&!CardHotkeys.Contains(Key)&&FRTSControls::Get().Match(Key,PanCtrl,PanShift,PanAlt,bBuildMenu?TEXT("build."):TEXT("idle.")).IsEmpty();};
 if(PanKey(EKeys::Left) || (Selection.IsEmpty() && PanKey(EKeys::A))) Pan.X-=1;
 if(PanKey(EKeys::Right) || PanKey(EKeys::D)) Pan.X+=1;
 if(PanKey(EKeys::Up) || PanKey(EKeys::W)) Pan.Y-=1;
 if(PanKey(EKeys::Down) || (Selection.IsEmpty() && PanKey(EKeys::S))) Pan.Y+=1;
 if(bScreenFocus && !bDragging && Mouse.X>=0 && Mouse.Y>=0 && Mouse.X<ViewSize.X && Mouse.Y<ViewSize.Y) {
  if(Mouse.X<9) Pan.X-=1; if(Mouse.X>ViewSize.X-9) Pan.X+=1;
  if(Mouse.Y<24) Pan.Y-=1; if(Mouse.Y>ViewSize.Y-9) Pan.Y+=1;
 }
 if(!Pan.IsNearlyZero()) {
  Pan.Normalize();
  const float Speed=35.f*Delta/Zoom*(IsInputKeyDown(EKeys::LeftShift)?1.7f:1.f);
  CameraCenter+=RotateMap(FVector2D(Pan.X+Pan.Y,Pan.Y-Pan.X),true)*Speed;
  CameraCenter.X=FMath::Clamp(CameraCenter.X,0.,160.);CameraCenter.Y=FMath::Clamp(CameraCenter.Y,0.,160.);
 }
}

void ARTSController::Issue(const FString& Action,FVector2D Point,int32 Target,const FString& Type,bool bQueued) {
 if(Phase!=1 && Action!=TEXT("lobby")) return;
 TArray<int32> Own;
 for(int32 Id:Selection) {const auto* E=FindEntity(Id);if(E && E->Owner==LocalSlot && !E->bRemembered && (ActiveSubgroup.IsEmpty()||FRTSCatalog::Get().Def(E->Def).Id==ActiveSubgroup)) Own.Add(Id);}
 if(Own.IsEmpty() && Action!=TEXT("resign") && Action!=TEXT("lobby")) {ShowNotice(TEXT("Select one of your units or buildings first."));return;}
 ServerOrder(Action,Own,Point,Target,Type,bQueued);
 bool ResourceAssignment=false,Compromise=false;
 if(Action==TEXT("context")||Action==TEXT("gather"))if(const auto* Site=FindEntity(Target)) {
  const auto& SD=FRTSCatalog::Get().Def(Site->Def);
  bool Worker=false;for(int32 Id:Own)if(const auto* Unit=FindEntity(Id))Worker|=RTS::IsWorker(FRTSCatalog::Get().Def(Unit->Def))||FRTSCatalog::Get().Def(Unit->Def).Kind==TEXT("beast");
  if(SD.Id==TEXT("seat_of_the_people")||SD.Id==TEXT("storehouse_of_provisions"))for(int32 Id:Own)if(const auto* Unit=FindEntity(Id))if(FRTSCatalog::Get().Def(Unit->Def).Id==TEXT("lawyer")&&Site->Owner!=LocalSlot)Compromise=true;
  ResourceAssignment=Worker&&(SD.Kind==TEXT("resource")||SD.Id==TEXT("fields_of_grain")||SD.Id==TEXT("gathering_of_flocks")||SD.Id==TEXT("hill_of_ore"));
 }
 CommandFlashColor=Compromise?FLinearColor(.73f,.39f,.89f,1):ResourceAssignment?FLinearColor(.24f,.90f,.63f,1):FLinearColor(.94f,.79f,.46f,1);
 if(Action==TEXT("storehouse_raid")||Action==TEXT("rally_economic")||Action==TEXT("market_galavant"))CommandFlashColor=FLinearColor(.20f,.78f,.73f,1);
 PlayCue(Compromise?TEXT("compromise"):ResourceAssignment?TEXT("assign_resource"):TEXT("order"));
 if(Action==TEXT("galavant")){for(int32 Id:Own){auto& Route=GalavantRoutes.FindOrAdd(Id);if(!bQueued)Route.Empty();if(Route.Num()<32)Route.Add(Point);}}
 else if(!bQueued&&(Action==TEXT("stop")||Action==TEXT("move")||Action==TEXT("context")))for(int32 Id:Own)GalavantRoutes.Remove(Id);
 if(Action==TEXT("storehouse_raid")||Action==TEXT("attack")||Action==TEXT("convert")||Action==TEXT("raid")||Action==TEXT("gather")||Action==TEXT("repair")||Action==TEXT("market_galavant")||Action==TEXT("rally_economic")||Action==TEXT("galavant")||Action==TEXT("raid_move")||Action==TEXT("wall_line")||Action==TEXT("move") || Action==TEXT("context") || Action==TEXT("attack_move") || Action==TEXT("patrol") || Action==TEXT("rally") || Action==TEXT("build")) {
  CommandFlash=Point;if(Target>0)if(const auto* TargetEntity=FindEntity(Target))CommandFlash=VisualPosition(*TargetEntity);CommandFlashTime=GetWorld()->GetRealTimeSeconds();
 }
}

void ARTSController::FocusHeadquarters() {
 for(const auto& E:Entities) if(E.Owner==LocalSlot && FRTSCatalog::Get().Def(E.Def).Id==TEXT("seat_of_the_people")) {CameraCenter=E.Pos;return;}
 for(const auto& E:Entities) if(E.Owner==LocalSlot) {CameraCenter=E.Pos;return;}
}

bool ARTSController::CanUse(const FRTSDef& D,FString& Reason) const {
 const auto* P=Me();if(!P) {Reason=TEXT("Waiting for player state");return false;}
 const auto& Catalog=FRTSCatalog::Get();const auto& F=Catalog.Faction(P->Faction);
 const bool bAge=D.Kind==TEXT("age_option"),bGate=D.Id==TEXT("gate_segment_upgrade");
 if(bAge){
  if(!F.Upgrades.Contains(D.Id)){Reason=TEXT("Unavailable to your civilization");return false;}
  const auto& Hero=Catalog.Def(D.HeroChoice);if(P->Population+Hero.Pop>P->Capacity){Reason=TEXT("Build habitations for your age-up hero");return false;}
  for(const auto& E:Entities)if(E.Owner==LocalSlot)for(const FString& Queued:E.Queue)if(Catalog.Def(Queued).Kind==TEXT("age_option")){Reason=TEXT("An age advancement is already queued");return false;}
 }

 if(!bAge && !(D.bBuilding?F.Buildings.Contains(D.Id):D.bUpgrade?F.Upgrades.Contains(D.Id):F.Units.Contains(D.Id))) {Reason=TEXT("Unavailable to your civilization");return false;}
 if(bAge && P->Age!=D.Age) {Reason=TEXT("Age already reached");return false;}
 if(P->Age<Catalog.RequiredAge(D,P->Faction)) {Reason=FString::Printf(TEXT("Requires Age %d"),Catalog.RequiredAge(D,P->Faction));return false;}
 if(D.Id==TEXT("temple_of_the_land")&&P->Faction==TEXT("laban"))for(const auto& E:Entities)if(E.Owner==LocalSlot&&E.Def==Catalog.Index(D.Id)){Reason=TEXT("Limit of one Great and Spacious Building");return false;}
 if(D.bUpgrade && !bGate && P->Upgrades.Contains(D.Id)) {Reason=TEXT("Already researched");return false;}
 if(D.bBuilding&&!CanConstruct(D)){Reason=TEXT("Select a builder able to construct this age and material");return false;}
 if(D.CementCost>0&&(!RTS::IsJaredite(P->Faction)||P->Cement<D.CementCost)){Reason=TEXT("Insufficient Cement");return false;}
 if(D.Id==TEXT("works_of_defense") && !P->Upgrades.Contains(TEXT("heaps_of_earth"))) {Reason=TEXT("Research ")+Catalog.DisplayName(Catalog.Def(TEXT("heaps_of_earth")),P->Faction)+TEXT(" first");return false;}
 if(D.Id==TEXT("age3")) {
  bool Workshop=false;for(const auto& E:Entities) if(E.Owner==LocalSlot && E.Progress>=1 && Catalog.Def(E.Def).Id==TEXT("house_of_curious_workmanship"))Workshop=true;
  if(!Workshop) {Reason=TEXT("Requires ")+Catalog.DisplayName(Catalog.Def(TEXT("house_of_curious_workmanship")),P->Faction);return false;}
 }
 if(D.bHero&&!P->HeroChoices.Contains(D.Id)){Reason=TEXT("Choose this hero when advancing ages first");return false;}
 if(D.bHero||(D.bUpgrade&&!bGate)) for(const auto& E:Entities) if(E.Owner==LocalSlot) {
  if(D.bHero&&Catalog.Def(E.Def).Id==D.Id) {Reason=TEXT("This hero already serves you");return false;}
  if(E.Queue.Contains(D.Id)) {Reason=TEXT("Already queued");return false;}
 }
 for(const auto& Req:D.Requires) {
  bool Found=P->Upgrades.Contains(Req);
  for(const auto& E:Entities) if(E.Owner==LocalSlot && E.HP>0 && E.Progress>=1 && Catalog.Def(E.Def).Id==Req) Found=true;
  if(!Found) {Reason=TEXT("Requires ")+Catalog.DisplayName(Catalog.Def(Req),P->Faction);return false;}
 }
 if(bGate){bool Eligible=false;for(int Id:Selection)if(const auto* E=FindEntity(Id)){const FString Wall=Catalog.Def(E->Def).Id;if(E->Owner==LocalSlot&&E->Progress>=1&&!E->bGate&&(Wall==TEXT("works_of_defense")||Wall==TEXT("cement_wall"))&&!E->Queue.Contains(D.Id))Eligible=true;}if(!Eligible){Reason=TEXT("Select a completed wall without a gate upgrade queued");return false;}}
 const FVector Price=Catalog.Price(D,P->Faction);
 if(P->Resources.X<Price.X || P->Resources.Y<Price.Y || P->Resources.Z<Price.Z) {Reason=TEXT("Insufficient resources");return false;}
 if(!D.bBuilding && !D.bUpgrade && P->Population+D.Pop>P->Capacity) {Reason=TEXT("Build habitations for more population");return false;}
 Reason.Empty();return true;
}

void ARTSController::MouseDown() {
 RefreshPointer();bDragging=false;
 ARTSHUD* HUD=Cast<ARTSHUD>(GetHUD());
 if(HUD && HUD->Click(Mouse)) return;
 if(bMenu || Phase==0 || bPauseMenu || bHelp || bHotkeySettings || Phase==2) return;
 FVector2D Mini;
 if(HUD && HUD->MinimapPoint(Mouse,Mini)) {if(bLocalLiahona) CameraCenter=Mini;return;}
 if(!InWorld(Mouse)) return;
 TextFocus.Empty();
 if(!PendingAction.IsEmpty()) {
  const int32 Target=EntityUnder(Mouse,true);FVector2D Point=ScreenToWorld(Mouse);
  if(PendingAction==TEXT("ability")&&Selection.Num()) {
   if(const auto* Unit=FindEntity(Selection[0])) if(FRTSCatalog::Get().Def(Unit->Def).Id==TEXT("samuel_the_lamanite")) Point=Unit->Pos;
  }
  const bool Queue=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift);
  const bool Wall=PendingAction==TEXT("build")&&(PendingType==TEXT("works_of_defense")||PendingType==TEXT("cement_wall"));
  if(Wall){
   if(!bWallAnchor){WallAnchor=Point;bWallAnchor=true;ShowNotice(TEXT("Choose the wall's other endpoint. Hold Shift to continue the chain."));return;}
   if(FVector2D::Distance(WallAnchor,Point)<.25f)return;
   Issue(TEXT("wall_line"),Point,0,FString::Printf(TEXT("%s|%.4f|%.4f"),*PendingType,WallAnchor.X,WallAnchor.Y),Queue);
   if(Queue){const float Spacing=2*FRTSCatalog::Get().Def(PendingType).Radius+.15f;const FVector2D Delta=Point-WallAnchor;const int32 LastSample=FMath::Min(127,FMath::FloorToInt(Delta.Size()/Spacing+.0001f));WallAnchor+=Delta.GetSafeNormal()*(LastSample*Spacing);}else{bWallAnchor=false;PendingAction.Empty();PendingType.Empty();}return;
  }
  Issue(PendingAction,Point,Target,PendingType,Queue);
  if(PendingAction==TEXT("galavant")||PendingAction==TEXT("market_galavant"))return;
  if((!IsInputKeyDown(EKeys::LeftShift)&&!IsInputKeyDown(EKeys::RightShift)) || PendingAction!=TEXT("build")) {PendingAction.Empty();PendingType.Empty();}
  return;
 }
 DragStart=Mouse;bDragging=true;
}
void ARTSController::MouseUp() {
 RefreshPointer();
 if(!bDragging) return;bDragging=false;
 const bool bAdd=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift);
 if(!bAdd) Selection.Empty();
 if(FVector2D::Distance(DragStart,Mouse)>7) {
  const FBox2D Box(FVector2D(FMath::Min(DragStart.X,Mouse.X),FMath::Min(DragStart.Y,Mouse.Y)),FVector2D(FMath::Max(DragStart.X,Mouse.X),FMath::Max(DragStart.Y,Mouse.Y)));
  bool HasUnits=false;
  for(const auto& E:Entities) if(E.Owner==LocalSlot && !FRTSCatalog::Get().Def(E.Def).bBuilding && Box.Intersect(EntityBounds(E))) {Selection.AddUnique(E.Id);HasUnits=true;}
  if(!HasUnits) for(const auto& E:Entities) if(E.Owner==LocalSlot && Box.Intersect(EntityBounds(E))) Selection.AddUnique(E.Id);
 } else {
  int32 Id=EntityUnder(Mouse);
  if(Id) {
   if(bAdd && Selection.Contains(Id)) Selection.Remove(Id);else Selection.AddUnique(Id);
   const float Now=GetWorld()->GetRealTimeSeconds();
   const auto* E=FindEntity(Id);
   if(E && E->Owner==LocalSlot && Id==LastSelectedId && Now-LastSelectTime<.3f) for(const auto& Other:Entities) if(Other.Owner==LocalSlot && Other.Def==E->Def && InWorld(WorldToScreen(Other.Pos))) Selection.AddUnique(Other.Id);
   LastSelectTime=Now;LastSelectedId=Id;
  }
 }
 bBuildMenu=false;ActionPage=0;ActiveSubgroup.Empty();
 if(Selection.Num()) PlayCue(TEXT("select"));
}
void ARTSController::RightClick() {
 RefreshPointer();bDragging=false;
 if(bHotkeySettings||bChat||bPauseMenu||bHelp)return;
 ARTSHUD* HUD=Cast<ARTSHUD>(GetHUD());FVector2D Point;
 if(HUD&&HUD->RightClick(Mouse))return;
 if(!PendingAction.IsEmpty()) {PendingAction.Empty();PendingType.Empty();bWallAnchor=false;return;}
 if(HUD && HUD->MinimapPoint(Mouse,Point)) {if(bLocalLiahona) Issue(TEXT("context"),Point,0,FString(),IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift));return;}
 if(!InWorld(Mouse)) return;
 const int32 Target=EntityUnder(Mouse,true);Point=ScreenToWorld(Mouse);
 Issue(TEXT("context"),Point,Target,FString(),IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift));
}

void ARTSController::CheckDraft(bool bSubmit) {
 if(!bDraftFailed && RTS::HasProfanity(ChatDraft,bSubmit)) {
  bDraftFailed=true;bLocalLiahona=false;bAwaitingLiahonaFailure=true;bAwaitingLiahonaRestore=false;LiahonaFailureTime=GetWorld()->GetRealTimeSeconds();
  ShowNotice(TEXT("The Liahona has ceased to work."));ServerLiahonaFailure();
  PlayCue(TEXT("failure"));
 }
}
void ARTSController::TextCharacter(TCHAR C) {
 if(C<32 || C==127) return;
 if(bChat || TextFocus==TEXT("chat")) {if(ChatDraft.Len()<220) {ChatDraft.AppendChar(C);CheckDraft(false);}return;}
 if(TextFocus==TEXT("name")) {if(PlayerName.Len()<24) PlayerName.AppendChar(C);}
 else if(TextFocus==TEXT("join")) {if(JoinAddress.Len()<100 && (FChar::IsAlnum(C) || C=='.'||C==':'||C=='-')) JoinAddress.AppendChar(C);}
}
void ARTSController::TextPaste(const FString& Text) {
 for(TCHAR C:Text) TextCharacter(C);
}
void ARTSController::SubmitText() {
 if(bChat || TextFocus==TEXT("chat")) {
  CheckDraft(true);
  if(RTS::IsApology(ChatDraft)&&!bLocalLiahona) {bLocalLiahona=true;bAwaitingLiahonaFailure=false;bAwaitingLiahonaRestore=true;}
  if(!ChatDraft.TrimStartAndEnd().IsEmpty()) ServerChat(ChatDraft);
  ChatDraft.Empty();bChat=false;TextFocus.Empty();bDraftFailed=false;
 } else if(TextFocus==TEXT("join")) {UIAction(TEXT("join"));}
 else if(TextFocus==TEXT("name")) {
  TextFocus.Empty();const auto& Fs=FRTSCatalog::Get().Factions;
  if(Fs.IsValidIndex(LobbyFaction)) ServerLobby(PlayerName,Fs[LobbyFaction].Id,LobbyTeam,false);
 }
}

bool ARTSController::InputKey(const FInputKeyParams& Params) {
 const bool bBase=Super::InputKey(Params);
 if(!IsLocalController()) return bBase;
 const FKey Key=Params.Key;const bool Press=Params.Event==IE_Pressed || Params.Event==IE_Repeat;
 if(Key==EKeys::LeftMouseButton) {if(Params.Event==IE_Pressed||Params.Event==IE_DoubleClick) MouseDown();else if(Params.Event==IE_Released) MouseUp();return true;}
 if(Key==EKeys::RightMouseButton && (Params.Event==IE_Pressed||Params.Event==IE_DoubleClick)) {RightClick();return true;}
 if(Key==EKeys::Delete&&Params.Event==IE_Released)DeleteHeldSince=-1;
 if(!Press) return bBase;
 const bool Ctrl=IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl);
 const bool Shift=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift);
 const bool Alt=IsInputKeyDown(EKeys::LeftAlt)||IsInputKeyDown(EKeys::RightAlt);
 if(bHotkeySettings){
  if(Key.IsModifierKey())return true;
  if(Key==EKeys::Escape){if(!HotkeyEditId.IsEmpty())HotkeyEditId.Empty();else bHotkeySettings=false;return true;}
  if(!HotkeyEditId.IsEmpty()&&Params.Event==IE_Pressed){
   auto& Controls=FRTSControls::Get();FString Result;
   if(Key==EKeys::BackSpace){Controls.Clear(HotkeyEditId);Controls.Save();HotkeyEditId.Empty();ShowNotice(TEXT("Shortcut cleared."));}
   else if(Controls.Rebind(HotkeyEditId,Key,Ctrl,Shift,Alt,Result)){Controls.Save();HotkeyEditId.Empty();ShowNotice(TEXT("Shortcut saved. Conflicting shortcuts are swapped."));}
   else ShowNotice(Result);
  }
  return true;
 }
 if(!ConnectionError.IsEmpty()) {if(Key==EKeys::Enter||Key==EKeys::Escape)UIAction(TEXT("dismiss_error"));return true;}
 if(bConnecting) {if(Key==EKeys::Escape)UIAction(TEXT("cancel_join"));return true;}
 if(bChat || !TextFocus.IsEmpty()) {
  if(Key==EKeys::Enter) SubmitText();
  else if(Key==EKeys::Escape) {bChat=false;TextFocus.Empty();}
  else if(Key==EKeys::BackSpace) {
   FString* Edit=bChat?&ChatDraft:TextFocus==TEXT("name")?&PlayerName:TextFocus==TEXT("join")?&JoinAddress:&ChatDraft;
   if(Ctrl) Edit->Empty();else if(!Edit->IsEmpty()) Edit->LeftChopInline(1);
   if(bChat) CheckDraft(false);
  } else if(Ctrl && Key==EKeys::V) {FString Clipboard;FPlatformApplicationMisc::ClipboardPaste(Clipboard);TextPaste(Clipboard);}
  else if(Ctrl && Key==EKeys::A) {if(bChat) ChatDraft.Empty();else if(TextFocus==TEXT("name")) PlayerName.Empty();else JoinAddress.Empty();}
  return true;
 }
 if(Key==EKeys::Enter && !bMenu) {bChat=true;TextFocus=TEXT("chat");return true;}
 if(Key==EKeys::F1) {bHelp=!bHelp;return true;}
 if(Key==EKeys::Escape || Key==EKeys::F10) {
  if(!PendingAction.IsEmpty()) {PendingAction.Empty();PendingType.Empty();bWallAnchor=false;}
  else if(bHelp) bHelp=false;else if(bMenu && MenuScreen) MenuScreen=0;else if(bBuildMenu) bBuildMenu=false;else if(!bMenu) bPauseMenu=!bPauseMenu;
  return true;
 }
 if(Phase!=1 || bMenu || bPauseMenu || bHelp) return bBase;
 if(Press&&!bBuildMenu&&HasSelectedBuilder()&&FRTSControls::Get().Match(Key,Ctrl,Shift,Alt,TEXT("menu."))==TEXT("menu.build")){if(Params.Event==IE_Pressed)UIAction(TEXT("build_toggle"));return true;}
 if(Press){
  const FString Action=FRTSControls::Get().Match(Key,Ctrl,Shift,Alt,bBuildMenu?TEXT("build."):TEXT("idle."));
  if(Action.StartsWith(TEXT("build."))&&HasSelectedBuilder()){
   if(Params.Event!=IE_Pressed)return true;
   const FString Id=Action.RightChop(6);FString Reason;
   if(CanUse(FRTSCatalog::Get().Def(Id),Reason))UIAction(TEXT("build"),Id);else ShowNotice(Reason);
   return true;
  }
  if(Action==TEXT("idle.next")||Action==TEXT("idle.all")){if(Params.Event!=IE_Pressed)return true;SelectIdleVillagers(Action==TEXT("idle.all"));return true;}
 }
 if(Key==EKeys::Delete&&Params.Event==IE_Pressed&&SupportsOrder(TEXT("delete"))){DeleteSelection=Selection;DeleteHeldSince=GetWorld()->GetRealTimeSeconds();return true;}
 if(!Ctrl&&!Alt&&!bBuildMenu)if(const auto* Card=CardHotkeys.Find(Key)){if(Params.Event==IE_Pressed)UIAction(Card->Key,Card->Value);return true;}
 if(Key==EKeys::MouseScrollUp || Key==EKeys::MouseScrollDown) {
  if(Shift){const FVector2D Before=ScreenToWorld(Mouse);CameraRotation+=Key==EKeys::MouseScrollUp?PI/12.f:-PI/12.f;CameraCenter+=Before-ScreenToWorld(Mouse);return true;}
  const FVector2D Before=ScreenToWorld(Mouse);Zoom=FMath::Clamp(Zoom*(Key==EKeys::MouseScrollUp?1.12f:1.f/1.12f),.48f,2.4f);
  CameraCenter+=Before-ScreenToWorld(Mouse);return true;
 }
 if(Key==EKeys::SpaceBar) {FocusHeadquarters();return true;}
 if(!Ctrl&&!Alt&&!Selection.IsEmpty()) {
  if(Key==EKeys::A&&SupportsOrder(TEXT("attack_move"))) PendingAction=TEXT("attack_move");
  if(Key==EKeys::S&&SupportsOrder(TEXT("stop"))) Issue(TEXT("stop"),FVector2D::ZeroVector,0,FString(),Shift);
  if(Key==EKeys::H&&SupportsOrder(TEXT("hold"))) Issue(TEXT("hold"),FVector2D::ZeroVector,0,FString(),Shift);
  if(Key==EKeys::P&&SupportsOrder(TEXT("patrol"))) PendingAction=TEXT("patrol");
  if(Key==EKeys::C&&SupportsOrder(TEXT("convert"))) PendingAction=TEXT("convert");
  if(Key==EKeys::R&&SupportsOrder(TEXT("raid"))) PendingAction=TEXT("raid");
  if(Key==EKeys::V&&SupportsOrder(TEXT("raid_move"))) PendingAction=TEXT("raid_move");
  if(Key==EKeys::G){if(SupportsOrder(TEXT("market_galavant")))UIAction(TEXT("order"),TEXT("market_galavant"));else if(SupportsOrder(TEXT("galavant")))UIAction(TEXT("order"),TEXT("galavant"));}
  if(Key==EKeys::E&&SupportsOrder(TEXT("ability"))) UIAction(TEXT("order"),TEXT("ability"));
  if(Key==EKeys::Y){if(Shift&&SupportsOrder(TEXT("rally_economic")))PendingAction=TEXT("rally_economic");else if(!Shift&&SupportsOrder(TEXT("storehouse_raid")))PendingAction=TEXT("storehouse_raid");else if(!Shift&&SupportsOrder(TEXT("rally")))PendingAction=TEXT("rally");}
 }
 const FKey Digits[]={EKeys::Zero,EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine};
 for(int32 I=0;I<10;I++) if(Key==Digits[I]) {
  if(Params.Event!=IE_Pressed)return true;const float Now=GetWorld()->GetRealTimeSeconds();const bool Center=Alt||(!Ctrl&&!Shift&&LastControlGroup==I&&Now-LastControlGroupTime<.35f);LastControlGroup=Ctrl||Shift?-1:I;LastControlGroupTime=Now;
  if(Shift){auto& Group=ControlGroups.FindOrAdd(I);for(int32 Id:Selection)if(const auto* E=FindEntity(Id))if(E->Owner==LocalSlot)Group.AddUnique(Id);ShowNotice(FString::Printf(TEXT("Added to control group %d."),I));}
  else if(Ctrl) {ControlGroups.Add(I,Selection);ShowNotice(FString::Printf(TEXT("Control group %d assigned."),I));}
  else if(const TArray<int32>* Group=ControlGroups.Find(I)) {
   Selection=*Group;Selection.RemoveAll([this](int32 Id){const auto* E=FindEntity(Id);return !E||E->Owner!=LocalSlot||E->bRemembered;});
   if(Center&&!Selection.IsEmpty()){FVector2D Sum=FVector2D::ZeroVector;for(int Id:Selection)Sum+=FindEntity(Id)->Pos;CameraCenter=Sum/Selection.Num();}
   bBuildMenu=false;ActionPage=0;SelectionPage=0;ActiveSubgroup.Empty();
  }
  return true;
 }
 return bBase;
}

void ARTSController::UIAction(const FString& Action,const FString& Value) {
 if(Action!=TEXT("focus")&&Action!=TEXT("sound")) PlayCue(TEXT("ui"));
 const auto& Fs=FRTSCatalog::Get().Factions;
 if(Action==TEXT("host")) {OpenIntoLobby=true;OpeningHost=true;UGameplayStatics::OpenLevel(this,FName(TEXT("/Engine/Maps/Entry")),true,TEXT("listen?Version=1"));}
 else if(Action==TEXT("solo")) {bMenu=false;MenuScreen=0;ShowNotice(TEXT("Choose your civilization, ready up, then begin your sandbox."));}
 else if(Action==TEXT("join_screen")) {MenuScreen=1;TextFocus=TEXT("join");}
 else if(Action==TEXT("join")) {
  FString Address=JoinAddress.TrimStartAndEnd();
  if(Address.IsEmpty()) {ShowNotice(TEXT("Enter the host computer's LAN address."));return;}
  OpenIntoLobby=true;bMenu=false;bConnecting=true;ConnectionStarted=GetWorld()->GetRealTimeSeconds();TextFocus.Empty();ClientTravel(Address+TEXT("?Version=1"),TRAVEL_Absolute);
  ShowNotice(TEXT("Connecting to ")+Address+TEXT(" ..."));
 }
 else if(Action==TEXT("back")) {MenuScreen=0;TextFocus.Empty();}
 else if(Action==TEXT("dismiss_error")) {ConnectionError.Empty();bMenu=true;MenuScreen=0;}
 else if(Action==TEXT("cancel_join")) ConnectionFailed(TEXT("The connection was cancelled. You can join another host from the main menu."));
 else if(Action==TEXT("focus")) TextFocus=Value;
 else if(Action==TEXT("faction")) {LobbyFaction=FCString::Atoi(*Value);if(Fs.IsValidIndex(LobbyFaction)) ServerLobby(PlayerName,Fs[LobbyFaction].Id,LobbyTeam,false);}
 else if(Action==TEXT("team")) {LobbyTeam=(LobbyTeam+1)%2;if(Fs.IsValidIndex(LobbyFaction)) ServerLobby(PlayerName,Fs[LobbyFaction].Id,LobbyTeam,false);}
 else if(Action==TEXT("ready")) {TextFocus.Empty();if(Fs.IsValidIndex(LobbyFaction)) ServerLobby(PlayerName,Fs[LobbyFaction].Id,LobbyTeam,Me()?!Me()->bReady:true);}
 else if(Action==TEXT("map")) {LobbyMap=FCString::Atoi(*Value);ServerOrder(TEXT("map"),TArray<int32>(),FVector2D::ZeroVector,LobbyMap,FString());}
 else if(Action==TEXT("lobby_setting")&&IsHost()) {
  int32 R=LobbyResources,F=LobbyFruitfulness;float S=LobbySpeed;bool FogEnabled=bLobbyFog;
  if(Value==TEXT("resources"))R=(R+1)%4;if(Value==TEXT("fruitfulness"))F=(F+1)%3;if(Value==TEXT("fog"))FogEnabled=!FogEnabled;
  if(Value==TEXT("speed")){const float Speeds[]={.75f,1.f,1.25f,1.5f,2.f};for(int I=0;I<5;I++)if(FMath::IsNearlyEqual(S,Speeds[I])){S=Speeds[(I+1)%5];break;}}
  ServerLobbySettings(R,S,F,FogEnabled);
 }
 else if(Action==TEXT("start")) ServerStart(LobbyMap);
 else if(Action==TEXT("help")) bHelp=!bHelp;
 else if(Action==TEXT("pause")) bPauseMenu=!bPauseMenu;
 else if(Action==TEXT("hotkeys")){bHotkeySettings=!bHotkeySettings;HotkeyEditId.Empty();bDragging=false;TextFocus.Empty();bChat=false;}
 else if(Action==TEXT("bind_key"))HotkeyEditId=Value;
 else if(Action==TEXT("reset_keys")){FRTSControls::Get().ResetDefaults();FRTSControls::Get().Save();HotkeyEditId.Empty();ShowNotice(TEXT("Default shortcuts restored."));}
 else if(Action==TEXT("idle"))SelectIdleVillagers(IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl));
 else if(Action==TEXT("workforce"))SelectResourceWorker(FCString::Atoi(*Value));
 else if(Action==TEXT("idle_military"))SelectIdleMilitary(IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl));
 else if(Action==TEXT("selection_unit")) {const int32 Id=FCString::Atoi(*Value);if(!Selection.Contains(Id))return;const bool Shift=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift);if(Shift)Selection.Remove(Id);else{Selection.Empty();Selection.Add(Id);}ActiveSubgroup.Empty();ActionPage=0;SelectionPage=FMath::Clamp(SelectionPage,0,FMath::Max(0,(Selection.Num()-1)/8));}
 else if(Action==TEXT("selection_page"))SelectionPage=FMath::Max(0,FCString::Atoi(*Value));
 else if(Action==TEXT("subgroup")){const bool Shift=IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift),Ctrl=IsInputKeyDown(EKeys::LeftControl)||IsInputKeyDown(EKeys::RightControl);if(Shift||Ctrl){Selection.RemoveAll([this,&Value,Shift](int32 Id){const auto* E=FindEntity(Id);const bool Match=E&&FRTSCatalog::Get().Def(E->Def).Id==Value;return Shift?Match:!Match;});ActiveSubgroup.Empty();SelectionPage=0;}else ActiveSubgroup=ActiveSubgroup==Value?FString():Value;ActionPage=0;}
 else if(Action==TEXT("motion")) bReducedMotion=!bReducedMotion;
 else if(Action==TEXT("sound")) {
  bSoundEnabled=!bSoundEnabled;SoundCuesEnabled=bSoundEnabled;
  if(!bSoundEnabled) {for(auto& Cue:PlayingCues) if(Cue.IsValid())Cue->Stop();PlayingCues.Empty();}
  else PlayCue(TEXT("ui"));
 }
 else if(Action==TEXT("resume")) {bHelp=false;bPauseMenu=false;bHotkeySettings=false;}
 else if(Action==TEXT("menu")) {OpenIntoLobby=false;UGameplayStatics::OpenLevel(this,FName(TEXT("/Engine/Maps/Entry")));}
 else if(Action==TEXT("lobby")) {ServerOrder(TEXT("lobby"),TArray<int32>(),FVector2D::ZeroVector,0,FString());bPauseMenu=false;}
 else if(Action==TEXT("exit")) ConsoleCommand(TEXT("quit"));
 else if(Action==TEXT("build_toggle")) {if(HasSelectedBuilder()){bBuildMenu=!bBuildMenu;PendingAction.Empty();PendingType.Empty();bWallAnchor=false;ActionPage=0;}}
 else if(Action==TEXT("cancel_queue")){TArray<FString> Parts;Value.ParseIntoArray(Parts,TEXT("|"));if(Parts.Num()==2){const int32 Building=FCString::Atoi(*Parts[0]);if(const auto* E=FindEntity(Building))if(E->Owner==LocalSlot)ServerOrder(TEXT("cancel"),TArray<int32>{Building},FVector2D::ZeroVector,0,TEXT("queue:")+Parts[1]);}}
 else if(Action==TEXT("page")) ActionPage=FCString::Atoi(*Value);
 else if(Action==TEXT("order")) {
  if(Value!=TEXT("resign")&&Value!=TEXT("cancel")&&!SupportsOrder(Value))return;
  if(Value==TEXT("ability")){bool Unloaded=false;for(int32 Id:Selection)if(const auto* Unit=FindEntity(Id))if(Unit->Owner==LocalSlot&&RTS::IsCarrier(FRTSCatalog::Get().Def(Unit->Def))&&(ActiveSubgroup.IsEmpty()||ActiveSubgroup==FRTSCatalog::Get().Def(Unit->Def).Id)){ServerOrder(TEXT("ability"),TArray<int32>{Id},Unit->Pos,0,FString());Unloaded=true;}if(Unloaded){PendingAction.Empty();PendingType.Empty();PlayCue(TEXT("order"));return;}}
  bool InstantAbility=false;if(Value==TEXT("ability"))for(int32 Id:Selection)if(const auto* Unit=FindEntity(Id))if(FRTSCatalog::Get().Def(Unit->Def).Kind==TEXT("ammonite"))InstantAbility=true;
  if(InstantAbility||Value==TEXT("stop") || Value==TEXT("hold") || Value==TEXT("cancel") || Value==TEXT("resign") || Value==TEXT("deliver")) Issue(Value,FVector2D::ZeroVector,0,FString(),IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift));
  else {PendingAction=Value;PendingType.Empty();bWallAnchor=false;if(Value==TEXT("galavant")||Value==TEXT("market_galavant"))ShowNotice(TEXT("Click to start a route. Shift+click adds waypoints. Esc finishes the route."));}
 }
 else if(Action==TEXT("build")) {PendingAction=TEXT("build");PendingType=Value;bWallAnchor=false;ShowNotice(Value==TEXT("works_of_defense")||Value==TEXT("cement_wall")?TEXT("Click the first wall endpoint, then the second. Shift continues the chain."):TEXT("Choose a clear construction site. Esc cancels."));}
 else if(Action==TEXT("train") || Action==TEXT("research") || Action==TEXT("age")) Issue(Action,FVector2D::ZeroVector,0,Value,Action==TEXT("train")&&(IsInputKeyDown(EKeys::LeftShift)||IsInputKeyDown(EKeys::RightShift)));
 else if(Action==TEXT("select")) {Selection.Empty();Selection.Add(FCString::Atoi(*Value));ActionPage=0;bBuildMenu=false;}
}

void ARTSController::PlayCue(FName Cue) {
  if(Cue==TEXT("select")&&IsLocalController()&&PlaySelectionVoice())return;
 if(!bSoundEnabled||!IsLocalController()||!GetWorld()||GetNetMode()==NM_DedicatedServer)return;
 const float Now=GetWorld()->GetRealTimeSeconds(),Gap=Cue==TEXT("combat")?.28f:Cue==TEXT("complete")?.4f:.055f;
 if(const float* Last=LastCueTimes.Find(Cue))if(Now-*Last<Gap)return;
 LastCueTimes.Add(Cue,Now);
 // Original synthesized cues: damped mallet tones, plucked acknowledgments and soft percussion.
 // No external samples are loaded and no audio state is sent over the network.
 const bool Combat=Cue==TEXT("combat"),Complete=Cue==TEXT("complete"),Fail=Cue==TEXT("failure");
 const float Seconds=Combat?.095f:Complete?.34f:Fail?.40f:Cue==TEXT("order")?.16f:Cue==TEXT("select")?.105f:.065f;
 constexpr int32 Rate=24000;TArray<int16> PCM;PCM.SetNumUninitialized(FMath::CeilToInt(Seconds*Rate));
 FRandomStream Noise(1977+int32(GetTypeHash(Cue)%1000));
 for(int32 I=0;I<PCM.Num();I++) {
  const float T=float(I)/Rate,Position=T/Seconds;
  const float Envelope=FMath::Min(T/.004f,1.f)*FMath::Square(FMath::Max(0.f,1-Position));float Sample=0;
  if(Combat) Sample=(.7f*FMath::Sin(2*PI*(110*T-180*T*T))+.20f*Noise.FRandRange(-1.f,1.f))*FMath::Exp(-T*16);
  else if(Complete) Sample=.42f*FMath::Sin(2*PI*523.25f*T)+.28f*FMath::Sin(2*PI*659.25f*T)+.24f*FMath::Sin(2*PI*783.99f*T);
  else if(Fail) Sample=.6f*FMath::Sin(2*PI*(659*T-390*T*T))+.2f*FMath::Sin(2*PI*330*T);
  else if(Cue==TEXT("select")) Sample=.65f*FMath::Sin(2*PI*880*T)+.20f*FMath::Sin(2*PI*1320*T)*FMath::Exp(-T*20);
  else if(Cue==TEXT("compromise")) Sample=.48f*FMath::Sin(2*PI*(420*T-160*T*T))+.22f*FMath::Sin(2*PI*630*T);
  else if(Cue==TEXT("assign_resource")) Sample=.48f*FMath::Sin(2*PI*740*T)+.25f*FMath::Sin(2*PI*1110*T)*FMath::Exp(-T*18);
  else if(Cue==TEXT("order")) Sample=.68f*FMath::Sin(2*PI*(523*T+370*T*T))+.16f*FMath::Sin(2*PI*1046*T);
  else Sample=.7f*FMath::Sin(2*PI*620*T)*FMath::Exp(-T*22)+.12f*Noise.FRandRange(-1.f,1.f)*FMath::Exp(-T*55);
  PCM[I]=int16(FMath::Clamp(Sample*Envelope*.24f,-1.f,1.f)*32760);
 }
 USoundWaveProcedural* Wave=NewObject<USoundWaveProcedural>(this);
 Wave->SetSampleRate(Rate);Wave->NumChannels=1;Wave->Duration=Seconds;Wave->bLooping=false;Wave->SoundGroup=SOUNDGROUP_UI;
 Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()),PCM.Num()*sizeof(int16));
 if(UAudioComponent* Audio=UGameplayStatics::SpawnSound2D(this,Wave,Combat?.55f:.75f,1.f,0.f,nullptr,false,true)) {
  PlayingCues.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& A){return !A.IsValid();});PlayingCues.Add(Audio);
  const TWeakObjectPtr<UAudioComponent> Weak=Audio;FTimerHandle Handle;
  GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([Weak](){if(Weak.IsValid())Weak->Stop();}),Seconds+.1f,false);
 }
}
