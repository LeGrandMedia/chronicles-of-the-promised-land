#include "RTSController.h"
#include "RTSNetworkV07Probe.h"
#include "RTSGameMode.h"
#include "RTSWorld.h"
#include "RTSHUD.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/GameSession.h"
void RunRTSNetworkProbe(ARTSController* C);

static ARTSWorld* Sim(ARTSController* C) { auto G=C->GetWorld()->GetAuthGameMode<ARTSGameMode>();return G?G->Simulation:nullptr; }
void ARTSController::ServerLobby_Implementation(const FString& N,const FString& F,int32 T,bool Ready) { if(auto S=Sim(this)) S->Lobby(this,N.Left(24),F.Left(40),T,Ready); }
void ARTSController::ServerLobbySettings_Implementation(int32 Resources,float Speed,int32 Fertility,bool FogEnabled) {if(auto S=Sim(this))S->ConfigureLobby(this,Resources,Speed,Fertility,FogEnabled);}
void ARTSController::ServerStart_Implementation(int32 M) { if(auto S=Sim(this)) S->Start(this,M); }
void ARTSController::ServerOrder_Implementation(const FString& A,const TArray<int32>& Ids,FVector2D P,int32 Target,const FString& Type,bool bQueued) {
 float Now=GetWorld()->GetTimeSeconds();CommandCredit=FMath::Min(40.f,CommandCredit+FMath::Max(0.f,Now-LastCommandTime)*20.f);LastCommandTime=Now;
 if(CommandCredit<1 || Ids.Num()>200 || A.Len()>24 || Type.Len()>80 || !FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)) return;CommandCredit-=1;
 if(auto S=Sim(this)) {
#if !UE_BUILD_SHIPPING
  const bool Audit=FParse::Param(FCommandLine::Get(),TEXT("RTSNetV07"));
  if(Audit)RTSNetworkV07OrderAudit(this,S,A,Ids,Type,false);
#endif
  S->Command(this,A,Ids,P,Target,Type,bQueued);
#if !UE_BUILD_SHIPPING
  if(Audit)RTSNetworkV07OrderAudit(this,S,A,Ids,Type,true);
#endif
 }
}
void ARTSController::ServerChat_Implementation(const FString& M) { if(auto S=Sim(this)) S->Chat(this,M.Left(240)); }
void ARTSController::ServerLiahonaFailure_Implementation() { if(auto S=Sim(this)) S->LiahonaFailure(this); }
void ARTSController::ClientState_Implementation(const TArray<FRTSEntityView>& E,const TArray<FRTSPlayerView>& P,const TArray<uint8>& F,float T,int32 Ph,int32 M,int32 W,int32 Slot) { Entities=E;Players=P;Fog=F;MatchTime=T;Phase=Ph;Map=M;Winner=W;LocalSlot=Slot;StateReceived();RunRTSNetworkProbe(this); }
void ARTSController::ClientNotice_Implementation(const FString& M) { ShowNotice(M); }
void ARTSController::ClientChat_Implementation(const FString& M) { ChatLines.Add(M); if(ChatLines.Num()>8) ChatLines.RemoveAt(0); }
ARTSGameMode::ARTSGameMode() { PlayerControllerClass=ARTSController::StaticClass();HUDClass=ARTSHUD::StaticClass();DefaultPawnClass=nullptr; bStartPlayersAsSpectators=true; }
void ARTSGameMode::BeginPlay() { Super::BeginPlay(); if(!Simulation) Simulation=GetWorld()->SpawnActor<ARTSWorld>(); }
void ARTSGameMode::PostLogin(APlayerController* P) { Super::PostLogin(P);if(!Simulation) Simulation=GetWorld()->SpawnActor<ARTSWorld>(); if(auto C=Cast<ARTSController>(P)) Simulation->Join(C); }
void ARTSGameMode::Logout(AController* C) { if(Simulation) if(auto P=Cast<ARTSController>(C)) Simulation->Leave(P);Super::Logout(C); }
