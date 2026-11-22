#include "RTSController.h"
#include "RTSHUD.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"

class FRTSTouchKeyboardEntry final : public IVirtualKeyboardEntry {
public:
 FRTSTouchKeyboardEntry(ARTSController* InController,const FString& InField)
  : Controller(InController), Field(InField) {}

 void SetTextFromVirtualKeyboard(const FText& NewText,ETextEntryType EntryType) override {
  ARTSController* Owner=Controller.Get();
  if(!Owner||Owner->TouchKeyboard.Get()!=this||Owner->TextFocus!=Field)return;
  if(EntryType!=ETextEntryType::TextEntryCanceled) {
   if(FString* Text=Owner->FocusedText()) {
    Text->Reset();
    Owner->TextPaste(NewText.ToString());
   }
  }
  if(EntryType==ETextEntryType::TextEntryUpdated)return;

  const FString CommittedField=Field;
  Owner->TouchKeyboard.Reset();
  if(EntryType==ETextEntryType::TextEntryAccepted&&CommittedField!=TEXT("join"))Owner->SubmitText();
  else {
   if(EntryType==ETextEntryType::TextEntryCanceled&&CommittedField==TEXT("name"))Owner->PlayerName=Owner->ConfirmedPlayerName;
   Owner->TextFocus.Empty();Owner->bChat=false;
  }
 }

 FText GetText() const override {
  if(ARTSController* Owner=Controller.Get())if(const FString* Text=Owner->FocusedText())return FText::FromString(*Text);
  return FText::GetEmpty();
 }
 FText GetHintText() const override {
  if(Field==TEXT("join"))return NSLOCTEXT("Chronicles","ServerAddress","Address shown in the host's lobby");
  if(Field==TEXT("name"))return NSLOCTEXT("Chronicles","PlayerName","Player name");
  return NSLOCTEXT("Chronicles","ChatMessage","Chat message");
 }
 EKeyboardType GetVirtualKeyboardType() const override {return Field==TEXT("join")?Keyboard_Web:Keyboard_Default;}
 FVirtualKeyboardOptions GetVirtualKeyboardOptions() const override {return FVirtualKeyboardOptions();}
 bool IsMultilineEntry() const override {return false;}
 bool GetSelection(int& OutStart,int& OutEnd) override {return false;}
 void SetSelectionFromVirtualKeyboard(int Start,int End) override {}

private:
 TWeakObjectPtr<ARTSController> Controller;
 FString Field;
};

namespace {
 bool IsNativeTouchBuild() {
  return PLATFORM_ANDROID||PLATFORM_IOS;
 }
}

void ARTSController::CreateTouchInterface() {
 // The engine's optional virtual joysticks intercept battlefield touches.
 if(!bTouchControls)Super::CreateTouchInterface();
}

void ARTSController::BeginTouchInput() {
 bTouchPreview=!IsNativeTouchBuild()&&FParse::Param(FCommandLine::Get(),TEXT("RTSTouchPreview"));
 bTouchControls=bTouchControls||bTouchPreview;
 if(!bTouchControls)return;

 bEnableTouchEvents=false;
 bEnableTouchOverEvents=false;
 bShowMouseCursor=bTouchPreview;
 ActivateTouchInterface(nullptr);
 TouchDeactivateHandle=FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this,&ARTSController::CancelTouchInput);
 TouchBackgroundHandle=FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this,&ARTSController::CancelTouchInput);
 if(FSlateApplication::IsInitialized()) {
  FSlateApplication& Slate=FSlateApplication::Get();
  TouchActivationHandle=Slate.OnApplicationActivationStateChanged().AddUObject(this,&ARTSController::TouchActivationChanged);
  if(bTouchPreview)Slate.SetGameIsFakingTouchEvents(true);
 }
}

