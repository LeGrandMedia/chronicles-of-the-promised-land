#include "RTSTouchTests.h"
#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSHUD.h"
#include "RTSWorld.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/SceneViewport.h"

namespace {
enum class ETouchProbeStage : uint8 {
 Menu, DragCanceled, MultitouchCanceled, FocusCanceled, Lobby,
 Ready, Match, Selected, Orders, PanCheck, Movement, PatrolArmed,
 PatrolDone, BoxMode, BoxComplete, CameraTransform, Finished
};

struct FTouchProbe {
 ETouchProbeStage Stage=ETouchProbeStage::Menu;
 double Started=FPlatformTime::Seconds();
 TArray<TSharedPtr<FJsonValue>> Cases;
 FVector2D SoloButton,WorkerPixel,Ground,OrderDestination,BoxStart,BoxEnd;
 int32 WorkerId=0;
 float StartingZoom=0,StartingRotation=0;
 FVector2D StartingCenter;
 TArray<int32> BeforeBox;
 TArray<int32> BoxWorkers;

 void Finish(bool bPass) {
  Stage=ETouchProbeStage::Finished;
  TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();
  Report->SetBoolField(TEXT("passed"),bPass);
  Report->SetStringField(TEXT("route"),TEXT("FSceneViewport touch handlers to UGameViewportClient to ARTSController"));
  Report->SetArrayField(TEXT("cases"),Cases);
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Tests");
  IFileManager::Get().MakeDirectory(*Directory,true);
  FString Json;
  FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  const bool bSaved=FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("v09-touch-tests.json")));
  const bool bSuccess=bPass&&bSaved;
  UE_LOG(LogTemp,Display,TEXT("RTS_V09_TOUCH_TESTS passed=%d cases=%d"),bSuccess,Cases.Num());
  FPlatformMisc::RequestExitWithStatus(!bSuccess,bSuccess?0:1);
 }

 bool Check(const TCHAR* Name,bool bPassed,const FString& Detail=FString()) {
  TSharedRef<FJsonObject> Case=MakeShared<FJsonObject>();
  Case->SetStringField(TEXT("name"),Name);
  Case->SetBoolField(TEXT("passed"),bPassed);
  Case->SetStringField(TEXT("detail"),Detail);
  Cases.Add(MakeShared<FJsonValueObject>(Case));
  UE_LOG(LogTemp,Display,TEXT("RTS_V09_TOUCH_%s %s %s"),bPassed?TEXT("PASS"):TEXT("FAIL"),Name,*Detail);
  if(!bPassed)Finish(false);
  return bPassed;
 }

 void Touch(FSceneViewport* View,ETouchType::Type Event,uint32 Finger,FVector2D Pixel) {
  const FGeometry Geometry=View->GetCachedGeometry();
  const double Scale=Geometry.GetAccumulatedLayoutTransform().GetScale();
  // Keep the synthetic contact inside the intended physical pixel when
  // Slate converts between screen space and viewport coordinates.
  const FVector2D Position=Geometry.LocalToAbsolute((Pixel+FVector2D(.1,.1))/Scale);
  const bool bPressed=Event!=ETouchType::Ended;
  const FPointerEvent Pointer(uint32(0),Finger,Position,Position,bPressed?1.f:0.f,bPressed);
  if(Event==ETouchType::Began)View->OnTouchStarted(Geometry,Pointer);
  else if(Event==ETouchType::Moved)View->OnTouchMoved(Geometry,Pointer);
  else View->OnTouchEnded(Geometry,Pointer);
 }

 void Tap(FSceneViewport* View,FVector2D Pixel) {
  Touch(View,ETouchType::Began,0,Pixel);
  Touch(View,ETouchType::Ended,0,Pixel);
 }

 bool Button(const ARTSHUD& HUD,const TCHAR* Action,FVector2D& Pixel,const TCHAR* Value=TEXT("")) const {
  FBox2D Bounds;
  if(!HUD.FindButtonBounds(Action,Value,Bounds))return false;
  Pixel=Bounds.GetCenter().RoundToVector();
  return true;
 }

 bool FindWorker(const ARTSController& C,const ARTSHUD& HUD) {
  for(const FRTSEntityView& Entity:C.Entities) {
   if(Entity.Owner!=C.LocalSlot||!RTS::IsWorker(FRTSCatalog::Get().Def(Entity.Def)))continue;
   const FBox2D Bounds=C.EntityBounds(Entity);
   if(!Bounds.bIsValid)continue;
   const FVector2D Center=Bounds.GetCenter().RoundToVector();
   for(int32 Y=-3;Y<=3;++Y) {
    for(int32 X=-3;X<=3;++X) {
     const FVector2D Pixel=Center+FVector2D(X*2,Y*2);
     if(HUD.BlocksWorld(Pixel)||C.EntityUnder(Pixel)!=Entity.Id)continue;
     WorkerId=Entity.Id;
     WorkerPixel=Pixel;
     return true;
    }
   }
  }
  return false;
 }

 bool FindGround(const ARTSController& C,const ARTSHUD& HUD,const ARTSWorld& World,const FRTSEntity& Worker) {
  for(int32 Y=-16;Y<=16;Y+=4) {
   for(int32 X=-16;X<=16;X+=4) {
    const FVector2D Pixel=C.WorldToScreen(Worker.Pos+FVector2D(X,Y)).RoundToVector();
    const FVector2D Point=C.ScreenToWorld(Pixel);
    if(HUD.BlocksWorld(Pixel)||C.EntityUnder(Pixel,true)||FVector2D::DistSquared(Point,Worker.Pos)<64)continue;
    if(!World.Walkable(Point,World.D(Worker).Radius,Worker.Id))continue;
    Ground=Pixel;
    OrderDestination=Point;
    return true;
   }
  }
  return false;
 }

 bool FindBox(const ARTSController& C,const ARTSHUD& HUD) {
  FBox2D Area(EForceInit::ForceInit);
  for(const FRTSEntityView& Entity:C.Entities) {
   if(Entity.Owner!=C.LocalSlot||!RTS::IsWorker(FRTSCatalog::Get().Def(Entity.Def)))continue;
   const FBox2D Bounds=C.EntityBounds(Entity);
   if(!Bounds.bIsValid||HUD.BlocksWorld(Bounds.GetCenter()))continue;
   Area+=Bounds.Min;
   Area+=Bounds.Max;
   BoxWorkers.Add(Entity.Id);
  }
  if(BoxWorkers.Num()<2)return false;
  BoxStart=(Area.Min-FVector2D(8,8)).RoundToVector();
  BoxEnd=(Area.Max+FVector2D(8,8)).RoundToVector();
  return !HUD.BlocksWorld(BoxStart)&&!HUD.BlocksWorld(BoxEnd);
 }

 void Tick(ARTSController& C) {
  if(Stage==ETouchProbeStage::Finished)return;
  if(FPlatformTime::Seconds()-Started>35.) {
   Check(TEXT("probe_timeout"),false,FString::Printf(TEXT("stage=%d"),int32(Stage)));
   return;
  }
  ARTSHUD* HUD=Cast<ARTSHUD>(C.GetHUD());
  UGameViewportClient* Viewport=C.GetWorld()->GetGameViewport();
  FSceneViewport* View=Viewport?Viewport->GetGameViewport():nullptr;
  if(!HUD||!View||View->GetSizeXY()==FIntPoint::ZeroValue||!C.Me())return;
  const ARTSGameMode* Mode=C.GetWorld()->GetAuthGameMode<ARTSGameMode>();
  ARTSWorld* World=Mode?Mode->Simulation:nullptr;
  if(!World||!World->Players.IsValidIndex(C.LocalSlot))return;
  FVector2D Pixel;

  switch(Stage) {
   case ETouchProbeStage::Menu:
    if(!Button(*HUD,TEXT("solo"),SoloButton))return;
    if(!Check(TEXT("touch_layout_enabled"),C.bTouchControls))return;
    Touch(View,ETouchType::Began,0,SoloButton);
    Touch(View,ETouchType::Moved,0,SoloButton+FVector2D(80,0));
    Touch(View,ETouchType::Ended,0,SoloButton+FVector2D(80,0));
    Stage=ETouchProbeStage::DragCanceled;
    break;
   case ETouchProbeStage::DragCanceled:
    if(!Check(TEXT("drag_off_button_does_not_activate"),C.bMenu))return;
    Touch(View,ETouchType::Began,0,SoloButton);
    Touch(View,ETouchType::Began,1,SoloButton+FVector2D(5,5));
    Touch(View,ETouchType::Ended,1,SoloButton+FVector2D(5,5));
    Touch(View,ETouchType::Ended,0,SoloButton);
    Stage=ETouchProbeStage::MultitouchCanceled;
    break;
   case ETouchProbeStage::MultitouchCanceled:
    if(!Check(TEXT("second_finger_cancels_button"),C.bMenu))return;
    Tap(View,SoloButton);
    C.FlushPressedKeys();
    Stage=ETouchProbeStage::FocusCanceled;
    break;
   case ETouchProbeStage::FocusCanceled:
    if(!Check(TEXT("focus_loss_cancels_buffered_release"),C.bMenu))return;
    Touch(View,ETouchType::Began,0,SoloButton);
    if(!Check(TEXT("button_activates_on_release"),C.bMenu))return;
    Touch(View,ETouchType::Ended,0,SoloButton);
    Stage=ETouchProbeStage::Lobby;
    break;
   case ETouchProbeStage::Lobby:
    if(!Button(*HUD,TEXT("ready"),Pixel))return;
    if(!Check(TEXT("solo_touch_opens_lobby"),!C.bMenu))return;
    Tap(View,Pixel);
    C.InputKey(FInputKeyParams(EKeys::LeftMouseButton,IE_Pressed,1.));
    C.InputKey(FInputKeyParams(EKeys::LeftMouseButton,IE_Released,0.));
    Stage=ETouchProbeStage::Ready;
    break;
   case ETouchProbeStage::Ready:
    if(!World->Players[C.LocalSlot].bReady){Check(TEXT("ready_tap_ignores_synthesized_mouse_click"),false);return;}
    if(!Button(*HUD,TEXT("start"),Pixel))return;
    Check(TEXT("ready_tap_ignores_synthesized_mouse_click"),true);
    Tap(View,Pixel);
    Stage=ETouchProbeStage::Match;
    break;
   case ETouchProbeStage::Match:
    if(C.Phase!=1||World->Phase!=1||!FindWorker(C,*HUD))return;
    if(!Check(TEXT("touch_starts_authoritative_match_and_default_zoom"),FMath::IsNearlyEqual(C.Zoom,1.65f)))return;
    Tap(View,WorkerPixel);
    Stage=ETouchProbeStage::Selected;
    break;
   case ETouchProbeStage::Selected:
    if(!Check(TEXT("selects_visible_worker"),C.Selection.Num()==1&&C.Selection[0]==WorkerId))return;
    if(!Button(*HUD,TEXT("touch_order"),Pixel)){Check(TEXT("orders_button_visible"),false);return;}
    Tap(View,Pixel);
    Stage=ETouchProbeStage::Orders;
    break;
   case ETouchProbeStage::Orders: {
    if(!Check(TEXT("orders_button_arms_context"),C.bTouchOrderMode&&!C.bTouchBoxSelect))return;
    const FVector2D From=(C.ViewSize*FVector2D(.54,.41)).RoundToVector();
    const FVector2D To=From+FVector2D(48,24);
    const FVector2D Anchor=C.ScreenToWorld(From);
    StartingCenter=C.CameraCenter;
    Touch(View,ETouchType::Began,0,From);
    Touch(View,ETouchType::Moved,0,To);
    Touch(View,ETouchType::Ended,0,To);
    const double Error=FVector2D::Distance(C.ScreenToWorld(To),Anchor);
    if(!Check(TEXT("one_finger_pan_anchors_world_in_orders_mode"),Error<.05&&!C.CameraCenter.Equals(StartingCenter,.5)&&C.Selection.Contains(WorkerId),FString::Printf(TEXT("anchor_drift=%.5f"),Error)))return;
    Stage=ETouchProbeStage::PanCheck;
    break;
   }
   case ETouchProbeStage::PanCheck: {
    const FRTSEntity* Worker=World->Find(WorkerId);
    if(!Check(TEXT("orders_ground_target_available"),Worker&&FindGround(C,*HUD,*World,*Worker)))return;
    Tap(View,Ground);
    Stage=ETouchProbeStage::Movement;
    break;
   }
   case ETouchProbeStage::Movement: {
    const FRTSEntity* Worker=World->Find(WorkerId);
    if(!Check(TEXT("orders_tap_dispatches_authoritative_move"),Worker&&Worker->Order==TEXT("move")&&Worker->Destination.Equals(OrderDestination,.1)))return;
    if(!Check(TEXT("patrol_target_card_visible"),Button(*HUD,TEXT("order"),Pixel,TEXT("patrol"))))return;
    Tap(View,Pixel);
    Stage=ETouchProbeStage::PatrolArmed;
    break;
   }
   case ETouchProbeStage::PatrolArmed: {
    if(!Check(TEXT("patrol_card_arms_over_orders"),C.PendingAction==TEXT("patrol")&&C.bTouchOrderMode))return;
    if(!Check(TEXT("select_button_visible"),Button(*HUD,TEXT("touch_select"),Pixel)))return;
    Touch(View,ETouchType::Began,0,Ground);
    Touch(View,ETouchType::Moved,0,Pixel);
    Touch(View,ETouchType::Ended,0,Pixel);
    if(!Check(TEXT("battlefield_drag_cannot_dispatch_hud_button"),C.bTouchOrderMode&&C.PendingAction==TEXT("patrol")))return;
    const FRTSEntity* Worker=World->Find(WorkerId);
    C.CameraCenter=StartingCenter;
    if(!Check(TEXT("patrol_ground_target_available"),Worker&&FindGround(C,*HUD,*World,*Worker)))return;
    Tap(View,Ground);
    Stage=ETouchProbeStage::PatrolDone;
    break;
   }
   case ETouchProbeStage::PatrolDone: {
    const FRTSEntity* Worker=World->Find(WorkerId);
    if(!Check(TEXT("targeted_card_overrides_context_order"),Worker&&Worker->Order==TEXT("patrol")&&C.PendingAction.IsEmpty()))return;
    if(!Check(TEXT("box_button_visible"),Button(*HUD,TEXT("touch_box"),Pixel)))return;
    Tap(View,Pixel);
    Stage=ETouchProbeStage::BoxMode;
    break;
   }
   case ETouchProbeStage::BoxMode:
    if(!Check(TEXT("box_mode_arms_owned_group_selection"),C.bTouchBoxSelect&&!C.bTouchOrderMode&&FindBox(C,*HUD)))return;
    BeforeBox=C.Selection;
    Touch(View,ETouchType::Began,0,BoxStart);
    Touch(View,ETouchType::Moved,0,BoxEnd);
    if(!Check(TEXT("box_drag_has_live_rectangle"),C.bDragging&&C.DragStart.Equals(BoxStart,.01)))return;
    C.FlushPressedKeys();
    Touch(View,ETouchType::Ended,0,BoxEnd);
    if(!Check(TEXT("focus_flush_cancels_box_drag"),!C.bDragging&&C.Selection==BeforeBox))return;
    Touch(View,ETouchType::Began,0,BoxStart);
    Touch(View,ETouchType::Moved,0,BoxEnd);
    Touch(View,ETouchType::Ended,0,BoxEnd);
    Stage=ETouchProbeStage::BoxComplete;
    break;
   case ETouchProbeStage::BoxComplete: {
    bool bAllWorkers=true;
    for(int32 Id:BoxWorkers)bAllWorkers&=C.Selection.Contains(Id);
    for(int32 Id:C.Selection) {
     const FRTSEntityView* Entity=C.FindEntity(Id);
     bAllWorkers&=Entity&&Entity->Owner==C.LocalSlot;
    }
    if(!Check(TEXT("box_release_selects_owned_workers"),bAllWorkers&&!C.bDragging))return;
    BeforeBox=C.Selection;
    StartingZoom=C.Zoom;
    StartingRotation=C.CameraRotation;
    const FVector2D Center=(C.ViewSize*FVector2D(.54,.43)).RoundToVector();
    const FVector2D First=Center-FVector2D(52,0),Second=Center+FVector2D(52,0);
    const FVector2D NewFirst=First-FVector2D(18,18),NewSecond=Second+FVector2D(46,30);
    const FVector2D Anchor=C.ScreenToWorld(Center);
    Touch(View,ETouchType::Began,0,First);
    Touch(View,ETouchType::Began,1,Second);
    Touch(View,ETouchType::Moved,0,NewFirst);
    Touch(View,ETouchType::Moved,1,NewSecond);
    const double AnchorError=FVector2D::Distance(C.ScreenToWorld((NewFirst+NewSecond)*.5),Anchor);
    if(!Check(TEXT("two_fingers_pan_pinch_and_twist"),C.Zoom>StartingZoom*1.3f&&C.CameraRotation>StartingRotation+.15f&&AnchorError<.05,
              FString::Printf(TEXT("zoom=%.4f->%.4f rotation=%.4f->%.4f anchor_drift=%.5f"),StartingZoom,C.Zoom,StartingRotation,C.CameraRotation,AnchorError)))return;
    Touch(View,ETouchType::Ended,0,NewFirst);
    StartingCenter=C.CameraCenter;
    Touch(View,ETouchType::Moved,1,NewSecond+FVector2D(49,37));
    Touch(View,ETouchType::Ended,1,NewSecond+FVector2D(49,37));
    Stage=ETouchProbeStage::CameraTransform;
    break;
   }
   case ETouchProbeStage::CameraTransform:
    if(!Check(TEXT("remaining_finger_after_pinch_never_taps_or_drags"),C.CameraCenter.Equals(StartingCenter,.001)&&C.Selection==BeforeBox&&!C.bDragging))return;
    Finish(true);
    break;
   case ETouchProbeStage::Finished: break;
  }
 }
};
}

void RunRTSTouchProbe(ARTSController* Controller) {
 static FTouchProbe Probe;
 Probe.Tick(*Controller);
}
