#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/EngineBaseTypes.h"
#include "RTSTypes.h"
#include "RTSController.generated.h"

UCLASS()
class CHRONICLES_API ARTSController : public APlayerController {
 GENERATED_BODY()
public:
 ARTSController();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
 virtual void PlayerTick(float Delta) override;
 virtual void SetupInputComponent() override;
 virtual bool InputKey(const FInputKeyParams& Params) override;
 // Client presentation state. Filled by ClientState; never authoritative.
 TArray<FRTSEntityView> Entities;
 TArray<FRTSPlayerView> Players;
 TArray<uint8> Fog;
 TArray<int32> Selection;
 struct FDeathAnimation {FRTSEntityView Entity;float Started=0;};
 TArray<FDeathAnimation> DeathAnimations;
 // Native presentation helper remains available to standalone input/tests.
 void ClientDeath_Implementation(const FRTSEntityView& Entity);
 UFUNCTION(Client,Reliable) void ClientDeaths(const TArray<FRTSDeathEvent>& Events);
 TArray<FRTSDeathEvent> PendingDeaths;
 float DeathQueueMatchTime=-1;
 int32 DroppedDeathEvents=0,LastDeathBatchesFlushed=0,LastDeathEventsFlushed=0;
 void EnqueueDeathEvent(const FRTSDeathEvent& Event,float SimulationTime);
 int32 FlushDeathEvents(float SimulationTime,int32 SimulationPhase);
 int32 LocalSlot=-1, Phase=0, Map=0, Winner=-1;
 float MatchTime=0;
 FString Notice, ChatDraft, PendingAction, PendingType;
 float NoticeUntil=0;
 bool bChat=false, bLocalLiahona=true, bHelp=false, bMenu=true;
 FVector2D CameraCenter=FVector2D(30,30);
 float Zoom=1, CameraRotation=0;
 FVector2D RotateMap(FVector2D Delta,bool Inverse=false) const;
 bool SupportsOrder(const FString& Action) const;
 TArray<int32> IdleMilitary() const;
 void SelectIdleMilitary(bool All);
 FVector Workforce() const;
 TArray<int32> ResourceWorkers(int32 Resource) const;
 void SelectResourceWorker(int32 Resource);
 bool CanConstruct(const FRTSDef& Building) const;
 FString FactionFor(int32 EntityOwner) const;
 FString DevotionLabel() const;
 float VisualOpacity(const FRTSEntityView& Entity) const;
 bool bWallAnchor=false;
 FVector2D WallAnchor=FVector2D::ZeroVector;
 TMap<int32,int32> LastResourceWorker;
 TMap<int32,TArray<FVector2D>> GalavantRoutes;
 FString ActiveSubgroup;
 TMap<FKey,TPair<FString,FString>> CardHotkeys;
 float DeleteHeldSince=-1;
 TArray<int32> DeleteSelection;
 FLinearColor CommandFlashColor=FLinearColor(.94f,.79f,.46f,1);
 // Server-only identity assigned by GameMode, not accepted from clients.
 int32 ServerSlot=-1;
 float LastCommandTime=-100, CommandCredit=40;
 UFUNCTION(Server,Reliable) void ServerLobby(const FString& Name,const FString& Faction,int32 Team,bool bReady);
 UFUNCTION(Server,Reliable) void ServerLobbySettings(int32 Resources,float Speed,int32 Fertility,bool FogEnabled);
 UFUNCTION(Server,Reliable) void ServerStart(int32 MapIndex);
 UFUNCTION(Server,Reliable) void ServerOrder(const FString& Action,const TArray<int32>& Ids,FVector2D Point,int32 Target,const FString& Type,bool bQueued=false);
 UFUNCTION(Server,Reliable) void ServerChat(const FString& Message);
 UFUNCTION(Server,Reliable) void ServerLiahonaFailure();
 UFUNCTION(Client,Unreliable) void ClientState(const TArray<FRTSEntityView>& NewEntities,const TArray<FRTSPlayerView>& NewPlayers,const TArray<uint8>& NewFog,float Time,int32 NewPhase,int32 NewMap,int32 WinningTeam,int32 Slot);
 void SendSnapshot(const TArray<FRTSEntityView>& NewEntities,const TArray<FRTSPlayerView>& NewPlayers,const TArray<uint8>& NewFog,float Time,int32 NewPhase,int32 NewMap,int32 WinningTeam,int32 Slot,int32 Sequence);
 UFUNCTION(Client,Reliable) void ClientPackedSnapshot(const TArray<uint8>& Bytes,int32 RawBytes,int32 Sequence,int32 PartIndex,int32 PartCount);
 int32 PackedSequence=-1,DeliveredPackedSequence=-1,PackedExpected=0,PackedRawSize=0,PackedReceived=0;
 int32 LargestSnapshotBytes=0,LargestPackedBytes=0;
 TArray<TArray<uint8>> PackedParts;
 UFUNCTION(Client,Reliable) void ClientNotice(const FString& Message);
 UFUNCTION(Client,Reliable) void ClientChat(const FString& Message);
 TArray<FString> ChatLines;
 // Implemented by presentation owner. Network RPC *_Implementation functions are in RTSNetwork.cpp.
 void StateReceived();
 void ShowNotice(const FString& Message);
 const FRTSPlayerView* Me() const;
 const FRTSEntityView* FindEntity(int32 Id) const;
 void Issue(const FString& Action, FVector2D Point=FVector2D::ZeroVector,int32 Target=0,const FString& Type=FString(),bool bQueued=false);
 // Canvas interface and viewport input. Positions here are pixels, world positions are meters.
 FVector2D ViewSize=FVector2D(1600,900), Mouse=FVector2D::ZeroVector, DragStart=FVector2D::ZeroVector;
 bool bDragging=false, bBuildMenu=false, bPauseMenu=false, bDraftFailed=false, bReducedMotion=false;
 bool bHotkeySettings=false;
 FString HotkeyEditId;
 int32 LastSelectedId=0,LastIdleId=0,LastControlGroup=-1,SelectionPage=0;
 float LastControlGroupTime=-100;
 int32 LobbyResources=1,LobbyFruitfulness=1;
 float LobbySpeed=1;
 bool bLobbyFog=true;
 int32 MenuScreen=0, LobbyFaction=0, LobbyTeam=0, LobbyMap=0, ActionPage=0;
 FString PlayerName=TEXT("Player"), JoinAddress=TEXT("127.0.0.1"), TextFocus;
 float LiahonaFailureTime=-100, CommandFlashTime=-100, LastSelectTime=-100;
 FVector2D CommandFlash=FVector2D::ZeroVector;
 TMap<int32,TArray<int32>> ControlGroups;
 TMap<int32,FVector2D> DisplayPositions;
 int32 PreviousPhase=-1;
 bool bIdentitySynced=false;
 bool bAwaitingLiahonaFailure=false,bAwaitingLiahonaRestore=false;
 bool bPreview=false,bPreviewExit=false,bMenuShot=false,bPreviewStarted=false,bScreenshotTaken=false;
 float PreviewStart=-1;
 bool bSoundEnabled=true,bAudioSnapshotSeen=false;
 TMap<FName,float> LastCueTimes;
 TMap<int32,FVector2D> AudioEntityState;
 TArray<TWeakObjectPtr<class UAudioComponent>> PlayingCues;
 void PlayCue(FName Cue);
 bool PlaySelectionVoice();
 bool bConnecting=false,bReturningToMenu=false,bJoinFailureTest=false;
 float ConnectionStarted=0,FailureTestReturnTime=-1;
 FString ConnectionError;
 FDelegateHandle NetworkFailureHandle,TravelFailureHandle;
 void HandleNetworkFailure(UWorld* FailedWorld,class UNetDriver* Driver,ENetworkFailure::Type Failure,const FString& Error);
 void HandleTravelFailure(UWorld* FailedWorld,ETravelFailure::Type Failure,const FString& Error);
 void ConnectionFailed(const FString& Message);
 void TextCharacter(TCHAR Character);
 void TextPaste(const FString& Text);
 void CheckDraft(bool bSubmit);
 void SubmitText();
 void UIAction(const FString& Action,const FString& Value=FString());
 void MouseDown();
 void MouseUp();
 void RightClick();
 FVector2D WorldToScreen(FVector2D Point) const;
 FVector2D ScreenToWorld(FVector2D Point) const;
 FVector2D VisualPosition(const FRTSEntityView& Entity) const;
 int32 EntityUnder(FVector2D Pixel,bool bContext=false) const;
 FBox2D EntityBounds(const FRTSEntityView& Entity,bool bContext=false) const;
 void RefreshPointer();
 TArray<int32> IdleVillagers() const;
 void SelectIdleVillagers(bool All);
 bool HasSelectedBuilder() const;
 bool InWorld(FVector2D Pixel) const;
 bool IsHost() const;
 bool CanUse(const FRTSDef& Def,FString& Reason) const;
 void FocusHeadquarters();
};