void ARTSController::EndTouchInput() {
 CancelTouchInput();
 CloseTouchKeyboard();
 if(!bTouchControls)return;
 FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(TouchDeactivateHandle);
 FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(TouchBackgroundHandle);
 if(FSlateApplication::IsInitialized()) {
  FSlateApplication& Slate=FSlateApplication::Get();
  Slate.OnApplicationActivationStateChanged().Remove(TouchActivationHandle);
  if(bTouchPreview)Slate.SetGameIsFakingTouchEvents(false);
 }
}

void ARTSController::FlushPressedKeys() {
 Super::FlushPressedKeys();
 CancelTouchInput();
}

void ARTSController::TouchActivationChanged(bool bActive) {
 if(!bActive)CancelTouchInput();
}

void ARTSController::CancelTouchInput() {
 if(!bTouchControls)return;
 TouchContacts.Reset();
 TouchRelease.Reset();
 TouchInteraction=ETouchInteraction::Idle;
 TouchButtonAction.Reset();
 TouchButtonValue.Reset();
 bDragging=false;
}

bool ARTSController::InputTouch(uint32 Handle,ETouchType::Type Type,const FVector2D& Pixel,float Force,FDateTime DeviceTimestamp,uint32 TouchpadIndex) {
 if(!bTouchControls||!IsLocalController())return Super::InputTouch(Handle,Type,Pixel,Force,DeviceTimestamp,TouchpadIndex);
 if(TouchpadIndex!=0||Pixel.ContainsNaN())return true;
 switch(Type) {
  case ETouchType::Began: BeginTouch(Handle,Pixel);break;
  case ETouchType::FirstMove:
  case ETouchType::Moved: MoveTouch(Handle,Pixel);break;
  case ETouchType::Ended: EndTouch(Handle,Pixel);break;
  default: break;
 }
 return true;
}

double ARTSController::TouchSlop() const {
 return FMath::Clamp(FMath::Min(ViewSize.X,ViewSize.Y)*.018,8.,32.);
}

bool ARTSController::CanInteractWithWorld(FVector2D Pixel) const {
 if(bChat||!InWorld(Pixel))return false;
 const ARTSHUD* HUD=Cast<ARTSHUD>(GetHUD());
 return !HUD||!HUD->BlocksWorld(Pixel);
}

void ARTSController::BeginTouch(uint32 Handle,FVector2D Pixel) {
 if(TouchContacts.Contains(Handle))return;
 const bool bFirst=TouchContacts.IsEmpty();
 TouchContacts.Add(Handle,Pixel);
 if(!bFirst) {
  if(TouchInteraction==ETouchInteraction::Transform)return;
  bDragging=false;
  const bool bWorldGesture=TouchInteraction==ETouchInteraction::WorldTap||TouchInteraction==ETouchInteraction::Pan||TouchInteraction==ETouchInteraction::Box;
  TouchInteraction=CanInteractWithWorld(Pixel)&&bWorldGesture?ETouchInteraction::Transform:ETouchInteraction::Consumed;
  SecondaryTouch=Handle;
  return;
 }

 Mouse=TouchStart=Pixel;
 PrimaryTouch=Handle;
 TouchButtonAction.Reset();
 TouchButtonValue.Reset();
 bDragging=false;
 const ARTSHUD* HUD=Cast<ARTSHUD>(GetHUD());
 FVector2D MinimapWorld;
 if(HUD&&HUD->HitButton(Pixel,TouchButtonAction,TouchButtonValue))TouchInteraction=ETouchInteraction::Button;
 else if(HUD&&Phase==1&&!bMenu&&!bPauseMenu&&!bHelp&&!bChat&&!bHotkeySettings&&HUD->MinimapPoint(Pixel,MinimapWorld))TouchInteraction=ETouchInteraction::Minimap;
 else TouchInteraction=CanInteractWithWorld(Pixel)?ETouchInteraction::WorldTap:ETouchInteraction::Consumed;
}

