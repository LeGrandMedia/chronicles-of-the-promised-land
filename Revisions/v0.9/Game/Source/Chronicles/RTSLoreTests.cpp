#include "RTSHUD.h"

#if !UE_BUILD_SHIPPING

#include "RTSController.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

void ARTSHUD::RunLoreTest() {
 static const bool bEnabled=FParse::Param(FCommandLine::Get(),TEXT("RTSLoreTest"));
 if(!bEnabled||!PC->IsLocalController())return;
 enum class EStage : uint8 {
  MenuDefaults, OpenJoin, EmptyJoin, TypeAddress, FilledJoin, ReopenJoin, BackFromJoin,
  OpenPlates, ReadPlates, CapturePlates, ClosePlates, OpenVessels, LightVessels, CaptureVessels,
  CloseVessels, ReopenVessels, VerifyVessels, OpenSkirmish, CaptureLobby, CheckLobbyDraft, OpenHelp, CaptureHelp, CloseHelp, Done
 };
 static EStage Stage=EStage::MenuDefaults;
 static int32 Vessel=0;
 static FString LobbyFactionBeforeEntry;
 static int32 LobbyTeamBeforeEntry=0;
 static bool bReadyBeforeEntry=false;
 static const double Started=FPlatformTime::Seconds();
 if(Stage==EStage::Done)return;
 const auto Finish=[&](bool bPass,const TCHAR* Detail) {
  if(bPass) {
   UE_LOG(LogTemp,Display,TEXT("RTS_LORE_TEST PASS %s"),Detail);
  } else {
   UE_LOG(LogTemp,Error,TEXT("RTS_LORE_TEST FAIL stage=%d %s"),int32(Stage),Detail);
  }
  Stage=EStage::Done;
  FPlatformMisc::RequestExitWithStatus(false,bPass?0:1);
 };
 if(FPlatformTime::Seconds()-Started>20) {
  Finish(false,TEXT("Timed out waiting for the menu or a lobby snapshot."));
  return;
 }
 const auto Tap=[&](const FString& Action,const FString& Value=FString()) {
  FBox2D Bounds;
  FString HitAction,HitValue;
  return FindButtonBounds(Action,Value,Bounds)&&HitButton(Bounds.GetCenter(),HitAction,HitValue)&&
   HitAction==Action&&HitValue==Value&&Click(Bounds.GetCenter());
 };
 const auto Capture=[](const TCHAR* Name) {
  const FString Folder=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots"));
  IFileManager::Get().MakeDirectory(*Folder,true);
  FScreenshotRequest::RequestScreenshot(Folder/Name,false,false);
 };
 switch(Stage) {
 case EStage::MenuDefaults: {
  if(!PC->Me())return;
  FString SavedName;
  if(GConfig)GConfig->GetString(TEXT("Chronicles.Player"),TEXT("DisplayName"),SavedName,GGameUserSettingsIni);
  SavedName=SavedName.TrimStartAndEnd().Left(24);
  FBox2D Practice,Skirmish;
  if(!PC->bMenu||!PC->JoinAddress.IsEmpty()||PC->PlayerName!=(SavedName.IsEmpty()?TEXT("Commander"):SavedName)||
     !FindButtonBounds(TEXT("solo"),FString(),Practice)||!FindButtonBounds(TEXT("skirmish"),FString(),Skirmish)||Practice.Min.Y>=Skirmish.Min.Y) {
   Finish(false,TEXT("Fresh menu did not offer Practice first, a neutral or saved name, and an empty join address."));
   return;
  }
  Capture(TEXT("beginner-menu-v09.png"));
  Stage=EStage::OpenJoin;
  break;
 }
 case EStage::OpenJoin:
  if(!Tap(TEXT("join_screen"))||PC->MenuScreen!=1) {
   Finish(false,TEXT("Join LAN did not open from the main menu."));
   return;
  }
  Stage=EStage::EmptyJoin;
  break;
 case EStage::EmptyJoin: {
  const auto* Join=Buttons.FindByPredicate([](const FRTSButton& B){return B.Action==TEXT("join");});
  const bool bBlocked=Join&&!Join->Enabled&&Click(Join->Bounds.GetCenter())&&!PC->bConnecting&&PC->bMenu;
  PC->UIAction(TEXT("join"));
  if(!bBlocked||PC->HasJoinAddress()||PC->bConnecting||!PC->bMenu||!PC->Notice.Contains(TEXT("host's lobby"))) {
   Finish(false,TEXT("An empty address did not block joining in the menu and controller."));
   return;
  }
  Capture(TEXT("beginner-join-lan-v09.png"));
  Stage=EStage::TypeAddress;
  break;
 }
 case EStage::TypeAddress:
  if(!Tap(TEXT("focus"),TEXT("join"))) {
   Finish(false,TEXT("The address field could not receive input."));
   return;
  }
  PC->TextPaste(TEXT("192.168.1.15:7777"));
  if(PC->JoinAddress!=TEXT("192.168.1.15:7777")) {
   Finish(false,TEXT("Entering a host address did not start with an empty field."));
   return;
  }
  Stage=EStage::FilledJoin;
  break;
 case EStage::FilledJoin: {
  FBox2D Join;
  if(!FindButtonBounds(TEXT("join"),FString(),Join)||!Tap(TEXT("back"))) {
   Finish(false,TEXT("The typed address did not enable Join or Back did not return to the menu."));
   return;
  }
  Stage=EStage::ReopenJoin;
  break;
 }
 case EStage::ReopenJoin:
  if(!Tap(TEXT("join_screen"))||PC->JoinAddress!=TEXT("192.168.1.15:7777")) {
   Finish(false,TEXT("An entered address did not survive closing and reopening the join screen."));
   return;
  }
  Stage=EStage::BackFromJoin;
  break;
 case EStage::BackFromJoin:
  if(!Tap(TEXT("back"))) {
   Finish(false,TEXT("Back did not leave the join screen."));
   return;
  }
  Stage=EStage::OpenPlates;
  break;
 case EStage::OpenPlates:
  if(!PC->Me())return;
  if(!PC->bMenu||!Tap(TEXT("relic_plates"))||OpenRelic!=EMenuRelic::Plates) {
   Finish(false,TEXT("The brass plates did not open from the main menu."));
   return;
  }
  Stage=EStage::ReadPlates;
  break;
 case EStage::ReadPlates:
  if(PlateLeaf!=0||!Tap(TEXT("relic_read"),TEXT("1"))||PlateLeaf!=1) {
   Finish(false,TEXT("The small plates did not switch from Nephi to Mormon."));
   return;
  }
  Stage=EStage::CapturePlates;
  break;
 case EStage::CapturePlates:
  Capture(TEXT("lore-small-plates-v09.png"));
  Stage=EStage::ClosePlates;
  break;
 case EStage::ClosePlates:
  if(!Tap(TEXT("relic_close"))||OpenRelic!=EMenuRelic::None) {
   Finish(false,TEXT("The small plates did not close."));
   return;
  }
  Stage=EStage::OpenVessels;
  break;
 case EStage::OpenVessels:
  if(CountButtons(TEXT("relic_read"))!=0||!Tap(TEXT("relic_vessels"))||OpenRelic!=EMenuRelic::Vessels||LitVessels!=0) {
   Finish(false,TEXT("The paired stones did not open an unlit register."));
   return;
  }
  Stage=EStage::LightVessels;
  break;
 case EStage::LightVessels:
  if(CountButtons(TEXT("relic_light"))!=8||!Tap(TEXT("relic_light"),FString::FromInt(Vessel))||LitVessels!=uint8((1<<(Vessel+1))-1)) {
   Finish(false,TEXT("An engraved vessel failed to light its pair of stones."));
   return;
  }
  if(++Vessel==8)Stage=EStage::CaptureVessels;
  break;
 case EStage::CaptureVessels:
  Capture(TEXT("lore-vessels-v09.png"));
  Stage=EStage::CloseVessels;
  break;
 case EStage::CloseVessels:
  if(!Tap(TEXT("relic_close"))||OpenRelic!=EMenuRelic::None) {
   Finish(false,TEXT("The vessel register did not close."));
   return;
  }
  Stage=EStage::ReopenVessels;
  break;
 case EStage::ReopenVessels:
  if(!Tap(TEXT("relic_vessels"))||OpenRelic!=EMenuRelic::Vessels) {
   Finish(false,TEXT("The vessel register did not reopen."));
   return;
  }
  Stage=EStage::VerifyVessels;
  break;
 case EStage::VerifyVessels:
  if(LitVessels!=255||!Tap(TEXT("relic_close"))||!PC->bMenu||PC->Phase!=0||!PC->Selection.IsEmpty()) {
   Finish(false,TEXT("The register lost its lights or altered match state."));
   return;
  }
  Stage=EStage::OpenSkirmish;
  break;
 case EStage::OpenSkirmish:
  if(!PC->Me())return;
  LobbyFactionBeforeEntry=PC->Me()->Faction;
  LobbyTeamBeforeEntry=PC->Me()->Team;
  bReadyBeforeEntry=PC->Me()->bReady;
  if(!Tap(TEXT("skirmish"))||PC->bMenu) {
   Finish(false,TEXT("The skirmish button did not reach setup."));
   return;
  }
  Stage=EStage::CaptureLobby;
  break;
 case EStage::CaptureLobby:
  if(!PC->Players.ContainsByPredicate([](const FRTSPlayerView& Player){return Player.bBot&&Player.bConnected;}))return;
  if(!PC->Me()||PC->Me()->Name!=PC->PlayerName||PC->Me()->Faction!=LobbyFactionBeforeEntry||
     PC->Me()->Team!=LobbyTeamBeforeEntry||PC->Me()->bReady!=bReadyBeforeEntry) {
   Finish(false,TEXT("Lobby entry did not publish the confirmed profile name with the same faction, team, and readiness."));
   return;
  }
  Capture(TEXT("lobby-v09.png"));
  Stage=EStage::CheckLobbyDraft;
  break;
 case EStage::CheckLobbyDraft: {
  const FString Confirmed=PC->PlayerName;
  const FString Draft=Confirmed==TEXT("Unconfirmed")?TEXT("Local draft"):TEXT("Unconfirmed");
  const auto EditName=[&](){
   if(!Tap(TEXT("focus"),TEXT("name")))return false;
   PC->PlayerName.Reset();
   PC->TextPaste(Draft);
   return PC->PlayerName==Draft;
  };
  if(!EditName()||!Tap(TEXT("team"))||!PC->Me()||PC->Me()->Name!=Confirmed||PC->PlayerName!=Draft) {
   Finish(false,TEXT("Changing teams published or discarded an unconfirmed name edit."));
   return;
  }
  PC->InputKey(FInputKeyParams(EKeys::Escape,IE_Pressed,1.));
  PC->InputKey(FInputKeyParams(EKeys::Escape,IE_Released,0.));
  if(PC->PlayerName!=Confirmed||!PC->TextFocus.IsEmpty()||PC->Me()->Name!=Confirmed) {
   Finish(false,TEXT("Escape did not restore the confirmed name."));
   return;
  }
  if(!GConfig) {Finish(false,TEXT("Player preferences are unavailable."));return;}
  constexpr const TCHAR* ProfileSection=TEXT("Chronicles.Player");
  constexpr const TCHAR* ProfileKey=TEXT("DisplayName");
  FString OriginalSavedName;
  const bool bHadSavedName=GConfig->GetString(ProfileSection,ProfileKey,OriginalSavedName,GGameUserSettingsIni);
  ON_SCOPE_EXIT {
   if(bHadSavedName)GConfig->SetString(ProfileSection,ProfileKey,*OriginalSavedName,GGameUserSettingsIni);
   else GConfig->RemoveKey(ProfileSection,ProfileKey,GGameUserSettingsIni);
   GConfig->Flush(false,GGameUserSettingsIni);
  };
  FString SavedName;
  if(!EditName()||!Tap(TEXT("ready"))||!PC->Me()||!PC->Me()->bReady||PC->Me()->Name!=Draft||
     !GConfig->GetString(ProfileSection,ProfileKey,SavedName,GGameUserSettingsIni)||SavedName!=Draft) {
   Finish(false,TEXT("Ready did not confirm the typed name in the server roster and local profile."));
   return;
  }
  PC->PlayerName=Confirmed;
  if(!Tap(TEXT("ready"))||!Tap(TEXT("team"))||!PC->Me()||PC->Me()->Name!=Confirmed) {
   Finish(false,TEXT("The probe could not restore the original lobby name and settings."));
   return;
  }
  Stage=EStage::OpenHelp;
  break;
 }
 case EStage::OpenHelp:
  if(!Tap(TEXT("help"))||!PC->bHelp) {
   Finish(false,TEXT("The guide did not open from skirmish setup."));
   return;
  }
  Stage=EStage::CaptureHelp;
  break;
 case EStage::CaptureHelp:
  Capture(TEXT("field-guide-v09.png"));
  Stage=EStage::CloseHelp;
  break;
 case EStage::CloseHelp:
  if(!Tap(TEXT("help"))||PC->bHelp) {
   Finish(false,TEXT("The guide did not return to setup."));
   return;
  }
  Finish(true,TEXT("Practice priority, confirmed lobby name and private drafts, LAN address entry, small plates, sixteen stones, skirmish setup and guide."));
  break;
 case EStage::Done:
  break;
 }
}

#endif
