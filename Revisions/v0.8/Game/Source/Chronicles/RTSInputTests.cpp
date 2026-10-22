#include "RTSInputTests.h"
#include "RTSController.h"
#include "RTSHUD.h"
#include "RTSGameMode.h"
#include "RTSWorld.h"
#include "RTSControls.h"
#include "RTSVisualAssets.h"
#include "RTSV08UITests.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Slate/SceneViewport.h"
#include "Input/Events.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace {
 enum class EProbeStage : uint8 {
  Menu,ReleaseSolo,Lobby,ReleaseReady,Ready,ReleaseStart,Match,
  ReleaseWorker,Worker,ReleaseResource,Gather,Delivery,
  DragPressed,DragMid,DragEnd,DragResult,ReleaseIdle,Idle,NextIdle,AllIdle,BuildMenu,ReleaseBuildCard,BuildCard,BuildKey,BuildChoice,
  BuildingPingTarget,BuildingPingUp,BuildingPingCheck,MilitaryCardUp,MilitaryPoint,MilitaryPointUp,MilitaryCheck,LobbySettings,LobbySettingUp,LobbySettingCheck,PortraitStart,PortraitShiftPress,PortraitShiftUp,PortraitShiftCheck,PortraitCtrlPress,PortraitCtrlUp,PortraitCtrlCheck,GroupDouble,AnimationSetup,AnimationA,AnimationB,AnimationNext,
  ReleaseHQ,HQ,ReleaseRally,Rally,EconomicCardUp,EconomicPoint,EconomicPointUp,EconomicCheck,RotateWheel,RotateCheck,GroupSaved,GroupWorkerUp,GroupWorker,GroupAppended,GroupRecall,ArmyReady,ArmyUp,ArmySelected,QueueHQUp,QueuePrepare,QueueVisible,QueueCancelUp,QueueCancelled,WorkforceReady,WorkforceUp,WorkforceCheck,ShiftHQUp,ShiftTrainPrepare,ShiftTrainPress,ShiftTrainUp,ShiftTrainCheck,CarrierReady,CarrierArmyUp,CarrierArmySelected,CarrierBoardUp,CarrierBoardWait,CarrierSelectUp,CarrierSelected,CarrierUnload,CarrierUnloadWait,FinalHQUp,GlobalAgeHidden,WallBuilderReady,WallBuilderUp,WallBuildOpen,ConstructionFirstSlot,ConstructionSlotCheck,WallBuildCard,WallBuildCardUp,WallFirst,WallFirstUp,WallAnchorCheck,WallSecond,WallSecondUp,WallLineCheck,WallThirdUp,WallChainCheck,WallShot,TraderReady,TraderUp,GalavantButton,GalavantButtonUp,GalavantFirst,GalavantFirstUp,GalavantFirstCheck,GalavantSecond,GalavantSecondUp,GalavantRouteCheck,GalavantShot,MarketReady,MarketUp,MarketButton,MarketButtonUp,MarketFirst,MarketFirstUp,MarketFirstCheck,MarketSecond,MarketSecondUp,MarketRouteCheck,MarketTrain,MarketTrainUp,MarketProduced,TempleReady,TempleUp,TempleCards,TempleOrderUp,TempleOrderQueued,TemplePrideReady,TemplePrideUp,TemplePrideQueued,ReligionHover,ReligionShot,TempleCancelUp,TempleCancelled,
  V08LabanSetup,V08BuildOpen,V08BuildLocks,V08MilitaryLocks,V08WineryCard,V08WineryUp,V08Concubine,V08District,V08FarDistrict,V08GadiantonSetup,V08RaidCard,V08RaidCardUp,V08RaidTarget,V08RaidTargetUp,V08RaidCheck,V08RaidKey,V08RaidGroundUp,V08RaidGroundCheck,V08BribeHover,V08BribeCheck,V08HideoutOrder,V08HideoutUp,V08HideoutBoard,V08HideoutSlot,V08DeathSetup,V08DeathA,V08DeathB,V08DeathExpiry,
  ShotGame,MenuHotkeys,ReleaseHotkeys,ShotHotkeys,Finished
 };
 struct FInputProbe {
  TWeakObjectPtr<ARTSController> Owner;
  EProbeStage Stage=EProbeStage::Menu;
  double Started=FPlatformTime::Seconds(),StageStarted=Started;
  uint64 Tick=0,StageTick=0;
  int32 Downs=0,Ups=0,Moves=0,WorkerId=0,ResourceId=0,HQId=0;
  int32 KeyDowns=0,KeyUps=0,PreviousIdle=0,ChordStep=0;
  uint64 ChordFrame=0;
  bool ChordActive=false;
  FKey ChordKey;
  FModifierKeysState ChordMods;
  TArray<FKey> ChordModifiers;
  TArray<int32> ExpectedIdle;
  int32 ArmyId=0,CarrierId=0,QueueBeforeBatch=0,PingBuildingId=0;float OriginalRotation=0;
  FString FixtureAgeChoice;
  int32 WallBuilderId=0,TraderId=0,MarketId=0,TempleId=0,BeforeMarketProduction=0;
  FVector2D EconomicPointExpected=FVector2D::ZeroVector;
  FVector2D WallPixels[3],WallPoints[3],RoutePixels[2],RoutePoints[2];
  float WallSpacing=0,WallTimberBefore=0;
  int32 FirstWallSamples=0,SettingStep=0,PortraitRemoved=0,PortraitIsolated=0,PortraitCount=0,AnimationIndex=0,AnimationEntityId=0;
  int32 AnimationSavedMap=0;float AnimationSavedZoom=1;FVector2D AnimationSavedCamera=FVector2D::ZeroVector;TArray<int32> AnimationIds;
  bool LobbySettingsDone=false;FVector2D GroupCameraBefore=FVector2D::ZeroVector;FRTSVisualPoseMetrics PoseA;
  int32 V08Builder=0,V08Winery=0,V08Barracks=0,V08Concubine=0,V08Storehouse=0,V08RaidTarget=0,V08Lawyer=0,V08Hideout=0,V08Robber=0,V08FirstTrained=0,V08Deaths[2]={0,0};
  FRTSVisualPoseMetrics V08DeathPoses[2];
  bool CapturePending=false;uint64 CaptureTick=0;
  FVector2D RotationAnchor=FVector2D::ZeroVector;
  float InitialFood=0,DeliveredFood=0;
  bool CargoSeen=false,Entered=false,Done=false;
  FVector2D Pointer=FVector2D::ZeroVector,DragFrom=FVector2D::ZeroVector,DragTo=FVector2D::ZeroVector,ExpectedRally=FVector2D::ZeroVector;
  FKey Held;
  TSet<FKey> Pressed;
  TArray<int32> DragWorkers;
  TArray<TSharedPtr<FJsonValue>> Results;

  void Advance(EProbeStage Next) {Stage=Next;StageStarted=FPlatformTime::Seconds();StageTick=Tick;CapturePending=false;}
  void Record(const FString& Name,bool Pass,const FString& Detail) {
   TSharedPtr<FJsonObject> Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Name);Row->SetBoolField(TEXT("passed"),Pass);Row->SetStringField(TEXT("detail"),Detail);
   Results.Add(MakeShared<FJsonValueObject>(Row));
   UE_LOG(LogTemp,Display,TEXT("RTS_INPUT_TEST_%s %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Name,*Detail);
  }
  void Finish(ARTSController* C,bool Pass,const FString& Failure=FString()) {
   if(Done)return;Done=true;
   if(!Failure.IsEmpty())Record(TEXT("probe_completion"),false,Failure);
   TSharedPtr<FJsonObject> Root=MakeShared<FJsonObject>();
   Root->SetNumberField(TEXT("schema_version"),1);Root->SetStringField(TEXT("suite"),TEXT("viewport_mouse_input"));Root->SetBoolField(TEXT("passed"),Pass);
   Root->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Started);Root->SetNumberField(TEXT("stage"),int32(Stage));Root->SetArrayField(TEXT("cases"),Results);
   Root->SetStringField(TEXT("input_route"),TEXT("FSceneViewport mouse handlers -> UGameViewportClient -> ARTSController"));
   Root->SetNumberField(TEXT("mouse_down_events"),Downs);Root->SetNumberField(TEXT("mouse_up_events"),Ups);Root->SetNumberField(TEXT("mouse_move_events"),Moves);
   Root->SetNumberField(TEXT("keyboard_down_events"),KeyDowns);Root->SetNumberField(TEXT("keyboard_up_events"),KeyUps);
   Root->SetNumberField(TEXT("worker_id"),WorkerId);Root->SetNumberField(TEXT("resource_id"),ResourceId);Root->SetNumberField(TEXT("headquarters_id"),HQId);
   Root->SetBoolField(TEXT("gather_cargo_observed"),CargoSeen);Root->SetNumberField(TEXT("food_delivered"),DeliveredFood);
   Root->SetStringField(TEXT("gameplay_screenshot"),TEXT("Screenshots/input-gameplay.png"));Root->SetStringField(TEXT("hotkeys_screenshot"),TEXT("Screenshots/input-hotkeys.png"));
   if(C&&C->GetWorld()&&C->GetWorld()->GetGameViewport()) {
    UGameViewportClient* Client=C->GetWorld()->GetGameViewport();Root->SetNumberField(TEXT("mouse_capture_mode"),int32(Client->GetMouseCaptureMode()));
    if(FSceneViewport* View=Client->GetGameViewport()) {
     Root->SetNumberField(TEXT("viewport_width"),View->GetSizeXY().X);Root->SetNumberField(TEXT("viewport_height"),View->GetSizeXY().Y);
     Root->SetNumberField(TEXT("geometry_scale"),View->GetCachedGeometry().GetAccumulatedLayoutTransform().GetScale());
    }
   }
   const FString Directory=FPaths::ProjectSavedDir()/TEXT("Tests"),Path=Directory/TEXT("input-tests.json");IFileManager::Get().MakeDirectory(*Directory,true);
   FString JSON;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&JSON));
   const bool Saved=FFileHelper::SaveStringToFile(JSON,*Path);
   UE_LOG(LogTemp,Display,TEXT("RTS_INPUT_TESTS passed=%s cases=%d report=%s"),Pass&&Saved?TEXT("true"):TEXT("false"),Results.Num(),*Path);
   const bool Successful=Pass&&Saved;Stage=EProbeStage::Finished;FPlatformMisc::RequestExitWithStatus(!Successful,Successful?0:1);
  }
  ARTSWorld* Simulation(ARTSController* C) const {
   if(auto* Mode=C->GetWorld()->GetAuthGameMode<ARTSGameMode>())if(Mode->Simulation)return Mode->Simulation;
   for(TActorIterator<ARTSWorld> It(C->GetWorld());It;++It)return *It;
   return nullptr;
  }
  bool MovePointer(FSceneViewport* View,FVector2D Pixel) {
   // SceneViewport converts AbsoluteToLocal * accumulated layout scale. Invert
   // that same transform, rather than assuming window size equals render size.
   const FGeometry Geometry=View->GetCachedGeometry();const float Scale=Geometry.GetAccumulatedLayoutTransform().GetScale();
   if(Scale<=0)return false;
   const FVector2D Absolute=Geometry.LocalToAbsolute(Pixel/Scale),Last=Geometry.LocalToAbsolute(Pointer/Scale);
   const FPointerEvent Event(uint32(0),uint32(0),Absolute,Last,Pressed,EKeys::Invalid,0.f,FModifierKeysState());
   if(!Entered) {View->OnMouseEnter(Geometry,Event);Entered=true;}
   View->OnMouseMove(Geometry,Event);Pointer=Pixel;++Moves;return true;
  }
  void Down(FSceneViewport* View,FVector2D Pixel,FKey Button) {
   MovePointer(View,Pixel);Pressed.Add(Button);Held=Button;
   const FGeometry Geometry=View->GetCachedGeometry();const float Scale=Geometry.GetAccumulatedLayoutTransform().GetScale();
   const FVector2D Absolute=Geometry.LocalToAbsolute(Pixel/Scale);
   const FPointerEvent Event(uint32(0),uint32(0),Absolute,Absolute,Pressed,Button,0.f,FModifierKeysState());
   View->OnMouseButtonDown(Geometry,Event);++Downs;
  }
  void Up(FSceneViewport* View) {
   const FKey Button=Held;Pressed.Remove(Button);
   const FGeometry Geometry=View->GetCachedGeometry();const float Scale=Geometry.GetAccumulatedLayoutTransform().GetScale();
   const FVector2D Absolute=Geometry.LocalToAbsolute(Pointer/Scale);
   const FPointerEvent Event(uint32(0),uint32(0),Absolute,Absolute,Pressed,Button,0.f,FModifierKeysState());
   View->OnMouseButtonUp(Geometry,Event);Held=EKeys::Invalid;++Ups;
  }
  void KeyEvent(FSceneViewport* View,FKey Key,bool DownKey,const FModifierKeysState& Mods=FModifierKeysState()) {
   const FKeyEvent Event(Key,Mods,uint32(0),false,0,0);
   if(DownKey) {View->OnKeyDown(View->GetCachedGeometry(),Event);++KeyDowns;}
   else {View->OnKeyUp(View->GetCachedGeometry(),Event);++KeyUps;}
  }
  void KeyPress(FSceneViewport* View,FKey Key) {
   KeyEvent(View,Key,true);KeyEvent(View,Key,false);
  }
  bool BeginChord(FSceneViewport* View,FKey Key,bool Ctrl,bool Shift,bool Alt) {
   if(!Key.IsValid())return false;ChordKey=Key;
   ChordMods=FModifierKeysState(Shift,false,Ctrl,false,Alt,false,false,false,false);ChordModifiers.Empty();
   if(Ctrl)ChordModifiers.Add(EKeys::LeftControl);if(Shift)ChordModifiers.Add(EKeys::LeftShift);if(Alt)ChordModifiers.Add(EKeys::LeftAlt);
   for(FKey Modifier:ChordModifiers)KeyEvent(View,Modifier,true,ChordMods);
   ChordActive=true;ChordStep=0;ChordFrame=Tick;return true;
  }
  bool BeginProfileChord(FSceneViewport* View,const FString& Action) {
   const auto& Profile=FRTSControls::Get();const FString Label=Profile.LabelFor(Action);
   return BeginChord(View,Profile.KeyFor(Action),Label.Contains(TEXT("Ctrl+")),Label.Contains(TEXT("Shift+")),Label.Contains(TEXT("Alt+")));
  }
  void Wheel(FSceneViewport* View,float Delta,bool Shift) {
   const FGeometry Geometry=View->GetCachedGeometry();const float Scale=Geometry.GetAccumulatedLayoutTransform().GetScale();
   const FVector2D Absolute=Geometry.LocalToAbsolute(Pointer/Scale);
   const FPointerEvent Event(uint32(0),uint32(0),Absolute,Absolute,Pressed,EKeys::Invalid,Delta,FModifierKeysState(Shift,false,false,false,false,false,false,false,false));
   View->OnMouseWheel(Geometry,Event);
  }
  bool ContinueChord(FSceneViewport* View) {
   if(!ChordActive)return false;
   if(Tick<=ChordFrame)return true;
   if(ChordStep==0) {KeyEvent(View,ChordKey,true,ChordMods);ChordStep=1;ChordFrame=Tick;return true;}
   KeyEvent(View,ChordKey,false,ChordMods);
   for(int32 I=ChordModifiers.Num()-1;I>=0;I--)KeyEvent(View,ChordModifiers[I],false);
   ChordActive=false;return true;
  }
  void Screenshot(const TCHAR* Name) {
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name),false,false);
  }
  bool CaptureFrame(const TCHAR* Name) {
   // Screenshot requests are consumed after HUD drawing. Keep the fixture's
   // selection, hover and zoom unchanged until that frame has been captured.
   if(!CapturePending){Screenshot(Name);CapturePending=true;CaptureTick=Tick;return false;}
   return Tick>CaptureTick+1;
  }
  bool Button(ARTSHUD* HUD,FSceneViewport* View,const FString& Action,EProbeStage ReleaseStage,const FString& Value=FString()) {
   FBox2D Bounds;if(!HUD->FindButtonBounds(Action,Value,Bounds)||!Bounds.bIsValid)return false;
   Down(View,Bounds.GetCenter(),EKeys::LeftMouseButton);Advance(ReleaseStage);return true;
  }
  bool EntityPixel(ARTSController* C,int32 Id,FVector2D& Out,bool Context=false) const {
   const auto* E=C->FindEntity(Id);if(!E)return false;const FBox2D Bounds=C->EntityBounds(*E,Context);if(!Bounds.bIsValid)return false;
   const float Samples[]={.5f,.25f,.75f,.1f,.9f};
   for(float Y:Samples)for(float X:Samples) {
    const FVector2D P=Bounds.Min+Bounds.GetSize()*FVector2D(X,Y);
    if(C->InWorld(P)&&C->EntityUnder(P,Context)==Id&&(!Context||C->EntityUnder(P)==Id)) {Out=P;return true;}
   }
   return false;
  }
  TArray<FVector2D> WallOrders(ARTSWorld* World) const {
   TArray<FVector2D> Points;if(!World)return Points;
   for(const auto& Pair:World->Entities)if(Pair.Value->Owner==Owner->LocalSlot&&World->D(*Pair.Value).Id==TEXT("works_of_defense"))Points.Add(Pair.Value->Pos);
   if(const auto* Builder=World->Find(WallBuilderId))for(const auto& Order:Builder->PendingOrders)if(Order.Action==TEXT("build")&&Order.Type==TEXT("works_of_defense"))Points.Add(Order.Point);
   return Points;
  }
  bool WallSamplesPresent(ARTSWorld* World,FVector2D Start,FVector2D End) const {
   const auto Points=WallOrders(World);FVector2D Direction=(End-Start).GetSafeNormal();const int32 Count=FMath::FloorToInt(FVector2D::Distance(Start,End)/WallSpacing+.0001f)+1;
   for(int32 I=0;I<Count;I++){const FVector2D Expected=Start+Direction*I*WallSpacing;bool Found=false;for(FVector2D Point:Points)if(FVector2D::Distance(Expected,Point)<.2f)Found=true;if(!Found)return false;}
   return true;
  }
  bool SetupWallFixture(ARTSController* C,ARTSWorld* World) {
   if(!World)return false;const auto* HQ=World->Find(HQId);if(!HQ)return false;const FVector2D Origin=HQ->Pos;
   auto& Player=World->Players[C->LocalSlot];Player.Resources+=FVector(5000,5000,5000);Player.Upgrades.AddUnique(TEXT("heaps_of_earth"));
   const auto& Wall=FRTSCatalog::Get().Def(TEXT("works_of_defense"));WallSpacing=2*Wall.Radius+.15f;
   for(const FString& Req:Wall.Requires) {
    const auto& Def=FRTSCatalog::Get().Def(Req);if(Def.bUpgrade){Player.Upgrades.AddUnique(Req);continue;}if(!Def.bBuilding||World->OwnedStructure(C->LocalSlot,Req))continue;
    bool Added=false;for(int32 Y=-16;Y<=16&&!Added;Y+=4)for(int32 X=-16;X<=16&&!Added;X+=4){const FVector2D P=Origin+FVector2D(X,Y);if(World->Walkable(P,Def.Radius)&&World->VisiblePoint(C->LocalSlot,P)){World->Spawn(Req,C->LocalSlot,P);Added=true;}}if(!Added)return false;
   }
   const FVector2D Directions[]={FVector2D(1,0),FVector2D(0,1),FVector2D(-1,0),FVector2D(0,-1)};
   for(int32 Y=-15;Y<=15;Y+=3)for(int32 X=-15;X<=15;X+=3)for(FVector2D Direction:Directions) {
    const FVector2D A=Origin+FVector2D(X,Y),B=A+Direction*WallSpacing*2.15f,Side(-Direction.Y,Direction.X),D=B+Side*WallSpacing*2.15f;const FVector2D Candidates[]={A,B,D};bool Clear=true;
    for(int32 I=0;I<3;I++){WallPixels[I]=C->WorldToScreen(Candidates[I]).RoundToVector();WallPoints[I]=C->ScreenToWorld(WallPixels[I]);if(!C->InWorld(WallPixels[I])||!World->VisiblePoint(C->LocalSlot,WallPoints[I]))Clear=false;}
    const FVector2D FirstDelta=WallPoints[1]-WallPoints[0],Corner=WallPoints[0]+FirstDelta.GetSafeNormal()*(FMath::FloorToInt(FirstDelta.Size()/WallSpacing+.0001f)*WallSpacing);WallPixels[2]=C->WorldToScreen(Corner+Side*WallSpacing*2.15f).RoundToVector();WallPoints[2]=C->ScreenToWorld(WallPixels[2]);if(!C->InWorld(WallPixels[2])||!World->VisiblePoint(C->LocalSlot,WallPoints[2]))Clear=false;
    for(int32 Leg=0;Leg<2&&Clear;Leg++){const FVector2D Start=Leg==1?Corner:WallPoints[0],End=WallPoints[Leg+1],Unit=(End-Start).GetSafeNormal();const int32 N=FMath::FloorToInt(FVector2D::Distance(Start,End)/WallSpacing+.0001f)+1;for(int32 I=0;I<N;I++){const FVector2D P=Start+Unit*I*WallSpacing;if(!World->Walkable(P,Wall.Radius+.1f))Clear=false;for(int32 E=0;E<8;E++)if(!World->ExploredPoint(C->LocalSlot,P+FVector2D(FMath::Cos(E*PI/4),FMath::Sin(E*PI/4))*Wall.Radius))Clear=false;}}
    const FVector2D BuilderAt=A-Direction*3;if(!Clear||!World->Walkable(BuilderAt,.5f))continue;
    auto* Builder=World->Spawn(TEXT("industrious_laborer"),C->LocalSlot,BuilderAt);if(!Builder)return false;WallBuilderId=Builder->Id;World->Snapshot();return true;
   }
   return false;
  }
  bool SetupTraderFixture(ARTSController* C,ARTSWorld* World) {
   if(!World)return false;const auto* HQ=World->Find(HQId);if(!HQ)return false;
   for(int32 Y=-10;Y<=10;Y+=3)for(int32 X=-10;X<=10;X+=3) {
    const FVector2D P=HQ->Pos+FVector2D(X,Y);if(!World->Walkable(P,.5f)||!C->InWorld(C->WorldToScreen(P))||C->EntityUnder(C->WorldToScreen(P)))continue;
    const FVector2D Candidates[]={P+FVector2D(4,0),P+FVector2D(4,5)};bool Clear=true;
    for(int32 I=0;I<2;I++){RoutePixels[I]=C->WorldToScreen(Candidates[I]).RoundToVector();RoutePoints[I]=C->ScreenToWorld(RoutePixels[I]);if(!C->InWorld(RoutePixels[I])||!World->Walkable(RoutePoints[I],.5f)||!World->VisiblePoint(C->LocalSlot,RoutePoints[I]))Clear=false;}if(!Clear)continue;
    auto* Trader=World->Spawn(TEXT("trader_in_precious_things"),C->LocalSlot,P);if(!Trader)return false;TraderId=Trader->Id;World->Snapshot();return true;
   }return false;
  }
  int32 SpawnUIBuilding(ARTSController* C,ARTSWorld* World,const FString& Type) {
   const auto* HQ=World?World->Find(HQId):nullptr;if(!HQ)return 0;const auto& Def=FRTSCatalog::Get().Def(Type);
   for(int32 Y=-18;Y<=18;Y+=3)for(int32 X=-18;X<=18;X+=3){const FVector2D At=HQ->Pos+FVector2D(X,Y);if(!World->Walkable(At,Def.Radius+.15f)||!World->VisiblePoint(C->LocalSlot,At)||!C->InWorld(C->WorldToScreen(At)))continue;auto* Building=World->Spawn(Type,C->LocalSlot,At);return Building?Building->Id:0;}return 0;
  }
  bool MarketWaypoints(ARTSController* C,ARTSWorld* World,FVector2D Center) {
   int32 Found=0;for(int32 I=0;I<16&&Found<2;I++){const float Angle=I*PI/8;const FVector2D At=Center+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*7.f;const FVector2D Pixel=C->WorldToScreen(At).RoundToVector();const FVector2D Actual=C->ScreenToWorld(Pixel);if(!C->InWorld(Pixel)||!World->Walkable(Actual,.5f)||!World->VisiblePoint(C->LocalSlot,Actual))continue;if(Found&&FVector2D::Distance(Actual,RoutePoints[0])<4)continue;RoutePixels[Found]=Pixel;RoutePoints[Found]=Actual;Found++;}return Found==2;
  }
  void EnsureEnemyPlayer(ARTSWorld* World){if(!World)return;while(World->Players.Num()<2){FRTSPlayer P;P.Slot=World->Players.Num();P.Team=1;P.Faction=TEXT("nephites");P.Age=4;P.bConnected=true;World->Players.Add(P);}}
  bool SetupAnimation(ARTSController* C,ARTSWorld* World){
   if(!World)return false;EnsureEnemyPlayer(World);if(AnimationIndex==0){AnimationSavedMap=World->Map;AnimationSavedZoom=C->Zoom;AnimationSavedCamera=C->CameraCenter;}World->Map=AnimationIndex==5?2:0;World->NavigationCount=-1;World->ObstacleCount=-1;
   const FString Types[]={TEXT("industrious_laborer"),TEXT("man_of_moroni"),TEXT("industrious_laborer"),TEXT("priestcrafter"),TEXT("curelom"),TEXT("jaredite_barge")};const FVector2D At=AnimationIndex==5?FVector2D(80,80):FVector2D(60,72);
   auto* Unit=World->Spawn(Types[AnimationIndex],C->LocalSlot,At);if(!Unit)return false;AnimationEntityId=Unit->Id;AnimationIds.Add(Unit->Id);C->CameraCenter=At;C->Zoom=1.15f;C->Selection={Unit->Id};C->ActiveSubgroup.Empty();C->bBuildMenu=false;
   if(AnimationIndex==0||AnimationIndex==4||AnimationIndex==5){World->ResetOrder(*Unit,TEXT("move"));Unit->Rally=Unit->Destination=At+FVector2D(0,15);}
   if(AnimationIndex==1){auto* Target=World->Spawn(TEXT("industrious_laborer"),1,At+FVector2D(1.3,0));if(!Target)return false;AnimationIds.Add(Target->Id);Target->HP=Target->MaxHP=100000;World->ResetOrder(*Target,TEXT("hold"));World->ResetOrder(*Unit,TEXT("attack"));Unit->Target=Target->Id;Unit->bDirectAttack=true;Unit->AttackTimer=0;}
   if(AnimationIndex==2){auto* Tree=World->Spawn(TEXT("tree"),-1,At+FVector2D(2,0));if(!Tree)return false;AnimationIds.Add(Tree->Id);World->ResetOrder(*Unit,TEXT("gather"));Unit->WorkTarget=Unit->Target=Tree->Id;Unit->Pos=World->WorkPosition(*Unit,*Tree);Unit->Cargo=FVector::ZeroVector;}
   if(AnimationIndex==3){World->Time=FMath::Max(World->Time,61.f);auto* Target=World->Spawn(TEXT("bowman"),1,At+FVector2D(3,0));if(!Target)return false;AnimationIds.Add(Target->Id);Target->DisarmedUntil=World->Time+30;World->ResetOrder(*Target,TEXT("hold"));World->ResetOrder(*Unit,TEXT("convert"));Unit->Target=Target->Id;}
   World->Snapshot();return true;
  }
  bool SetupV08Scene(ARTSController* C,ARTSWorld* World,bool Laban){
   if(!World)return false;EnsureEnemyPlayer(World);
   // Earlier input cases have completed. An isolated fixture keeps screenshots
   // and target picking deterministic without bypassing the commands under test.
   World->Entities.Empty();World->Map=0;World->NavigationCount=-1;World->ObstacleCount=-1;World->bFogOfWar=false;
   for(auto& P:World->Players){P.KnownBuildings.Empty();P.bDefeated=false;P.DefeatRemaining=60;}
   auto& P=World->Players[C->LocalSlot];P.Faction=Laban?TEXT("laban"):TEXT("gadianton");P.Age=2;P.Resources=FVector(10000);P.Favor=50;P.Upgrades.Empty();P.bLiahona=true;
   C->CameraCenter=FVector2D(62,74);C->CameraRotation=0;C->Zoom=1.1f;C->ActiveSubgroup.Empty();C->PendingAction.Empty();C->PendingType.Empty();C->bBuildMenu=false;C->ActionPage=0;C->DeathAnimations.Empty();
   const auto Add=[&](const FString& Type,FVector2D At,int32 Owner=-2){auto* E=World->Spawn(Type,Owner==-2?C->LocalSlot:Owner,At);return E?E->Id:0;};
   HQId=Add(TEXT("seat_of_the_people"),FVector2D(60,72));Add(TEXT("tents_and_habitations"),FVector2D(48,66));Add(TEXT("tents_and_habitations"),FVector2D(58,56));
   if(Laban){
    V08Winery=Add(TEXT("winery"),FVector2D(75,68));V08Barracks=Add(TEXT("encampment_of_war"),FVector2D(76,83));
    Add(TEXT("house_of_curious_workmanship"),FVector2D(48,83));Add(TEXT("place_of_trade"),FVector2D(62,91));Add(TEXT("storehouse_of_provisions"),FVector2D(42,74));Add(TEXT("temple_of_the_land"),FVector2D(75,54));
    V08Builder=Add(TEXT("industrious_laborer"),FVector2D(63,74));C->Selection={V08Builder};
   }else{
    V08Storehouse=Add(TEXT("storehouse_of_provisions"),FVector2D(69,74));V08Hideout=Add(TEXT("tents_and_habitations"),FVector2D(52,74));
    V08Lawyer=Add(TEXT("lawyer"),FVector2D(61,76));V08Robber=Add(TEXT("gadianton_robber"),FVector2D(48,74));
    const int32 Enemy=C->LocalSlot==0?1:0;World->Players[Enemy].Team=P.Team+1;
    V08RaidTarget=Add(TEXT("fields_of_grain"),FVector2D(80,76),Enemy);C->Selection={V08Storehouse};
   }
   World->Population();World->Snapshot();return HQId>0&&(Laban?(V08Builder&&V08Winery&&V08Barracks):(V08Storehouse&&V08Hideout&&V08Lawyer&&V08Robber&&V08RaidTarget));
  }
  void Run(ARTSController* C) {
   if(Done)return;++Tick;
   if(Owner.IsValid()&&Owner.Get()!=C) {Finish(C,false,TEXT("The controller changed during an input-only sandbox test."));return;}
   Owner=C;
   const double Now=FPlatformTime::Seconds();
   if(Now-Started>140) {Finish(C,false,FString::Printf(TEXT("140-second timeout at stage %d; phase=%d menu=%d selection=%d"),int32(Stage),C->Phase,C->bMenu?1:0,C->Selection.Num()));return;}
   UGameViewportClient* Client=C->GetWorld()->GetGameViewport();FSceneViewport* View=Client?Client->GetGameViewport():nullptr;
   ARTSHUD* HUD=Cast<ARTSHUD>(C->GetHUD());
   if(!View||!HUD||View->GetSizeXY().X<300||View->GetCachedGeometry().GetLocalSize().X<10||!C->Me())return;
   ARTSWorld* World=Simulation(C);
   if(ContinueChord(View))return;
   if(Tick<=StageTick)return;
   const double Age=Now-StageStarted;
   switch(Stage) {
   case EProbeStage::Menu:
    if(Now-Started<.5)return;
    if(!C->bMenu) {Finish(C,false,TEXT("Test requires the ordinary initial main menu, without auto-start flags."));return;}
    Button(HUD,View,TEXT("solo"),EProbeStage::ReleaseSolo);break;
   case EProbeStage::ReleaseSolo: Up(View);Advance(EProbeStage::Lobby);break;
   case EProbeStage::Lobby:
    if(Age<.1)return;
    if(C->bMenu||C->Phase!=0) {if(Age>1)Finish(C,false,TEXT("A single viewport click on Solo did not enter the lobby. Mouse-down forwarding may be broken."));return;}
    Record(TEXT("menu_solo_single_click"),true,TEXT("One mouse down/up through SceneViewport entered the lobby."));
    if(!Button(HUD,View,TEXT("ready"),EProbeStage::ReleaseReady)&&Age>2)Finish(C,false,TEXT("Ready button was not available after entering the lobby."));break;
   case EProbeStage::ReleaseReady: Up(View);Advance(EProbeStage::Ready);break;
   case EProbeStage::Ready:
    if(Age<.1)return;
    if(!C->Me()->bReady) {if(Age>2)Finish(C,false,TEXT("Viewport click on Ready did not ready the local commander."));return;}
    Record(TEXT("lobby_ready_single_click"),true,TEXT("The lobby snapshot confirms ready after a pointer click."));
    if(!LobbySettingsDone){Advance(EProbeStage::LobbySettings);break;}
    if(!Button(HUD,View,TEXT("start"),EProbeStage::ReleaseStart)&&Age>3)Finish(C,false,TEXT("Start button was not available for a ready sandbox."));break;
   case EProbeStage::LobbySettings:{
    const TCHAR* Steps[]={TEXT("resources"),TEXT("speed"),TEXT("fruitfulness"),TEXT("fog"),TEXT("resources"),TEXT("resources"),TEXT("resources"),TEXT("speed"),TEXT("speed"),TEXT("speed"),TEXT("speed"),TEXT("fruitfulness"),TEXT("fruitfulness"),TEXT("fog")};
    if(SettingStep>=UE_ARRAY_COUNT(Steps)){if(!World||World->StartingResources!=1||World->Fruitfulness!=1||!FMath::IsNearlyEqual(World->GameSpeed,1.f)||!World->bFogOfWar){Finish(C,false,TEXT("Lobby settings did not cycle back to standard before the gameplay fixture."));return;}LobbySettingsDone=true;Record(TEXT("v07.lobby_settings_pointer_roundtrip"),true,TEXT("Viewport clicks changed all four settings, reset readiness, replicated values, and restored Standard / 1x / Normal / Fog On."));if(!Button(HUD,View,TEXT("ready"),EProbeStage::ReleaseReady))Finish(C,false,TEXT("Ready unavailable after settings restoration."));break;}
    if(!Button(HUD,View,TEXT("lobby_setting"),EProbeStage::LobbySettingUp,Steps[SettingStep])&&Age>2)Finish(C,false,TEXT("Lobby setting button unavailable."));break;
   }
   case EProbeStage::LobbySettingUp:Up(View);Advance(EProbeStage::LobbySettingCheck);break;
   case EProbeStage::LobbySettingCheck:
    if(Age<.08)return;if(!World||C->LobbyResources!=World->StartingResources||C->LobbyFruitfulness!=World->Fruitfulness||!FMath::IsNearlyEqual(C->LobbySpeed,World->GameSpeed)||C->bLobbyFog!=World->bFogOfWar||C->Me()->bReady){if(Age>2)Finish(C,false,TEXT("Lobby setting click failed authoritative replication or did not clear readiness."));return;}SettingStep++;Advance(EProbeStage::LobbySettings);break;
   case EProbeStage::ReleaseStart: Up(View);Advance(EProbeStage::Match);break;
   case EProbeStage::Match: {
    if(C->Phase!=1||!World||World->Phase!=1||C->Entities.Num()<8) {if(Age>4)Finish(C,false,TEXT("Viewport click on Start did not begin the match."));return;}
    if(Age<.4)return;
    Record(TEXT("lobby_start_single_click"),true,TEXT("The authoritative world and client snapshot both entered the match."));
    float Best=TNumericLimits<float>::Max();FVector2D WorkerPoint;
    for(const auto& W:C->Entities)if(W.Owner==C->LocalSlot&&RTS::IsWorker(FRTSCatalog::Get().Def(W.Def))) {
     FVector2D WP;if(!EntityPixel(C,W.Id,WP))continue;
     for(const auto& R:C->Entities)if(FRTSCatalog::Get().Def(R.Def).Id==TEXT("berries")) {
      FVector2D RP;if(!EntityPixel(C,R.Id,RP))continue;
      const float Distance=FVector2D::Distance(W.Pos,R.Pos);
      if(Distance<Best) {Best=Distance;WorkerId=W.Id;ResourceId=R.Id;WorkerPoint=WP;}
     }
    }
    if(!WorkerId||!ResourceId) {Finish(C,false,TEXT("No visible clickable worker and provisions pair was found."));return;}
    const auto* Resource=C->FindEntity(ResourceId);const FBox2D SelectResource=C->EntityBounds(*Resource),ContextResource=C->EntityBounds(*Resource,true);
    bool Split=ContextResource.GetSize().X>SelectResource.GetSize().X&&ContextResource.GetSize().Y>SelectResource.GetSize().Y;
    for(const auto& Building:C->Entities)if(Building.Owner==C->LocalSlot&&FRTSCatalog::Get().Def(Building.Def).bBuilding){const auto A=C->EntityBounds(Building),B=C->EntityBounds(Building,true);Split&=B.GetSize().X>A.GetSize().X&&B.GetSize().Y>A.GetSize().Y;}
    if(!Split){Finish(C,false,TEXT("Selection and context target hitboxes were not independently sized for resources and structures."));return;}
    Record(TEXT("separate_selection_context_hitboxes"),true,TEXT("Resource and structure right-click targets are larger than their compact selection bounds."));
    InitialFood=World->Players[C->LocalSlot].Resources.X;Down(View,WorkerPoint,EKeys::LeftMouseButton);Advance(EProbeStage::ReleaseWorker);break;
   }
   case EProbeStage::ReleaseWorker: Up(View);Advance(EProbeStage::Worker);break;
   case EProbeStage::Worker: {
    if(C->Selection.Num()!=1||!C->Selection.Contains(WorkerId)) {if(Age>.5)Finish(C,false,TEXT("A single viewport click did not select exactly the clicked worker."));return;}
    Record(TEXT("unit_single_click_selection"),true,FString::Printf(TEXT("Selected worker %d."),WorkerId));
    FVector2D ResourcePoint;if(!EntityPixel(C,ResourceId,ResourcePoint)) {Finish(C,false,TEXT("The provisions were not clickable after worker selection."));return;}
    Down(View,ResourcePoint,EKeys::RightMouseButton);Advance(EProbeStage::ReleaseResource);break;
   }
   case EProbeStage::ReleaseResource: Up(View);Advance(EProbeStage::Gather);break;
   case EProbeStage::Gather: {
    auto* W=World?World->Find(WorkerId):nullptr;
    if(!W||W->Order!=TEXT("gather")||W->WorkTarget!=ResourceId) {if(Age>1)Finish(C,false,TEXT("Right-clicking provisions did not create the authoritative gather order."));return;}
    const auto* PingTarget=C->FindEntity(ResourceId);if(!PingTarget||FVector2D::Distance(C->CommandFlash,PingTarget->Pos)>.01f){Finish(C,false,TEXT("Resource assignment ping did not snap to target center."));return;}Record(TEXT("v07.target_ping_center"),true,TEXT("Actual resource right-click placed the order indicator at the target center."));
    Record(TEXT("resource_right_click_gather"),true,FString::Printf(TEXT("Worker %d is gathering from resource %d."),WorkerId,ResourceId));Advance(EProbeStage::Delivery);break;
   }
   case EProbeStage::Delivery: {
    auto* W=World?World->Find(WorkerId):nullptr;if(!W) {Finish(C,false,TEXT("Gathering worker disappeared."));return;}
    CargoSeen|=W->Cargo.X>0.05f;DeliveredFood=World->Players[C->LocalSlot].Resources.X-InitialFood;
    if(DeliveredFood<1||!CargoSeen) {if(Age>28)Finish(C,false,FString::Printf(TEXT("Gather delivery timed out: order=%s cargo=%.2f food_gain=%.2f drop=%d"),*W->Order,W->Cargo.X,DeliveredFood,W->DropTarget));return;}
    Record(TEXT("gather_cargo_and_delivery"),true,FString::Printf(TEXT("Observed food cargo, then %.2f Food delivered to the settlement."),DeliveredFood));
    FBox2D Bounds(EForceInit::ForceInit);DragWorkers.Empty();
    for(const auto& E:C->Entities)if(E.Owner==C->LocalSlot&&RTS::IsWorker(FRTSCatalog::Get().Def(E.Def))) {const FBox2D B=C->EntityBounds(E);if(B.bIsValid){Bounds+=B.Min;Bounds+=B.Max;DragWorkers.Add(E.Id);}}
    if(!Bounds.bIsValid||DragWorkers.Num()<2) {Finish(C,false,TEXT("Not enough owned workers were available for drag selection."));return;}
    DragFrom=Bounds.Min-FVector2D(14,14);DragTo=Bounds.Max+FVector2D(14,14);
    if(!C->InWorld(DragFrom)||!C->InWorld(DragTo)) {Finish(C,false,TEXT("The owned-worker drag rectangle was outside the battlefield."));return;}
    Down(View,DragFrom,EKeys::LeftMouseButton);Advance(EProbeStage::DragPressed);break;
   }
   case EProbeStage::DragPressed: MovePointer(View,FMath::Lerp(DragFrom,DragTo,.5f));Advance(EProbeStage::DragMid);break;
   case EProbeStage::DragMid: MovePointer(View,DragTo);Advance(EProbeStage::DragEnd);break;
   case EProbeStage::DragEnd: Up(View);Advance(EProbeStage::DragResult);break;
   case EProbeStage::DragResult: {
    bool Complete=true;for(int32 Id:DragWorkers)if(!C->Selection.Contains(Id))Complete=false;
    for(int32 Id:C->Selection) {const auto* E=C->FindEntity(Id);if(!E||E->Owner!=C->LocalSlot)Complete=false;}
    if(!Complete) {Finish(C,false,FString::Printf(TEXT("Drag box selected %d entities but did not contain every one of %d owned workers."),C->Selection.Num(),DragWorkers.Num()));return;}
    Record(TEXT("drag_box_owned_workers"),true,FString::Printf(TEXT("Mouse down/move/move/up selected all %d owned workers."),DragWorkers.Num()));
    PortraitRemoved=C->Selection[0];PortraitCount=C->Selection.Num();Advance(EProbeStage::PortraitStart);break;
   }
   case EProbeStage::PortraitStart:KeyEvent(View,EKeys::LeftShift,true,FModifierKeysState(true,false,false,false,false,false,false,false,false));Advance(EProbeStage::PortraitShiftPress);break;
   case EProbeStage::PortraitShiftPress:if(!Button(HUD,View,TEXT("selection_unit"),EProbeStage::PortraitShiftUp,FString::FromInt(PortraitRemoved))&&Age>2)Finish(C,false,TEXT("Dragged selection lacks an individual portrait."));break;
   case EProbeStage::PortraitShiftUp:Up(View);KeyEvent(View,EKeys::LeftShift,false);Advance(EProbeStage::PortraitShiftCheck);break;
   case EProbeStage::PortraitShiftCheck:
    if(C->Selection.Contains(PortraitRemoved)||C->Selection.Num()!=PortraitCount-1){Finish(C,false,TEXT("Shift+portrait did not remove exactly that entity."));return;}Record(TEXT("v07.portrait_shift_remove"),true,TEXT("Viewport Shift+click removed one member of the actual drag selection."));PortraitIsolated=C->Selection[0];KeyEvent(View,EKeys::LeftControl,true,FModifierKeysState(false,false,true,false,false,false,false,false,false));Advance(EProbeStage::PortraitCtrlPress);break;
   case EProbeStage::PortraitCtrlPress:if(!Button(HUD,View,TEXT("selection_unit"),EProbeStage::PortraitCtrlUp,FString::FromInt(PortraitIsolated))&&Age>2)Finish(C,false,TEXT("Remaining individual portrait unavailable."));break;
   case EProbeStage::PortraitCtrlUp:Up(View);KeyEvent(View,EKeys::LeftControl,false);Advance(EProbeStage::PortraitCtrlCheck);break;
   case EProbeStage::PortraitCtrlCheck:
    if(C->Selection.Num()!=1||C->Selection[0]!=PortraitIsolated){Finish(C,false,TEXT("Ctrl+portrait failed to isolate its entity."));return;}Record(TEXT("v07.portrait_ctrl_isolate"),true,TEXT("Viewport Ctrl+click isolated exactly the clicked entity."));Screenshot(TEXT("input-v07-selection.png"));if(!Button(HUD,View,TEXT("idle"),EProbeStage::ReleaseIdle))Finish(C,false,TEXT("Idle-worker button unavailable after portrait checks."));break;
   case EProbeStage::ReleaseIdle: Up(View);Advance(EProbeStage::Idle);break;
   case EProbeStage::Idle:
    if(C->Selection.Num()!=1||!C->IdleVillagers().Contains(C->Selection[0])) {Finish(C,false,TEXT("Clicking Idle Workers did not select one nonbusy owned villager."));return;}
    Record(TEXT("idle_worker_button"),true,TEXT("Viewport click selected exactly one idle villager."));
    PreviousIdle=C->Selection[0];
    if(!BeginProfileChord(View,TEXT("idle.next"))) {Finish(C,false,TEXT("The current profile has no next-idle shortcut; preferences were preserved."));return;}
    Advance(EProbeStage::NextIdle);break;
   case EProbeStage::NextIdle:
    if(C->Selection.Num()!=1||!C->IdleVillagers().Contains(C->Selection[0])||(C->IdleVillagers().Num()>1&&C->Selection[0]==PreviousIdle)) {Finish(C,false,TEXT("The next-idle viewport keyboard chord did not cycle to another idle worker."));return;}
    Record(TEXT("next_idle_keyboard_chord"),true,TEXT("Viewport keyboard chord cycled to another idle worker: ")+FRTSControls::Get().LabelFor(TEXT("idle.next")));
    ExpectedIdle=C->IdleVillagers();
    if(!BeginProfileChord(View,TEXT("idle.all"))) {Finish(C,false,TEXT("The current profile has no all-idle shortcut; preferences were preserved."));return;}
    Advance(EProbeStage::AllIdle);break;
   case EProbeStage::AllIdle: {
    bool Matches=C->Selection.Num()==ExpectedIdle.Num();for(int32 Id:ExpectedIdle)if(!C->Selection.Contains(Id))Matches=false;
    if(!Matches||C->Selection.Contains(WorkerId)) {Finish(C,false,TEXT("The all-idle viewport modifier chord did not select all idle workers while excluding the busy gatherer."));return;}
    Record(TEXT("all_idle_keyboard_chord"),true,TEXT("Selected all idle workers using the saved modifier chord: ")+FRTSControls::Get().LabelFor(TEXT("idle.all")));
    if(!BeginProfileChord(View,TEXT("menu.build"))){Finish(C,false,TEXT("Construction menu shortcut is unbound."));return;}Advance(EProbeStage::BuildMenu);break;
   }
   case EProbeStage::BuildMenu: {
    if(!C->bBuildMenu) {Finish(C,false,TEXT("Saved construction-menu shortcut did not open the selected builder's construction menu."));return;}
    if(!Button(HUD,View,TEXT("build"),EProbeStage::ReleaseBuildCard,TEXT("tents_and_habitations"))&&Age>1)Finish(C,false,TEXT("The habitation build card was not available after opening construction."));break;
   }
   case EProbeStage::ReleaseBuildCard: Up(View);Advance(EProbeStage::BuildCard);break;
   case EProbeStage::BuildCard:
    if(C->PendingAction!=TEXT("build")||C->PendingType!=TEXT("tents_and_habitations")) {Finish(C,false,TEXT("A single viewport click on the habitation card did not arm construction."));return;}
    Record(TEXT("build_card_single_click"),true,TEXT("A single viewport down/up on the build card armed Tents and Habitations."));
    KeyPress(View,EKeys::Escape);if(!C->bBuildMenu)BeginProfileChord(View,TEXT("menu.build"));Advance(EProbeStage::BuildKey);break;
   case EProbeStage::BuildKey: {
    if(!C->bBuildMenu||!BeginProfileChord(View,TEXT("build.tents_and_habitations"))) {Finish(C,false,TEXT("The current profile has no usable habitation build shortcut; preferences were preserved."));return;}
    Advance(EProbeStage::BuildChoice);break;
   }
   case EProbeStage::BuildChoice: {
    if(C->PendingAction!=TEXT("build")||C->PendingType!=TEXT("tents_and_habitations")) {Finish(C,false,TEXT("Viewport habitation shortcut did not arm the correct stable building ID."));return;}
    Record(TEXT("build_keyboard_shortcut"),true,TEXT("Saved construction-menu key then habitation shortcut (defaults Q then D) armed Tents and Habitations without changing preferences."));
    KeyPress(View,EKeys::Escape);
    for(const auto& E:C->Entities)if(E.Owner==C->LocalSlot&&FRTSCatalog::Get().Def(E.Def).Id==TEXT("seat_of_the_people")){HQId=E.Id;break;}
    FVector2D HQPoint;if(!HQId||!EntityPixel(C,HQId,HQPoint)) {Finish(C,false,TEXT("Headquarters was not clickable after drag selection."));return;}
    Down(View,HQPoint,EKeys::LeftMouseButton);Advance(EProbeStage::ReleaseHQ);break;
   }
   case EProbeStage::ReleaseHQ: Up(View);Advance(EProbeStage::HQ);break;
   case EProbeStage::HQ: {
    if(C->Selection.Num()!=1||!C->Selection.Contains(HQId)) {Finish(C,false,TEXT("Clicking headquarters did not replace the drag selection."));return;}
    Record(TEXT("headquarters_single_click"),true,TEXT("Selected only the headquarters after selecting an army."));
    FBox2D InvalidOrder;
    if(C->SupportsOrder(TEXT("attack_move"))||C->SupportsOrder(TEXT("patrol"))||HUD->FindButtonBounds(TEXT("order"),TEXT("attack_move"),InvalidOrder)){Finish(C,false,TEXT("A stationary headquarters exposed a unit movement command."));return;}
    Record(TEXT("building_context_commands"),true,TEXT("Headquarters exposes no attack-move or patrol command."));
    const auto* HQ=World?World->Find(HQId):nullptr;if(!HQ) {Finish(C,false,TEXT("Authoritative headquarters was not found."));return;}
    const FVector2D Offsets[]={FVector2D(9,-5),FVector2D(-5,9),FVector2D(10,2),FVector2D(2,10)};bool Found=false;FVector2D Pixel;
    for(FVector2D Offset:Offsets) {Pixel=C->WorldToScreen(HQ->Pos+Offset).RoundToVector();if(C->InWorld(Pixel)&&!C->EntityUnder(Pixel)) {Found=true;break;}}
    if(!Found) {Finish(C,false,TEXT("No clear visible point was available for the headquarters rally."));return;}
    ExpectedRally=C->ScreenToWorld(Pixel);Down(View,Pixel,EKeys::RightMouseButton);Advance(EProbeStage::ReleaseRally);break;
   }
   case EProbeStage::ReleaseRally: Up(View);Advance(EProbeStage::Rally);break;
   case EProbeStage::Rally: {
    const auto* HQ=World?World->Find(HQId):nullptr;const auto* Snapshot=C->FindEntity(HQId);
    const bool Server=HQ&&HQ->bEconomicRallySet&&HQ->EconomicRallyTarget==0&&FVector2D::Distance(HQ->EconomicRallyPoint,ExpectedRally)<.5f;
    const bool Visible=Snapshot&&Snapshot->bEconomicRallySet&&FVector2D::Distance(Snapshot->EconomicRallyPoint,ExpectedRally)<.5f;
    if(!Server||!Visible) {if(Age>2)Finish(C,false,TEXT("Headquarters right-click rally did not reach both server state and the client snapshot."));return;}
    Record(TEXT("headquarters_right_click_economic_rally"),true,TEXT("Right-click set the economic rally on server and client."));
    if(!Button(HUD,View,TEXT("order"),EProbeStage::MilitaryCardUp,TEXT("rally")))Finish(C,false,TEXT("Explicit military rally card unavailable."));break;
   }
   case EProbeStage::MilitaryCardUp:Up(View);Advance(EProbeStage::MilitaryPoint);break;
   case EProbeStage::MilitaryPoint: {if(C->PendingAction!=TEXT("rally")){Finish(C,false,TEXT("Military rally card failed to arm targeting."));return;}const auto* HQ=World->Find(HQId);FVector2D Pixel=C->WorldToScreen(HQ->Pos+FVector2D(-7,8)).RoundToVector();ExpectedRally=C->ScreenToWorld(Pixel);Down(View,Pixel,EKeys::LeftMouseButton);Advance(EProbeStage::MilitaryPointUp);break;}
   case EProbeStage::MilitaryPointUp:Up(View);Advance(EProbeStage::MilitaryCheck);break;
   case EProbeStage::MilitaryCheck: {const auto* HQ=World->Find(HQId);if(!HQ->bRallySet||FVector2D::Distance(HQ->RallyPoint,ExpectedRally)>.5f){if(Age>2)Finish(C,false,TEXT("Explicit military rally failed."));return;}
    if(!Button(HUD,View,TEXT("order"),EProbeStage::EconomicCardUp,TEXT("rally_economic"))){Finish(C,false,TEXT("Headquarters did not expose its separate Economic Rally card."));return;}break;
   }
   case EProbeStage::EconomicCardUp:Up(View);Advance(EProbeStage::EconomicPoint);break;
   case EProbeStage::EconomicPoint: {
    if(C->PendingAction!=TEXT("rally_economic")){Finish(C,false,TEXT("Economic Rally card did not arm the economic targeting mode."));return;}FVector2D Point;if(!EntityPixel(C,ResourceId,Point)){Finish(C,false,TEXT("Resource not clickable for economic rally."));return;}EconomicPointExpected=C->ScreenToWorld(Point);Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::EconomicPointUp);break;
   }
   case EProbeStage::EconomicPointUp:Up(View);Advance(EProbeStage::EconomicCheck);break;
   case EProbeStage::EconomicCheck: {
    const auto* HQ=World?World->Find(HQId):nullptr;const auto* Visible=C->FindEntity(HQId);
    if(!HQ||!Visible||!HQ->bEconomicRallySet||HQ->EconomicRallyTarget!=ResourceId||!Visible->bEconomicRallySet||Visible->EconomicRallyTarget!=ResourceId||FVector2D::Distance(HQ->EconomicRallyPoint,EconomicPointExpected)>.2f){if(Age>2)Finish(C,false,TEXT("Economic resource rally did not reach authoritative and snapshot state."));return;}
    if(FVector2D::Distance(HQ->RallyPoint,ExpectedRally)>.5f||HQ->RallyTarget!=0){Finish(C,false,TEXT("Setting economic rally overwrote the independent military rally."));return;}
    Record(TEXT("headquarters_split_economic_military_rallies"),true,TEXT("Viewport Economic Rally targeted a resource while the original military destination remained unchanged."));
    OriginalRotation=C->CameraRotation;MovePointer(View,FVector2D(C->ViewSize.X*.5f,C->ViewSize.Y*.4f));RotationAnchor=C->ScreenToWorld(Pointer);KeyEvent(View,EKeys::LeftShift,true,FModifierKeysState(true,false,false,false,false,false,false,false,false));Advance(EProbeStage::RotateWheel);break;
   }
   case EProbeStage::RotateWheel:
    Wheel(View,1,true);KeyEvent(View,EKeys::LeftShift,false);Advance(EProbeStage::RotateCheck);break;
   case EProbeStage::RotateCheck: {
    bool Exact=!FMath::IsNearlyEqual(C->CameraRotation,OriginalRotation);
    const FVector2D Samples[]={C->CameraCenter,C->CameraCenter+FVector2D(5,7),C->CameraCenter+FVector2D(-8,3),FVector2D(80,80)};
    for(FVector2D Point:Samples)Exact&=FVector2D::Distance(C->ScreenToWorld(C->WorldToScreen(Point)),Point)<.001;
    Exact&=FVector2D::Distance(C->ScreenToWorld(Pointer),RotationAnchor)<.05;
    if(!Exact){Finish(C,false,TEXT("Shift+wheel did not rotate the map with a matching inverse and stable pointer anchor."));return;}
    Record(TEXT("rotated_projection_inverse"),true,TEXT("Actual viewport Shift+wheel changed rotation; four inverse projections and cursor anchoring matched."));
    FString MiniFailure;if(!VerifyRTSV08Minimap(C,HUD,MiniFailure)){Finish(C,false,MiniFailure);return;}
    Record(TEXT("v08.rotated_minimap_projection_inverse"),true,TEXT("Rendered minimap roundtrips nine edge/interior points, follows camera heading and rejects outside-world clicks."));
    BeginChord(View,EKeys::Nine,true,false,false);Advance(EProbeStage::GroupSaved);break;
   }
   case EProbeStage::GroupSaved: {
    const auto* Group=C->ControlGroups.Find(9);if(!Group||Group->Num()!=1||!Group->Contains(HQId)){Finish(C,false,TEXT("Ctrl+9 did not save the selected headquarters."));return;}
    FVector2D Point;if(!EntityPixel(C,WorkerId,Point)){Finish(C,false,TEXT("Worker was not clickable for control-group append."));return;}
    Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::GroupWorkerUp);break;
   }
   case EProbeStage::GroupWorkerUp:Up(View);Advance(EProbeStage::GroupWorker);break;
   case EProbeStage::GroupWorker:
    if(C->Selection.Num()!=1||C->Selection[0]!=WorkerId){Finish(C,false,TEXT("The append fixture did not select its worker through the viewport."));return;}
    BeginChord(View,EKeys::Nine,false,true,false);Advance(EProbeStage::GroupAppended);break;
   case EProbeStage::GroupAppended: {
    const auto* Group=C->ControlGroups.Find(9);if(!Group||Group->Num()!=2||!Group->Contains(HQId)||!Group->Contains(WorkerId)){Finish(C,false,TEXT("Shift+9 overwrote or failed to append to the existing control group."));return;}
    GroupCameraBefore=C->CameraCenter;KeyPress(View,EKeys::Nine);Advance(EProbeStage::GroupRecall);break;
   }
   case EProbeStage::GroupRecall: {
    if(C->Selection.Num()!=2||!C->Selection.Contains(HQId)||!C->Selection.Contains(WorkerId)){Finish(C,false,TEXT("9 did not recall both original and appended control-group members."));return;}
    Record(TEXT("shift_number_append_group"),true,TEXT("Viewport Ctrl+9 saved HQ, Shift+9 appended a worker, and 9 recalled both."));
    if(FVector2D::Distance(C->CameraCenter,GroupCameraBefore)>.01f){Finish(C,false,TEXT("Single control-group key unexpectedly moved the camera."));return;}KeyPress(View,EKeys::Nine);Advance(EProbeStage::GroupDouble);break;
   }
   case EProbeStage::GroupDouble: {FVector2D Average=FVector2D::ZeroVector;for(int32 Id:C->Selection)Average+=C->FindEntity(Id)->Pos;Average/=C->Selection.Num();if(FVector2D::Distance(C->CameraCenter,Average)>.75f){Finish(C,false,TEXT("Double-tapping a control group failed to center its average position."));return;}Record(TEXT("v07.double_tap_control_group"),true,TEXT("First viewport number key preserved camera; second centered the recalled group average."));
    // An isolated test-only soldier fixture allows an Age-I input sandbox to exercise the military banner.
    const auto* HQ=World?World->Find(HQId):nullptr;if(!HQ){Finish(C,false,TEXT("Missing headquarters for the idle military fixture."));return;}
    auto* Soldier=World->Spawn(TEXT("man_of_the_sword"),C->LocalSlot,HQ->Pos+FVector2D(6,-6));if(!Soldier){Finish(C,false,TEXT("Could not create isolated idle-military UI fixture."));return;}
    ArmyId=Soldier->Id;World->Population();World->Snapshot();Advance(EProbeStage::ArmyReady);break;
   }
   case EProbeStage::ArmyReady:
    if(!Button(HUD,View,TEXT("idle_military"),EProbeStage::ArmyUp)&&Age>2)Finish(C,false,TEXT("Idle military banner did not become available for an idle soldier."));break;
   case EProbeStage::ArmyUp:Up(View);Advance(EProbeStage::ArmySelected);break;
   case EProbeStage::ArmySelected: {
    if(C->Selection.Num()!=1||C->Selection[0]!=ArmyId){Finish(C,false,TEXT("Clicking the idle military banner failed to select exactly the idle soldier."));return;}
    Record(TEXT("idle_military_banner"),true,TEXT("A viewport click selected the isolated idle soldier, excluding busy and civilian units."));
    EnsureEnemyPlayer(World);PingBuildingId=SpawnUIBuilding(C,World,TEXT("tents_and_habitations"));auto* Target=World->Find(PingBuildingId);if(!Target){Finish(C,false,TEXT("Could not create target-building ping fixture."));return;}Target->Owner=1;Target->HP=Target->MaxHP=100000;World->Snapshot();Advance(EProbeStage::BuildingPingTarget);break;
   }
   case EProbeStage::BuildingPingTarget:{if(Age<.12)return;FVector2D Point;if(!EntityPixel(C,PingBuildingId,Point)){if(Age>2)Finish(C,false,TEXT("Enemy structure is not clickable for the target-ping fixture."));return;}Down(View,Point,EKeys::RightMouseButton);Advance(EProbeStage::BuildingPingUp);break;}
   case EProbeStage::BuildingPingUp:Up(View);Advance(EProbeStage::BuildingPingCheck);break;
   case EProbeStage::BuildingPingCheck:{const auto* Target=C->FindEntity(PingBuildingId);const auto* Soldier=World->Find(ArmyId);if(!Target||!Soldier||Soldier->Target!=PingBuildingId||FVector2D::Distance(C->CommandFlash,Target->Pos)>.01f){if(Age>1)Finish(C,false,TEXT("Military right-click indicator did not snap to the attacked structure's center."));return;}Record(TEXT("v07.attack_building_ping_center"),true,TEXT("Viewport right-click assigned the soldier to the enemy structure and centered its order indicator on that structure."));World->Entities.Remove(PingBuildingId);World->ObstacleCount=-1;World->ResetOrder(*World->Find(ArmyId),TEXT("idle"));World->Snapshot();FVector2D Point;if(!EntityPixel(C,HQId,Point)){Finish(C,false,TEXT("HQ not clickable for production queue fixture."));return;}Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::QueueHQUp);break;}
   case EProbeStage::QueueHQUp:Up(View);Advance(EProbeStage::QueuePrepare);break;
   case EProbeStage::QueuePrepare: {
    if(C->Selection.Num()!=1||C->Selection[0]!=HQId){Finish(C,false,TEXT("Queue fixture could not select HQ through the viewport."));return;}
    if(Age<.1)return;auto& Player=World->Players[C->LocalSlot];Player.Resources+=FVector(2000,2000,2000);World->Snapshot();
    const auto& Catalog=FRTSCatalog::Get();for(const FString& Id:Catalog.Faction(Player.Faction).Upgrades){const auto& D=Catalog.Def(Id);if(D.Kind==TEXT("age_option")&&D.Age==Player.Age){FixtureAgeChoice=Id;break;}}
    if(FixtureAgeChoice.IsEmpty()){Finish(C,false,TEXT("The current civilization has no matching hero age option."));return;}
    Advance(EProbeStage::QueueVisible);break;
   }
   case EProbeStage::QueueVisible: {
    if(Age<.2)return;FBox2D Bounds;const auto& Catalog=FRTSCatalog::Get();bool Correct=HUD->FindButtonBounds(TEXT("age"),FixtureAgeChoice,Bounds)&&!HUD->FindButtonBounds(TEXT("research"),FixtureAgeChoice,Bounds)&&!HUD->FindButtonBounds(TEXT("age"),TEXT("age2"),Bounds);
    for(const auto& D:Catalog.Defs)if(D.Kind==TEXT("age_option")&&D.Age!=C->Me()->Age)Correct&=!HUD->FindButtonBounds(TEXT("age"),D.Id,Bounds);
    const auto& Hero=Catalog.Def(Catalog.Def(FixtureAgeChoice).HeroChoice);Correct&=!HUD->FindButtonBounds(TEXT("train"),Hero.Id,Bounds);
    Correct&=HUD->CountButtons(TEXT("age"),FixtureAgeChoice)==1&&HUD->CountButtons(TEXT("research"),FixtureAgeChoice)==0&&HUD->CountButtons(TEXT("train"),Hero.Id)==0;
    if(!Correct){Finish(C,false,TEXT("Age choice cards were duplicated as research, showed the wrong age, or exposed an unchosen hero."));return;}
    Record(TEXT("hero_age_choice_card_prerequisites"),true,TEXT("Only current faction age choices appear as age actions; generic ages, future choices, duplicate research and unchosen hero retrain are absent."));
    auto* HQ=World->Find(HQId);if(!HQ||!HQ->Jobs.IsEmpty()){Finish(C,false,TEXT("Queue fixture requires an empty HQ production queue."));return;}
    FRTSJob First;First.Type=TEXT("industrious_laborer");First.Paid=FVector::ZeroVector;First.Total=90;First.Remaining=90;First.Pop=1;HQ->Jobs.Add(First);
    FRTSJob Second;Second.Type=FixtureAgeChoice;Second.Paid=FVector::ZeroVector;Second.Total=90;Second.Remaining=90;Second.Pop=Hero.Pop;HQ->Jobs.Add(Second);
    World->Snapshot();Advance(EProbeStage::QueueCancelUp);break;
   }
   case EProbeStage::QueueCancelUp: {
    if(Age<.2)return;FBox2D Bounds;if(!HUD->FindButtonBounds(TEXT("queue_info"),FString(),Bounds)){if(Age>2)Finish(C,false,TEXT("Square queue cards did not appear above the bottom UI."));return;}
    const FString SelectedItem=FString::Printf(TEXT("%d|1"),HQId);const auto* SelectedHQ=C->FindEntity(HQId);
    if(HUD->CountButtons(TEXT("selected_queue_info"),SelectedItem)!=1||HUD->CountButtons(TEXT("cancel_queue"),SelectedItem)!=1||!SelectedHQ||SelectedHQ->Status==TEXT("idle")){Finish(C,false,TEXT("Selected HQ queue did not expose named item/progress/cancel controls or still reported idle."));return;}
    Record(TEXT("selected_building_named_queue"),true,TEXT("Selected HQ exposes its second queue item and explicit cancel button while the corner queue remains available."));
    // FindButtonBounds searches back-to-front, so the last of the two fixture cards is index 1.
    Down(View,Bounds.GetCenter(),EKeys::RightMouseButton);Advance(EProbeStage::QueueCancelled);break;
   }
   case EProbeStage::QueueCancelled: {
    if(Held.IsValid()){Up(View);return;}
    const auto* HQ=World?World->Find(HQId):nullptr;
    if(!HQ||HQ->Jobs.Num()!=1||HQ->Jobs[0].Type!=TEXT("industrious_laborer")){if(Age>2)Finish(C,false,TEXT("Right-clicking the second queue card failed to remove exactly index 1 while preserving the first training item."));return;}
    Record(TEXT("production_queue_right_click_index"),true,TEXT("Right-clicking square card index 1 cancelled the distinct age choice and retained the first worker job."));
    Advance(EProbeStage::WorkforceReady);break;
   }
   case EProbeStage::WorkforceReady:
    if(!Button(HUD,View,TEXT("workforce"),EProbeStage::WorkforceUp,TEXT("0"))&&Age>2)Finish(C,false,TEXT("Food workforce count did not expose a clickable assigned worker."));break;
   case EProbeStage::WorkforceUp:Up(View);Advance(EProbeStage::WorkforceCheck);break;
   case EProbeStage::WorkforceCheck: {
    const auto Workers=C->ResourceWorkers(0);const auto* Worker=World?World->Find(WorkerId):nullptr;
    if(C->Selection.Num()!=1||!Workers.Contains(C->Selection[0])||!Worker||Worker->Order!=TEXT("gather")){Finish(C,false,TEXT("Clicking the workforce count failed to select an assigned worker while preserving its gather order."));return;}
    Record(TEXT("click_resource_workforce_count"),true,TEXT("Clicking the Food workforce count selects and centers an assigned gatherer without interrupting work."));
    FVector2D Point;if(!EntityPixel(C,HQId,Point)){Finish(C,false,TEXT("HQ not clickable for Shift training test."));return;}Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::ShiftHQUp);break;
   }
   case EProbeStage::ShiftHQUp:Up(View);Advance(EProbeStage::ShiftTrainPrepare);break;
   case EProbeStage::ShiftTrainPrepare: {
    if(Age<.1)return;const auto* HQ=World?World->Find(HQId):nullptr;if(!HQ||C->Selection.Num()!=1||C->Selection[0]!=HQId){Finish(C,false,TEXT("Shift training test did not select HQ through the viewport."));return;}
    QueueBeforeBatch=HQ->Jobs.Num();KeyEvent(View,EKeys::LeftShift,true,FModifierKeysState(true,false,false,false,false,false,false,false,false));Advance(EProbeStage::ShiftTrainPress);break;
   }
   case EProbeStage::ShiftTrainPress:
    if(!Button(HUD,View,TEXT("train"),EProbeStage::ShiftTrainUp,TEXT("industrious_laborer"))&&Age>1)Finish(C,false,TEXT("Worker training card was unavailable for Shift+click."));break;
   case EProbeStage::ShiftTrainUp:Up(View);KeyEvent(View,EKeys::LeftShift,false);Advance(EProbeStage::ShiftTrainCheck);break;
   case EProbeStage::ShiftTrainCheck: {
    const auto* HQ=World?World->Find(HQId):nullptr;if(!HQ||HQ->Jobs.Num()!=QueueBeforeBatch+5){if(Age>2)Finish(C,false,TEXT("Viewport Shift+click did not append exactly five training jobs."));return;}
    Record(TEXT("shift_click_train_five"),true,TEXT("Actual viewport Shift+click on a visible unit card appended five paid training jobs."));
    auto* Soldier=World->Find(ArmyId);if(!Soldier){Finish(C,false,TEXT("Idle soldier fixture missing for garrison regression."));return;}
    auto* Carrier=World->Spawn(TEXT("kings_chariot"),C->LocalSlot,Soldier->Pos+FVector2D(3,2));if(!Carrier){Finish(C,false,TEXT("Could not create the isolated chariot fixture."));return;}
    CarrierId=Carrier->Id;World->Snapshot();Advance(EProbeStage::CarrierReady);break;
   }
   case EProbeStage::CarrierReady: {
    if(Age<.2)return;FVector2D Point;if(!EntityPixel(C,ArmyId,Point)){if(Age>2)Finish(C,false,TEXT("Soldier was not clickable beside the chariot."));return;}
    Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::CarrierArmyUp);break;
   }
   case EProbeStage::CarrierArmyUp:Up(View);Advance(EProbeStage::CarrierArmySelected);break;
   case EProbeStage::CarrierArmySelected: {
    if(C->Selection.Num()!=1||C->Selection[0]!=ArmyId){Finish(C,false,TEXT("Garrison test did not select the soldier through viewport input."));return;}
    FVector2D Point;if(!EntityPixel(C,CarrierId,Point)){Finish(C,false,TEXT("Chariot was not clickable for contextual boarding."));return;}
    Down(View,Point,EKeys::RightMouseButton);Advance(EProbeStage::CarrierBoardUp);break;
   }
   case EProbeStage::CarrierBoardUp:Up(View);Advance(EProbeStage::CarrierBoardWait);break;
   case EProbeStage::CarrierBoardWait: {
    const auto* Carrier=World?World->Find(CarrierId):nullptr;const auto* Visible=C->FindEntity(CarrierId);
    if(!Carrier||!Carrier->Passengers.Contains(ArmyId)||!Visible||!Visible->PassengerIds.Contains(ArmyId)){if(Age>4)Finish(C,false,TEXT("Right-clicking the chariot failed to board the soldier and replicate passenger identities."));return;}
    FVector2D Point;if(!EntityPixel(C,CarrierId,Point)){Finish(C,false,TEXT("Chariot was not selectable after boarding."));return;}
    Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::CarrierSelectUp);break;
   }
   case EProbeStage::CarrierSelectUp:Up(View);Advance(EProbeStage::CarrierSelected);break;
   case EProbeStage::CarrierSelected: {
    if(Age<.1)return;FBox2D Bounds;
    if(C->Selection.Num()!=1||C->Selection[0]!=CarrierId||!HUD->FindButtonBounds(TEXT("garrison_info"),FString::FromInt(ArmyId),Bounds)){Finish(C,false,TEXT("Selecting the chariot did not expose its actual passenger icon in the bottom panel."));return;}
    const auto* Visible=C->FindEntity(CarrierId);if(!Visible){Finish(C,false,TEXT("Missing carrier snapshot."));return;}
    FRTSEntityView Ghost=*Visible;Ghost.bStealthed=true;const float Fade=C->VisualOpacity(Ghost);Ghost.bStealthed=false;
    if(!(Fade>0&&Fade<1&&C->VisualOpacity(Ghost)==1)){Finish(C,false,TEXT("Friendly stealth opacity did not distinguish stealthed and visible presentation."));return;}
    Record(TEXT("friendly_stealth_opacity"),true,TEXT("Friendly stealth uses a reduced positive opacity and normal visibility restores full opacity."));
    Record(TEXT("right_click_garrison_panel"),true,TEXT("Right-click boarded a land soldier; selecting the carrier exposed its replicated passenger ID and icon."));
    Screenshot(TEXT("input-garrison.png"));Advance(EProbeStage::CarrierUnload);break;
   }
   case EProbeStage::CarrierUnload:
    if(Age<.25)return;KeyPress(View,EKeys::E);Advance(EProbeStage::CarrierUnloadWait);break;
   case EProbeStage::CarrierUnloadWait: {
    const auto* Carrier=World?World->Find(CarrierId):nullptr;const auto* Soldier=World?World->Find(ArmyId):nullptr;
    if(!Carrier||!Carrier->Passengers.IsEmpty()||!Soldier||Soldier->Carrier!=0){if(Age>3)Finish(C,false,TEXT("Viewport E failed to unload the selected carrier near its own position."));return;}
    Record(TEXT("carrier_e_unload"),true,TEXT("Viewport E unloaded the carrier without targeting the world or sending passengers to map origin."));
    World->Players[C->LocalSlot].Age=4;World->Snapshot();FVector2D Point;if(!EntityPixel(C,HQId,Point)){Finish(C,false,TEXT("HQ not clickable for global age visibility check."));return;}
    Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::FinalHQUp);break;
   }
   case EProbeStage::FinalHQUp:Up(View);Advance(EProbeStage::GlobalAgeHidden);break;
   case EProbeStage::GlobalAgeHidden:
    if(Age<.2)return;
    if(C->Me()->Age!=4||HUD->CountButtons(TEXT("age"))!=0){Finish(C,false,TEXT("A headquarters still displayed age-up hero choices after the civilization globally reached Age IV."));return;}
    Record(TEXT("global_age_choices_hidden"),true,TEXT("All age-up hero choice cards disappear at the civilization's global Age IV, independent of headquarters instance."));
    if(!SetupWallFixture(C,World)){Finish(C,false,TEXT("Could not find an explored, walkable, visible L-shaped wall fixture area."));return;}Advance(EProbeStage::WallBuilderReady);break;
   case EProbeStage::WallBuilderReady: {
    if(Age<.2)return;FVector2D Point;if(!EntityPixel(C,WallBuilderId,Point)){if(Age>2)Finish(C,false,TEXT("Wall fixture builder was not clickable."));return;}Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::WallBuilderUp);break;
   }
   case EProbeStage::WallBuilderUp:Up(View);Advance(EProbeStage::WallBuildOpen);break;
   case EProbeStage::WallBuildOpen:
    if(C->Selection.Num()!=1||C->Selection[0]!=WallBuilderId){Finish(C,false,TEXT("Wall interaction did not select its builder through the viewport."));return;}if(!BeginProfileChord(View,TEXT("menu.build"))){Finish(C,false,TEXT("Construction menu shortcut is unbound."));return;}Advance(EProbeStage::ConstructionFirstSlot);break;
   case EProbeStage::ConstructionFirstSlot:
    if(!C->bBuildMenu||!BeginProfileChord(View,TEXT("build.encampment_of_war"))){Finish(C,false,TEXT("Construction menu or first build slot was unavailable."));return;}Advance(EProbeStage::ConstructionSlotCheck);break;
   case EProbeStage::ConstructionSlotCheck:
    if(!C->bBuildMenu||C->PendingAction!=TEXT("build")||C->PendingType!=TEXT("encampment_of_war")){Finish(C,false,TEXT("Construction opening key swallowed the first build shortcut instead of selecting its card."));return;}
    Record(TEXT("construction_q_opens_then_selects_first_slot"),true,TEXT("Saved menu shortcut opened construction; saved first-slot shortcut armed its building without closing the menu (default Q, Q)."));KeyPress(View,EKeys::Escape);Advance(EProbeStage::WallBuildCard);break;
   case EProbeStage::WallBuildCard:
    if(!Button(HUD,View,TEXT("build"),EProbeStage::WallBuildCardUp,TEXT("works_of_defense"))&&Age>2)Finish(C,false,TEXT("Researched wall card was unavailable for the authorized builder."));break;
   case EProbeStage::WallBuildCardUp:Up(View);Advance(EProbeStage::WallFirst);break;
   case EProbeStage::WallFirst:
    if(C->PendingAction!=TEXT("build")||C->PendingType!=TEXT("works_of_defense")){Finish(C,false,TEXT("Wall card did not arm endpoint construction."));return;}Down(View,WallPixels[0],EKeys::LeftMouseButton);Advance(EProbeStage::WallFirstUp);break;
   case EProbeStage::WallFirstUp:Up(View);Advance(EProbeStage::WallAnchorCheck);break;
   case EProbeStage::WallAnchorCheck:
    if(!C->bWallAnchor||FVector2D::Distance(C->WallAnchor,WallPoints[0])>.1||!WallOrders(World).IsEmpty()){Finish(C,false,TEXT("First wall click should choose only an anchor, without constructing a dot foundation."));return;}
    WallTimberBefore=World->Players[C->LocalSlot].Resources.Y;KeyEvent(View,EKeys::LeftShift,true,FModifierKeysState(true,false,false,false,false,false,false,false,false));Advance(EProbeStage::WallSecond);break;
   case EProbeStage::WallSecond:Down(View,WallPixels[1],EKeys::LeftMouseButton);Advance(EProbeStage::WallSecondUp);break;
   case EProbeStage::WallSecondUp:Up(View);Advance(EProbeStage::WallLineCheck);break;
   case EProbeStage::WallLineCheck:{
    if(!WallSamplesPresent(World,WallPoints[0],WallPoints[1])){if(Age>2)Finish(C,false,TEXT("Second endpoint did not create every continuously spaced wall foundation/order."));return;}
    const FVector2D FirstDelta=WallPoints[1]-WallPoints[0],ExpectedCorner=WallPoints[0]+FirstDelta.GetSafeNormal()*(FMath::FloorToInt(FirstDelta.Size()/WallSpacing+.0001f)*WallSpacing);
    if(!C->bWallAnchor||C->PendingType!=TEXT("works_of_defense")||FVector2D::Distance(C->WallAnchor,ExpectedCorner)>.1){Finish(C,false,TEXT("Shift+second endpoint did not retain the actual last wall sample as its continuation anchor."));return;}WallPoints[1]=ExpectedCorner;
    FirstWallSamples=WallOrders(World).Num();Record(TEXT("wall_two_endpoints_continuous_orders"),true,TEXT("First click only anchored; second click created all authoritative foundation/queued samples at 2*radius+0.15m spacing."));
    Down(View,WallPixels[2],EKeys::LeftMouseButton);Advance(EProbeStage::WallThirdUp);break;}
   case EProbeStage::WallThirdUp:Up(View);Advance(EProbeStage::WallChainCheck);break;
   case EProbeStage::WallChainCheck:{
    if(!WallSamplesPresent(World,WallPoints[0],WallPoints[1])||!WallSamplesPresent(World,WallPoints[1],WallPoints[2])||WallOrders(World).Num()<=FirstWallSamples){if(Age>2)Finish(C,false,TEXT("Shift+third endpoint failed to append the second wall leg while preserving the first."));return;}
    const FVector2D LastDelta=WallPoints[2]-WallPoints[1],ExpectedEnd=WallPoints[1]+LastDelta.GetSafeNormal()*(FMath::FloorToInt(LastDelta.Size()/WallSpacing+.0001f)*WallSpacing);if(!C->bWallAnchor||FVector2D::Distance(C->WallAnchor,ExpectedEnd)>.1){Finish(C,false,TEXT("Third endpoint did not advance to its last authoritative wall sample."));return;}
    const int32 ExpectedTotal=FirstWallSamples+FMath::FloorToInt(LastDelta.Size()/WallSpacing+.0001f);const float ExpectedTimber=WallTimberBefore-ExpectedTotal*FRTSCatalog::Get().Def(TEXT("works_of_defense")).Cost.Y;if(WallOrders(World).Num()!=ExpectedTotal||!FMath::IsNearlyEqual(float(World->Players[C->LocalSlot].Resources.Y),ExpectedTimber,.01f)){Finish(C,false,TEXT("Chained wall duplicated its shared corner or charged for it twice."));return;}
    Record(TEXT("wall_shift_endpoint_chain"),true,TEXT("Shift+third click appended the next continuous wall leg and kept construction armed at the third endpoint."));Screenshot(TEXT("input-walls.png"));Advance(EProbeStage::WallShot);break;}
   case EProbeStage::WallShot:
    if(Age<.25)return;KeyEvent(View,EKeys::LeftShift,false);KeyPress(View,EKeys::Escape);if(!SetupTraderFixture(C,World)){Finish(C,false,TEXT("Could not find a clear, visible trader waypoint fixture."));return;}Advance(EProbeStage::TraderReady);break;
   case EProbeStage::TraderReady: {
    if(Age<.2)return;FVector2D Point;if(!EntityPixel(C,TraderId,Point)){if(Age>2)Finish(C,false,TEXT("Galavant fixture trader was not clickable."));return;}Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::TraderUp);break;
   }
   case EProbeStage::TraderUp:Up(View);Advance(EProbeStage::GalavantButton);break;
   case EProbeStage::GalavantButton:
    if(!Button(HUD,View,TEXT("order"),EProbeStage::GalavantButtonUp,TEXT("galavant"))&&Age>2)Finish(C,false,TEXT("Selected trader did not expose a Galavant button."));break;
   case EProbeStage::GalavantButtonUp:Up(View);Advance(EProbeStage::GalavantFirst);break;
   case EProbeStage::GalavantFirst:
    if(C->PendingAction!=TEXT("galavant")){Finish(C,false,TEXT("Galavant button did not arm route selection."));return;}Down(View,RoutePixels[0],EKeys::LeftMouseButton);Advance(EProbeStage::GalavantFirstUp);break;
   case EProbeStage::GalavantFirstUp:Up(View);Advance(EProbeStage::GalavantFirstCheck);break;
   case EProbeStage::GalavantFirstCheck: {
    const auto* Trader=World?World->Find(TraderId):nullptr;if(!Trader||Trader->Order!=TEXT("galavant")||Trader->TradeRoute.Num()!=1||FVector2D::Distance(Trader->TradeRoute[0],RoutePoints[0])>.1){if(Age>2)Finish(C,false,TEXT("Galavant first point did not replace the authoritative route with exactly one waypoint."));return;}
    Record(TEXT("galavant_button_first_waypoint"),true,TEXT("Viewport Galavant button and first world click created exactly one authoritative route point."));KeyEvent(View,EKeys::LeftShift,true,FModifierKeysState(true,false,false,false,false,false,false,false,false));Advance(EProbeStage::GalavantSecond);break;
   }
   case EProbeStage::GalavantSecond:Down(View,RoutePixels[1],EKeys::LeftMouseButton);Advance(EProbeStage::GalavantSecondUp);break;
   case EProbeStage::GalavantSecondUp:Up(View);KeyEvent(View,EKeys::LeftShift,false);Advance(EProbeStage::GalavantRouteCheck);break;
   case EProbeStage::GalavantRouteCheck: {
    const auto* Trader=World?World->Find(TraderId):nullptr;if(!Trader||Trader->TradeRoute.Num()!=2||FVector2D::Distance(Trader->TradeRoute[0],RoutePoints[0])>.1||FVector2D::Distance(Trader->TradeRoute[1],RoutePoints[1])>.1){if(Age>2)Finish(C,false,TEXT("Shift+second Galavant point overwrote the first waypoint or failed to append to the server route."));return;}
    if(C->PendingAction!=TEXT("galavant")){Finish(C,false,TEXT("Galavant route mode unexpectedly closed between waypoint clicks."));return;}
    Record(TEXT("galavant_shift_append_route"),true,TEXT("Actual viewport Shift+click appended the second server waypoint, preserved the first, and left route editing armed."));Screenshot(TEXT("input-routes.png"));Advance(EProbeStage::GalavantShot);break;
   }
   case EProbeStage::GalavantShot:
    if(Age<.25)return;KeyPress(View,EKeys::Escape);if(!World->OwnedStructure(C->LocalSlot,TEXT("tents_and_habitations"))&&!SpawnUIBuilding(C,World,TEXT("tents_and_habitations"))){Finish(C,false,TEXT("Could not place the housing fixture needed for Market training capacity."));return;}World->Population();MarketId=SpawnUIBuilding(C,World,TEXT("place_of_trade"));if(!MarketId||!MarketWaypoints(C,World,World->Find(MarketId)->Pos)){Finish(C,false,TEXT("Could not find a visible Market fixture and two walkable waypoints."));return;}World->Snapshot();Advance(EProbeStage::MarketReady);break;
   case EProbeStage::MarketReady: {
    if(Age<.2)return;FVector2D Point;if(!EntityPixel(C,MarketId,Point)){if(Age>2)Finish(C,false,TEXT("Market fixture was not clickable."));return;}Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::MarketUp);break;
   }
   case EProbeStage::MarketUp:Up(View);Advance(EProbeStage::MarketButton);break;
   case EProbeStage::MarketButton:
    if(!Button(HUD,View,TEXT("order"),EProbeStage::MarketButtonUp,TEXT("market_galavant"))&&Age>2)Finish(C,false,TEXT("Market did not expose Set Galavant Route."));break;
   case EProbeStage::MarketButtonUp:Up(View);Advance(EProbeStage::MarketFirst);break;
   case EProbeStage::MarketFirst:
    if(C->PendingAction!=TEXT("market_galavant")){Finish(C,false,TEXT("Market route button did not arm its waypoint mode."));return;}Down(View,RoutePixels[0],EKeys::LeftMouseButton);Advance(EProbeStage::MarketFirstUp);break;
   case EProbeStage::MarketFirstUp:Up(View);Advance(EProbeStage::MarketFirstCheck);break;
   case EProbeStage::MarketFirstCheck: {
    const auto* Market=World?World->Find(MarketId):nullptr;if(!Market||Market->GalavantRoute.Num()!=1||FVector2D::Distance(Market->GalavantRoute[0],RoutePoints[0])>.1){if(Age>2)Finish(C,false,TEXT("Market first click did not replace its default route with one waypoint."));return;}KeyEvent(View,EKeys::LeftShift,true,FModifierKeysState(true,false,false,false,false,false,false,false,false));Advance(EProbeStage::MarketSecond);break;
   }
   case EProbeStage::MarketSecond:Down(View,RoutePixels[1],EKeys::LeftMouseButton);Advance(EProbeStage::MarketSecondUp);break;
   case EProbeStage::MarketSecondUp:Up(View);KeyEvent(View,EKeys::LeftShift,false);Advance(EProbeStage::MarketRouteCheck);break;
   case EProbeStage::MarketRouteCheck: {
    const auto* Market=World?World->Find(MarketId):nullptr;const auto* Visible=C->FindEntity(MarketId);
    if(!Market||!Visible||Market->GalavantRoute.Num()!=2||Visible->GalavantRoute.Num()!=2||FVector2D::Distance(Market->GalavantRoute[0],RoutePoints[0])>.1||FVector2D::Distance(Market->GalavantRoute[1],RoutePoints[1])>.1){if(Age>2)Finish(C,false,TEXT("Market Shift waypoint did not append and replicate the route."));return;}
    Record(TEXT("market_default_galavant_route"),true,TEXT("Actual Market route button, first world click and Shift second click set and replicate the default route."));KeyPress(View,EKeys::Escape);Advance(EProbeStage::MarketTrain);break;
   }
   case EProbeStage::MarketTrain:
    BeforeMarketProduction=World->NextId;if(!Button(HUD,View,TEXT("train"),EProbeStage::MarketTrainUp,TEXT("trader_in_precious_things"))&&Age>2)Finish(C,false,TEXT("Market trader card unavailable after setting a route."));break;
   case EProbeStage::MarketTrainUp:Up(View);Advance(EProbeStage::MarketProduced);break;
   case EProbeStage::MarketProduced: {
    auto* Market=World?World->Find(MarketId):nullptr;if(!Market){Finish(C,false,TEXT("Market disappeared during route inheritance test."));return;}if(Market->Jobs.Num())Market->Jobs[0].Remaining=FMath::Min(Market->Jobs[0].Remaining,.01f);
    const FRTSEntity* NewTrader=nullptr;for(const auto& Pair:World->Entities)if(Pair.Key>=BeforeMarketProduction&&Pair.Value->Owner==C->LocalSlot&&World->D(*Pair.Value).Kind==TEXT("trader"))NewTrader=Pair.Value.Get();
    if(!NewTrader||NewTrader->Order!=TEXT("galavant")||NewTrader->TradeRoute.Num()!=2||FVector2D::Distance(NewTrader->TradeRoute[0],RoutePoints[0])>.1||FVector2D::Distance(NewTrader->TradeRoute[1],RoutePoints[1])>.1){if(Age>3)Finish(C,false,TEXT("Trader trained through the Market card failed to inherit the configured route."));return;}
    Record(TEXT("market_new_trader_inherits_route"),true,TEXT("Viewport-trained trader inherited both Market default waypoints and started Galavant; test shortened only its queue duration."));
    auto& PlayerState=World->Players[C->LocalSlot];PlayerState.Faction=TEXT("lamanites");PlayerState.Age=3;PlayerState.Resources+=FVector(5000,5000,5000);PlayerState.Upgrades.Remove(TEXT("order_of_the_nehors"));PlayerState.Upgrades.Remove(TEXT("by_the_pride_of_their_hearts"));
    TempleId=SpawnUIBuilding(C,World,TEXT("temple_of_the_land"));if(!TempleId){Finish(C,false,TEXT("Could not find a visible Lamanite temple fixture."));return;}World->Spawn(TEXT("priestcrafter"),C->LocalSlot,World->Find(TempleId)->Pos+FVector2D(4,0));World->Snapshot();Advance(EProbeStage::TempleReady);break;
   }
   case EProbeStage::TempleReady: {
    if(Age<.2)return;FVector2D Point;if(!EntityPixel(C,TempleId,Point)){if(Age>2)Finish(C,false,TEXT("Lamanite temple was not clickable."));return;}Down(View,Point,EKeys::LeftMouseButton);Advance(EProbeStage::TempleUp);break;
   }
   case EProbeStage::TempleUp:Up(View);Advance(EProbeStage::TempleCards);break;
   case EProbeStage::TempleCards: {
    if(Age<.1)return;FBox2D Locked;
    const int32 OrderCards=HUD->CountButtons(TEXT("research"),TEXT("order_of_the_nehors")),PrideCards=HUD->CountButtons(TEXT("research"),TEXT("by_the_pride_of_their_hearts"));
    const auto& Faction=FRTSCatalog::Get().Faction(C->Me()->Faction);const bool OrderAllowed=Faction.Upgrades.Contains(TEXT("order_of_the_nehors")),PrideAllowed=Faction.Upgrades.Contains(TEXT("by_the_pride_of_their_hearts"));
    if(OrderCards!=1||PrideCards!=1||HUD->FindButtonBounds(TEXT("research"),TEXT("by_the_pride_of_their_hearts"),Locked)){FString OrderReason,PrideReason;C->CanUse(FRTSCatalog::Get().Def(TEXT("order_of_the_nehors")),OrderReason);C->CanUse(FRTSCatalog::Get().Def(TEXT("by_the_pride_of_their_hearts")),PrideReason);Finish(C,false,FString::Printf(TEXT("Temple research visibility: faction=%s selection=%d expectedTemple=%d cards Order=%d Pride=%d whitelist Order=%d Pride=%d; Order reason=%s; Pride reason=%s. Both cards must exist and Pride remain locked until Order."),*C->Me()->Faction,C->Selection.Num()?C->Selection[0]:0,TempleId,OrderCards,PrideCards,OrderAllowed?1:0,PrideAllowed?1:0,*OrderReason,*PrideReason));return;}
    if(!Button(HUD,View,TEXT("research"),EProbeStage::TempleOrderUp,TEXT("order_of_the_nehors"))&&Age>2)Finish(C,false,TEXT("Order of the Nehors was not clickable at an eligible Lamanite temple."));break;
   }
   case EProbeStage::TempleOrderUp:Up(View);Advance(EProbeStage::TempleOrderQueued);break;
   case EProbeStage::TempleOrderQueued: {
    auto* Temple=World?World->Find(TempleId):nullptr;bool Queued=false;if(Temple)for(auto& Job:Temple->Jobs)if(Job.Type==TEXT("order_of_the_nehors")){Queued=true;Job.Remaining=.01f;}
    if(!Queued){if(Age>2)Finish(C,false,TEXT("Clicking Order of the Nehors failed to create its authoritative research job."));return;}
    Record(TEXT("lamanite_temple_order_visible_clickable"),true,TEXT("Selected Lamanite temple displayed both upgrades and accepted an actual viewport click on Order of the Nehors."));Advance(EProbeStage::TemplePrideReady);break;
   }
   case EProbeStage::TemplePrideReady:
    if(!C->Me()->Upgrades.Contains(TEXT("order_of_the_nehors"))){if(Age>2)Finish(C,false,TEXT("Completed Order research failed to reach the temple snapshot."));return;}
    if(!Button(HUD,View,TEXT("research"),EProbeStage::TemplePrideUp,TEXT("by_the_pride_of_their_hearts"))&&Age>3)Finish(C,false,TEXT("Pride upgrade did not become clickable after its prerequisite."));break;
   case EProbeStage::TemplePrideUp:Up(View);Advance(EProbeStage::TemplePrideQueued);break;
   case EProbeStage::TemplePrideQueued: {
    const auto* Temple=World?World->Find(TempleId):nullptr;const auto* Visible=C->FindEntity(TempleId);const FString Item=FString::Printf(TEXT("%d|0"),TempleId);
    if(!Temple||Temple->Jobs.Num()!=1||Temple->Jobs[0].Type!=TEXT("by_the_pride_of_their_hearts")||!Visible||Visible->QueueProgress<=0||HUD->CountButtons(TEXT("selected_queue_info"),Item)!=1){if(Age>2)Finish(C,false,TEXT("Clicking Pride did not display its active named queue and progress at the selected temple."));return;}
    Record(TEXT("lamanite_temple_pride_visible_clickable"),true,TEXT("After researching Order, viewport click queued Pride; selected temple shows its named item and nonzero progress."));
    FBox2D Bounds;if(!HUD->FindButtonBounds(TEXT("religion_info"),FString(),Bounds)){Finish(C,false,TEXT("Religion statistic lacks its hover target."));return;}MovePointer(View,Bounds.GetCenter());Advance(EProbeStage::ReligionHover);break;
   }
   case EProbeStage::ReligionHover: {
    if(Age<.15)return;const auto* PlayerState=C->Me();const FString Counts=FString::Printf(TEXT("Religious units: %d | Structures: %d"),PlayerState->ReligiousUnits,PlayerState->ReligiousStructures);const FString Tip=HUD->CurrentHoverTip();
    if(PlayerState->ReligiousUnits<1||PlayerState->ReligiousStructures<1||!Tip.Contains(Counts)||!Tip.Contains(TEXT("Gathering speed:"))||!Tip.Contains(TEXT("Attack speed:"))||!Tip.StartsWith(C->DevotionLabel())){if(Age>2)Finish(C,false,TEXT("Actual religion-stat hover did not show authoritative religious counts and current faction bonuses."));return;}
    TArray<FString> TipLines;Tip.ParseIntoArrayLines(TipLines,false);const auto Bounds=HUD->TooltipBounds;const bool TooltipFits=Bounds.bIsValid&&Bounds.Min.X>=0&&Bounds.Min.Y>=0&&Bounds.Max.X<=View->GetSizeXY().X&&Bounds.Max.Y<=View->GetSizeXY().Y&&HUD->TooltipLineCount>=TipLines.Num();if(!TooltipFits){Finish(C,false,TEXT("Full religion tooltip does not fit its frame inside the physical viewport."));return;}Record(TEXT("v07.tooltip_full_frame_inside_viewport"),true,FString::Printf(TEXT("%dx%d viewport: %d drawn lines; frame %s to %s."),View->GetSizeXY().X,View->GetSizeXY().Y,HUD->TooltipLineCount,*Bounds.Min.ToString(),*Bounds.Max.ToString()));Screenshot(TEXT("input-v07-tooltip.png"));
    Record(TEXT("religion_authoritative_counts_hover"),true,TEXT("Actual viewport hover on Anger displays authoritative unit/structure counts, gathering bonus and attack-speed bonus."));Screenshot(TEXT("input-religion.png"));Advance(EProbeStage::ReligionShot);break;
   }
   case EProbeStage::ReligionShot: {
    if(Age<.25)return;const FString Item=FString::Printf(TEXT("%d|0"),TempleId);if(!Button(HUD,View,TEXT("cancel_queue"),EProbeStage::TempleCancelUp,Item)){Finish(C,false,TEXT("Selected temple queue lacks its explicit cancel control."));return;}break;
   }
   case EProbeStage::TempleCancelUp:Up(View);Advance(EProbeStage::TempleCancelled);break;
   case EProbeStage::TempleCancelled: {
    const auto* Temple=World?World->Find(TempleId):nullptr;if(!Temple||!Temple->Jobs.IsEmpty()){if(Age>2)Finish(C,false,TEXT("Selected building queue cancel control did not cancel the queued technology."));return;}
    Record(TEXT("selected_building_queue_explicit_cancel"),true,TEXT("Viewport click on the selected temple's queue x cancelled its technology while retaining the corner queue interface."));Screenshot(TEXT("input-gameplay.png"));Advance(EProbeStage::AnimationSetup);break;
   }
   case EProbeStage::AnimationSetup:
    if(Age<.12)return;if(!SetupAnimation(C,World)){Finish(C,false,TEXT("Could not prepare the isolated live animation fixture."));return;}Advance(EProbeStage::AnimationA);break;
   case EProbeStage::AnimationA: {
    const TCHAR* States[]={TEXT("move"),TEXT("attack"),TEXT("gather_timber"),TEXT("convert"),TEXT("move"),TEXT("move")};FRTSVisualPoseMetrics Pose;
    if(!FRTSVisualAssets::Get().GetPoseMetrics(AnimationEntityId,Pose)||Pose.State!=States[AnimationIndex]||Pose.Elapsed<.03f||(AnimationIndex!=1&&Age<.65)){if(Age>4)Finish(C,false,FString::Printf(TEXT("Animation %d never rendered its expected live state; actual=%s."),AnimationIndex,*Pose.State));return;}
    PoseA=Pose;const FString Shot=FString::Printf(TEXT("input-v07-animation-%d-a.png"),AnimationIndex);Screenshot(*Shot);Advance(EProbeStage::AnimationB);break;
   }
   case EProbeStage::AnimationB: {
    if(Age<.09)return;FRTSVisualPoseMetrics Pose;if(!FRTSVisualAssets::Get().GetPoseMetrics(AnimationEntityId,Pose)||Pose.State!=PoseA.State||Pose.PoseHash==PoseA.PoseHash||Pose.NonRigidDisplacement<=.00001f||PoseA.NonRigidDisplacement<=.00001f||Pose.Triangles<2){if(Age>.2)Finish(C,false,FString::Printf(TEXT("Live animation %d did not produce two distinct nonrigid meshes: state=%s hash=%u/%u deformation=%.5f/%.5f."),AnimationIndex,*Pose.State,PoseA.PoseHash,Pose.PoseHash,PoseA.NonRigidDisplacement,Pose.NonRigidDisplacement));return;}
    Record(FString::Printf(TEXT("v07.live_animation_pair_%d"),AnimationIndex),true,FString::Printf(TEXT("%s rig=%s: mesh hashes %u/%u, nonrigid displacement %.5f/%.5f, triangles %d."),*Pose.State,*Pose.Rig,PoseA.PoseHash,Pose.PoseHash,PoseA.NonRigidDisplacement,Pose.NonRigidDisplacement,Pose.Triangles));const FString Shot=FString::Printf(TEXT("input-v07-animation-%d-b.png"),AnimationIndex);Screenshot(*Shot);Advance(EProbeStage::AnimationNext);break;
   }
   case EProbeStage::AnimationNext:
    if(Age<.15)return;for(int32 Id:AnimationIds)World->Entities.Remove(Id);AnimationIds.Empty();World->ObstacleCount=-1;World->NavigationCount=-1;AnimationIndex++;if(AnimationIndex<6)Advance(EProbeStage::AnimationSetup);else{World->Map=AnimationSavedMap;C->CameraCenter=AnimationSavedCamera;C->Zoom=AnimationSavedZoom;C->Selection={HQId};World->Snapshot();Advance(EProbeStage::V08LabanSetup);}break;
   case EProbeStage::V08LabanSetup:
    if(!SetupV08Scene(C,World,true)){Finish(C,false,TEXT("Could not create the isolated Laban presentation fixture."));return;}Advance(EProbeStage::V08BuildOpen);break;
   case EProbeStage::V08BuildOpen:
    if(Age<.15)return;if(!BeginProfileChord(View,TEXT("menu.build"))){Finish(C,false,TEXT("Build menu has no input binding."));return;}Advance(EProbeStage::V08BuildLocks);break;
   case EProbeStage::V08BuildLocks: {
    if(Age<.15)return;FBox2D Enabled;
    const auto& Temple=FRTSCatalog::Get().Def(TEXT("temple_of_the_land"));
    if(!C->bBuildMenu||HUD->CountButtons(TEXT("build"),Temple.Id)!=1||HUD->FindButtonBounds(TEXT("build"),Temple.Id,Enabled)||FRTSCatalog::Get().RequiredAge(Temple,TEXT("laban"))!=4){if(Age>2)Finish(C,false,TEXT("Age-II Laban build menu must show its locked Age-IV Great and Spacious Building card."));return;}
    if(!CaptureFrame(TEXT("input-v08-laban-age4-build-lock.png")))return;Record(TEXT("v08.laban_locked_great_spacious_build_card"),true,TEXT("Build-menu keyboard input exposed the unavailable Age-IV structure card and its Roman numeral overlay."));
    KeyPress(View,EKeys::Escape);C->Selection={V08Barracks};Advance(EProbeStage::V08MilitaryLocks);break;
   }
   case EProbeStage::V08MilitaryLocks: {
    if(Age<.15)return;FBox2D Enabled;
    if(HUD->CountButtons(TEXT("train"),TEXT("kings_chariot"))!=1||HUD->FindButtonBounds(TEXT("train"),TEXT("kings_chariot"),Enabled)){if(Age>2)Finish(C,false,TEXT("Age-II Laban barracks must show its unavailable Age-III chariot card."));return;}
    if(!CaptureFrame(TEXT("input-v08-laban-age3-unit-lock.png")))return;Record(TEXT("v08.laban_locked_age3_unit_card"),true,TEXT("Age-II barracks displays the locked King's Chariot with its III overlay."));C->Selection={V08Winery};Advance(EProbeStage::V08WineryCard);break;
   }
   case EProbeStage::V08WineryCard:
    if(Age<.15)return;V08FirstTrained=World->NextId;if(!Button(HUD,View,TEXT("train"),EProbeStage::V08WineryUp,TEXT("drunken_concubine"))&&Age>2)Finish(C,false,TEXT("Laban Winery did not expose an enabled Drunken Concubine training card."));break;
   case EProbeStage::V08WineryUp:Up(View);Advance(EProbeStage::V08Concubine);break;
   case EProbeStage::V08Concubine: {
    auto* Winery=World->Find(V08Winery);if(Winery)for(auto& Job:Winery->Jobs)if(Job.Type==TEXT("drunken_concubine"))Job.Remaining=FMath::Min(Job.Remaining,.01f);
    for(const auto& Pair:World->Entities)if(Pair.Key>=V08FirstTrained&&Pair.Value->Owner==C->LocalSlot&&World->D(*Pair.Value).Id==TEXT("drunken_concubine"))V08Concubine=Pair.Key;
    if(!V08Concubine||!C->FindEntity(V08Concubine)){if(Age>3)Finish(C,false,TEXT("Viewport Winery training failed to produce a replicated Drunken Concubine."));return;}
    Record(TEXT("v08.winery_trains_concubine"),true,TEXT("Viewport training card produced a Drunken Concubine from the Winery; only queue duration was shortened."));C->Selection={HQId};MovePointer(View,FVector2D(C->ViewSize.X*.35f,95));Advance(EProbeStage::V08District);break;
   }
   case EProbeStage::V08District: {
    if(Age<.2)return;const auto* Concubine=C->FindEntity(V08Concubine);
    if(!Concubine||Concubine->AuraRadius<=0||C->Selection.Contains(V08Concubine)){Finish(C,false,TEXT("Unselected Laban Concubine did not retain its visible aura radius in the client snapshot."));return;}
    MovePointer(View,FVector2D(C->ViewSize.X*.35f,95));
    if(!CaptureFrame(TEXT("input-v08-laban-district-normal.png")))return;Record(TEXT("v08.unselected_concubine_aura_and_laban_district"),true,TEXT("Unselected Concubine retains its aura; normal-zoom screenshot includes Laban structures, Seat, and small homes."));C->Zoom=.58f;Advance(EProbeStage::V08FarDistrict);break;
   }
   case EProbeStage::V08FarDistrict:
    if(Age<.2)return;MovePointer(View,FVector2D(C->ViewSize.X*.35f,95));if(!CaptureFrame(TEXT("input-v08-laban-district-far.png")))return;Record(TEXT("v08.laban_district_far_zoom"),true,TEXT("Captured the same Laban district at far zoom for structure and housing scale review."));Advance(EProbeStage::V08GadiantonSetup);break;
   case EProbeStage::V08GadiantonSetup:
    if(Age<.1)return;if(!SetupV08Scene(C,World,false)){Finish(C,false,TEXT("Could not create the isolated Gadianton UI fixture."));return;}Advance(EProbeStage::V08RaidCard);break;
   case EProbeStage::V08RaidCard:
    if(Age<.15)return;if(HUD->CountButtons(TEXT("order"),TEXT("attack_move"))||C->SupportsOrder(TEXT("attack_move"))){Finish(C,false,TEXT("Storehouse exposed a unit-only attack-move command."));return;}
    if(!Button(HUD,View,TEXT("order"),EProbeStage::V08RaidCardUp,TEXT("storehouse_raid"))&&Age>2)Finish(C,false,TEXT("Selected Gadianton Storehouse lacked its Raid Rally action card."));break;
   case EProbeStage::V08RaidCardUp:Up(View);Advance(EProbeStage::V08RaidTarget);break;
   case EProbeStage::V08RaidTarget: {
    if(C->PendingAction!=TEXT("storehouse_raid")){Finish(C,false,TEXT("Raid Rally card did not arm the Storehouse route targeting mode."));return;}
    FVector2D Pixel;if(!EntityPixel(C,V08RaidTarget,Pixel)){if(Age>2)Finish(C,false,TEXT("Enemy raid fixture could not be targeted through the viewport."));return;}Down(View,Pixel,EKeys::LeftMouseButton);Advance(EProbeStage::V08RaidTargetUp);break;
   }
   case EProbeStage::V08RaidTargetUp:Up(View);Advance(EProbeStage::V08RaidCheck);break;
   case EProbeStage::V08RaidCheck: {
    const auto* Site=World->Find(V08Storehouse);const auto* Visible=C->FindEntity(V08Storehouse);
    if(!Site||!Visible||!Site->bRaidRallySet||!Visible->bRaidRallySet||Site->RaidRallyTarget!=V08RaidTarget||Visible->RaidRallyTarget!=V08RaidTarget){if(Age>2)Finish(C,false,TEXT("Raid Rally viewport target did not reach Storehouse server and snapshot state."));return;}
    if(!CaptureFrame(TEXT("input-v08-storehouse-raid-rally.png")))return;Record(TEXT("v08.storehouse_raid_card_target"),true,TEXT("Raid Rally card and enemy-site click set and replicate the Storehouse's separate raid destination."));KeyPress(View,EKeys::Y);Advance(EProbeStage::V08RaidKey);break;
   }
   case EProbeStage::V08RaidKey: {
    if(C->PendingAction!=TEXT("storehouse_raid")){Finish(C,false,TEXT("Y did not arm Raid Rally for a selected Gadianton Storehouse."));return;}
    bool Found=false;for(int32 I=0;I<12&&!Found;I++){const FVector2D At(66+I%4*3,61+I/4*3);const FVector2D Pixel=C->WorldToScreen(At).RoundToVector();if(C->InWorld(Pixel)&&C->EntityUnder(Pixel)==0){ExpectedRally=C->ScreenToWorld(Pixel);Down(View,Pixel,EKeys::LeftMouseButton);Found=true;}}
    if(!Found){Finish(C,false,TEXT("No empty viewport point was available for the Raid Rally keyboard test."));return;}Advance(EProbeStage::V08RaidGroundUp);break;
   }
   case EProbeStage::V08RaidGroundUp:Up(View);Advance(EProbeStage::V08RaidGroundCheck);break;
   case EProbeStage::V08RaidGroundCheck: {
    const auto* Site=World->Find(V08Storehouse);const auto* Visible=C->FindEntity(V08Storehouse);
    if(!Site||!Visible||Site->RaidRallyTarget||Visible->RaidRallyTarget||FVector2D::Distance(Site->RaidRallyPoint,ExpectedRally)>.1||FVector2D::Distance(Visible->RaidRallyPoint,ExpectedRally)>.1){if(Age>2)Finish(C,false,TEXT("Y and world click failed to replace the Storehouse raid destination."));return;}
    Record(TEXT("v08.storehouse_raid_keyboard_ground"),true,TEXT("Y armed Raid Rally; viewport ground click replaced and replicated the destination independently of production rallies."));C->Selection={V08Lawyer};Advance(EProbeStage::V08BribeHover);break;
   }
   case EProbeStage::V08BribeHover: {
    if(Age<.15)return;FBox2D Bounds;if(!HUD->FindButtonBounds(TEXT("order"),TEXT("convert"),Bounds)){if(Age>2)Finish(C,false,TEXT("Lawyer lacked its Bribe command card."));return;}MovePointer(View,Bounds.GetCenter());Advance(EProbeStage::V08BribeCheck);break;
   }
   case EProbeStage::V08BribeCheck:
    if(Age<.15)return;if(!HUD->CurrentHoverTip().StartsWith(TEXT("Bribe:"))){if(Age>2)Finish(C,false,TEXT("Lawyer command hover must identify its action as Bribe."));return;}if(!CaptureFrame(TEXT("input-v08-lawyer-bribe.png")))return;Record(TEXT("v08.lawyer_bribe_card_hover"),true,TEXT("Actual viewport hover labels the Lawyer's conversion action Bribe."));C->Selection={V08Robber};Advance(EProbeStage::V08HideoutOrder);break;
   case EProbeStage::V08HideoutOrder: {
    if(Age<.15)return;FVector2D Pixel;if(!EntityPixel(C,V08Hideout,Pixel,true)){if(Age>2)Finish(C,false,TEXT("Gadianton hideout lacked a visible point accepted by the actual right-click context picker."));return;}
    UE_LOG(LogTemp,Display,TEXT("RTS_V08_HIDEOUT_CLICK expected=%d context=%d visible=%d selected=%d pixel=%s pending=%s"),V08Hideout,C->EntityUnder(Pixel,true),C->EntityUnder(Pixel),C->Selection.Num()?C->Selection[0]:0,*Pixel.ToString(),*C->PendingAction);
    Down(View,Pixel,EKeys::RightMouseButton);Advance(EProbeStage::V08HideoutUp);break;
   }
   case EProbeStage::V08HideoutUp:Up(View);Advance(EProbeStage::V08HideoutBoard);break;
   case EProbeStage::V08HideoutBoard: {
    const auto* Robber=World->Find(V08Robber);const auto* Hideout=World->Find(V08Hideout);
    if(!Robber||!Hideout||Robber->Carrier!=V08Hideout||!Hideout->Passengers.Contains(V08Robber)||C->FindEntity(V08Robber)){if(Age>3)Finish(C,false,FString::Printf(TEXT("Public right-click boarding failed: robber=%d hideout=%d order=%s target=%d carrier=%d passengers=%d clientVisible=%d distance=%.2f pending=%s."),V08Robber,V08Hideout,Robber?*Robber->Order:TEXT("missing"),Robber?Robber->Target:0,Robber?Robber->Carrier:0,Hideout?Hideout->Passengers.Num():0,C->FindEntity(V08Robber)?1:0,Robber&&Hideout?FVector2D::Distance(Robber->Pos,Hideout->Pos):-1.,*C->PendingAction));return;}
    if(C->DeathAnimations.ContainsByPredicate([&](const ARTSController::FDeathAnimation& D){return D.Entity.Id==V08Robber;})){Finish(C,false,TEXT("Boarding generated a false death animation."));return;}
    C->Selection={V08Hideout};Advance(EProbeStage::V08HideoutSlot);break;
   }
   case EProbeStage::V08HideoutSlot:
    if(Age<.15)return;if(HUD->CountButtons(TEXT("garrison_info"),FString::FromInt(V08Robber))!=1){if(Age>2)Finish(C,false,TEXT("Hideout garrison panel did not show the publicly boarded robber."));return;}if(!CaptureFrame(TEXT("input-v08-hideout-garrison.png")))return;Record(TEXT("v08.hideout_public_right_click_garrison"),true,TEXT("Viewport right-click boarded the robber, replicated its garrison portrait, and produced no false death event."));Advance(EProbeStage::V08DeathSetup);break;
   case EProbeStage::V08DeathSetup: {
    auto* Unit=World->Spawn(TEXT("industrious_laborer"),C->LocalSlot,FVector2D(62,72));auto* Building=World->Spawn(TEXT("storehouse_of_provisions"),C->LocalSlot,FVector2D(64,84));
    if(!Unit||!Building){Finish(C,false,TEXT("Could not prepare explicit death and destruction fixtures."));return;}
    V08Deaths[0]=Unit->Id;V08Deaths[1]=Building->Id;C->Selection.Empty();World->Snapshot();World->Kill(V08Deaths[0]);World->Kill(V08Deaths[1]);World->Snapshot();Advance(EProbeStage::V08DeathA);break;
   }
   case EProbeStage::V08DeathA: {
    if(Age<.12)return;bool Ready=true;
    for(int32 I=0;I<2;I++){const FString Expected=I?TEXT("destroy"):TEXT("death");FRTSVisualPoseMetrics Pose;const bool Event=C->DeathAnimations.ContainsByPredicate([&](const ARTSController::FDeathAnimation& D){return D.Entity.Id==V08Deaths[I]&&D.Entity.Status==Expected;});if(!Event||!FRTSVisualAssets::Get().GetPoseMetrics(V08Deaths[I],Pose)||Pose.State!=Expected||Pose.Elapsed<.05f)Ready=false;else V08DeathPoses[I]=Pose;}
    if(!Ready){if(Age>1)Finish(C,false,TEXT("Explicit unit/building kills did not render both death and destruction states."));return;}if(!CaptureFrame(TEXT("input-v08-deaths-a.png")))return;Advance(EProbeStage::V08DeathB);break;
   }
   case EProbeStage::V08DeathB: {
    if(Age<.25)return;bool Changed=true;for(int32 I=0;I<2;I++){FRTSVisualPoseMetrics Pose;if(!FRTSVisualAssets::Get().GetPoseMetrics(V08Deaths[I],Pose)||Pose.State!=V08DeathPoses[I].State||Pose.PoseHash==V08DeathPoses[I].PoseHash||Pose.Elapsed<=V08DeathPoses[I].Elapsed)Changed=false;}
    if(!Changed){if(Age>.8)Finish(C,false,TEXT("Death and destruction did not produce two distinct visible animation meshes."));return;}if(!CaptureFrame(TEXT("input-v08-deaths-b.png")))return;Record(TEXT("v08.explicit_unit_building_death_animation_pair"),true,TEXT("Authoritative Kill events rendered separate death/destruction states and two changing mesh poses, captured as a screenshot pair."));Advance(EProbeStage::V08DeathExpiry);break;
   }
   case EProbeStage::V08DeathExpiry:
    if(Age<3.3)return;for(int32 Id:V08Deaths)if(C->DeathAnimations.ContainsByPredicate([&](const ARTSController::FDeathAnimation& D){return D.Entity.Id==Id;})){Finish(C,false,TEXT("Transient death/destruction presentation outlived its expiration."));return;}Record(TEXT("v08.death_animation_expiry"),true,TEXT("Both explicit death and destruction presentations expired after their bounded lifetime."));Advance(EProbeStage::ShotGame);break;
   case EProbeStage::ShotGame:
    if(Age<.35)return;KeyPress(View,EKeys::Escape);Advance(EProbeStage::MenuHotkeys);break;
   case EProbeStage::MenuHotkeys:
    if(Age<.1)return;
    if(!Button(HUD,View,TEXT("hotkeys"),EProbeStage::ReleaseHotkeys)&&Age>2)Finish(C,false,TEXT("Customize Hotkeys was not available from the pause menu."));break;
   case EProbeStage::ReleaseHotkeys:
    Up(View);
    if(!C->bHotkeySettings) {Finish(C,false,TEXT("Viewport click did not open the hotkey overlay."));return;}
    Screenshot(TEXT("input-hotkeys.png"));Advance(EProbeStage::ShotHotkeys);break;
   case EProbeStage::ShotHotkeys:
    if(Age<.35)return;
    Record(TEXT("hotkey_overlay_view"),true,TEXT("Opened and photographed hotkey settings without editing or saving preferences."));
    KeyPress(View,EKeys::Escape);Finish(C,true);break;
   default: break;
   }
  }
 };
}

void RunRTSInputProbe(ARTSController* Controller) {
 if(!Controller||!Controller->IsLocalController()||!Controller->GetWorld()||!FParse::Param(FCommandLine::Get(),TEXT("RTSInputTest")))return;
 static FInputProbe Probe;Probe.Run(Controller);
}
