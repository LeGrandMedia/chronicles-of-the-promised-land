#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace {
struct FBotMatchMilestones {
 float Gathered=-1,Construction=-1,Worker=-1,Age=-1,Trained=-1,Research=-1,Attack=-1;
 int32 PeakArmy=0,PeakWorkers=0,BestAge=1;

 bool Complete() const {
  return Gathered>=0&&Construction>=0&&Worker>=0&&Age>=0&&Trained>=0&&Research>=0&&Attack>=0;
 }
};

void SetFirst(float& Existing,bool bReached,float Time) {
 if(Existing<0&&bReached)Existing=Time;
}
}

bool RunRTSBotTests(ARTSWorld& W) {
 int32 Passed=0,Failed=0;
 TArray<TSharedPtr<FJsonValue>> Assertions,Matches;
 auto Check=[&](const FString& Name,bool bOk){
  auto Result=MakeShared<FJsonObject>();
  Result->SetStringField(TEXT("name"),Name);
  Result->SetBoolField(TEXT("passed"),bOk);
  Assertions.Add(MakeShared<FJsonValueObject>(Result));
  if(bOk)++Passed;else{++Failed;UE_LOG(LogTemp,Error,TEXT("RTS_V09_BOT_FAIL %s"),*Name);}
 };
 auto Reset=[&](){
  W.Players.Empty();
  W.Entities.Empty();
  W.Obstacles.Empty();
  W.TraderVisitAt.Empty();
  W.NextId=1;
  W.Time=0;
  W.Accumulator=0;
  W.Phase=0;
  W.Map=0;
  W.Winner=-1;
  W.bSandbox=false;
  W.bFogOfWar=true;
  W.StartingResources=1;
  W.Fruitfulness=1;
  W.NavigationCount=-1;
  W.NavigationTime=-100;
  W.ObstacleCount=-1;
 };
 const FVector2D Zero=FVector2D::ZeroVector;
 auto* Host=W.GetWorld()->SpawnActor<ARTSController>();
 auto* Guest=W.GetWorld()->SpawnActor<ARTSController>();
 auto* Replacement=W.GetWorld()->SpawnActor<ARTSController>();
 if(!Host||!Guest||!Replacement)return false;
 Reset();
 W.Join(Host);
 W.Join(Guest);
 W.Lobby(Host,TEXT("Host"),TEXT("nephites"),0,true);
 W.Lobby(Guest,TEXT("Guest"),TEXT("lamanites"),1,true);
 W.Command(Guest,TEXT("bot_add"),{},Zero,0,TEXT("laban"));
 Check(TEXT("lobby.guest_cannot_add_bot"),W.Players.Num()==2);
 W.Command(Host,TEXT("bot_add"),{},Zero,0,TEXT("laban"));
 Check(TEXT("lobby.host_adds_owned_normal_bot"),W.Players.Num()==3&&W.Players[2].bBot&&W.Players[2].bConnected&&W.Players[2].bReady
  &&W.Players[2].Team==1&&W.Players[2].BotDifficulty==1&&!W.Players[2].Controller.IsValid());
 W.Command(Guest,TEXT("bot_remove"),{},Zero,2,TEXT(""));
 W.Command(Host,TEXT("bot_config"),{0,8},Zero,2,TEXT("shiz"));
 Check(TEXT("lobby.guest_cannot_remove_and_invalid_settings_are_rejected"),W.Players[2].bBot&&W.Players[2].Faction==TEXT("laban"));
 W.Command(Host,TEXT("bot_config"),{0,2},Zero,2,TEXT("shiz"));
 Check(TEXT("lobby.host_sets_faction_team_and_difficulty"),W.Players[2].Faction==TEXT("shiz")&&W.Players[2].Team==0&&W.Players[2].BotDifficulty==2);
 W.Command(Host,TEXT("bot_add"),{},Zero,0,TEXT("gadianton"));
 W.Command(Host,TEXT("bot_add"),{},Zero,0,TEXT("coriantumr"));
 Check(TEXT("lobby.four_slot_limit"),W.Players.Num()==4&&W.Players[3].Faction==TEXT("gadianton"));
 W.Command(Host,TEXT("bot_remove"),{},Zero,2,TEXT(""));
 W.Join(Replacement);
 Check(TEXT("lobby.removed_bot_slot_is_reused_by_human"),Replacement->ServerSlot==2&&!W.Players[2].bBot&&W.Players[2].Controller.Get()==Replacement);
 W.Start(Host,0);
 Check(TEXT("lobby.human_readiness_still_required"),W.Phase==0);
 W.Lobby(Replacement,TEXT("Replacement"),TEXT("coriantumr"),0,true);
 W.Start(Host,0);
 Check(TEXT("lobby.humans_and_bot_start_competitive_match"),W.Phase==1&&!W.bSandbox&&W.Players[3].bBot);
 FRTSEntity* GuestUnit=nullptr;
 FRTSEntity* BotUnit=nullptr;
 for(const auto& Entry:W.Entities)if(RTS::IsWorker(W.D(*Entry.Value))){
  if(Entry.Value->Owner==1)GuestUnit=Entry.Value.Get();
  if(Entry.Value->Owner==3)BotUnit=Entry.Value.Get();
 }
 if(GuestUnit&&BotUnit){
  W.Command(Guest,TEXT("move"),{GuestUnit->Id,BotUnit->Id},GuestUnit->Pos+FVector2D(4,0),0,TEXT(""));
  Check(TEXT("orders.human_controls_own_worker_and_cannot_move_bot_worker"),GuestUnit->Order==TEXT("move")&&BotUnit->Order==TEXT("idle"));
  W.Command(Host,TEXT("bot_remove"),{},Zero,3,TEXT(""));
  Check(TEXT("lobby.bot_roster_is_locked_during_play"),W.Players[3].bBot);
 }else Check(TEXT("orders.match_has_human_and_bot_workers"),false);

 W.Players[0].Age=4;
 W.Players[0].Upgrades.Add(TEXT("heaps_of_earth"));
 W.Spawn(TEXT("house_of_curious_workmanship"),0,{17,32});
 auto* WallBuilder=W.Spawn(TEXT("industrious_laborer"),0,{40,46});
 const int32 FirstWall=W.NextId;
 W.Command(Host,TEXT("wall_line"),{WallBuilder->Id},{45,50},0,TEXT("works_of_defense|45|43"));
 const auto* Wall=W.Find(FirstWall);
 Check(TEXT("orders.wall_line_segments_follow_the_drag_direction"),Wall&&W.D(*Wall).Id==TEXT("works_of_defense")&&FMath::IsNearlyEqual(Wall->Heading,PI*.5f,.001f));
 Host->Destroy();
 Guest->Destroy();
 Replacement->Destroy();

 auto AddSlots=[&](const FString& Faction,int32 Difficulty){
  FRTSPlayer Bot;
  Bot.Slot=0;
  Bot.Team=0;
  Bot.bBot=true;
  Bot.bReady=true;
  Bot.BotDifficulty=Difficulty;
  Bot.Faction=Faction;
  W.Players.Add(MoveTemp(Bot));
  FRTSPlayer Opponent;
  Opponent.Slot=1;
  Opponent.Team=1;
  Opponent.bReady=true;
  W.Players.Add(MoveTemp(Opponent));
 };

 auto RunTactical=[&](bool bHiddenEnemy,int32 Difficulty){
  Reset();
  AddSlots(TEXT("lamanites"),Difficulty);
  W.Phase=1;
  W.Time=240;
  auto& Bot=W.Players[0];
  Bot.Age=2;
  Bot.Resources=FVector::ZeroVector;
  Bot.Bot.Home={24,24};
  W.Spawn(TEXT("seat_of_the_people"),0,{24,24});
  for(int32 I=0;I<9;++I)W.Spawn(TEXT("cimeter_bearer"),0,{25.+I,29});
  if(bHiddenEnemy)W.Spawn(TEXT("seat_of_the_people"),1,{136,136});
  W.Population();
  W.UpdateBots();
 };
 RunTactical(false,1);
 const FVector2D BaselineGoal=W.Players[0].Bot.ArmyGoal;
 RunTactical(true,1);
 auto& Tactical=W.Players[0];
 Check(TEXT("fog.hidden_enemy_does_not_change_orders_or_seed_building_memory"),Tactical.Bot.bCampaign&&Tactical.Bot.ArmyGoal.Equals(BaselineGoal)
  &&Tactical.Bot.ArmyTarget==0&&Tactical.Bot.EnemyBuildings.IsEmpty());
 Check(TEXT("economy.one_decision_cannot_grant_resources_age_or_units"),Tactical.Resources.IsZero()&&Tactical.Cement==0&&Tactical.Age==2&&Tactical.Population==9);
 auto* Intruder=W.Spawn(TEXT("man_of_the_sword"),1,{24,31});
 auto* Spotted=W.Spawn(TEXT("tents_and_habitations"),1,{24,16});
 W.Time=Tactical.Bot.NextThink;
 W.UpdateBots();
 const auto* Defender=W.Find(2);
 Check(TEXT("combat.visible_threat_draws_a_defensive_response"),Tactical.Bot.ArmyTarget==Intruder->Id&&Defender
  &&Defender->Order==TEXT("attack")&&Defender->Target==Intruder->Id);
 Check(TEXT("fog.bot_records_a_building_only_after_observing_it"),Tactical.Bot.EnemyBuildings.Contains(Spotted->Id));
 for(auto& Entry:W.Entities)if(Entry.Value->Owner==0)Entry.Value->Pos+=FVector2D(50,50);
 const int32 SpottedId=Spotted->Id;
 W.Kill(SpottedId);
 W.Time=Tactical.Bot.NextThink;
 W.UpdateBots();
 Check(TEXT("fog.unseen_destruction_does_not_update_memory"),Tactical.Bot.EnemyBuildings.Contains(SpottedId));
 RunTactical(true,0);
 const float EasyNext=W.Players[0].Bot.NextThink;
 const bool bEasyCampaign=W.Players[0].Bot.bCampaign;
 RunTactical(true,2);
 Check(TEXT("difficulty.hard_reacts_faster_and_launches_a_smaller_earlier_wave"),!bEasyCampaign&&W.Players[0].Bot.bCampaign&&W.Players[0].Bot.NextThink<EasyNext);

 auto StartSupportScenario=[&](const TCHAR* Faction){
  Reset();
  AddSlots(Faction,1);
  W.Phase=1;
  W.Time=240;
  W.Players[0].Age=2;
  W.Players[0].Resources=FVector::ZeroVector;
  W.Players[0].Bot.Home={24,24};
  W.Spawn(TEXT("seat_of_the_people"),0,{24,24});
  W.Spawn(TEXT("seat_of_the_people"),1,{136,136});
 };
 auto ThinkSupport=[&](){
  W.Population();
  W.Time=FMath::Max(W.Time,W.Players[0].Bot.NextThink);
  W.UpdateBots();
 };
 const FVector OpeningStock(110,120,100);
 auto StartForagingTrial=[&](bool bCombatFirst){
  StartSupportScenario(TEXT("gadianton"));
  auto& Bot=W.Players[0];
  Bot.Bot.bGadiantonCombatFirstForProbe=bCombatFirst;
  Bot.Bot.GadiantonRaidTrace.bEnabled=true;
  Bot.Resources=OpeningStock;
  auto* Work=W.Spawn(TEXT("tents_and_habitations"),0,{30,24});
  Work->Progress=.5f;
  W.Spawn(TEXT("tents_and_habitations"),0,{24,30})->Progress=.5f;
  for(int32 Index=0;Index<6;++Index){
   auto* Worker=W.Spawn(TEXT("industrious_laborer"),0,{21.+Index,19});
   Worker->Order=TEXT("build");
   Worker->Target=Work->Id;
  }
 };
 const TCHAR* Foraging=TEXT("gadianton_storehouse_wild_food");
 const auto& OpeningCatalog=FRTSCatalog::Get();
 const FVector ForagingCost=OpeningCatalog.Price(OpeningCatalog.Def(Foraging),TEXT("gadianton"));
 StartForagingTrial(false);
 auto* OpeningCamp=W.Spawn(TEXT("encampment_of_war"),0,{30,34});
 auto* OpeningStore=W.Spawn(TEXT("storehouse_of_provisions"),0,{20,29});
 ThinkSupport();
 Check(TEXT("opening.gadianton_normal_bot_keeps_foraging_priority"),OpeningStore->Jobs.Num()==1&&
  OpeningStore->Jobs[0].Type==Foraging&&OpeningStore->Jobs[0].Paid.Equals(ForagingCost)&&
  OpeningCamp->Jobs.IsEmpty()&&W.Players[0].Resources.Equals(OpeningStock-ForagingCost));

 StartForagingTrial(true);
 OpeningCamp=W.Spawn(TEXT("encampment_of_war"),0,{30,34});
 OpeningStore=W.Spawn(TEXT("storehouse_of_provisions"),0,{20,29});
 auto& TrialBot=W.Players[0];
 ThinkSupport();
 Check(TEXT("opening.combat_first_preserves_scout_production_without_counting_it"),OpeningStore->Jobs.IsEmpty()&&
  OpeningCamp->Jobs.Num()==1&&OpeningCamp->Jobs[0].Type==TEXT("wilderness_spy")&&TrialBot.Bot.GadiantonOpeningFightersOrdered==0);
 bool bPaidForTwoFighters=true;
 for(int32 Fighter=1;Fighter<=2;++Fighter){
  W.Production(*OpeningCamp,40);
  const FVector Funds(150,120,100);
  TrialBot.Resources=Funds;
  ThinkSupport();
  const bool bFighter=OpeningCamp->Jobs.Num()==1&&(OpeningCamp->Jobs[0].Type==TEXT("bowman")||
   OpeningCamp->Jobs[0].Type==TEXT("cimeter_bearer"));
  bPaidForTwoFighters&=bFighter&&OpeningStore->Jobs.IsEmpty()&&TrialBot.Bot.GadiantonOpeningFightersOrdered==Fighter&&
   TrialBot.Resources.Equals(Funds-OpeningCamp->Jobs[0].Paid);
 }
 Check(TEXT("opening.combat_first_commissions_two_paid_camp_fighters_before_foraging"),bPaidForTwoFighters);
 const FVector FundsBeforeForaging=TrialBot.Resources;
 ThinkSupport();
 const auto& JobTrace=TrialBot.Bot.GadiantonRaidTrace.OpeningJobs;
 Check(TEXT("opening.combat_first_resumes_the_paid_foraging_job_and_records_debit"),OpeningStore->Jobs.Num()==1&&
  OpeningStore->Jobs[0].Type==Foraging&&TrialBot.Resources.Equals(FundsBeforeForaging-ForagingCost)&&
  !JobTrace.IsEmpty()&&JobTrace.Last().Type==Foraging&&JobTrace.Last().Paid.Equals(ForagingCost));

 struct FOpeningAssignment {
  FString SiteType;
  int32 Diverted=0;
  bool bRetainedWork=false,bOwnedFarmIneligible=false,bHiddenFoodPrivate=true;
 };
 auto AssignOpeningWorker=[&](bool bFoodAssignment,double Food,double Precious,const TCHAR* FoodSource){
  StartSupportScenario(TEXT("gadianton"));
  auto& Bot=W.Players[0];
  Bot.Age=1;
  Bot.Resources=FVector(Food,0,Precious);
  Bot.Bot.bGadiantonFoodAssignmentForProbe=bFoodAssignment;
  Bot.Bot.GadiantonRaidTrace.bEnabled=true;
  auto* Idle=W.Spawn(TEXT("industrious_laborer"),0,{32,27});
  auto* Ore=W.Spawn(TEXT("ore"),-1,{33,26});
  auto* OpeningMine=W.Spawn(TEXT("hill_of_ore"),0,Ore->Pos);
  OpeningMine->WorkTarget=Ore->Id;
  const auto* Tree=W.Spawn(TEXT("tree"),-1,{24,35});
  const auto* OwnedFarm=W.Spawn(TEXT("fields_of_grain"),0,{43,32});
  const FString Source(FoodSource);
  FRTSEntity* Berries=nullptr;
  if(Source!=TEXT("none"))Berries=W.Spawn(TEXT("berries"),-1,Source==TEXT("hidden")?FVector2D(106,106):FVector2D(37,31));
  if(Source==TEXT("depleted"))Berries->ResourceRemaining=0;
  auto AddWorker=[&](FVector2D At,const FRTSEntity* WorkSite){
   auto* Worker=W.Spawn(TEXT("industrious_laborer"),0,At);
   Worker->LastMove=W.Time;
   Worker->Order=WorkSite?TEXT("gather"):TEXT("hold");
   if(WorkSite)Worker->Target=Worker->WorkTarget=WorkSite->Id;
   return Worker;
  };
  for(int32 Index=0;Index<4;++Index)AddWorker({22.+Index,34},Tree);
  for(int32 Index=0;Index<2;++Index)AddWorker({35.+Index,30},Source==TEXT("wild")?Berries:nullptr);
  auto* WorkingMiner=AddWorker({33,28},OpeningMine);
  const FVector Load(0,0,3);
  WorkingMiner->Cargo=Load;
  const auto Fog=W.ComputeFog(Bot);
  ThinkSupport();
  const auto* Target=W.Find(Idle->WorkTarget);
  return FOpeningAssignment{Target?W.D(*Target).Id:FString(),Bot.Bot.GadiantonRaidTrace.SurplusOreDiversions,
   WorkingMiner->Order==TEXT("gather")&&WorkingMiner->WorkTarget==OpeningMine->Id&&WorkingMiner->Cargo.Equals(Load),
   !W.CanAssignWork(*Idle,*OwnedFarm),Source!=TEXT("hidden")||!W.VisibleInFog(0,Fog,*Berries)};
 };
 const auto BaseAssignment=AssignOpeningWorker(false,270,200,TEXT("wild"));
 const auto FoodAssignment=AssignOpeningWorker(true,270,200,TEXT("wild"));
 Check(TEXT("opening.surplus_ore_trial_reassigns_only_an_idle_worker_to_legal_wild_food"),
  BaseAssignment.SiteType==TEXT("hill_of_ore")&&FoodAssignment.SiteType==TEXT("berries")&&FoodAssignment.Diverted==1&&
  FoodAssignment.bRetainedWork&&FoodAssignment.bOwnedFarmIneligible);
 const auto FundedBase=AssignOpeningWorker(false,500,200,TEXT("wild"));
 const auto FundedTrial=AssignOpeningWorker(true,500,200,TEXT("wild"));
 const auto BelowReserve=AssignOpeningWorker(true,270,199,TEXT("wild"));
 Check(TEXT("opening.surplus_ore_trial_preserves_assignments_after_food_is_funded_or_ore_is_required"),
  !FundedBase.SiteType.IsEmpty()&&FundedTrial.SiteType==FundedBase.SiteType&&FundedTrial.Diverted==0&&
  BelowReserve.SiteType==BaseAssignment.SiteType&&BelowReserve.Diverted==0);
 bool bFallsBackToOre=true;
 for(const TCHAR* Source:{TEXT("none"),TEXT("hidden"),TEXT("depleted")}){
  const auto Fallback=AssignOpeningWorker(true,270,200,Source);
  bFallsBackToOre&=Fallback.SiteType==BaseAssignment.SiteType&&Fallback.Diverted==0&&Fallback.bHiddenFoodPrivate&&
   Fallback.bOwnedFarmIneligible;
 }
 Check(TEXT("opening.surplus_ore_trial_uses_normal_ore_order_without_eligible_visible_wild_food"),bFallsBackToOre);

 StartSupportScenario(TEXT("gadianton"));
 auto* FirstBand=W.Spawn(TEXT("gadianton_robber"),0,{35,32});
 const auto* OtherBand=W.Spawn(TEXT("gadianton_robber"),0,{28,29});
 ThinkSupport();
 const FVector2D InitialRally=FirstBand->Rally;
 Check(TEXT("raid.no_visible_target_keeps_bands_with_the_army"),FirstBand->Order==TEXT("idle")&&FirstBand->Target==0&&
  OtherBand->Order==TEXT("idle"));

 StartSupportScenario(TEXT("gadianton"));
 FirstBand=W.Spawn(TEXT("gadianton_robber"),0,{35,32});
 W.Spawn(TEXT("gadianton_robber"),0,{28,29});
 const auto* HiddenStore=W.Spawn(TEXT("storehouse_of_provisions"),1,{106,98});
 const auto RaidFog=W.ComputeFog(W.Players[0]);
 ThinkSupport();
 Check(TEXT("raid.hidden_building_cannot_change_band_orders"),!W.VisibleInFog(0,RaidFog,*HiddenStore)&&
  FirstBand->Order==TEXT("idle")&&FirstBand->Target==0&&FirstBand->Rally.Equals(InitialRally));

 StartSupportScenario(TEXT("gadianton"));
 FirstBand=W.Spawn(TEXT("gadianton_robber"),0,{40,40});
 const auto* DistantField=W.Spawn(TEXT("fields_of_grain"),1,{69,38});
 const auto* NearbyStore=W.Spawn(TEXT("storehouse_of_provisions"),1,{48,40});
 W.Spawn(TEXT("wilderness_spy"),0,{67,45});
 const auto LocalRaidFog=W.ComputeFog(W.Players[0]);
 ThinkSupport();
 const bool bDistantVisible=W.VisibleInFog(0,LocalRaidFog,*DistantField);
 const bool bNearbyVisible=W.VisibleInFog(0,LocalRaidFog,*NearbyStore);
 const bool bChoseNearby=FirstBand->Order==TEXT("raid")&&FirstBand->Target==NearbyStore->Id;
 if(!bDistantVisible||!bNearbyVisible||!bChoseNearby)
  UE_LOG(LogTemp,Display,TEXT("RTS_V09_RAID_LOCAL nearby_visible=%d distant_visible=%d line_of_sight=%d nearby=%d order=%s target=%d"),
   bNearbyVisible,bDistantVisible,W.LOS(FirstBand->Pos,NearbyStore->Pos,FirstBand->Id,NearbyStore->Id),
   NearbyStore->Id,*FirstBand->Order,FirstBand->Target);
 Check(TEXT("raid.band_chooses_nearby_accessible_site_over_first_visible_entity"),DistantField->Id<NearbyStore->Id&&
  bDistantVisible&&bNearbyVisible&&bChoseNearby);

 auto AvoidGuard=[&](bool bDisarmed){
  StartSupportScenario(TEXT("gadianton"));
  FirstBand=W.Spawn(TEXT("gadianton_robber"),0,{40,40});
  auto* Guard=W.Spawn(TEXT("bowman"),1,{43,44});
  const auto* GuardedStore=W.Spawn(TEXT("storehouse_of_provisions"),1,{48.75,41.25});
  if(bDisarmed)Guard->DisarmedUntil=W.Time+10;
  const auto GuardFog=W.ComputeFog(W.Players[0]);
  ThinkSupport();
  const FVector2D ArmyRally=W.Players[0].Bot.Home+(FVector2D(80,80)-W.Players[0].Bot.Home).GetSafeNormal()*8;
  return W.VisibleInFog(0,GuardFog,*Guard)&&W.VisibleInFog(0,GuardFog,*GuardedStore)&&
   FirstBand->Order==TEXT("attack_move")&&FirstBand->Target==0&&FirstBand->Rally.Equals(ArmyRally);
 };
 const bool bAvoidedActiveGuard=AvoidGuard(false);
 const FVector2D GuardedRally=FirstBand->Rally;
 Check(TEXT("raid.visible_guard_avoidance_is_independent_of_private_disarm_timer"),bAvoidedActiveGuard&&AvoidGuard(true)&&
  FirstBand->Rally.Equals(GuardedRally));

 StartSupportScenario(TEXT("gadianton"));
 FirstBand=W.Spawn(TEXT("gadianton_robber"),0,{38,35});
 const auto* HomeStore=W.Spawn(TEXT("storehouse_of_provisions"),0,{32,35});
 const auto* DefenderBand=W.Spawn(TEXT("gadianton_robber"),0,{25,28});
 const auto* RaidIntruder=W.Spawn(TEXT("man_of_the_sword"),1,{24,31});
 const auto* LastRaidTarget=W.Spawn(TEXT("fields_of_grain"),1,{46,35});
 const FVector RaidCargo(3,2,1);
 FirstBand->Order=TEXT("raid");
 FirstBand->RaidSource=FirstBand->Target=LastRaidTarget->Id;
 FirstBand->Cargo=RaidCargo;
 ThinkSupport();
 const bool bReturnsWhileDefending=FirstBand->Order==TEXT("deliver")&&FirstBand->DropTarget==HomeStore->Id&&
  FirstBand->RaidSource==0&&DefenderBand->Order==TEXT("attack")&&DefenderBand->Target==RaidIntruder->Id;
 for(int32 Tick=0;Tick<55&&FirstBand->Order==TEXT("deliver");++Tick)W.Step(RTS::Step);
 Check(TEXT("raid.loaded_band_deposits_while_another_band_defends_the_base"),bReturnsWhileDefending&&
  FirstBand->Cargo.IsZero()&&W.Players[0].Resources.Equals(RaidCargo,.001));

 StartSupportScenario(TEXT("nephites"));
 auto* Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{36,32});
 auto* Convert=W.Spawn(TEXT("industrious_laborer"),1,{42.5,32});
 const int32 ConvertId=Convert->Id;
 ThinkSupport();
 const double ApproachRange=FVector2D::Distance(Teacher->Rally,Convert->Pos);
 Check(TEXT("support.teacher_moves_to_a_nearby_cast_position_without_pursuing"),Teacher->Order==TEXT("move")&&
  Teacher->Target==0&&ApproachRange<W.ConversionRange(*Teacher)&&ApproachRange>4);
 bool bChanneled=false;
 for(int32 Tick=0;Tick<95&&Convert->Owner==1;++Tick){
  W.Step(RTS::Step);
  bChanneled|=Teacher->Order==TEXT("convert")&&Teacher->Target==ConvertId&&Teacher->Channel>0;
 }
 Check(TEXT("support.authority_completes_the_teacher_conversion_with_stable_identity"),bChanneled&&
  W.Find(ConvertId)==Convert&&Convert->Owner==0&&Convert->bConverted&&W.D(*Convert).Id==TEXT("ammonite_covenant_keeper"));

 StartSupportScenario(TEXT("nephites"));
 Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{36,32});
 auto* SecondTeacher=W.Spawn(TEXT("teacher_of_the_word"),0,{36,33});
 Convert=W.Spawn(TEXT("industrious_laborer"),1,{40,32});
 W.ResetOrder(*Teacher,TEXT("hold"));
 ThinkSupport();
 Check(TEXT("support.only_one_teacher_claims_the_available_target"),Teacher->Order==TEXT("convert")&&
  Teacher->Target==Convert->Id&&SecondTeacher->Target!=Convert->Id);
 for(int32 Tick=0;Tick<12;++Tick)W.Step(RTS::Step);
 const float Channel=Teacher->Channel;
 ThinkSupport();
 Check(TEXT("support.bot_think_preserves_an_in_progress_channel"),Channel>0&&Teacher->Channel==Channel&&
  Teacher->Order==TEXT("convert")&&Teacher->Target==Convert->Id&&SecondTeacher->Target!=Convert->Id);

 StartSupportScenario(TEXT("nephites"));
 Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{36,32});
 ThinkSupport();
 const FString ClearOrder=Teacher->Order;
 const FVector2D ClearRally=Teacher->Rally;
 StartSupportScenario(TEXT("nephites"));
 Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{36,32});
 W.Players[1].Faction=TEXT("gadianton");
 const auto* Hidden=W.Spawn(TEXT("lawyer"),1,{40,32});
 const auto HiddenFog=W.ComputeFog(W.Players[0]);
 ThinkSupport();
 Check(TEXT("support.concealed_lawyer_cannot_change_the_teachers_order"),!W.VisibleInFog(0,HiddenFog,*Hidden)&&
  Teacher->Order==ClearOrder&&Teacher->Rally.Equals(ClearRally)&&Teacher->Target==0);

 StartSupportScenario(TEXT("nephites"));
 Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{36,32});
 Convert=W.Spawn(TEXT("industrious_laborer"),1,{41,32});
 W.Spawn(TEXT("man_of_the_sword"),0,{41,39});
 W.Spawn(TEXT("works_of_defense"),0,{38.5,32});
 const auto WallFog=W.ComputeFog(W.Players[0]);
 ThinkSupport();
 Check(TEXT("support.visible_target_behind_wall_does_not_get_a_conversion_order"),W.VisibleInFog(0,WallFog,*Convert)&&
  !W.LOS(Teacher->Pos,Convert->Pos,Teacher->Id,Convert->Id)&&Teacher->Order!=TEXT("convert")&&Teacher->Target==0);

 StartSupportScenario(TEXT("nephites"));
 Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{36,32});
 const auto* Bowman=W.Spawn(TEXT("bowman"),1,{41,32});
 const auto ThreatFog=W.ComputeFog(W.Players[0]);
 ThinkSupport();
 Check(TEXT("support.unescorted_teacher_withdraws_from_visible_ranged_threat"),W.VisibleInFog(0,ThreatFog,*Bowman)&&
  Teacher->Order==TEXT("move")&&Teacher->Target==0&&
  FVector2D::DistSquared(Teacher->Rally,Bowman->Pos)>FVector2D::DistSquared(Teacher->Pos,Bowman->Pos));

 StartSupportScenario(TEXT("nephites"));
 Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{38,37.8});
 W.Players[0].Bot.Home={38,35};
 W.ResetOrder(*W.Spawn(TEXT("man_of_the_sword"),0,{42,40}),TEXT("hold"));
 W.ResetOrder(*W.Spawn(TEXT("man_of_the_sword"),0,{44,40}),TEXT("hold"));
 ThinkSupport();
 const FVector2D SupportStation=Teacher->Rally;
 const bool bQueuedHoldAfterMove=Teacher->Order==TEXT("move")&&Teacher->PendingOrders.Num()==1&&
  Teacher->PendingOrders[0].Action==TEXT("hold");
 auto* Interceptor=W.Spawn(TEXT("man_of_the_sword"),1,SupportStation-FVector2D(4.4,0));
 W.ResetOrder(*Interceptor,TEXT("hold"));
 const int32 InterceptorId=Interceptor->Id;
 bool bAcquiredOnArrival=false;
 for(int32 Tick=0;Tick<14;++Tick){
  W.Step(RTS::Step);
  bAcquiredOnArrival|=Teacher->Order==TEXT("convert");
 }
 Interceptor=W.Find(InterceptorId);
 const bool bHeldInEligibleRange=Interceptor&&bQueuedHoldAfterMove&&W.Time<W.Players[0].Bot.NextThink&&Teacher->Order==TEXT("hold")&&
  Teacher->PendingOrders.IsEmpty()&&Teacher->Pos.Equals(SupportStation,.55)&&!bAcquiredOnArrival&&
  W.CanConvertWith(*Teacher,*Interceptor)&&W.CanSee(0,*Interceptor)&&W.LOS(Teacher->Pos,Interceptor->Pos,Teacher->Id,InterceptorId)&&
  FVector2D::Distance(Teacher->Pos,Interceptor->Pos)<=W.ConversionRange(*Teacher);
 W.Kill(InterceptorId);
 Convert=W.Spawn(TEXT("industrious_laborer"),1,Teacher->Pos-FVector2D(3,0));
 ThinkSupport();
 W.Step(RTS::Step);
 Check(TEXT("support.move_completion_holds_between_thinks_then_allows_a_safe_conversion"),bHeldInEligibleRange&&
  Teacher->Order==TEXT("convert")&&Teacher->Target==Convert->Id&&Teacher->Channel>0);

 StartSupportScenario(TEXT("nephites"));
 Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{36,32});
 Convert=W.Spawn(TEXT("industrious_laborer"),1,{40,32});
 W.ResetOrder(*Teacher,TEXT("move"));
 const FVector2D OriginalMove(32,36);
 Teacher->Rally=OriginalMove;
 FRTSQueuedOrder OriginalHold;
 OriginalHold.Action=TEXT("hold");
 Teacher->PendingOrders.Add(OriginalHold);
 ThinkSupport();
 Check(TEXT("support.preserves_an_existing_hold_queue_from_outside_the_planner"),Teacher->Order==TEXT("move")&&
  Teacher->Rally.Equals(OriginalMove)&&Teacher->PendingOrders.Num()==1&&Teacher->PendingOrders[0].Action==TEXT("hold")&&
  Teacher->PendingOrders[0].Type.IsEmpty());

 StartSupportScenario(TEXT("nephites"));
 Teacher=W.Spawn(TEXT("teacher_of_the_word"),0,{31,30});
 const auto* Guard=W.Spawn(TEXT("man_of_the_sword"),0,{42,40});
 W.Spawn(TEXT("man_of_the_sword"),0,{44,40});
 const auto* BotSupportSpy=W.Spawn(TEXT("wilderness_spy"),0,{66,55});
 ThinkSupport();
 Check(TEXT("support.teacher_follows_behind_the_escort_instead_of_the_scout"),Teacher->Order==TEXT("move")&&
  FVector2D::DistSquared(Teacher->Rally,Guard->Pos)<16&&Teacher->Rally.X<Guard->Pos.X&&
  FVector2D::DistSquared(Teacher->Rally,BotSupportSpy->Pos)>100);

 StartSupportScenario(TEXT("lamanites"));
 W.Spawn(TEXT("temple_of_the_land"),0,{27,40})->Progress=.25f;
 const auto* Platform=W.Spawn(TEXT("temple_of_the_land"),0,{38,31});
 const auto* Priest=W.Spawn(TEXT("priestcrafter"),0,{34,31});
 const auto* FieldPriest=W.Spawn(TEXT("priestcrafter"),0,{46,28});
 const auto* Soldier=W.Spawn(TEXT("cimeter_bearer"),0,{35,38});
 const float AttackBefore=W.AttackStrength(*Soldier);
 ThinkSupport();
 Check(TEXT("support.one_priestcrafter_occupies_the_completed_rameumptom"),Priest->Order==TEXT("occupy")&&
  Priest->Target==Platform->Id&&FieldPriest->Order!=TEXT("occupy"));
 for(int32 Tick=0;Tick<30;++Tick)W.Step(RTS::Step);
 Check(TEXT("support.priestcrafter_keeps_the_platform_and_applies_the_occupied_aura"),Priest->Order==TEXT("occupy")&&
  Priest->Target==Platform->Id&&FMath::IsNearlyEqual(W.AttackStrength(*Soldier),AttackBefore*1.15f,.001f));

 StartSupportScenario(TEXT("laban"));
 const auto* Concubine=W.Spawn(TEXT("drunken_concubine"),0,{36,39});
 const auto* Field=W.Spawn(TEXT("fields_of_grain"),0,{44,32});
 TArray<FRTSEntity*> FarmWorkers;
 for(int32 Index=0;Index<3;++Index){
  auto* Laborer=W.Spawn(TEXT("industrious_laborer"),0,{50,32});
  W.ResetOrder(*Laborer,TEXT("gather"));
  Laborer->Target=Laborer->WorkTarget=Field->Id;
  Laborer->LastMove=W.Time;
  FarmWorkers.Add(Laborer);
 }
 for(auto* Laborer:FarmWorkers)Laborer->Pos=W.WorkPosition(*Laborer,*Field);
 ThinkSupport();
 const bool bMovedToWorkers=Concubine->Order==TEXT("move")&&FVector2D::DistSquared(Concubine->Rally,FarmWorkers[1]->Pos)<25;
 int32 BoostedWorkers=0;
 for(int32 Tick=0;Tick<100&&BoostedWorkers<2;++Tick){
  W.Step(RTS::Step);
  BoostedWorkers=0;
  for(const auto* Laborer:FarmWorkers)BoostedWorkers+=Laborer->Order==TEXT("gather")&&W.LaborerAuraMultiplier(*Laborer)>1.19f;
 }
 Check(TEXT("support.concubine_moves_to_working_laborers_and_buffs_their_work"),bMovedToWorkers&&BoostedWorkers>=2);

 StartSupportScenario(TEXT("nephites"));
 auto* Ammonite=W.Spawn(TEXT("ammonite_covenant_keeper"),0,{31,31});
 const auto* Berries=W.Spawn(TEXT("berries"),-1,{32,31});
 W.ResetOrder(*Ammonite,TEXT("gather"));
 Ammonite->Target=Ammonite->WorkTarget=Berries->Id;
 Ammonite->LastMove=W.Time;
 W.Spawn(TEXT("man_of_the_sword"),1,{34,31});
 ThinkSupport();
 Check(TEXT("support.ammonite_keeps_its_gather_assignment"),Ammonite->Order==TEXT("gather")&&Ammonite->WorkTarget==Berries->Id);

 const auto& Factions=FRTSCatalog::Get().Factions;
 Check(TEXT("match_probe.covers_all_six_factions"),Factions.Num()==6);
 for(int32 Index=0;Index<Factions.Num();++Index){
  Reset();
  const FString Faction=Factions[Index].Id;
  AddSlots(Faction,1);
  const int32 MapIndex=Index%3;
  const bool bPlayToVictory=Faction==TEXT("gadianton");
  W.Start(nullptr,MapIndex);
  auto& Bot=W.Players[0];
  FBotMatchMilestones Progress;
  TArray<double> StepTimes;
  StepTimes.Reserve(bPlayToVictory?14000:11000);
  bool bLegalEconomy=true;
  for(int32 Tick=0;Tick<(bPlayToVictory?14000:11000)&&W.Phase==1&&(bPlayToVictory||!Progress.Complete());++Tick){
   const double Started=FPlatformTime::Seconds();
   W.Step(RTS::Step);
   StepTimes.Add((FPlatformTime::Seconds()-Started)*1000);
   if(Tick%5!=0)continue;
   int32 Workers=0,Army=0;
   for(const auto& Entry:W.Entities){
    const auto& E=*Entry.Value;
    if(E.Owner!=0)continue;
    const auto& Def=W.D(E);
    const bool bMilitary=RTS::IsMilitary(Def);
    Workers+=RTS::IsWorker(Def)?1:0;
    Army+=bMilitary?1:0;
    SetFirst(Progress.Gathered,RTS::IsWorker(Def)&&E.ResourceEventTime>=0,W.Time);
    SetFirst(Progress.Construction,Def.bBuilding&&Def.Id!=TEXT("seat_of_the_people")&&E.Progress>=1,W.Time);
    SetFirst(Progress.Trained,bMilitary&&!Def.bHero,W.Time);
    SetFirst(Progress.Attack,bMilitary&&E.AttackEventTime>=0,W.Time);
   }
   Progress.PeakWorkers=FMath::Max(Progress.PeakWorkers,Workers);
   Progress.PeakArmy=FMath::Max(Progress.PeakArmy,Army);
   Progress.BestAge=FMath::Max(Progress.BestAge,Bot.Age);
   SetFirst(Progress.Worker,Workers>6,W.Time);
   SetFirst(Progress.Age,Bot.Age>=2&&!Bot.HeroChoices.IsEmpty(),W.Time);
   SetFirst(Progress.Research,Bot.Upgrades.ContainsByPredicate([](const FString& U){
    return FRTSCatalog::Get().Def(U).bUpgrade&&FRTSCatalog::Get().Def(U).Kind!=TEXT("age_option");
   }),W.Time);
   bLegalEconomy&=Bot.Resources.X>=-.001&&Bot.Resources.Y>=-.001&&Bot.Resources.Z>=-.001&&Bot.Cement>=-.001&&Bot.Population<=Bot.Capacity;
  }
  Check(Faction+TEXT(".standard_opening_builds_gathers_trains_researches_ages_and_attacks"),Progress.Complete());
  Check(Faction+TEXT(".standard_opening_obeys_resources_and_population"),bLegalEconomy);
  if(bPlayToVictory)Check(Faction+TEXT(".river_match_ends_in_a_competitive_victory"),W.Phase==2&&W.Winner==0);
  auto Report=MakeShared<FJsonObject>();
  Report->SetStringField(TEXT("faction"),Faction);
  Report->SetNumberField(TEXT("map"),MapIndex);
  Report->SetNumberField(TEXT("first_deposit_seconds"),Progress.Gathered);
  Report->SetNumberField(TEXT("first_structure_seconds"),Progress.Construction);
  Report->SetNumberField(TEXT("first_worker_seconds"),Progress.Worker);
  Report->SetNumberField(TEXT("first_age_advance_seconds"),Progress.Age);
  Report->SetNumberField(TEXT("first_military_seconds"),Progress.Trained);
  Report->SetNumberField(TEXT("first_research_seconds"),Progress.Research);
  Report->SetNumberField(TEXT("first_hit_seconds"),Progress.Attack);
  Report->SetNumberField(TEXT("best_age"),Progress.BestAge);
  Report->SetNumberField(TEXT("peak_workers"),Progress.PeakWorkers);
  Report->SetNumberField(TEXT("peak_army"),Progress.PeakArmy);
  Report->SetNumberField(TEXT("simulated_seconds"),W.Time);
  Report->SetNumberField(TEXT("simulation_steps"),StepTimes.Num());
  if(!StepTimes.IsEmpty()){
   StepTimes.Sort();
   Report->SetNumberField(TEXT("step_ms_median"),StepTimes[StepTimes.Num()/2]);
   Report->SetNumberField(TEXT("step_ms_p95"),StepTimes[FMath::FloorToInt((StepTimes.Num()-1)*.95)]);
   Report->SetNumberField(TEXT("step_ms_max"),StepTimes.Last());
  }
  Report->SetNumberField(TEXT("winning_team"),W.Winner);
  Report->SetNumberField(TEXT("victory_seconds"),W.Winner==0?W.Time:-1);
  Report->SetBoolField(TEXT("passed"),Progress.Complete()&&bLegalEconomy&&(!bPlayToVictory||W.Winner==0));
  Matches.Add(MakeShared<FJsonValueObject>(Report));
  UE_LOG(LogTemp,Display,TEXT("RTS_V09_BOT_MATCH faction=%s map=%d gather=%.1f build=%.1f worker=%.1f age=%.1f train=%.1f research=%.1f hit=%.1f workers=%d army=%d"),
   *Faction,MapIndex,Progress.Gathered,Progress.Construction,Progress.Worker,Progress.Age,Progress.Trained,Progress.Research,Progress.Attack,Progress.PeakWorkers,Progress.PeakArmy);
 }
 auto Report=MakeShared<FJsonObject>();
 Report->SetNumberField(TEXT("passed"),Passed);
 Report->SetNumberField(TEXT("failed"),Failed);
 Report->SetArrayField(TEXT("assertions"),Assertions);
 Report->SetArrayField(TEXT("matches"),Matches);
 FString Json;
 FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");
 IFileManager::Get().MakeDirectory(*Dir,true);
 FFileHelper::SaveStringToFile(Json,*(Dir/TEXT("v09-bot-tests.json")));
 UE_LOG(LogTemp,Display,TEXT("RTS_V09_BOT_TESTS passed=%d failed=%d"),Passed,Failed);
 return Failed==0;
}
