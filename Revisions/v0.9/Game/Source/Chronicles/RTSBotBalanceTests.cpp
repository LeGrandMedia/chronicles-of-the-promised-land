#include "RTSBotBalanceTests.h"
#include "RTSMaps.h"
#include "RTSWorld.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/UnrealTemplate.h"

namespace {
constexpr int32 RTSBalanceMaximumSteps=12000;
constexpr int32 RTSBalanceDefenseSampleSteps=100;
constexpr int32 RTSBalanceDefenseRecordLimit=128;
constexpr double RTSBalanceSeatDefenseRadius=24.0;

struct FBalanceRoster {
 int32 Workers=0,IdleWorkers=0,GatheringWorkers=0,Army=0,ArmyPop=0,Seats=0,Beasts=0,EconomyBuildings=0;
 int32 PlannerArmy=0,PrimarySeatId=0;
 int32 FaithBuildings=0,Converters=0,Ammonites=0,ActiveConversions=0;
 FVector ArmyCost=FVector::ZeroVector;
 double ArmyCementCost=0;
 double ArmyHealth=0,ArmyMaxHealth=0;
};

struct FBalanceContact {
 FBalanceRoster Roster;
 FVector Resources=FVector::ZeroVector;
 float Time=-1,Favor=0;
 int32 Age=1;
 bool bTracedOpening=false;
 FVector WorkerPulses=FVector::ZeroVector;
 FVector WorkerAssignments=FVector::ZeroVector;
 TMap<int32,int32> ArmyByDef;
 int32 NearbyDefenders=0,SurplusOreDiversions=0;
 double NearbyDefenderMaxHealth=0;
};

struct FBalanceSupportState {
 FString Order;
 int32 Target=0;
 float Channel=0;
};

struct FBalanceSlingerLife {
 int32 Id=0,AppearanceStep=0,FirstNearSeatStep=-1,LastNearSeatStep=-1,NearSeatSteps=0,EndStep=-1,NewOwner=INDEX_NONE;
 float AppearanceTime=0,FirstNearSeatTime=-1,LastNearSeatTime=-1,EndTime=-1;
 const TCHAR* EndObservation=TEXT("present_at_finish");
};

struct FBalanceBandState {
 FString Order;
 int32 Target=0;
 float Channel=0;
};

struct FBalanceSide {
 float FirstDeposit=-1,FirstConstruction=-1,FirstAgeII=-1,FirstAgeIII=-1,FirstAgeIV=-1;
 float FirstResearch=-1,FirstMilitary=-1,FirstCampaign=-1,FirstAttack=-1,LastAttack=-1;
 float FirstDamageTaken=-1,LastDamageTaken=-1,FirstSeatLoss=-1,FirstRaid=-1;
 float FirstFaithBuilding=-1,FirstConverter=-1,FirstConversionChannel=-1;
 float FirstCampStarted=-1,FirstCampComplete=-1,FirstTempleStarted=-1,FirstWorkshopStarted=-1,FirstWorkshopComplete=-1;
 int32 PeakWorkers=0,PeakArmy=0,PeakArmyPop=0,PeakPopulation=0,PeakCapacity=0;
 int32 AttackEvents=0,DamageTicks=0,BattleLosses=0,WorkerBattleLosses=0,MilitaryBattleLosses=0;
 int32 BuildingBattleLosses=0,EconomyBuildingBattleLosses=0,SeatLosses=0,OtherRemovals=0,ConvertedAway=0,ConvertedIn=0;
 int32 StepsOverCapacity=0;
 int32 AgeIIUnderstrengthThreatSteps=0;
 double PeakArmyResourceValue=0,ObservedDamageTaken=0,PeakRaiderCargo=0;
 FBalanceRoster Final;
 FBalanceContact Contact;
 FVector OpeningWorkerPulses=FVector::ZeroVector;
 TMap<int32,FBalanceSupportState> SupportStates;
 TArray<TSharedPtr<FJsonValue>> SupportEvents;
 int32 DroppedSupportEvents=0;
 TArray<TSharedPtr<FJsonValue>> DefenseSamples;
 TArray<FBalanceSlingerLife> SlingerLives;
 TMap<int32,int32> ActiveSlingers;
 int32 DroppedDefenseSamples=0,DroppedSlingerLives=0,TrackedSlingerSeatSteps=0;
 float FirstBandFielded=-1,FirstRaidChannel=-1,FirstRaidChannelComplete=-1,PeakRaidChannel=0;
 int32 BandsFielded=0,BandsLost=0,RaidDamageResets=0,RaidOtherResets=0,DroppedBandEvents=0;
 TMap<int32,FBalanceBandState> Bands;
 TArray<TSharedPtr<FJsonValue>> BandEvents;
};

struct FBalanceBeforeStep {
 TSharedPtr<FRTSEntity> Body;
 int32 Owner=-1;
 float HealthFraction=1;
 bool bMortalWound=false;
 bool bBuilding=false,bEconomyBuilding=false,bSeat=false,bWorker=false,bMilitary=false;
 FVector ResourcePulse=FVector::ZeroVector;
 float ResourcePulseTime=-100;
};

void BalanceWhen(float& First,float Time,bool bOccurred=true) {
 if(bOccurred&&First<0)First=Time;
}

bool BalanceEconomyBuilding(const FRTSDef& Def) {
 return Def.Id==TEXT("seat_of_the_people")||Def.Id==TEXT("fields_of_grain")||Def.Id==TEXT("gathering_of_flocks")||
  Def.Id==TEXT("hill_of_ore")||Def.Id==TEXT("storehouse_of_provisions")||Def.Id==TEXT("place_of_trade")||
  Def.Id==TEXT("tents_and_habitations")||Def.Id==TEXT("winery");
}

int32 BalanceOpeningWorkResource(const FString& Type) {
 if(Type==TEXT("berries")||Type==TEXT("wild_game")||Type==TEXT("fields_of_grain")||Type==TEXT("gathering_of_flocks"))return 0;
 if(Type==TEXT("tree"))return 1;
 if(Type==TEXT("ore")||Type==TEXT("hill_of_ore"))return 2;
 return INDEX_NONE;
}

void BalanceAttack(FBalanceSide& Side,float Time) {
 ++Side.AttackEvents;
 BalanceWhen(Side.FirstAttack,Time);
 Side.LastAttack=Time;
}

TSharedPtr<FJsonObject> BalanceVector(FVector Amount) {
 auto Result=MakeShared<FJsonObject>();
 Result->SetNumberField(TEXT("food"),Amount.X);
 Result->SetNumberField(TEXT("timber"),Amount.Y);
 Result->SetNumberField(TEXT("precious"),Amount.Z);
 return Result;
}

void BalanceBandEvent(ARTSWorld& W,FBalanceSide& Side,const FRTSEntity& Band,const TCHAR* Kind,const FBalanceBandState* Previous) {
 if(Side.BandEvents.Num()>=256){++Side.DroppedBandEvents;return;}
 auto Event=MakeShared<FJsonObject>();
 Event->SetStringField(TEXT("event"),Kind);
 Event->SetNumberField(TEXT("time_seconds"),W.Time);
 Event->SetNumberField(TEXT("band_id"),Band.Id);
 Event->SetStringField(TEXT("order"),Band.Order);
 Event->SetNumberField(TEXT("target_id"),Band.Target);
 Event->SetNumberField(TEXT("channel_seconds"),Band.Channel);
 Event->SetNumberField(TEXT("hit_points"),Band.HP);
 Event->SetObjectField(TEXT("cargo"),BalanceVector(Band.Cargo));
 if(Previous){
  Event->SetStringField(TEXT("previous_order"),Previous->Order);
  Event->SetNumberField(TEXT("previous_target_id"),Previous->Target);
  Event->SetNumberField(TEXT("previous_channel_seconds"),Previous->Channel);
 }
 if(const auto* Target=W.Find(Band.Target)){
  Event->SetStringField(TEXT("target_type"),W.D(*Target).Id);
  Event->SetNumberField(TEXT("target_distance"),FVector2D::Distance(Band.Pos,Target->Pos));
 }
 Side.BandEvents.Add(MakeShared<FJsonValueObject>(Event));
}

void BalanceObserveBand(ARTSWorld& W,FBalanceSide& Side,const FRTSEntity& Band) {
 const auto* Previous=Side.Bands.Find(Band.Id);
 if(!Previous){
  ++Side.BandsFielded;
  BalanceWhen(Side.FirstBandFielded,W.Time);
  BalanceBandEvent(W,Side,Band,TEXT("fielded"),nullptr);
 }else if(Band.Order!=Previous->Order||Band.Target!=Previous->Target){
  BalanceBandEvent(W,Side,Band,TEXT("order_or_target_changed"),Previous);
 }
 if(Band.Order==TEXT("raid")){
  BalanceWhen(Side.FirstRaidChannel,W.Time,Band.Channel>0);
  BalanceWhen(Side.FirstRaidChannelComplete,W.Time,Band.Channel>=3);
  Side.PeakRaidChannel=FMath::Max(Side.PeakRaidChannel,Band.Channel);
  if(Band.Channel>0&&(!Previous||Previous->Order!=TEXT("raid")||Previous->Channel<=0)){
   BalanceBandEvent(W,Side,Band,TEXT("channel_started"),Previous);
  }else if(Previous&&Previous->Order==TEXT("raid")&&Previous->Channel>0&&Band.Channel<Previous->Channel&&Band.Target==Previous->Target){
   const bool bDamage=W.Time-Band.LastDamage<.2f;
   if(bDamage)++Side.RaidDamageResets;else ++Side.RaidOtherResets;
   BalanceBandEvent(W,Side,Band,bDamage?TEXT("channel_reset_under_damage"):TEXT("channel_reset"),Previous);
  }
 }
 Side.Bands.Add(Band.Id,{Band.Order,Band.Target,Band.Channel});
}

TSharedPtr<FJsonObject> BalanceGadiantonRaids(const FRTSPlayer& P,const FBalanceSide& Side) {
 const auto& Trace=P.Bot.GadiantonRaidTrace;
 auto Result=MakeShared<FJsonObject>();
 Result->SetNumberField(TEXT("first_storehouse_started_seconds"),Trace.FirstStoreStarted);
 Result->SetNumberField(TEXT("first_storehouse_completed_seconds"),Trace.FirstStoreComplete);
 Result->SetNumberField(TEXT("first_foraging_queued_seconds"),Trace.FirstForagingQueued);
 Result->SetNumberField(TEXT("first_foraging_completed_seconds"),Trace.FirstForagingComplete);
 Result->SetNumberField(TEXT("first_band_queued_seconds"),Trace.FirstBandQueued);
 Result->SetNumberField(TEXT("first_band_fielded_seconds"),Side.FirstBandFielded);
 Result->SetNumberField(TEXT("first_channel_seconds"),Side.FirstRaidChannel);
 Result->SetNumberField(TEXT("first_channel_completed_seconds"),Side.FirstRaidChannelComplete);
 Result->SetNumberField(TEXT("maximum_channel_seconds"),Side.PeakRaidChannel);
 Result->SetNumberField(TEXT("first_steal_seconds"),Trace.FirstSteal);
 Result->SetNumberField(TEXT("first_delivery_seconds"),Trace.FirstDelivery);
 Result->SetNumberField(TEXT("first_foreign_food_seconds"),Trace.FirstForeignFood);
 Result->SetNumberField(TEXT("bands_fielded"),Side.BandsFielded);
 Result->SetNumberField(TEXT("bands_lost"),Side.BandsLost);
 Result->SetNumberField(TEXT("defense_interruptions"),Trace.DefenseInterruptions);
 Result->SetNumberField(TEXT("loaded_defense_interruptions"),Trace.LoadedDefenseInterruptions);
 Result->SetNumberField(TEXT("channel_resets_under_damage"),Side.RaidDamageResets);
 Result->SetNumberField(TEXT("other_channel_resets"),Side.RaidOtherResets);
 Result->SetNumberField(TEXT("cargo_deliveries"),Trace.Deliveries);
 Result->SetNumberField(TEXT("surplus_ore_diversions"),Trace.SurplusOreDiversions);
 Result->SetNumberField(TEXT("foreign_farm_food_collected"),Trace.ForeignFarmFood);
 Result->SetNumberField(TEXT("foreign_flock_food_collected"),Trace.ForeignFlockFood);
 Result->SetObjectField(TEXT("from_building_stock"),BalanceVector(Trace.BuildingStockCargo));
 Result->SetObjectField(TEXT("from_worker_cargo"),BalanceVector(Trace.WorkerCargo));
 Result->SetObjectField(TEXT("from_intercepted_gathering"),BalanceVector(Trace.DivertedCargo));
 Result->SetObjectField(TEXT("delivered_cargo"),BalanceVector(Trace.DeliveredCargo));
 const auto& Catalog=FRTSCatalog::Get();
 for(const auto* Key:{TEXT("gadianton_storehouse_wild_food"),TEXT("gadianton_robber")}){
  const auto& Def=Catalog.Def(Key);
  Result->SetObjectField(FString(Key)+TEXT("_price"),BalanceVector(Catalog.Price(Def,P.Faction)));
 }
 TArray<TSharedPtr<FJsonValue>> Production,Visibility;
 for(const auto& Change:Trace.Production){
  auto Event=MakeShared<FJsonObject>();
  Event->SetNumberField(TEXT("time_seconds"),Change.Time);
  Event->SetStringField(TEXT("gate"),Change.Gate);
  Event->SetNumberField(TEXT("storehouse_id"),Change.StoreId);
  Event->SetStringField(TEXT("storehouse_job_after_decision"),Change.StoreJob);
  Event->SetObjectField(TEXT("resources_before_decision"),BalanceVector(Change.Resources));
  Event->SetNumberField(TEXT("age"),Change.Age);
  Event->SetNumberField(TEXT("workers_including_jobs"),Change.Workers);
  Event->SetNumberField(TEXT("planner_army"),Change.Army);
  Event->SetNumberField(TEXT("free_population"),Change.FreePopulation);
  Event->SetBoolField(TEXT("visible_base_threat"),Change.bThreat);
  Event->SetBoolField(TEXT("foraging_completed"),Change.bForagingComplete);
  Production.Add(MakeShared<FJsonValueObject>(Event));
 }
 for(const auto& Change:Trace.Visibility){
  auto Event=MakeShared<FJsonObject>();
  Event->SetNumberField(TEXT("time_seconds"),Change.Time);
  Event->SetNumberField(TEXT("bands"),Change.Bands);
  Event->SetNumberField(TEXT("visible_raid_buildings"),Change.Buildings);
  Event->SetNumberField(TEXT("visible_workers"),Change.Workers);
  Event->SetNumberField(TEXT("visible_workers_with_cargo_authority_only"),Change.LoadedWorkers);
  Event->SetNumberField(TEXT("base_threat_id"),Change.ThreatId);
  Event->SetNumberField(TEXT("nearest_building_id"),Change.NearestBuildingId);
  Event->SetNumberField(TEXT("nearest_worker_id"),Change.NearestWorkerId);
  Event->SetNumberField(TEXT("nearest_building_distance"),Change.NearestBuildingDistance);
  Event->SetNumberField(TEXT("nearest_worker_distance"),Change.NearestWorkerDistance);
  Visibility.Add(MakeShared<FJsonValueObject>(Event));
 }
 Result->SetArrayField(TEXT("storehouse_decisions"),Production);
 Result->SetArrayField(TEXT("visible_targets_from_first_band_or_home"),Visibility);
 Result->SetArrayField(TEXT("band_events"),Side.BandEvents);
 TArray<TSharedPtr<FJsonValue>> OpeningJobs;
 for(const auto& Job:Trace.OpeningJobs){
  if(Side.Contact.Time>=0&&Job.Time>Side.Contact.Time+30)break;
  auto Event=MakeShared<FJsonObject>();
  Event->SetNumberField(TEXT("time_seconds"),Job.Time);
  Event->SetStringField(TEXT("type"),Job.Type);
  Event->SetStringField(TEXT("producer"),Job.Producer);
  Event->SetObjectField(TEXT("paid"),BalanceVector(Job.Paid));
  OpeningJobs.Add(MakeShared<FJsonValueObject>(Event));
 }
 Result->SetArrayField(TEXT("accepted_opening_jobs"),OpeningJobs);
 Result->SetNumberField(TEXT("dropped_visibility_transitions"),Trace.DroppedVisibility);
 Result->SetNumberField(TEXT("dropped_band_events"),Side.DroppedBandEvents);
 return Result;
}

bool BalanceNearSeat(const FRTSEntity* SeatEntity,const FRTSEntity& Unit) {
 return SeatEntity&&Unit.HP>0&&!Unit.Carrier&&
  FVector2D::DistSquared(SeatEntity->Pos,Unit.Pos)<=FMath::Square(RTSBalanceSeatDefenseRadius);
}

void BalanceSlingerAppearance(FBalanceSide& Side,const FRTSEntity& Unit,int32 StepIndex,float Time) {
 if(Side.SlingerLives.Num()>=RTSBalanceDefenseRecordLimit){++Side.DroppedSlingerLives;return;}
 FBalanceSlingerLife Life;
 Life.Id=Unit.Id;
 Life.AppearanceStep=StepIndex;
 Life.AppearanceTime=Time;
 Side.ActiveSlingers.Add(Unit.Id,Side.SlingerLives.Add(Life));
}

void BalanceSlingerEnd(FBalanceSide& Side,const FRTSEntity& Unit,int32 StepIndex,float Time,bool bPresent,bool bDamaged) {
 int32 Index=0;
 if(!Side.ActiveSlingers.RemoveAndCopyValue(Unit.Id,Index))return;
 auto& Life=Side.SlingerLives[Index];
 Life.EndStep=StepIndex;
 Life.EndTime=Time;
 if(bPresent){
  Life.EndObservation=TEXT("ownership_changed");
  Life.NewOwner=Unit.Owner;
 }else{
  // The prior step's shared pointer survives removal. Only a same-step lethal hit certifies combat death.
  Life.EndObservation=bDamaged&&Unit.HP<=0?TEXT("combat_death"):TEXT("removed");
 }
}

void BalanceObserveLocalDefense(ARTSWorld& W,const FRTSPlayer& P,FBalanceSide& Side,int32 StepIndex) {
 const auto* SeatEntity=W.Find(Side.Final.PrimarySeatId);
 const bool bCheckpoint=P.Age>=2&&StepIndex%RTSBalanceDefenseSampleSteps==0;
 TArray<int32> NearbyTrackedSlingers;
 for(const auto& Active:Side.ActiveSlingers){
  const auto* Unit=W.Find(Active.Key);
  if(!Unit||!BalanceNearSeat(SeatEntity,*Unit))continue;
  auto& Life=Side.SlingerLives[Active.Value];
  if(Life.FirstNearSeatStep<0){Life.FirstNearSeatStep=StepIndex;Life.FirstNearSeatTime=W.Time;}
  Life.LastNearSeatStep=StepIndex;
  Life.LastNearSeatTime=W.Time;
  ++Life.NearSeatSteps;
  ++Side.TrackedSlingerSeatSteps;
  if(bCheckpoint)NearbyTrackedSlingers.Add(Unit->Id);
 }
 if(!bCheckpoint)return;
 if(Side.DefenseSamples.Num()>=RTSBalanceDefenseRecordLimit){++Side.DroppedDefenseSamples;return;}

 const auto& Catalog=FRTSCatalog::Get();
 int32 Defenders=0,DefenderPopulation=0,LivingSlingers=0,LocalSlingers=0;
 int32 Jobs=0,CampJobs=0,MilitaryJobs=0,SlingerJobs=0,TeacherJobs=0;
 double DefenderHP=0,DefenderMaxHP=0;
 FVector DefenderCost=FVector::ZeroVector;
 for(const auto& KV:W.Entities){
  const auto& E=*KV.Value;
  if(E.Owner!=P.Slot||E.HP<=0)continue;
  const auto& Def=W.D(E);
  if(Def.bBuilding){
   Jobs+=E.Jobs.Num();
   if(Def.Id==TEXT("encampment_of_war"))CampJobs+=E.Jobs.Num();
   for(const auto& Job:E.Jobs){
    const auto& JobDef=Catalog.Def(Job.Type);
    MilitaryJobs+=RTS::IsMilitary(JobDef)&&JobDef.Kind!=TEXT("ship");
    SlingerJobs+=Job.Type==TEXT("slinger_of_stones");
    TeacherJobs+=Job.Type==TEXT("teacher_of_the_word");
   }
   continue;
  }
  const bool bNearSeat=BalanceNearSeat(SeatEntity,E);
  if(Def.Id==TEXT("slinger_of_stones")){
   LivingSlingers+=!E.Carrier;
   LocalSlingers+=bNearSeat;
  }
  if(!bNearSeat||E.Progress<1||E.DisarmedUntil>W.Time||!RTS::IsMilitary(Def)||RTS::IsReligious(Def)||
     Def.Kind==TEXT("ship")||Def.Id==TEXT("wilderness_spy"))continue;
  ++Defenders;
  DefenderPopulation+=Def.Pop;
  DefenderHP+=E.HP;
  DefenderMaxHP+=E.MaxHP;
  DefenderCost+=Catalog.Price(Def,P.Faction);
 }
 auto Record=MakeShared<FJsonObject>();
 Record->SetNumberField(TEXT("step"),StepIndex);
 Record->SetNumberField(TEXT("checkpoint_seconds"),10*(StepIndex/RTSBalanceDefenseSampleSteps));
 Record->SetNumberField(TEXT("time_seconds"),W.Time);
 Record->SetNumberField(TEXT("age"),P.Age);
 Record->SetBoolField(TEXT("last_decision_visible_base_threat"),P.Bot.EmergencySlingerTrace.bObservedBaseThreat);
 Record->SetBoolField(TEXT("defeated"),P.bDefeated);
 Record->SetNumberField(TEXT("seat_id"),Side.Final.PrimarySeatId);
 Record->SetNumberField(TEXT("completed_seats"),Side.Final.Seats);
 Record->SetNumberField(TEXT("seat_hp"),SeatEntity?SeatEntity->HP:0);
 Record->SetNumberField(TEXT("seat_max_hp"),SeatEntity?SeatEntity->MaxHP:0);
 Record->SetNumberField(TEXT("owned_live_land_army"),Side.Final.PlannerArmy);
 Record->SetNumberField(TEXT("nearby_defenders"),Defenders);
 Record->SetNumberField(TEXT("nearby_defender_population"),DefenderPopulation);
 Record->SetNumberField(TEXT("nearby_defender_hp"),DefenderHP);
 Record->SetNumberField(TEXT("nearby_defender_max_hp"),DefenderMaxHP);
 Record->SetObjectField(TEXT("nearby_defender_catalog_cost"),BalanceVector(DefenderCost));
 Record->SetNumberField(TEXT("worker_combat_losses"),Side.WorkerBattleLosses);
 Record->SetNumberField(TEXT("total_jobs"),Jobs);
 Record->SetNumberField(TEXT("camp_jobs"),CampJobs);
 Record->SetNumberField(TEXT("queued_land_military"),MilitaryJobs);
 Record->SetNumberField(TEXT("queued_slingers"),SlingerJobs);
 Record->SetNumberField(TEXT("queued_teachers"),TeacherJobs);
 Record->SetNumberField(TEXT("living_unembarked_slingers"),LivingSlingers);
 Record->SetNumberField(TEXT("slingers_near_seat"),LocalSlingers);
 Record->SetNumberField(TEXT("tracked_slinger_steps_near_seat"),Side.TrackedSlingerSeatSteps);
 NearbyTrackedSlingers.Sort();
 TArray<TSharedPtr<FJsonValue>> NearbyIds;
 for(int32 Id:NearbyTrackedSlingers)NearbyIds.Add(MakeShared<FJsonValueNumber>(Id));
 Record->SetArrayField(TEXT("nearby_trial_slinger_ids"),NearbyIds);
 Side.DefenseSamples.Add(MakeShared<FJsonValueObject>(Record));
}

TSharedPtr<FJsonObject> BalanceLocalDefenseReport(const FBalanceSide& Side) {
 auto Result=MakeShared<FJsonObject>();
 Result->SetNumberField(TEXT("radius_m"),RTSBalanceSeatDefenseRadius);
 Result->SetNumberField(TEXT("checkpoint_interval_steps"),RTSBalanceDefenseSampleSteps);
 Result->SetNumberField(TEXT("checkpoint_interval_seconds"),10);
 Result->SetNumberField(TEXT("sample_limit"),RTSBalanceDefenseRecordLimit);
 Result->SetNumberField(TEXT("slinger_lifecycle_limit"),RTSBalanceDefenseRecordLimit);
 Result->SetNumberField(TEXT("dropped_samples"),Side.DroppedDefenseSamples);
 Result->SetNumberField(TEXT("dropped_slinger_lifecycles"),Side.DroppedSlingerLives);
 Result->SetStringField(TEXT("slinger_lifecycle_scope"),TEXT("Newly observed unconverted owned Slingers at trial step end. Production queue orders are not linked to unit IDs."));
 Result->SetArrayField(TEXT("samples"),Side.DefenseSamples);
 TArray<TSharedPtr<FJsonValue>> Lives;
 for(const auto& Life:Side.SlingerLives){
  auto Entry=MakeShared<FJsonObject>();
  Entry->SetNumberField(TEXT("unit_id"),Life.Id);
  Entry->SetNumberField(TEXT("appearance_step"),Life.AppearanceStep);
  Entry->SetNumberField(TEXT("appearance_seconds"),Life.AppearanceTime);
  Entry->SetNumberField(TEXT("first_near_seat_step"),Life.FirstNearSeatStep);
  Entry->SetNumberField(TEXT("first_near_seat_seconds"),Life.FirstNearSeatTime);
  Entry->SetNumberField(TEXT("last_near_seat_step"),Life.LastNearSeatStep);
  Entry->SetNumberField(TEXT("last_near_seat_seconds"),Life.LastNearSeatTime);
  Entry->SetNumberField(TEXT("near_seat_steps"),Life.NearSeatSteps);
  Entry->SetNumberField(TEXT("near_seat_seconds"),Life.NearSeatSteps*double(RTS::Step));
  Entry->SetNumberField(TEXT("end_step"),Life.EndStep);
  Entry->SetNumberField(TEXT("end_seconds"),Life.EndTime);
  Entry->SetStringField(TEXT("end_observation"),Life.EndObservation);
  if(Life.NewOwner!=INDEX_NONE)Entry->SetNumberField(TEXT("new_owner"),Life.NewOwner);
  Lives.Add(MakeShared<FJsonValueObject>(Entry));
 }
 Result->SetArrayField(TEXT("trial_slinger_lifecycles"),Lives);
 return Result;
}

TSharedPtr<FJsonObject> BalanceTeacherSnapshot(const FRTSTeacherProductionSnapshot& Observation) {
 auto Result=MakeShared<FJsonObject>();
 Result->SetObjectField(TEXT("resources"),BalanceVector(Observation.Resources));
 Result->SetNumberField(TEXT("free_population"),Observation.FreePopulation);
 Result->SetNumberField(TEXT("temple_jobs"),Observation.TempleJobs);
 Result->SetNumberField(TEXT("military_jobs"),Observation.MilitaryJobs);
 Result->SetBoolField(TEXT("can_afford_teacher"),Observation.bCanAfford);
 return Result;
}

TSharedPtr<FJsonObject> BalanceFirstTeacher(const FRTSFirstTeacherTrace& Trace) {
 auto Result=MakeShared<FJsonObject>();
 Result->SetNumberField(TEXT("first_attempt_seconds"),Trace.FirstAttempt);
 Result->SetNumberField(TEXT("first_queued_seconds"),Trace.FirstQueued);
 Result->SetNumberField(TEXT("dropped_transitions"),Trace.DroppedTransitions);
 TArray<TSharedPtr<FJsonValue>> Events;
 for(const auto& Observation:Trace.Events){
  auto Entry=MakeShared<FJsonObject>();
  Entry->SetNumberField(TEXT("time_seconds"),Observation.Time);
  Entry->SetStringField(TEXT("gate"),Observation.Gate);
  Entry->SetStringField(TEXT("blocking_order"),Observation.BlockingOrder);
  Entry->SetNumberField(TEXT("temple_id"),Observation.TempleId);
  Entry->SetNumberField(TEXT("age"),Observation.Age);
  Entry->SetNumberField(TEXT("army_units"),Observation.Army);
  Entry->SetNumberField(TEXT("nearby_defenders"),Observation.NearbyDefenders);
  Entry->SetBoolField(TEXT("visible_base_threat"),Observation.bVisibleBaseThreat);
  Entry->SetBoolField(TEXT("saving_age"),Observation.bSavingAge);
  Entry->SetBoolField(TEXT("age_reserve"),Observation.bAgeReserve);
  Entry->SetObjectField(TEXT("before"),BalanceTeacherSnapshot(Observation.Before));
  Entry->SetObjectField(TEXT("after"),BalanceTeacherSnapshot(Observation.After));
  TArray<TSharedPtr<FJsonValue>> Queued;
  for(const auto& Type:Observation.QueuedOrders)Queued.Add(MakeShared<FJsonValueString>(Type));
  Entry->SetArrayField(TEXT("queued_orders"),Queued);
  Events.Add(MakeShared<FJsonValueObject>(Entry));
 }
 Result->SetArrayField(TEXT("events"),Events);
 return Result;
}

void BalanceRecordSupport(ARTSWorld& W,FBalanceSide& Side,const FRTSEntity& E,const TCHAR* Event,const FBalanceSupportState* Previous) {
 if(Side.SupportEvents.Num()>=192){++Side.DroppedSupportEvents;return;}
 // This observer sees authoritative end-of-step state for diagnostics only.
 auto Record=MakeShared<FJsonObject>();
 Record->SetNumberField(TEXT("time_seconds"),W.Time);
 Record->SetStringField(TEXT("event"),Event);
 Record->SetNumberField(TEXT("unit_id"),E.Id);
 Record->SetStringField(TEXT("unit_type"),W.D(E).Id);
 Record->SetStringField(TEXT("order"),E.Order);
 Record->SetNumberField(TEXT("position_x"),E.Pos.X);
 Record->SetNumberField(TEXT("position_y"),E.Pos.Y);
 Record->SetNumberField(TEXT("rally_x"),E.Rally.X);
 Record->SetNumberField(TEXT("rally_y"),E.Rally.Y);
 Record->SetNumberField(TEXT("hp"),E.HP);
 Record->SetNumberField(TEXT("max_hp"),E.MaxHP);
 Record->SetNumberField(TEXT("channel_seconds"),E.Channel);
 Record->SetNumberField(TEXT("last_damage_seconds"),E.LastDamage);
 if(Previous){
  Record->SetStringField(TEXT("prior_order"),Previous->Order);
  Record->SetNumberField(TEXT("prior_channel_seconds"),Previous->Channel);
 }
 Record->SetNumberField(TEXT("target_id"),E.Target);
 const int32 PreviousTarget=Previous?Previous->Target:0;
 Record->SetNumberField(TEXT("prior_target_id"),PreviousTarget);
 if(const auto* Target=W.Find(PreviousTarget?PreviousTarget:E.Target)){
  Record->SetNumberField(TEXT("observed_target_id"),Target->Id);
  Record->SetNumberField(TEXT("observed_target_owner"),Target->Owner);
  Record->SetNumberField(TEXT("observed_target_hp"),Target->HP);
  Record->SetStringField(TEXT("observed_target_type"),W.D(*Target).Id);
  Record->SetNumberField(TEXT("observed_target_distance"),FVector2D::Distance(E.Pos,Target->Pos));
  Record->SetBoolField(TEXT("observed_target_line_of_sight"),W.LOS(E.Pos,Target->Pos,E.Id,Target->Id));
  Record->SetBoolField(TEXT("observed_target_visible"),W.CanSee(E.Owner,*Target));
 }
 Side.SupportEvents.Add(MakeShared<FJsonValueObject>(Record));
}

void BalanceObserveSupport(ARTSWorld& W,FBalanceSide& Side,const FRTSEntity& E) {
 if(auto* Previous=Side.SupportStates.Find(E.Id)){
  if(E.Channel+KINDA_SMALL_NUMBER<Previous->Channel){
   const auto* Target=W.Find(Previous->Target);
   const bool bTargetConverted=Target&&Target->bConverted&&Target->Owner==E.Owner;
   BalanceRecordSupport(W,Side,E,bTargetConverted?TEXT("target_converted"):TEXT("channel_reset"),Previous);
  }else if(E.Channel>0&&Previous->Channel<=0){
   BalanceRecordSupport(W,Side,E,TEXT("channel_started"),Previous);
  }else if(E.Order!=Previous->Order||E.Target!=Previous->Target){
   BalanceRecordSupport(W,Side,E,TEXT("order_changed"),Previous);
  }
  Previous->Order=E.Order;
  Previous->Target=E.Target;
  Previous->Channel=E.Channel;
 }else{
  BalanceRecordSupport(W,Side,E,TEXT("first_observed"),nullptr);
  Side.SupportStates.Add(E.Id,{E.Order,E.Target,E.Channel});
 }
}

TSharedPtr<FJsonObject> BalancePlayerReport(const FRTSPlayer& P,const FBalanceSide& Side,const FBalanceSide& Opposing) {
 auto Result=MakeShared<FJsonObject>();
 Result->SetStringField(TEXT("faction"),P.Faction);
 Result->SetNumberField(TEXT("slot"),P.Slot);
 Result->SetNumberField(TEXT("team"),P.Team);
 Result->SetBoolField(TEXT("defeated"),P.bDefeated);
 Result->SetNumberField(TEXT("first_deposit_seconds"),Side.FirstDeposit);
 Result->SetNumberField(TEXT("first_construction_seconds"),Side.FirstConstruction);
 Result->SetNumberField(TEXT("age_2_seconds"),Side.FirstAgeII);
 Result->SetNumberField(TEXT("age_3_seconds"),Side.FirstAgeIII);
 Result->SetNumberField(TEXT("age_4_seconds"),Side.FirstAgeIV);
 Result->SetNumberField(TEXT("first_research_seconds"),Side.FirstResearch);
 Result->SetNumberField(TEXT("first_faith_building_seconds"),Side.FirstFaithBuilding);
 Result->SetNumberField(TEXT("first_camp_started_seconds"),Side.FirstCampStarted);
 Result->SetNumberField(TEXT("first_camp_completed_seconds"),Side.FirstCampComplete);
 Result->SetNumberField(TEXT("first_temple_started_seconds"),Side.FirstTempleStarted);
 Result->SetNumberField(TEXT("first_workshop_started_seconds"),Side.FirstWorkshopStarted);
 Result->SetNumberField(TEXT("first_workshop_completed_seconds"),Side.FirstWorkshopComplete);
 Result->SetNumberField(TEXT("first_converter_seconds"),Side.FirstConverter);
 Result->SetNumberField(TEXT("first_conversion_channel_seconds"),Side.FirstConversionChannel);
 Result->SetNumberField(TEXT("first_military_seconds"),Side.FirstMilitary);
 Result->SetNumberField(TEXT("first_campaign_seconds"),Side.FirstCampaign);
 Result->SetNumberField(TEXT("first_attack_seconds"),Side.FirstAttack);
 Result->SetNumberField(TEXT("last_attack_seconds"),Side.LastAttack);
 Result->SetNumberField(TEXT("first_damage_taken_seconds"),Side.FirstDamageTaken);
 Result->SetNumberField(TEXT("last_damage_taken_seconds"),Side.LastDamageTaken);
 Result->SetNumberField(TEXT("first_seat_loss_seconds"),Side.FirstSeatLoss);
 Result->SetNumberField(TEXT("first_raid_seconds"),Side.FirstRaid);
 Result->SetNumberField(TEXT("peak_raider_cargo"),Side.PeakRaiderCargo);
 Result->SetNumberField(TEXT("attack_events"),Side.AttackEvents);
 Result->SetNumberField(TEXT("damaged_victim_steps"),Side.DamageTicks);
 Result->SetNumberField(TEXT("observed_damage_taken"),Side.ObservedDamageTaken);
 Result->SetNumberField(TEXT("observed_damage_dealt"),Opposing.ObservedDamageTaken);
 Result->SetNumberField(TEXT("observed_combat_losses"),Side.BattleLosses);
 Result->SetNumberField(TEXT("observed_combat_kills"),Opposing.BattleLosses);
 Result->SetNumberField(TEXT("worker_combat_losses"),Side.WorkerBattleLosses);
 Result->SetNumberField(TEXT("military_combat_losses"),Side.MilitaryBattleLosses);
 Result->SetNumberField(TEXT("building_combat_losses"),Side.BuildingBattleLosses);
 Result->SetNumberField(TEXT("economy_building_combat_losses"),Side.EconomyBuildingBattleLosses);
 Result->SetNumberField(TEXT("seat_losses"),Side.SeatLosses);
 Result->SetNumberField(TEXT("other_owned_removals"),Side.OtherRemovals);
 Result->SetNumberField(TEXT("converted_away"),Side.ConvertedAway);
 Result->SetNumberField(TEXT("converted_in"),Side.ConvertedIn);
 Result->SetNumberField(TEXT("peak_workers"),Side.PeakWorkers);
 Result->SetNumberField(TEXT("peak_army_units"),Side.PeakArmy);
 Result->SetNumberField(TEXT("peak_army_population"),Side.PeakArmyPop);
 Result->SetNumberField(TEXT("peak_population_with_reservations"),Side.PeakPopulation);
 Result->SetNumberField(TEXT("peak_capacity"),Side.PeakCapacity);
 Result->SetNumberField(TEXT("steps_above_remaining_capacity"),Side.StepsOverCapacity);
 Result->SetNumberField(TEXT("peak_army_resource_cost"),Side.PeakArmyResourceValue);
 Result->SetObjectField(TEXT("ending_resources"),BalanceVector(P.Resources));
 Result->SetObjectField(TEXT("ending_army_cost"),BalanceVector(Side.Final.ArmyCost));
 Result->SetNumberField(TEXT("ending_army_cement_cost"),Side.Final.ArmyCementCost);
 Result->SetNumberField(TEXT("ending_cement"),P.Cement);
 Result->SetNumberField(TEXT("ending_age"),P.Age);
 Result->SetNumberField(TEXT("ending_favor"),P.Favor);
 Result->SetNumberField(TEXT("ending_workers"),Side.Final.Workers);
 Result->SetNumberField(TEXT("ending_idle_workers"),Side.Final.IdleWorkers);
 Result->SetNumberField(TEXT("ending_working_workers"),Side.Final.GatheringWorkers);
 Result->SetNumberField(TEXT("ending_army_units"),Side.Final.Army);
 Result->SetNumberField(TEXT("ending_army_population"),Side.Final.ArmyPop);
 Result->SetNumberField(TEXT("ending_population_with_reservations"),P.Population);
 Result->SetNumberField(TEXT("ending_capacity"),P.Capacity);
 Result->SetNumberField(TEXT("ending_seats"),Side.Final.Seats);
 Result->SetNumberField(TEXT("ending_builder_beasts"),Side.Final.Beasts);
 Result->SetNumberField(TEXT("ending_economy_buildings"),Side.Final.EconomyBuildings);
 Result->SetNumberField(TEXT("ending_scout_visits"),P.Bot.ScoutVisits.Num());
 Result->SetNumberField(TEXT("ending_known_enemy_buildings"),P.Bot.EnemyBuildings.Num());
 Result->SetBoolField(TEXT("ending_campaign"),P.Bot.bCampaign);
 if(P.Bot.GadiantonRaidTrace.bEnabled)Result->SetObjectField(TEXT("gadianton_raids"),BalanceGadiantonRaids(P,Side));
 if(P.Bot.EmergencySlingerMode!=ERTSEmergencySlingerMode::Inactive){
  const auto& Trace=P.Bot.EmergencySlingerTrace;
  auto Observation=MakeShared<FJsonObject>();
  Observation->SetNumberField(TEXT("eligible_camp_decisions"),Trace.EligibleCamps);
  Observation->SetNumberField(TEXT("slinger_attempts"),Trace.Attempts);
  Observation->SetNumberField(TEXT("slingers_queued"),Trace.Queued);
  Observation->SetNumberField(TEXT("first_eligible_seconds"),Trace.FirstEligible);
  Observation->SetNumberField(TEXT("first_queued_seconds"),Trace.FirstQueued);
  Observation->SetNumberField(TEXT("age_ii_understrength_threat_steps"),Side.AgeIIUnderstrengthThreatSteps);
  Observation->SetNumberField(TEXT("age_ii_understrength_threat_seconds"),Side.AgeIIUnderstrengthThreatSteps*double(RTS::Step));
  Observation->SetObjectField(TEXT("local_defense"),BalanceLocalDefenseReport(Side));
  Result->SetObjectField(TEXT("emergency_slinger"),Observation);
 }
 if(RTSBotSupportTraceEnabled()){
  Result->SetArrayField(TEXT("support_events"),Side.SupportEvents);
  Result->SetNumberField(TEXT("dropped_support_events"),Side.DroppedSupportEvents);
  if(P.Faction==TEXT("nephites"))Result->SetObjectField(TEXT("first_teacher_eligibility"),BalanceFirstTeacher(P.Bot.FirstTeacherTrace));
 }
 if(Side.Contact.Time>=0){
  const auto& Contact=Side.Contact;
  auto First=MakeShared<FJsonObject>();
  First->SetNumberField(TEXT("time_seconds"),Contact.Time);
  First->SetNumberField(TEXT("age_before_step"),Contact.Age);
  First->SetNumberField(TEXT("favor_before_step"),Contact.Favor);
  First->SetObjectField(TEXT("resources_before_step"),BalanceVector(Contact.Resources));
  First->SetNumberField(TEXT("workers"),Contact.Roster.Workers);
  First->SetNumberField(TEXT("army_units"),Contact.Roster.Army);
  First->SetNumberField(TEXT("army_population"),Contact.Roster.ArmyPop);
  First->SetObjectField(TEXT("army_cost"),BalanceVector(Contact.Roster.ArmyCost));
  First->SetNumberField(TEXT("army_current_hp"),Contact.Roster.ArmyHealth);
  First->SetNumberField(TEXT("army_max_hp"),Contact.Roster.ArmyMaxHealth);
  First->SetNumberField(TEXT("faith_buildings"),Contact.Roster.FaithBuildings);
  First->SetNumberField(TEXT("converters"),Contact.Roster.Converters);
  First->SetNumberField(TEXT("active_conversions"),Contact.Roster.ActiveConversions);
  First->SetNumberField(TEXT("ammonites"),Contact.Roster.Ammonites);
  if(Contact.bTracedOpening){
   auto ArmyByType=MakeShared<FJsonObject>();
   for(const auto& Entry:Contact.ArmyByDef)ArmyByType->SetNumberField(FRTSCatalog::Get().Def(Entry.Key).Id,Entry.Value);
   First->SetObjectField(TEXT("army_by_type"),ArmyByType);
   First->SetObjectField(TEXT("worker_resource_pulses_before_contact"),BalanceVector(Contact.WorkerPulses));
   First->SetObjectField(TEXT("worker_assignments_before_contact"),BalanceVector(Contact.WorkerAssignments));
   First->SetNumberField(TEXT("surplus_ore_diversions_before_contact"),Contact.SurplusOreDiversions);
   First->SetNumberField(TEXT("defenders_within_24m_of_seat"),Contact.NearbyDefenders);
   First->SetNumberField(TEXT("defender_max_hp_within_24m_of_seat"),Contact.NearbyDefenderMaxHealth);
  }
  Result->SetObjectField(TEXT("first_battle_contact"),First);
 }
 return Result;
}

void BalanceBefore(ARTSWorld& W,TArray<FBalanceBeforeStep>& Before,bool bTraceOpening) {
 Before.Reset();
 for(const auto& KV:W.Entities){
  const auto& E=*KV.Value;
  if(E.Owner<0||E.Owner>1)continue;
  const auto& Def=W.D(E);
  Before.Add({KV.Value,E.Owner,E.MaxHP>0?E.HP/E.MaxHP:0,E.DeathAt>0,Def.bBuilding,BalanceEconomyBuilding(Def),
   Def.Id==TEXT("seat_of_the_people"),RTS::IsWorker(Def),RTS::IsMilitary(Def)});
  if(bTraceOpening&&Before.Last().bWorker){
   Before.Last().ResourcePulse=E.ResourceEvent;
   Before.Last().ResourcePulseTime=E.ResourceEventTime;
  }
 }
}

void BalanceAfter(ARTSWorld& W,const TArray<FBalanceBeforeStep>& Before,int32 FirstNewId,int32 StepIndex,
 FBalanceSide (&Sides)[2],bool bTraceSupport,bool bTraceOpening) {
 for(const auto& Previous:Before){
  const auto& E=*Previous.Body;
  auto& Side=Sides[Previous.Owner];
  if(bTraceOpening&&Previous.bWorker&&E.Owner==Previous.Owner&&E.ResourceEventTime>=0){
   Side.OpeningWorkerPulses+=E.ResourceEventTime==Previous.ResourcePulseTime?E.ResourceEvent-Previous.ResourcePulse:E.ResourceEvent;
  }
  const bool bDamaged=E.LastDamage==W.Time;
  if(bDamaged){
   ++Side.DamageTicks;
   BalanceWhen(Side.FirstDamageTaken,W.Time);
   Side.LastDamageTaken=W.Time;
   // Strip HP changes from Favor, auras, and construction scaling. A repair
   // in the same step can still reduce the observed damage.
   Side.ObservedDamageTaken+=FMath::Max(0.f,Previous.HealthFraction*E.MaxHP-FMath::Max(0.f,E.HP));
  }
  if(E.AttackEventTime==W.Time)BalanceAttack(Side,W.Time);
  const bool bPresent=W.Entities.Contains(E.Id);
  if(!bPresent||E.Owner!=Previous.Owner){
   FBalanceBandState BandState;
   if(Side.Bands.RemoveAndCopyValue(E.Id,BandState)){
    ++Side.BandsLost;
    BalanceBandEvent(W,Side,E,bPresent?TEXT("ownership_changed"):TEXT("removed"),&BandState);
   }
  }
  if((!bPresent||E.Owner!=Previous.Owner)&&!Side.ActiveSlingers.IsEmpty())
   BalanceSlingerEnd(Side,E,StepIndex,W.Time,bPresent,bDamaged);
  if(bTraceSupport&&(!bPresent||E.Owner!=Previous.Owner)){
   if(const auto* SupportState=Side.SupportStates.Find(E.Id)){
    const TCHAR* Event=bPresent?TEXT("ownership_changed"):bDamaged||Previous.bMortalWound?TEXT("combat_death"):TEXT("removed");
    BalanceRecordSupport(W,Side,E,Event,SupportState);
    Side.SupportStates.Remove(E.Id);
   }
  }
  if(!bPresent){
   if(Previous.bSeat){
    ++Side.SeatLosses;
    BalanceWhen(Side.FirstSeatLoss,W.Time);
   }
   if(bDamaged||Previous.bMortalWound){
    ++Side.BattleLosses;
    Side.WorkerBattleLosses+=Previous.bWorker;
    Side.MilitaryBattleLosses+=Previous.bMilitary;
    Side.BuildingBattleLosses+=Previous.bBuilding;
    Side.EconomyBuildingBattleLosses+=Previous.bEconomyBuilding;
   }else ++Side.OtherRemovals;
  }else if(E.Owner!=Previous.Owner&&E.Owner>=0&&E.Owner<2){
   ++Side.ConvertedAway;
   ++Sides[E.Owner].ConvertedIn;
  }
 }

 for(auto& Side:Sides)Side.Final=FBalanceRoster();
 for(const auto& KV:W.Entities){
  const auto& E=*KV.Value;
  if(E.Owner<0||E.Owner>1)continue;
  const auto& P=W.Players[E.Owner];
  const auto& Def=W.D(E);
  auto& Side=Sides[E.Owner];
  auto& Live=Side.Final;
  if(P.Bot.GadiantonRaidTrace.bEnabled){
   auto& Trace=W.Players[E.Owner].Bot.GadiantonRaidTrace;
   if(Def.Id==TEXT("gadianton_robber"))BalanceObserveBand(W,Side,E);
   if(Def.Id==TEXT("storehouse_of_provisions")){
    BalanceWhen(Trace.FirstStoreStarted,W.Time);
    BalanceWhen(Trace.FirstStoreComplete,W.Time,E.Progress>=1);
    BalanceWhen(Trace.FirstForagingQueued,W.Time,E.Jobs.ContainsByPredicate([](const FRTSJob& Job){
     return Job.Type==TEXT("gadianton_storehouse_wild_food");
    }));
   }
  }
  if(P.Bot.EmergencySlingerMode==ERTSEmergencySlingerMode::Priority&&E.Id>=FirstNewId&&
     E.HP>0&&!E.bConverted&&Def.Id==TEXT("slinger_of_stones"))BalanceSlingerAppearance(Side,E,StepIndex,W.Time);
  if(RTS::IsReligious(Def)){
   if(bTraceSupport)BalanceObserveSupport(W,Side,E);
   ++Live.Converters;
   Live.ActiveConversions+=E.Order==TEXT("convert")&&E.Channel>0;
   BalanceWhen(Side.FirstConverter,W.Time);
   BalanceWhen(Side.FirstConversionChannel,W.Time,E.Order==TEXT("convert")&&E.Channel>0);
  }
  Live.Ammonites+=Def.Kind==TEXT("ammonite");
  if(E.Id>=FirstNewId&&E.AttackEventTime==W.Time)BalanceAttack(Side,W.Time);
  if(RTS::IsWorker(Def)){
   ++Live.Workers;
   Live.IdleWorkers+=E.Order==TEXT("idle");
   Live.GatheringWorkers+=E.Order==TEXT("gather")||E.Order==TEXT("deliver");
   BalanceWhen(Side.FirstDeposit,W.Time,E.ResourceEventTime>=0&&E.ResourceEvent.SizeSquared()>0);
  }
  if(RTS::IsMilitary(Def)){
   ++Live.Army;
   Live.PlannerArmy+=Def.Kind!=TEXT("ship")&&!E.Carrier&&E.HP>0;
   Live.ArmyPop+=Def.Pop;
   Live.ArmyCost+=FRTSCatalog::Get().Price(Def,P.Faction);
   Live.ArmyCementCost+=Def.CementCost;
   Live.ArmyHealth+=E.HP;
   Live.ArmyMaxHealth+=E.MaxHP;
   BalanceWhen(Side.FirstMilitary,W.Time,!Def.bHero);
  }
  if(Def.bBuilding){
   BalanceWhen(Side.FirstCampStarted,W.Time,Def.Id==TEXT("encampment_of_war"));
   BalanceWhen(Side.FirstCampComplete,W.Time,Def.Id==TEXT("encampment_of_war")&&E.Progress>=1);
   BalanceWhen(Side.FirstTempleStarted,W.Time,Def.Id==TEXT("temple_of_the_land"));
   BalanceWhen(Side.FirstWorkshopStarted,W.Time,Def.Id==TEXT("house_of_curious_workmanship"));
   BalanceWhen(Side.FirstWorkshopComplete,W.Time,Def.Id==TEXT("house_of_curious_workmanship")&&E.Progress>=1);
  }
  if(Def.bBuilding&&E.Progress>=1){
   Live.Seats+=Def.Id==TEXT("seat_of_the_people");
   if(Def.Id==TEXT("seat_of_the_people")&&E.HP>0&&(!Live.PrimarySeatId||E.Id<Live.PrimarySeatId))Live.PrimarySeatId=E.Id;
   Live.EconomyBuildings+=BalanceEconomyBuilding(Def);
   BalanceWhen(Side.FirstConstruction,W.Time,Def.Id!=TEXT("seat_of_the_people"));
   const bool Faith=Def.Id==TEXT("temple_of_the_land")||Def.Id==TEXT("secret_combination_lodge")||Def.Id==TEXT("winery");
   Live.FaithBuildings+=Faith;
   BalanceWhen(Side.FirstFaithBuilding,W.Time,Faith);
  }
  Live.Beasts+=Def.Kind==TEXT("beast");
  if(E.Order==TEXT("raid")||E.Order==TEXT("raid_return")){
   BalanceWhen(Side.FirstRaid,W.Time);
   Side.PeakRaiderCargo=FMath::Max(Side.PeakRaiderCargo,E.Cargo.X+E.Cargo.Y+E.Cargo.Z);
  }
 }

 for(int32 Slot=0;Slot<2;++Slot){
  const auto& P=W.Players[Slot];
  auto& Side=Sides[Slot];
  if(P.Bot.GadiantonRaidTrace.bEnabled)BalanceWhen(W.Players[Slot].Bot.GadiantonRaidTrace.FirstForagingComplete,W.Time,
   P.Upgrades.Contains(TEXT("gadianton_storehouse_wild_food")));
  if(P.Bot.EmergencySlingerMode!=ERTSEmergencySlingerMode::Inactive&&P.Age==2&&!P.bDefeated&&
     P.Bot.EmergencySlingerTrace.bObservedBaseThreat&&Side.Final.PlannerArmy<4)++Side.AgeIIUnderstrengthThreatSteps;
  if(P.Bot.EmergencySlingerMode!=ERTSEmergencySlingerMode::Inactive)BalanceObserveLocalDefense(W,P,Side,StepIndex);
  BalanceWhen(Side.FirstAgeII,W.Time,P.Age>=2);
  BalanceWhen(Side.FirstAgeIII,W.Time,P.Age>=3);
  BalanceWhen(Side.FirstAgeIV,W.Time,P.Age>=4);
  BalanceWhen(Side.FirstCampaign,W.Time,P.Bot.bCampaign);
  if(Side.FirstResearch<0)BalanceWhen(Side.FirstResearch,W.Time,P.Upgrades.ContainsByPredicate([](const FString& Id){
   const auto& Def=FRTSCatalog::Get().Def(Id);
   return Def.bUpgrade&&Def.Kind!=TEXT("age_option");
  }));
  Side.PeakWorkers=FMath::Max(Side.PeakWorkers,Side.Final.Workers);
  Side.PeakArmy=FMath::Max(Side.PeakArmy,Side.Final.Army);
  Side.PeakArmyPop=FMath::Max(Side.PeakArmyPop,Side.Final.ArmyPop);
  Side.PeakPopulation=FMath::Max(Side.PeakPopulation,P.Population);
  Side.PeakCapacity=FMath::Max(Side.PeakCapacity,P.Capacity);
  Side.PeakArmyResourceValue=FMath::Max(Side.PeakArmyResourceValue,Side.Final.ArmyCost.X+Side.Final.ArmyCost.Y+Side.Final.ArmyCost.Z);
  Side.StepsOverCapacity+=P.Population>P.Capacity;
 }
}

TSharedPtr<FJsonObject> BalanceMatch(ARTSWorld& W,int32 Pair,int32 Leg,const FString& First,const FString& Second,int32 Map,int32 StepLimit,
 ERTSEmergencySlingerMode SlingerMode,bool bGadiantonCombatFirst,bool bGadiantonFoodAssignment) {
 W.Entities.Empty();W.Players.Reset();W.Obstacles.Reset();
 W.Phase=0;W.Time=0;W.Winner=-1;W.SnapshotTime=0;W.GameSpeed=1;
 W.StartingResources=1;W.Fruitfulness=1;W.bFogOfWar=true;
 for(int32 Slot=0;Slot<2;++Slot){
  FRTSPlayer P;
  P.Slot=P.Team=Slot;
  P.Faction=Slot==0?First:Second;
  P.Name=P.Faction;
  P.bBot=P.bReady=true;
  P.BotDifficulty=1;
  W.Players.Add(MoveTemp(P));
 }
 W.Start(nullptr,Map);
 const bool bTraceRaids=FParse::Param(FCommandLine::Get(),TEXT("RTSV09BotRaidTrace"));
 const bool bTraceGadiantonOpening=bTraceRaids&&(First==TEXT("gadianton")||Second==TEXT("gadianton"));
 for(auto& P:W.Players){
  if(P.Faction==TEXT("nephites"))P.Bot.EmergencySlingerMode=SlingerMode;
  P.Bot.GadiantonRaidTrace.bEnabled=bTraceRaids&&P.Faction==TEXT("gadianton");
  P.Bot.bGadiantonCombatFirstForProbe=bGadiantonCombatFirst&&P.Faction==TEXT("gadianton");
  P.Bot.bGadiantonFoodAssignmentForProbe=bGadiantonFoodAssignment&&P.Faction==TEXT("gadianton");
 }
 bool bValid=W.Phase==1&&!W.bSandbox&&W.Players.Num()==2&&W.Map==Map;
 for(const auto& P:W.Players)bValid&=P.bBot&&P.bConnected&&!P.Controller.IsValid()&&P.Age==1&&P.Population==6&&P.Capacity==15&&
  P.Resources.Equals(FVector(300,300,200))&&P.Cement==0;
 FBalanceSide Sides[2];
 const bool bTraceSupport=RTSBotSupportTraceEnabled();
 TArray<FBalanceBeforeStep> Before;
 TArray<double> Times;
 Times.Reserve(StepLimit);
 const double WallStart=FPlatformTime::Seconds();
 UE_LOG(LogTemp,Display,TEXT("RTS_BALANCE_BEGIN pair=%d leg=%d map=%s first=%s second=%s"),Pair,Leg,RTS::MapInfo(W.Map).Id,*First,*Second);
 for(int32 Tick=0;Tick<StepLimit&&W.Phase==1;++Tick){
  const bool bRecordContact=Sides[0].Contact.Time<0;
  const bool bTraceOpening=bRecordContact&&bTraceGadiantonOpening;
  BalanceBefore(W,Before,bTraceOpening);
  FBalanceContact Prior[2];
  if(bRecordContact)for(int32 Slot=0;Slot<2;++Slot){
   const auto& P=W.Players[Slot];
   Prior[Slot]={Sides[Slot].Final,P.Resources,W.Time,P.Favor,P.Age};
   if(bTraceOpening){
    Prior[Slot].bTracedOpening=true;
    Prior[Slot].WorkerPulses=Sides[Slot].OpeningWorkerPulses;
    Prior[Slot].SurplusOreDiversions=P.Bot.GadiantonRaidTrace.SurplusOreDiversions;
   }
  }
  if(bTraceOpening){
   const FRTSEntity* Seats[]={W.Find(Sides[0].Final.PrimarySeatId),W.Find(Sides[1].Final.PrimarySeatId)};
   for(const auto& Previous:Before){
    const auto& E=*Previous.Body;
    auto& Opening=Prior[Previous.Owner];
    if(Previous.bWorker&&(E.Order==TEXT("gather")||E.Order==TEXT("deliver"))){
     if(const auto* Work=W.Find(E.WorkTarget)){
      const int32 Resource=BalanceOpeningWorkResource(W.D(*Work).Id);
      if(Resource!=INDEX_NONE)++Opening.WorkerAssignments[Resource];
     }
    }
    if(!Previous.bMilitary)continue;
    ++Opening.ArmyByDef.FindOrAdd(E.Def);
    const auto& Def=W.D(E);
    if(BalanceNearSeat(Seats[Previous.Owner],E)&&E.Progress>=1&&E.DisarmedUntil<=W.Time&&
       Def.Kind!=TEXT("ship")&&Def.Id!=TEXT("wilderness_spy")&&E.Order!=TEXT("raid")&&
       E.Order!=TEXT("raid_move")&&E.Order!=TEXT("raid_return")&&E.Order!=TEXT("deliver")){
     ++Opening.NearbyDefenders;
     Opening.NearbyDefenderMaxHealth+=E.MaxHP;
    }
   }
  }
  const int32 FirstNewId=W.NextId;
  const double Start=FPlatformTime::Seconds();
  W.Step(RTS::Step);
  Times.Add((FPlatformTime::Seconds()-Start)*1000);
  BalanceAfter(W,Before,FirstNewId,Tick+1,Sides,bTraceSupport,bTraceOpening);
  if(bRecordContact&&(Sides[0].AttackEvents>0||Sides[1].AttackEvents>0||Sides[0].DamageTicks>0||Sides[1].DamageTicks>0)){
   for(int32 Slot=0;Slot<2;++Slot){Prior[Slot].Time=W.Time;Sides[Slot].Contact=Prior[Slot];}
  }
  for(const auto& P:W.Players)bValid&=!P.Resources.ContainsNaN()&&P.Resources.X>=-.001&&P.Resources.Y>=-.001&&P.Resources.Z>=-.001&&
   FMath::IsFinite(P.Cement)&&P.Cement>=-.001&&P.Capacity>=0&&P.Capacity<=200&&P.Population>=0;
 }
 const double WallSeconds=FPlatformTime::Seconds()-WallStart;
 const bool bVictory=W.Phase==2&&W.Players.IsValidIndex(W.Winner);
 auto Result=MakeShared<FJsonObject>();
 Result->SetNumberField(TEXT("pair"),Pair);
 Result->SetNumberField(TEXT("leg"),Leg);
 Result->SetNumberField(TEXT("map"),W.Map);
 Result->SetNumberField(TEXT("winner_slot"),bVictory?W.Winner:-1);
 Result->SetStringField(TEXT("winner_faction"),bVictory?W.Players[W.Winner].Faction:FString());
 Result->SetStringField(TEXT("result"),bVictory?TEXT("victory"):W.Phase==2?TEXT("mutual_defeat"):TEXT("time_limit"));
 Result->SetNumberField(TEXT("game_seconds"),W.Time);
 Result->SetNumberField(TEXT("steps"),Times.Num());
 Result->SetNumberField(TEXT("observer_inclusive_wall_seconds"),WallSeconds);
 Result->SetBoolField(TEXT("valid"),bValid&&!Times.IsEmpty());
 if(!Times.IsEmpty()){
  Times.Sort();
  Result->SetNumberField(TEXT("step_ms_median"),Times[Times.Num()/2]);
  Result->SetNumberField(TEXT("step_ms_p95"),Times[FMath::FloorToInt((Times.Num()-1)*.95)]);
  Result->SetNumberField(TEXT("step_ms_max"),Times.Last());
 }
 TArray<TSharedPtr<FJsonValue>> Players;
 for(int32 Slot=0;Slot<2;++Slot)Players.Add(MakeShared<FJsonValueObject>(BalancePlayerReport(W.Players[Slot],Sides[Slot],Sides[1-Slot])));
 Result->SetArrayField(TEXT("players"),Players);
 UE_LOG(LogTemp,Display,TEXT("RTS_BALANCE_END pair=%d leg=%d winner=%d seconds=%.1f hits=%d,%d battle_losses=%d,%d wall=%.1f valid=%d"),
  Pair,Leg,W.Winner,W.Time,Sides[0].AttackEvents,Sides[1].AttackEvents,Sides[0].BattleLosses,Sides[1].BattleLosses,WallSeconds,bValid);
 return Result;
}

TSharedPtr<FJsonObject> BalanceEmergencySlingerFixture(ARTSWorld& W,ERTSEmergencySlingerMode SlingerMode) {
 const auto& Catalog=FRTSCatalog::Get();
 const TCHAR* FixtureWorker=TEXT("industrious_laborer");
 auto Report=MakeShared<FJsonObject>();
 TArray<TSharedPtr<FJsonValue>> Checks;
 bool bPassed=true;
 for(const bool bHasTimber:{true,false}){
  W.Entities.Reset();W.Players.Reset();W.Obstacles.Reset();
  W.NextId=1;W.Time=120;W.Map=0;W.Phase=1;W.bSandbox=true;W.bFogOfWar=true;W.ObstacleCount=-1;
  W.Players.SetNum(2);
  auto& Bot=W.Players[0];
  Bot.Slot=Bot.Team=0;Bot.bBot=true;Bot.Age=2;Bot.Bot.Home={24,24};
  Bot.Faction=TEXT("nephites");
  Bot.Bot.EmergencySlingerMode=SlingerMode;
  W.Players[1].Slot=W.Players[1].Team=1;
  W.Players[1].Faction=TEXT("laban");
  const FVector Stock(95,bHasTimber?50:0,50);
  Bot.Resources=Stock+Catalog.Price(Catalog.Def(FixtureWorker),Bot.Faction);
  auto* SeatEntity=W.Spawn(TEXT("seat_of_the_people"),0,{24,24});
  auto* CampEntity=W.Spawn(TEXT("encampment_of_war"),0,{28,31});
  const auto* Detector=W.Spawn(TEXT("wilderness_spy"),0,{26,26});
  const auto* Enemy=W.Spawn(TEXT("man_of_the_sword"),1,{31,27});
  const bool bSetup=SeatEntity&&CampEntity&&Detector&&Enemy;
  if(bSetup){
   // A prior laborer order keeps the Seat busy while the Camp chooses reinforcement.
   W.Queue(*SeatEntity,FixtureWorker,nullptr);
   W.UpdateBots();
  }
  const auto& SlingerTrace=Bot.Bot.EmergencySlingerTrace;
  const TCHAR* Expected=bHasTimber?TEXT("slinger_of_stones"):TEXT("man_of_the_sword");
  const FString Actual=CampEntity&&CampEntity->Jobs.Num()==1?CampEntity->Jobs[0].Type:FString();
  const bool bCasePassed=bSetup&&SeatEntity->Jobs.Num()==1&&Actual==Expected&&SlingerTrace.bObservedBaseThreat&&
   SlingerTrace.EligibleCamps==1&&SlingerTrace.Attempts==1&&SlingerTrace.Queued==(bHasTimber?1:0)&&
   Bot.Resources.Equals(Stock-Catalog.Price(Catalog.Def(Expected),Bot.Faction),.001);
  bPassed&=bCasePassed;
  auto Check=MakeShared<FJsonObject>();
  Check->SetStringField(TEXT("name"),bHasTimber?TEXT("slinger_precedes_bowman"):TEXT("blocked_slinger_falls_back_to_swordsman"));
  Check->SetStringField(TEXT("expected"),Expected);
  Check->SetStringField(TEXT("queued"),Actual);
  Check->SetBoolField(TEXT("passed"),bCasePassed);
  Checks.Add(MakeShared<FJsonValueObject>(Check));
 }
 Report->SetBoolField(TEXT("passed"),bPassed);
 Report->SetArrayField(TEXT("checks"),Checks);
 UE_LOG(LogTemp,Display,TEXT("RTS_BALANCE_EMERGENCY_SLINGER_FIXTURE passed=%d"),bPassed);
 return Report;
}
}

