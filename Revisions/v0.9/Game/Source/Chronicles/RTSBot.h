#pragma once

#include "CoreMinimal.h"

struct FRTSBotHideoutClue {
 FVector2D Position=FVector2D::ZeroVector;
 bool bObservedSeat=false;
};

struct FRTSTeacherProductionSnapshot {
 FVector Resources=FVector::ZeroVector;
 int32 FreePopulation=0,TempleJobs=0,MilitaryJobs=0;
 bool bCanAfford=false;
};

struct FRTSTeacherProductionEvent {
 float Time=0;
 FString Gate,BlockingOrder;
 FRTSTeacherProductionSnapshot Before,After;
 TArray<FString> QueuedOrders;
 int32 TempleId=0,Age=0,Army=0,NearbyDefenders=0;
 bool bVisibleBaseThreat=false,bSavingAge=false,bAgeReserve=false;
};

struct FRTSFirstTeacherTrace {
 TArray<FRTSTeacherProductionEvent> Events;
 FString LastGate,LastBlockingOrder;
 int32 LastTempleId=0,DroppedTransitions=0;
 uint32 LastContext=0;
 float FirstAttempt=-1,FirstQueued=-1;
};

enum class ERTSEmergencySlingerMode : uint8 { Inactive,Baseline,Priority };

struct FRTSEmergencySlingerTrace {
 int32 EligibleCamps=0,Attempts=0,Queued=0;
 float FirstEligible=-1,FirstQueued=-1;
 bool bObservedBaseThreat=false;
};

struct FRTSRaidProductionEvent {
 FString Gate,StoreJob;
 FVector Resources=FVector::ZeroVector;
 float Time=0;
 int32 Age=1,Workers=0,Army=0,FreePopulation=0,StoreId=0;
 bool bThreat=false,bForagingComplete=false;
};

struct FRTSRaidVisibilityEvent {
 float Time=0;
 int32 Bands=0,Buildings=0,Workers=0,LoadedWorkers=0,ThreatId=0;
 int32 NearestBuildingId=0,NearestWorkerId=0;
 double NearestBuildingDistance=-1,NearestWorkerDistance=-1;
 bool SameTargets(const FRTSRaidVisibilityEvent& Other) const {
  return Bands==Other.Bands&&Buildings==Other.Buildings&&Workers==Other.Workers&&LoadedWorkers==Other.LoadedWorkers&&
   ThreatId==Other.ThreatId&&NearestBuildingId==Other.NearestBuildingId&&NearestWorkerId==Other.NearestWorkerId;
 }
};

struct FRTSRaidOpeningJob {
 FString Type,Producer;
 FVector Paid=FVector::ZeroVector;
 float Time=0;
};

struct FRTSGadiantonRaidTrace {
 bool bEnabled=false;
 TArray<FRTSRaidProductionEvent> Production;
 TArray<FRTSRaidVisibilityEvent> Visibility;
 TArray<FRTSRaidOpeningJob> OpeningJobs;
 int32 DroppedVisibility=0;
 float FirstStoreStarted=-1,FirstStoreComplete=-1,FirstForagingQueued=-1,FirstForagingComplete=-1;
 float FirstBandQueued=-1,FirstSteal=-1,FirstDelivery=-1,FirstForeignFood=-1;
 FVector BuildingStockCargo=FVector::ZeroVector,WorkerCargo=FVector::ZeroVector,DivertedCargo=FVector::ZeroVector;
 FVector DeliveredCargo=FVector::ZeroVector;
 double ForeignFarmFood=0,ForeignFlockFood=0;
 int32 Deliveries=0,DefenseInterruptions=0,LoadedDefenseInterruptions=0;
 int32 SurplusOreDiversions=0;
};

struct FRTSBotState {
 float NextThink=0;
 float NextOffense=0;
 FVector2D Home=FVector2D::ZeroVector;
 FVector2D ArmyGoal=FVector2D::ZeroVector;
 FVector2D ScoutGoal=FVector2D::ZeroVector;
 int32 ScoutId=0;
 float ScoutGoalUntil=0;
 float ArmyGoalUntil=0;
 float RaidScoutUntil=0;
 int32 ArmyTarget=0;
 bool bCampaign=false;
 TMap<int32,FVector2D> EnemyBuildings;
 TMap<int32,FRTSBotHideoutClue> HideoutClues;
 FVector2D HideoutSweepCenter=FVector2D::ZeroVector;
 int32 HideoutSweepOwner=INDEX_NONE;
 uint32 HideoutSweepVisited=0;
 float HideoutSweepUntil=0,NextHideoutSweep=0;
 TMap<int32,float> ScoutVisits;
 TMap<int32,float> RejectedWork;
 int32 SupportTrainingTraceCount=0;
 FRTSFirstTeacherTrace FirstTeacherTrace;
 ERTSEmergencySlingerMode EmergencySlingerMode=ERTSEmergencySlingerMode::Inactive;
 FRTSEmergencySlingerTrace EmergencySlingerTrace;
 FRTSGadiantonRaidTrace GadiantonRaidTrace;
 bool bGadiantonCombatFirstForProbe=false;
 bool bGadiantonFoodAssignmentForProbe=false;
 int32 GadiantonOpeningFightersOrdered=0;
};

bool RunRTSBotTests(class ARTSWorld& World);
bool RTSBotSupportEnabled(int32 Slot);
bool RTSBotSupportTraceEnabled();
enum class ERTSBotRaidPolicy : uint8 { Legacy,Expedition,Opportunistic };
ERTSBotRaidPolicy RTSBotRaidPolicy();