void ARTSController::MoveTouch(uint32 Handle,FVector2D Pixel) {
 FVector2D* Contact=TouchContacts.Find(Handle);
 if(!Contact)return;
 const FVector2D Previous=*Contact;
 *Contact=Pixel;

 if(TouchInteraction==ETouchInteraction::Transform) {
  if(Handle!=PrimaryTouch&&Handle!=SecondaryTouch)return;
  const uint32 OtherHandle=Handle==PrimaryTouch?SecondaryTouch:PrimaryTouch;
  const FVector2D Other=TouchContacts.FindChecked(OtherHandle);
  const FVector2D PreviousSpan=Previous-Other,NewSpan=Pixel-Other;
  const double PreviousDistance=PreviousSpan.Size(),NewDistance=NewSpan.Size();
  const double MinimumSpan=TouchSlop()*2.;
  const bool bValidSpan=PreviousDistance>=MinimumSpan&&NewDistance>=MinimumSpan;
  const double Scale=bValidSpan?NewDistance/PreviousDistance:1.;
  const double Twist=bValidSpan?FMath::Atan2(PreviousSpan^NewSpan,PreviousSpan|NewSpan):0.;
  Mouse=(Pixel+Other)*.5;
  TransformCamera((Previous+Other)*.5,Mouse,Scale,Twist,true);
  return;
 }

 if(Handle!=PrimaryTouch)return;
 Mouse=Pixel;
 if(TouchInteraction==ETouchInteraction::Pan) {
  TransformCamera(Previous,Pixel,1.,0.,true);
  return;
 }
 if(FVector2D::DistSquared(TouchStart,Pixel)<=FMath::Square(TouchSlop()))return;

 if(TouchInteraction==ETouchInteraction::WorldTap) {
  if(bTouchBoxSelect&&PendingAction.IsEmpty()) {
   TouchInteraction=ETouchInteraction::Box;
   DragStart=TouchStart;
   bDragging=true;
  } else {
   TouchInteraction=ETouchInteraction::Pan;
   TransformCamera(TouchStart,Pixel,1.,0.,true);
  }
 } else if(TouchInteraction==ETouchInteraction::Button||TouchInteraction==ETouchInteraction::Minimap) {
  TouchInteraction=ETouchInteraction::Consumed;
 }
}

void ARTSController::EndTouch(uint32 Handle,FVector2D Pixel) {
 if(!TouchContacts.Contains(Handle))return;
 MoveTouch(Handle,Pixel);
 TouchContacts.Remove(Handle);
 if(TouchInteraction==ETouchInteraction::Transform&&(Handle==PrimaryTouch||Handle==SecondaryTouch))TouchInteraction=ETouchInteraction::Consumed;
 if(!TouchContacts.IsEmpty())return;
 if(TouchInteraction==ETouchInteraction::Button||TouchInteraction==ETouchInteraction::WorldTap||
    TouchInteraction==ETouchInteraction::Box||TouchInteraction==ETouchInteraction::Minimap) {
  // Process after Slate has had a frame to deliver any accompanying focus loss.
  TouchRelease=FTouchRelease{TouchInteraction,TouchStart,Pixel,TouchButtonAction,TouchButtonValue,GFrameCounter};
 }
 TouchInteraction=ETouchInteraction::Idle;
 bDragging=false;
}

void ARTSController::DispatchTouchRelease() {
 if(!TouchRelease.IsSet()||TouchRelease->Frame==GFrameCounter)return;
 const FTouchRelease Tap=MoveTemp(TouchRelease.GetValue());
 TouchRelease.Reset();
 ARTSHUD* HUD=Cast<ARTSHUD>(GetHUD());
 switch(Tap.Interaction) {
  case ETouchInteraction::Button: {
   FString Action,Value;
   if(HUD&&HUD->HitButton(Tap.Pixel,Action,Value)&&Action==Tap.Action&&Value==Tap.Value)HUD->Click(Tap.Pixel);
   break;
  }
  case ETouchInteraction::Minimap: {
   FVector2D World;
   if(HUD&&Phase==1&&!bMenu&&!bPauseMenu&&!bHelp&&!bChat&&!bHotkeySettings&&bLocalLiahona&&HUD->MinimapPoint(Tap.Pixel,World)) {
    if(bTouchOrderMode&&PendingAction.IsEmpty())Issue(TEXT("context"),World);
    else CameraCenter=World;
   }
   break;
  }
  case ETouchInteraction::Box:
   SelectBox(Tap.Start,Tap.Pixel,false);
   break;
  case ETouchInteraction::WorldTap:
   if(CanInteractWithWorld(Tap.Pixel)) {
    CloseTouchKeyboard();
    TextFocus.Empty();
    if(!PendingAction.IsEmpty()) {
     const bool bRoute=PendingAction==TEXT("galavant")||PendingAction==TEXT("market_galavant");
     PlacePendingAction(Tap.Pixel,bRoute&&bTouchRouteStarted);
     bTouchRouteStarted=bRoute;
    } else if(bTouchOrderMode)IssueContextAt(Tap.Pixel,false);
    else SelectAt(Tap.Pixel,false,true);
   }
   break;
  default: break;
 }
}