bool RunRTSBotBalanceTests(ARTSWorld& W) {
 const TCHAR* CommandLine=FCommandLine::Get();
 int32 Shard=0,ShardCount=1,StepLimit=RTSBalanceMaximumSteps,Map=0;
 FParse::Value(CommandLine,TEXT("RTSBalanceShard="),Shard);
 FParse::Value(CommandLine,TEXT("RTSBalanceShards="),ShardCount);
 FParse::Value(CommandLine,TEXT("RTSBalanceStepLimit="),StepLimit);
 FString MapValue;
 if(FParse::Value(CommandLine,TEXT("RTSBalanceMap="),MapValue,false)||FParse::Param(CommandLine,TEXT("RTSBalanceMap"))){
  TCHAR* End=nullptr;
  const int64 ParsedMap=FCString::Strtoi64(*MapValue,&End,10);
  if(MapValue.IsEmpty()||End!=*MapValue+MapValue.Len()||ParsedMap<0||ParsedMap>=RTS::MapCount()){
   UE_LOG(LogTemp,Error,TEXT("RTSBalanceMap must be an integer from 0 through %d"),RTS::MapCount()-1);
   return false;
  }
  Map=static_cast<int32>(ParsedMap);
 }
 const auto& Factions=FRTSCatalog::Get().Factions;
 if(Factions.Num()!=6||ShardCount<1||ShardCount>15||Shard<0||Shard>=ShardCount||StepLimit<1||StepLimit>RTSBalanceMaximumSteps)return false;
 const FString Directory=FPaths::ProjectSavedDir()/TEXT("Tests");
 IFileManager::Get().MakeDirectory(*Directory,true);
 const FString ReportStem=FString::Printf(TEXT("v09-bot-balance-%02d-of-%02d"),Shard,ShardCount);
 const FString MapSuffix=Map==0?FString():FString::Printf(TEXT("-map-%02d"),Map);
 const FString ReportPath=Directory/(ReportStem+MapSuffix+TEXT(".json"));
 const bool bFirstSupport=RTSBotSupportEnabled(0),bSecondSupport=RTSBotSupportEnabled(1);
 const TCHAR* SupportPolicy=bFirstSupport?(bSecondSupport?TEXT("enabled"):TEXT("slot_1_baseline")):
  (bSecondSupport?TEXT("slot_0_baseline"):TEXT("baseline"));
 ERTSEmergencySlingerMode SlingerMode=ERTSEmergencySlingerMode::Inactive;
 bool bGadiantonCombatFirst=false;
 bool bGadiantonFoodAssignment=false;
#if !UE_BUILD_SHIPPING
 bGadiantonCombatFirst=FParse::Param(CommandLine,TEXT("RTSV09BotBalance"))&&
  FParse::Param(CommandLine,TEXT("RTSV09BotGadiantonCombatFirst"));
 bGadiantonFoodAssignment=FParse::Param(CommandLine,TEXT("RTSV09BotBalance"))&&
  FParse::Param(CommandLine,TEXT("RTSV09BotGadiantonFoodAssignment"));
 if(bGadiantonCombatFirst&&bGadiantonFoodAssignment){
  UE_LOG(LogTemp,Error,TEXT("Choose one Gadianton opening policy per probe"));
  return false;
 }
 const bool bLegacy=FParse::Param(CommandLine,TEXT("RTSV09BotLegacyCivilizationTuning"));
 const bool bFirstPass=FParse::Param(CommandLine,TEXT("RTSV09BotFirstPassRecruitment"));
 const bool bSecondPass=FParse::Param(CommandLine,TEXT("RTSV09BotSecondPassRecruitment"));
 if(int32(bLegacy)+int32(bFirstPass)+int32(bSecondPass)>1){
  UE_LOG(LogTemp,Error,TEXT("Choose one civilization tuning control per probe"));
  return false;
 }
 const TGuardValue<bool> LegacyCivilizationTuning(RTS::bLegacyCivilizationTuningForProbe,bLegacy);
 const TGuardValue<bool> SecondPassRecruitment(RTS::bSecondPassRecruitmentForProbe,bSecondPass);
 if(FParse::Param(CommandLine,TEXT("RTSV09BotBalance"))){
  SlingerMode=FParse::Param(CommandLine,TEXT("RTSV09BotEmergencySlinger"))?
   ERTSEmergencySlingerMode::Priority:ERTSEmergencySlingerMode::Baseline;
 }
#endif
 TArray<TSharedPtr<FJsonValue>> Matches;
 TSharedPtr<FJsonObject> SlingerFixture;
 bool bValid=true;
 auto Save=[&](bool bComplete){
  auto Report=MakeShared<FJsonObject>();
  Report->SetNumberField(TEXT("shard"),Shard);
  Report->SetNumberField(TEXT("shard_count"),ShardCount);
  Report->SetNumberField(TEXT("maximum_steps_per_match"),StepLimit);
  Report->SetNumberField(TEXT("fixed_simulation_step_seconds"),RTS::Step);
  Report->SetNumberField(TEXT("map"),Map);
  Report->SetStringField(TEXT("map_id"),RTS::MapInfo(Map).Id);
  Report->SetStringField(TEXT("difficulty"),TEXT("normal"));
  Report->SetStringField(TEXT("civilization_tuning"),RTS::UseLegacyCivilizationTuning()?TEXT("v09_legacy"):
   RTS::UseSecondPassNephiteRecruitment()?TEXT("v09_second_pass"):TEXT("v09_first_pass"));
  Report->SetStringField(TEXT("opening_formation"),FParse::Param(CommandLine,TEXT("RTSV09BotBalance"))&&
   FParse::Param(CommandLine,TEXT("RTSV09BotLegacyOpening"))?TEXT("legacy_world_axes"):TEXT("resource_relative"));
  const auto RaidPolicy=RTSBotRaidPolicy();
  Report->SetStringField(TEXT("gadianton_raid_policy"),RaidPolicy==ERTSBotRaidPolicy::Legacy?TEXT("legacy"):
   RaidPolicy==ERTSBotRaidPolicy::Expedition?TEXT("raid_expedition"):TEXT("opportunistic"));
  Report->SetStringField(TEXT("gadianton_opening_policy"),bGadiantonCombatFirst?TEXT("combat_first"):
   bGadiantonFoodAssignment?TEXT("local_food_assignment"):TEXT("foraging_first"));
  Report->SetStringField(TEXT("nephite_policy"),SlingerMode==ERTSEmergencySlingerMode::Priority?TEXT("emergency_slinger_priority"):TEXT("baseline"));
  Report->SetStringField(TEXT("support_policy"),SupportPolicy);
  Report->SetBoolField(TEXT("slot_0_support_enabled"),bFirstSupport);
  Report->SetBoolField(TEXT("slot_1_support_enabled"),bSecondSupport);
  Report->SetBoolField(TEXT("support_trace"),RTSBotSupportTraceEnabled());
  Report->SetBoolField(TEXT("fog"),true);
  Report->SetBoolField(TEXT("stock_standard_match_layout"),true);
  Report->SetBoolField(TEXT("complete"),bComplete);
  Report->SetBoolField(TEXT("valid"),bValid);
  Report->SetArrayField(TEXT("matches"),Matches);
  if(SlingerFixture)Report->SetObjectField(TEXT("emergency_slinger_fixture"),SlingerFixture);
  FString Json;
  return FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json))&&FFileHelper::SaveStringToFile(Json,*ReportPath);
 };
 int32 Pair=0;
 for(int32 A=0;A<Factions.Num();++A)for(int32 B=A+1;B<Factions.Num();++B,++Pair){
  if(Pair%ShardCount!=Shard)continue;
  for(int32 Leg=0;Leg<2;++Leg){
   auto Result=BalanceMatch(W,Pair,Leg,Factions[Leg==0?A:B].Id,Factions[Leg==0?B:A].Id,Map,StepLimit,SlingerMode,
    bGadiantonCombatFirst,bGadiantonFoodAssignment);
   bValid&=Result->GetBoolField(TEXT("valid"));
   Matches.Add(MakeShared<FJsonValueObject>(Result));
   if(!Save(false))return false;
  }
 }
 if(SlingerMode==ERTSEmergencySlingerMode::Priority){
  SlingerFixture=BalanceEmergencySlingerFixture(W,SlingerMode);
  bValid&=SlingerFixture->GetBoolField(TEXT("passed"));
 }
 const bool bCompleted=Save(true);
 UE_LOG(LogTemp,Display,TEXT("RTS_BALANCE_SHARD shard=%d count=%d complete=%d valid=%d"),Shard,Matches.Num(),bCompleted,bValid);
 return bValid&&bCompleted;
}
