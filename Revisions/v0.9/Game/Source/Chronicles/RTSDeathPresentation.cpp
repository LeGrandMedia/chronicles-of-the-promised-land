#include "RTSWorld.h"
#include "RTSController.h"
#include "RTSHUD.h"
#include "RTSVisualAssets.h"
#include "Engine/World.h"

namespace {
 FRTSDeathEvent DeathEvent(const FRTSEntityView& E){FRTSDeathEvent Event;Event.Id=E.Id;Event.Owner=E.Owner;Event.Def=E.Def;Event.Pos=E.Pos;Event.Heading=E.Heading;Event.Progress=E.Progress;Event.bGate=E.bGate;return Event;}
 bool ValidDeath(const FRTSDeathEvent& E){return E.Id>0&&E.Owner>=-1&&E.Owner<4&&!FRTSCatalog::Get().Def(E.Def).Id.IsEmpty()&&FMath::IsFinite(E.Pos.X)&&FMath::IsFinite(E.Pos.Y)&&FMath::IsFinite(E.Heading)&&FMath::IsFinite(E.Progress);}
 FRTSEntityView DeathView(const FRTSDeathEvent& E){FRTSEntityView View;View.Id=E.Id;View.Owner=E.Owner;View.Def=E.Def;View.Pos=E.Pos;View.Heading=E.Heading;View.Progress=FMath::Clamp(E.Progress,0.f,1.f);View.bGate=E.bGate;View.HP=0;View.Status=FRTSCatalog::Get().Def(E.Def).bBuilding?TEXT("destroy"):TEXT("death");return View;}
}

void ARTSWorld::RecordDeath(const FRTSEntity& E) {
 const auto& Def=D(E);
 if(E.Carrier||Def.bUpgrade||(Def.Kind==TEXT("resource")&&Def.Id!=TEXT("wild_game")))return;
 // An explicit, visibility-filtered event distinguishes death from fog, boarding,
 // and ownership transitions. It contains no cargo, orders, or hidden state.
 FRTSDeathEvent Event=DeathEvent(E);Event.Owner=E.DisguisedOwner>=0?E.DisguisedOwner:E.Owner;
 for(auto& P:Players)if(P.Controller.IsValid()&&CanSee(P.Slot,E)){
  auto C=P.Controller.Get();
  if(C->IsLocalController()||!C->GetNetConnection())C->ClientDeath_Implementation(DeathView(Event));
  else C->EnqueueDeathEvent(Event,Time);
 }
}

void ARTSController::EnqueueDeathEvent(const FRTSDeathEvent& E,float SimulationTime){
 if(!ValidDeath(E))return;
 if(DeathQueueMatchTime>=0&&SimulationTime<DeathQueueMatchTime){PendingDeaths.Empty();DroppedDeathEvents=0;}
 DeathQueueMatchTime=SimulationTime;
 if(PendingDeaths.ContainsByPredicate([&](const FRTSDeathEvent& Existing){return Existing.Id==E.Id;}))return;
 if(PendingDeaths.Num()>=RTSDeath::PendingLimit){PendingDeaths.RemoveAt(0);++DroppedDeathEvents;}
 PendingDeaths.Add(E);
}

int32 ARTSController::FlushDeathEvents(float SimulationTime,int32 SimulationPhase){
 LastDeathBatchesFlushed=LastDeathEventsFlushed=0;
 // Lobby return and a fresh match clock both invalidate prior-match events.
 if(SimulationPhase==0||(DeathQueueMatchTime>=0&&SimulationTime<DeathQueueMatchTime)){
  PendingDeaths.Empty();DeathQueueMatchTime=SimulationTime;DroppedDeathEvents=0;return 0;
 }
 DeathQueueMatchTime=SimulationTime;
 // Recipients were fixed in RecordDeath while the body still existed. Flushing
 // does not consult current fog, which may have changed since the casualty.
 for(int BatchIndex=0;BatchIndex<RTSDeath::BatchesPerFlush&&!PendingDeaths.IsEmpty();++BatchIndex){
  const int Count=FMath::Min(RTSDeath::EventsPerBatch,PendingDeaths.Num());TArray<FRTSDeathEvent> Batch;Batch.Append(PendingDeaths.GetData(),Count);PendingDeaths.RemoveAt(0,Count);
  if(IsLocalController()||!GetNetConnection())ClientDeaths_Implementation(Batch);else ClientDeaths(Batch);
  ++LastDeathBatchesFlushed;LastDeathEventsFlushed+=Count;
 }
 return LastDeathEventsFlushed;
}