int32 ARTSController::TouchEntityUnder(FVector2D Pixel) const {
 if(const int32 Hit=EntityUnder(Pixel))return Hit;
 int32 Closest=0;
 double ClosestDistance=FMath::Square(TouchSlop()*.5);
 for(const FRTSEntityView& Entity:Entities) {
  if(Entity.bRemembered||Entity.HP<=0)continue;
  const FBox2D Bounds=EntityBounds(Entity);
  if(!Bounds.bIsValid)continue;
  const FVector2D Nearest(FMath::Clamp(Pixel.X,Bounds.Min.X,Bounds.Max.X),FMath::Clamp(Pixel.Y,Bounds.Min.Y,Bounds.Max.Y));
  const double Distance=FVector2D::DistSquared(Pixel,Nearest);
  if(Distance<ClosestDistance) {Closest=Entity.Id;ClosestDistance=Distance;}
 }
 return Closest;
}

void ARTSController::TransformCamera(FVector2D Before,FVector2D After,double ZoomScale,double RotationDelta,bool bClampCenter) {
 const FVector2D WorldAnchor=ScreenToWorld(Before);
 Zoom=FMath::Clamp(Zoom*ZoomScale,.55,4.);
 CameraRotation=FMath::UnwindRadians(CameraRotation+RotationDelta);
 CameraCenter+=WorldAnchor-ScreenToWorld(After);
 if(bClampCenter) {
  CameraCenter.X=FMath::Clamp(CameraCenter.X,0.,double(RTS::MapSize));
  CameraCenter.Y=FMath::Clamp(CameraCenter.Y,0.,double(RTS::MapSize));
 }
}

FString* ARTSController::FocusedText() {
 if(bChat||TextFocus==TEXT("chat"))return &ChatDraft;
 if(TextFocus==TEXT("join"))return &JoinAddress;
 if(TextFocus==TEXT("name"))return &PlayerName;
 return nullptr;
}

void ARTSController::FocusTextEntry(const FString& Field) {
 CloseTouchKeyboard();
 TextFocus=Field;
 if(Field==TEXT("chat"))bChat=!IsNativeTouchBuild();
 if(!bTouchControls||!IsNativeTouchBuild()||!FSlateApplication::IsInitialized()||!FocusedText())return;

#if PLATFORM_ANDROID
 // The native dialog starts with the field text and delivers the full accepted
 // string, including paste and IME edits, to the Canvas field.
 if(IConsoleVariable* AndroidKeyboard=IConsoleManager::Get().FindConsoleVariable(TEXT("Android.NewKeyboard")))AndroidKeyboard->Set(2,ECVF_SetByGameOverride);
#endif
 TouchKeyboard=MakeShared<FRTSTouchKeyboardEntry>(this,Field);
 FSlateApplication::Get().ShowVirtualKeyboard(true,0,TouchKeyboard);
}

void ARTSController::CloseTouchKeyboard() {
 if(!TouchKeyboard.IsValid())return;
 // Unreal's platform keyboard keeps a weak reference to its current entry.
 if(FSlateApplication::IsInitialized())FSlateApplication::Get().ShowVirtualKeyboard(false,0);
 TouchKeyboard.Reset();
}