void ARTSController::ClientDeaths_Implementation(const TArray<FRTSDeathEvent>& Events){
 if(Events.Num()>RTSDeath::EventsPerBatch)return;
 for(const auto& E:Events)if(ValidDeath(E))ClientDeath_Implementation(DeathView(E));
}

void ARTSController::ClientDeath_Implementation(const FRTSEntityView& E) {
 if(!GetWorld()||!ValidDeath(DeathEvent(E)))return;
 if(DeathAnimations.ContainsByPredicate([&](const FDeathAnimation& A){return A.Entity.Id==E.Id;}))return;
 if(DeathAnimations.Num()>=RTSDeath::PendingLimit)DeathAnimations.RemoveAt(0);
 DeathAnimations.Add({DeathView(DeathEvent(E)),float(GetWorld()->GetRealTimeSeconds())});
}

void ARTSHUD::DrawDeaths() {
 const float Now=GetWorld()->GetRealTimeSeconds();
 PC->DeathAnimations.RemoveAll([&](const ARTSController::FDeathAnimation& A){return Now-A.Started>3.2f;});
 for(const auto& A:PC->DeathAnimations){
  const auto& D=FRTSCatalog::Get().Def(A.Entity.Def);const FVector2D S=PC->WorldToScreen(A.Entity.Pos)/U;
  if(S.X<-220||S.X>W+220||S.Y<10||S.Y>H-155)continue;
  const float Age=FMath::Max(0.f,Now-A.Started),Z=PC->Zoom;
  const float Alpha=FMath::Clamp((3.2f-Age)/1.3f,0.f,1.f);
  FRTSEntityView Dead=A.Entity;Dead.Status=D.bBuilding?TEXT("destroy"):TEXT("death");
  // For presentation-only death entities Cooldown carries animation elapsed time.
  Dead.Cooldown=PC->bReducedMotion?2.f:Age;
  if(!FRTSVisualAssets::Get().TryDraw(Canvas,Dead,D,S,Z,U,RTS::PlayerColor(Dead.Owner),Now,Alpha,PC->FactionFor(Dead.Owner))){
   const float Radius=(D.bBuilding?D.Radius*9:7)*Z;
   Circle(S,Radius,FLinearColor(.36f,.28f,.19f,Alpha*.6f),true,.35f);
  }
  if(D.bBuilding&&!PC->bReducedMotion){
   const float Fall=FMath::Clamp(Age/1.3f,0.f,1.f),Dust=Alpha*FMath::Sin(FMath::Min(1.f,Age/2.8f)*PI)*.28f;
   for(int I=0;I<9;I++){
    const float Angle=I*2*PI/9+Dead.Id*.3f;const FVector2D P=S+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle)*.45f)*(12+24*Fall)*Z;
    Circle(P-FVector2D(0,9*Fall*Z),(7+10*Fall)*Z,FLinearColor(.64f,.53f,.37f,Dust),true,.75f);
    const float Arc=FMath::Sin(Fall*PI)*(9+I%3*5)*Z;const FVector2D Rock=P-FVector2D(0,Arc);
    Poly({Rock+FVector2D(-3,-2)*Z,Rock+FVector2D(3,-1)*Z,Rock+FVector2D(2,3)*Z,Rock+FVector2D(-2,2)*Z},FLinearColor(.43f,.34f,.24f,Alpha*.85f));
   }
  }
 }
}
