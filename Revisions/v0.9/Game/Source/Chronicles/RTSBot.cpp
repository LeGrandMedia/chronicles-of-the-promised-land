#include "RTSBot.h"
#include "RTSWorld.h"
#include "RTSController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"

bool RTSBotSupportEnabled(int32 Slot) {
#if UE_BUILD_SHIPPING
 return true;
#else
 // A mask lets balance replays compare both policies in the same executable.
 static const uint32 BaselineSlots=[](){
  const TCHAR* Args=FCommandLine::Get();
  if(!FParse::Param(Args,TEXT("RTSV09BotBalance")))return 0u;
  if(FParse::Param(Args,TEXT("RTSV09BotSupportBaseline")))return 3u;
  int32 BaselineSlot=INDEX_NONE;
  if(FParse::Value(Args,TEXT("RTSV09BotSupportBaselineSlot="),BaselineSlot)&&BaselineSlot>=0&&BaselineSlot<2)return 1u<<BaselineSlot;
  return 0u;
 }();
 return Slot<0||Slot>=2||(BaselineSlots&(1u<<Slot))==0;
#endif
}

bool RTSBotSupportTraceEnabled() {
#if UE_BUILD_SHIPPING
 return false;
#else
 static const bool bTrace=FParse::Param(FCommandLine::Get(),TEXT("RTSV09BotBalance"))&&
  FParse::Param(FCommandLine::Get(),TEXT("RTSV09BotSupportTrace"));
 return bTrace;
#endif
}

ERTSBotRaidPolicy RTSBotRaidPolicy() {
#if !UE_BUILD_SHIPPING
 static const auto Policy=[](){
  const TCHAR* Args=FCommandLine::Get();
  if(FParse::Param(Args,TEXT("RTSV09BotBalance"))){
   if(FParse::Param(Args,TEXT("RTSV09BotLegacyRaidPolicy")))return ERTSBotRaidPolicy::Legacy;
   if(FParse::Param(Args,TEXT("RTSV09BotRaidExpedition")))return ERTSBotRaidPolicy::Expedition;
  }
  return ERTSBotRaidPolicy::Opportunistic;
 }();
 return Policy;
#else
 return ERTSBotRaidPolicy::Opportunistic;
#endif
}

namespace {
struct FBotDifficulty {
 float ThinkSeconds;
 int32 OpeningWorkers;
 int32 Workers;
 int32 Army;
 int32 Wave;
 float FirstAttack;
 int32 Camps;
};

constexpr FBotDifficulty Difficulties[]={
 {3.5f,8,14,22,12,330,1},
 {1.8f,9,19,40,8,205,2},
 {.9f,10,25,65,6,155,3}
};

struct FBotFactionPlan {
 const TCHAR* Faction;
 const TCHAR* Heroes[3];
 const TCHAR* Frontline;
 const TCHAR* Ranged;
 const TCHAR* Advanced;
 const TCHAR* SpecialtyBuilding;
 const TCHAR* Support;
 const TCHAR* Research[3];
};

constexpr FBotFactionPlan FactionPlans[]={
 {TEXT("nephites"),{TEXT("nephi"),TEXT("amulek"),TEXT("mormon")},TEXT("man_of_the_sword"),TEXT("bowman"),TEXT("man_of_moroni"),TEXT("temple_of_the_land"),TEXT("teacher_of_the_word"),{TEXT("nephites_storehouse_food"),TEXT("thick_clothing"),TEXT("metallurgy")}},
 {TEXT("lamanites"),{TEXT("laman"),TEXT("ammoron"),TEXT("zerahemnah")},TEXT("cimeter_bearer"),TEXT("slinger_of_stones"),TEXT("man_of_the_sword"),TEXT("temple_of_the_land"),TEXT("priestcrafter"),{TEXT("lamanites_storehouse_food"),TEXT("exceedingly_industrious"),TEXT("fight_like_dragons")}},
 {TEXT("gadianton"),{TEXT("gadianton"),TEXT("giddianhi"),TEXT("jacob_gadianton")},TEXT("cimeter_bearer"),TEXT("bowman"),TEXT("cimeter_bearer"),TEXT("secret_combination_lodge"),TEXT("lawyer"),{TEXT("gadianton_storehouse_wild_food"),TEXT("plunder_share"),TEXT("gadianton_storehouse_raid_yield")}},
 {TEXT("coriantumr"),{TEXT("brother_of_jared"),TEXT("morianton"),TEXT("coriantumr")},TEXT("jaredite_warrior"),TEXT("jaredite_archer"),TEXT("jaredite_shield_bearer"),TEXT("temple_of_the_land"),TEXT("oath_keeper"),{TEXT("coriantumr_storehouse_beast_income"),TEXT("thick_clothing"),TEXT("coriantumr_storehouse_beast_income_2")}},
 {TEXT("shiz"),{TEXT("jared"),TEXT("akish"),TEXT("shiz")},TEXT("jaredite_warrior"),TEXT("jaredite_archer"),TEXT("jaredite_shield_bearer"),TEXT("temple_of_the_land"),TEXT("oath_keeper"),{TEXT("shiz_storehouse_beast_income"),TEXT("shiz_storehouse_build_speed"),TEXT("thick_clothing")}},
 {TEXT("laban"),{TEXT("zoram"),TEXT("nehor"),TEXT("korihor")},TEXT("man_of_the_sword"),TEXT("bowman"),TEXT("one_of_the_fifty"),TEXT("winery"),TEXT("drunken_concubine"),{TEXT("laban_storehouse_food"),TEXT("thick_clothing"),TEXT("winery_double_food")}}
};

const FBotFactionPlan& FactionPlan(const FString& Faction) {
 for(const auto& Plan:FactionPlans)if(Faction==Plan.Faction)return Plan;
 return FactionPlans[0];
}

constexpr const TCHAR* Seat=TEXT("seat_of_the_people");
constexpr const TCHAR* Farm=TEXT("fields_of_grain");
constexpr const TCHAR* Store=TEXT("storehouse_of_provisions");
constexpr const TCHAR* House=TEXT("tents_and_habitations");
constexpr const TCHAR* Mine=TEXT("hill_of_ore");
constexpr const TCHAR* Camp=TEXT("encampment_of_war");
constexpr const TCHAR* BotWorkshop=TEXT("house_of_curious_workmanship");
constexpr const TCHAR* WorkerType=TEXT("industrious_laborer");
constexpr const TCHAR* Spy=TEXT("wilderness_spy");
constexpr const TCHAR* Flock=TEXT("gathering_of_flocks");
constexpr const TCHAR* BotBandType=TEXT("gadianton_robber");
constexpr const TCHAR* Rameumptom=TEXT("temple_of_the_land");
constexpr const TCHAR* SupportMoveHoldTag=TEXT("bot_support_move_hold");
constexpr double BotSpyDetectionRange=6.0;
constexpr double BotBaseDefenseRadius=24.0;
constexpr int32 BotFightersBeforeForaging=2;

int32 FogAt(FVector2D Point) {
 return FMath::Clamp(int32(Point.Y/2.5),0,63)*64+FMath::Clamp(int32(Point.X/2.5),0,63);
}

int32 ResourceIndex(const FString& Id) {
 if(Id==TEXT("tree"))return 1;
 if(Id==TEXT("ore")||Id==Mine)return 2;
 if(Id==TEXT("berries")||Id==TEXT("wild_game")||Id==Farm||Id==Flock)return 0;
 return INDEX_NONE;
}

bool IsRaidOrder(const FRTSEntity& Entity) {
 return Entity.Order==TEXT("raid")||Entity.Order==TEXT("raid_move")||Entity.Order==TEXT("raid_return");
}

bool IsRaidBuilding(const FString& Type) {
 return Type==Seat||Type==Store||Type==Farm||Type==Flock||Type==Mine||Type==TEXT("winery");
}

bool IsLand(FVector2D Point,int32 Map,float Margin=0) {
 return Point.X>Margin&&Point.Y>Margin&&Point.X<RTS::MapSize-Margin&&Point.Y<RTS::MapSize-Margin&&
  !RTS::WaterAt(Point,Map)&&!RTS::RockAt(Point,Map);
}
}

void ARTSWorld::ConfigureBot(ARTSController* C,const FString& Action,const TArray<int32>& Values,int32 Slot,const FString& Faction) {
 if(Phase!=0||!C||C->ServerSlot!=0||!Players.IsValidIndex(0)||Players[0].Controller.Get()!=C||!Players[0].bConnected)return;
 const auto& Catalog=FRTSCatalog::Get();
 if(Action==TEXT("bot_add")){
  if(Catalog.Factions.IsEmpty()||(!Faction.IsEmpty()&&Catalog.Faction(Faction).Id.IsEmpty()))return;
  Slot=Players.IndexOfByPredicate([](const FRTSPlayer& P){return !P.bConnected;});
  if(Slot==INDEX_NONE)Slot=Players.Num();
  if(Slot>=4){Error(C,TEXT("The lobby already has four players."));return;}
  FRTSPlayer BotPlayer;
  BotPlayer.Slot=Slot;
  BotPlayer.Team=1-Players[0].Team;
  BotPlayer.Faction=Faction;
  if(BotPlayer.Faction.IsEmpty()){
   const int32 HostFaction=Catalog.Factions.IndexOfByPredicate([&](const FRTSFaction& F){return F.Id==Players[0].Faction;});
   BotPlayer.Faction=Catalog.Factions[(FMath::Max(0,HostFaction)+Slot)%Catalog.Factions.Num()].Id;
  }
  BotPlayer.Name=FString::Printf(TEXT("Computer %d"),Slot+1);
  BotPlayer.bBot=true;
  BotPlayer.bReady=true;
  if(Players.IsValidIndex(Slot))Players[Slot]=MoveTemp(BotPlayer);else Players.Add(MoveTemp(BotPlayer));
 }else{
  if(!Players.IsValidIndex(Slot)||!Players[Slot].bBot)return;
  auto& BotPlayer=Players[Slot];
  if(Action==TEXT("bot_remove")){
   BotPlayer=FRTSPlayer();
   BotPlayer.Slot=Slot;
   BotPlayer.bConnected=false;
   BotPlayer.bDefeated=true;
  }else{
   if(Values.Num()!=2||Values[0]<0||Values[0]>1||Values[1]<0||Values[1]>2||Catalog.Faction(Faction).Id.IsEmpty())return;
   BotPlayer.Team=Values[0];
   BotPlayer.BotDifficulty=Values[1];
   BotPlayer.Faction=Faction;
  }
 }
 Snapshot();
}

class FRTSBotPlanner {
public:
 FRTSBotPlanner(ARTSWorld& World,FRTSPlayer& Player)
  :W(World),P(Player),State(Player.Bot),Config(Difficulties[FMath::Clamp(Player.BotDifficulty,0,2)]),
   Catalog(FRTSCatalog::Get()),Faction(Catalog.Faction(P.Faction)),Plan(FactionPlan(P.Faction)),
   bJaredite(RTS::IsJaredite(P.Faction)),bGadianton(P.Faction==TEXT("gadianton")),bSupportEnabled(RTSBotSupportEnabled(P.Slot)),
   Fog(W.ComputeFog(P)) {
  TArray<int32> Ids;
  W.Entities.GetKeys(Ids);
  Ids.Sort();
  for(int32 Id:Ids){
   auto* E=W.Find(Id);
   if(!E||!W.VisibleInFog(P.Slot,Fog,*E))continue;
   Visible.Add(E);
   const auto& Def=W.D(*E);
   if(E->Owner==P.Slot){
    Owned.Add(E);
    if(Def.bBuilding){Buildings.Add(E);if(Def.Id==Seat&&E->Progress>=1&&!HQ)HQ=E;}
    if(RTS::IsWorker(Def))Workers.Add(E);
    if(RTS::IsMilitary(Def)&&Def.Kind!=TEXT("ship"))Army.Add(E);
    if(bSupportEnabled&&RTS::IsReligious(Def))Support.Add(E);
   }else if(W.Players.IsValidIndex(E->Owner)&&!W.Allied(P.Slot,E->Owner)&&!W.Allied(P.Slot,E->DisguisedOwner)){
    Enemies.Add(E);
    if(Def.bBuilding){
     State.EnemyBuildings.Add(Id,E->Pos);
     if(W.Players[E->Owner].Faction==TEXT("gadianton")){
      auto& Clue=State.HideoutClues.FindOrAdd(E->Owner);
      if(Def.Id==Seat||!Clue.bObservedSeat)Clue.Position=E->Pos;
      Clue.bObservedSeat|=Def.Id==Seat;
     }
    }
   }
  }
  // A remembered building's position stays frozen until its cell is visible again.
  for(auto It=State.EnemyBuildings.CreateIterator();It;++It){
   if(Fog.Cells[FogAt(It.Value())]==2&&!Enemies.ContainsByPredicate([&](const FRTSEntity* E){return E->Id==It.Key();}))It.RemoveCurrent();
  }
  if(HQ&&W.Time<10)State.Home=HQ->Pos;
 }

 void Think() {
  if(Buildings.IsEmpty()&&Workers.IsEmpty()&&Army.IsEmpty()&&Support.IsEmpty())return;
  BaseDefense();
  ManageConstruction();
  ManageProduction();
  AssignWorkers();
  ManageArmy();
  if(bSupportEnabled)ManageSupport();
 }

private:
 ARTSWorld& W;
 FRTSPlayer& P;
 FRTSBotState& State;
 const FBotDifficulty& Config;
 const FRTSCatalog& Catalog;
 const FRTSFaction& Faction;
 const FBotFactionPlan& Plan;
 const bool bJaredite,bGadianton,bSupportEnabled;
 const FRTSFog Fog;
 TArray<FRTSEntity*> Owned,Visible,Buildings,Workers,Army,Support,Enemies;
 TArray<const FRTSEntity*> Escorts;
 FVector2D EscortCenter=FVector2D::ZeroVector;
 FRTSEntity* HQ=nullptr;
 FRTSEntity* Threat=nullptr;
 FRTSTeacherProductionEvent* TeacherObservation=nullptr;

 void Order(const TCHAR* Action,const TArray<int32>& Ids,FVector2D Point=FVector2D::ZeroVector,int32 Target=0,const FString& Type=FString()) {
  if(!Ids.IsEmpty())W.CommandForOwner(P.Slot,nullptr,Action,Ids,Point,Target,Type);
 }

 int32 Count(const TCHAR* Type,bool bIncludeJobs=false) const {
  int32 Result=0;
  for(const auto* E:Owned){
   if(W.D(*E).Id==Type)++Result;
   if(bIncludeJobs)for(const auto& Job:E->Jobs)if(Job.Type==Type)++Result;
  }
  return Result;
 }

 bool Prerequisites(const FRTSDef& Def) const {
  for(const FString& Req:Def.Requires)if(!W.Has(P.Slot,Req)&&!W.OwnedStructure(P.Slot,Req))return false;
  return true;
 }

 bool CanAfford(FVector Cost,float Cement=0) const {
  return P.Resources.X>=Cost.X&&P.Resources.Y>=Cost.Y&&P.Resources.Z>=Cost.Z&&P.Cement>=Cement;
 }

 bool QueueType(FRTSEntity& Producer,const FString& Type,const TCHAR* Action) {
  const auto& Def=Catalog.Def(Type);
  if(Def.Id.IsEmpty()||P.Age<Catalog.RequiredAge(Def,P.Faction)||!Prerequisites(Def)||
   !CanAfford(Catalog.Price(Def,P.Faction),Def.CementCost))return false;
  const int32 Before=Producer.Jobs.Num();
  Order(Action,{Producer.Id},Producer.Pos,0,Type);
  const bool bQueued=Producer.Jobs.Num()>Before;
  if(bQueued&&TeacherObservation)TeacherObservation->QueuedOrders.Add(Type);
  if(bQueued&&bGadianton){
   auto& RaidTrace=State.GadiantonRaidTrace;
   const auto& ProducerType=W.D(Producer).Id;
   if(RaidTrace.bEnabled&&RaidTrace.OpeningJobs.Num()<64)
    RaidTrace.OpeningJobs.Add({Type,ProducerType,Producer.Jobs.Last().Paid,W.Time});
   if(State.bGadiantonCombatFirstForProbe&&State.GadiantonOpeningFightersOrdered<BotFightersBeforeForaging&&
      ProducerType==Camp&&(Type==Plan.Frontline||Type==Plan.Ranged))++State.GadiantonOpeningFightersOrdered;
  }
  return bQueued;
 }

 bool DeferForestForaging() const {
  return State.bGadiantonCombatFirstForProbe&&State.GadiantonOpeningFightersOrdered<BotFightersBeforeForaging;
 }

 bool Research(const TCHAR* Type) {
  if(bGadianton&&DeferForestForaging()&&FCString::Strcmp(Type,Plan.Research[0])==0)return false;
  if(!Faction.Upgrades.Contains(Type)||W.Has(P.Slot,Type))return false;
  const auto& Def=Catalog.Def(Type);
  for(auto* E:Buildings)if(E->Progress>=1&&E->Jobs.IsEmpty()&&W.D(*E).Id==Def.Producer)return QueueType(*E,Type,TEXT("research"));
  return false;
 }

 FRTSEntity* BestBuilder(const FRTSDef& Site,FVector2D Pos,const FRTSEntity* Existing=nullptr) const {
  FRTSEntity* Best=nullptr;
  double BestScore=TNumericLimits<double>::Max();
  for(auto* Unit:Owned){
   if(Unit->Order==TEXT("build")||Unit->Order==TEXT("repair")||!Unit->Evolution.IsEmpty())continue;
   const int32 Tier=W.BeastTier(*Unit);
   if(bJaredite?(Tier<=0||Site.Age>Tier+1||(Site.CementCost>0&&Tier<3)):!RTS::IsWorker(W.D(*Unit)))continue;
   if(Existing&&!W.CanBuildSite(*Unit,*Existing))continue;
   const double Score=FVector2D::DistSquared(Unit->Pos,Pos)+(Unit->Order==TEXT("idle")?0:25)+Unit->Cargo.SizeSquared();
   if(Score<BestScore){BestScore=Score;Best=Unit;}
  }
  return Best;
 }

 bool ClearSite(const FRTSDef& Def,FVector2D Point,int32 Ignore=0) const {
  if(Fog.Cells[FogAt(Point)]!=2||!IsLand(Point,W.Map,Def.Radius+1))return false;
  for(const auto* E:Visible){
   const auto& Other=W.D(*E);
   if(E->Id==Ignore||(!Other.bBuilding&&Other.Id!=TEXT("tree")&&Other.Id!=TEXT("ore")))continue;
   if(FVector2D::DistSquared(Point,E->Pos)<FMath::Square(Def.Radius+Other.Radius+.7f))return false;
  }
  return true;
 }

 bool Construct(const TCHAR* Type) {
  if(!Faction.Buildings.Contains(Type))return false;
  const auto& Def=Catalog.Def(Type);
  if(P.Age<Catalog.RequiredAge(Def,P.Faction)||!Prerequisites(Def)||!CanAfford(Def.Cost,Def.CementCost))return false;
  const FVector2D Base=HQ?HQ->Pos:State.Home;
  auto Place=[&](FVector2D At,int32 Target){
   if(!ClearSite(Def,At,Target))return false;
   auto* Builder=BestBuilder(Def,At);
   if(!Builder)return false;
   const int32 Before=W.NextId;
   Order(TEXT("build"),{Builder->Id},At,Target,Type);
   return W.NextId>Before;
  };
  if(Def.Id==Mine){
   TArray<FRTSEntity*> Deposits;
   for(auto* E:Visible)if(E->Owner<0&&W.D(*E).Id==TEXT("ore")&&E->ResourceRemaining>60&&FVector2D::DistSquared(E->Pos,Base)<FMath::Square(32.0))Deposits.Add(E);
   Deposits.Sort([&](const FRTSEntity& A,const FRTSEntity& B){
    const double DA=FVector2D::DistSquared(A.Pos,Base),DB=FVector2D::DistSquared(B.Pos,Base);
    return DA==DB?A.Id<B.Id:DA<DB;
   });
   for(const auto* Ore:Deposits)if(Place(Ore->Pos,Ore->Id))return true;
   return false;
  }
  if(!BestBuilder(Def,Base))return false;
  const FVector2D Toward=(FVector2D(80,80)-Base).GetSafeNormal();
  const float Facing=FMath::Atan2(Toward.Y,Toward.X)+(Def.Id==Farm?PI:0);
  for(const float Radius:{7.f,10.f,13.f,16.f,19.f,22.f,25.f}){
   for(int32 I=0;I<16;++I){
    const int32 Direction=(I+1)/2*(I%2==0?1:-1);
    const double Angle=Facing+Direction*PI/8;
    const FVector2D At=Base+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius;
    if(Place(At,0))return true;
   }
  }
  return false;
 }

 void BaseDefense() {
  double Nearest=FMath::Square(BotBaseDefenseRadius);
  for(auto* E:Enemies){
   const auto& Def=W.D(*E);
   if(!Def.bBuilding&&!RTS::IsMilitary(Def)&&!RTS::IsReligious(Def))continue;
   if(Def.bBuilding&&Def.Attack<=0)continue;
   const double Distance=FVector2D::DistSquared(State.Home,E->Pos);
   if(Distance<Nearest){Nearest=Distance;Threat=E;}
  }
  if(State.EmergencySlingerMode!=ERTSEmergencySlingerMode::Inactive)
   State.EmergencySlingerTrace.bObservedBaseThreat=Threat!=nullptr;
 }

 void ManageConstruction() {
  int32 Foundations=0;
  for(auto* Site:Buildings){
   if(Site->Progress>=1)continue;
   ++Foundations;
   const bool bAssigned=Owned.ContainsByPredicate([&](const FRTSEntity* E){return E->Target==Site->Id&&E->Order==TEXT("build");});
   if(!bAssigned)if(auto* Builder=BestBuilder(W.D(*Site),Site->Pos,Site))Order(TEXT("context"),{Builder->Id},Site->Pos,Site->Id);
  }
  if(!HQ){if(Count(Seat)==0)Construct(Seat);return;}
  if(Threat&&HQ->HP<HQ->MaxHP*.75&&P.Resources.Y>=30&&
   !Owned.ContainsByPredicate([&](const FRTSEntity* E){return E->Target==HQ->Id&&E->Order==TEXT("repair");})){
   if(auto* Builder=BestBuilder(W.D(*HQ),HQ->Pos,HQ))Order(TEXT("context"),{Builder->Id},HQ->Pos,HQ->Id);
  }
  if(Foundations>=(bJaredite?1:2))return;
  const int32 Free=P.Capacity-P.Population;
  if(P.Capacity<200&&Free<=(P.Age==1?5:7)&&Count(House)<(P.Capacity-15)/10+1&&Construct(House))return;
  if(P.Age>=2&&Count(Camp)==0&&Construct(Camp))return;
  if(!bGadianton&&Count(Farm)==0&&Construct(Farm))return;
  if(Count(Mine)==0&&Construct(Mine))return;
  if(Count(Store)==0&&Construct(Store))return;
  if(P.Age<2)return;
  if(Count(BotWorkshop)==0&&Construct(BotWorkshop))return;
  if(Threat&&Count(TEXT("tower_of_watchmen"))==0&&Construct(TEXT("tower_of_watchmen")))return;
  if(bJaredite&&Count(Flock)==0&&Construct(Flock))return;
  if(Count(Plan.SpecialtyBuilding)==0&&Construct(Plan.SpecialtyBuilding))return;
  if(P.Faction==TEXT("laban")&&P.Age==4&&Count(TEXT("temple_of_the_land"))==0&&Construct(TEXT("temple_of_the_land")))return;
  if(!bGadianton&&Workers.Num()>=12&&Count(Farm)<2&&Construct(Farm))return;
  if(P.Resources.Y>=160&&Count(Camp)<Config.Camps&&Construct(Camp))return;
  if(bJaredite&&P.Age>=4&&Count(TEXT("cement_tower"))==0)Construct(TEXT("cement_tower"));
 }

 const FRTSDef& AgeChoice() const {
  if(P.Age>=4)return Catalog.Def(INDEX_NONE);
  return Catalog.Def(FString::Printf(TEXT("age%d_%s_%s"),P.Age+1,*P.Faction,Plan.Heroes[P.Age-1]));
 }

 const FRTSEntity* FirstTeacherTemple() const {
  if(!RTSBotSupportTraceEnabled()||P.Faction!=TEXT("nephites")||State.FirstTeacherTrace.FirstQueued>=0)return nullptr;
  const FRTSEntity* Complete=nullptr;
  for(const auto* E:Buildings){
   if(E->Progress<1||W.D(*E).Id!=Rameumptom)continue;
   if(E->Jobs.IsEmpty())return E;
   if(!Complete)Complete=E;
  }
  return Complete;
 }

 FRTSTeacherProductionSnapshot ObserveTeacherProduction(const FRTSEntity& Temple) const {
  FRTSTeacherProductionSnapshot Result;
  Result.Resources=P.Resources;
  Result.FreePopulation=P.Capacity-P.Population;
  Result.TempleJobs=Temple.Jobs.Num();
  const auto& Teacher=Catalog.Def(Plan.Support);
  Result.bCanAfford=CanAfford(Catalog.Price(Teacher,P.Faction),Teacher.CementCost);
  for(const auto* E:Owned)for(const auto& Job:E->Jobs)Result.MilitaryJobs+=RTS::IsMilitary(Catalog.Def(Job.Type));
  return Result;
 }

 void RecordTeacherDecision(FRTSTeacherProductionEvent& Observation) {
  auto& Trace=State.FirstTeacherTrace;
  // Record changes in eligibility and affordability without logging each bot think.
  const uint32 Context=(Observation.bVisibleBaseThreat?1u:0u)|(Observation.bSavingAge?2u:0u)|
   (Observation.bAgeReserve?4u:0u)|(Observation.Before.bCanAfford?8u:0u)|(Observation.After.bCanAfford?16u:0u)|
   (Observation.Before.FreePopulation>=2?32u:0u)|(Observation.After.FreePopulation>=2?64u:0u)|
   (Observation.NearbyDefenders>=3?128u:0u)|(Observation.Before.TempleJobs>0?256u:0u);
  if(Trace.LastGate==Observation.Gate&&Trace.LastBlockingOrder==Observation.BlockingOrder&&
     Trace.LastTempleId==Observation.TempleId&&Trace.LastContext==Context)return;
  Trace.LastGate=Observation.Gate;
  Trace.LastBlockingOrder=Observation.BlockingOrder;
  Trace.LastTempleId=Observation.TempleId;
  Trace.LastContext=Context;
  constexpr int32 EventLimit=64;
  // Keep one event available for the first accepted Teacher even in a long siege.
  const int32 Limit=Observation.Gate==TEXT("teacher_queued")?EventLimit:EventLimit-1;
  if(Trace.Events.Num()>=Limit){++Trace.DroppedTransitions;return;}
  UE_LOG(LogTemp,Display,TEXT("RTS_BALANCE_FIRST_TEACHER time=%.2f slot=%d gate=%s blocked_by=%s temple=%d army=%d nearby=%d free_pop=%d->%d funds=%.1f,%.1f,%.1f->%.1f,%.1f,%.1f threat=%d saving_age=%d age_reserve=%d queued=%s"),
   Observation.Time,P.Slot,*Observation.Gate,*Observation.BlockingOrder,Observation.TempleId,Observation.Army,Observation.NearbyDefenders,
   Observation.Before.FreePopulation,Observation.After.FreePopulation,
   Observation.Before.Resources.X,Observation.Before.Resources.Y,Observation.Before.Resources.Z,
   Observation.After.Resources.X,Observation.After.Resources.Y,Observation.After.Resources.Z,
   Observation.bVisibleBaseThreat,Observation.bSavingAge,Observation.bAgeReserve,*FString::Join(Observation.QueuedOrders,TEXT(",")));
  Trace.Events.Add(MoveTemp(Observation));
 }

 void ManageProduction() {
  const auto* TeacherTemple=FirstTeacherTemple();
  FRTSTeacherProductionEvent Observation;
  const TCHAR* TeacherGate=TEXT("production_army_or_food_gate");
  const TCHAR* BlockingOrder=TEXT("");
  if(TeacherTemple){
   TeacherObservation=&Observation;
   Observation.Time=W.Time;
   Observation.TempleId=TeacherTemple->Id;
   Observation.Age=P.Age;
   Observation.Army=Army.Num();
   Observation.bVisibleBaseThreat=Threat!=nullptr;
   Observation.Before=ObserveTeacherProduction(*TeacherTemple);
   const FVector2D Base=HQ?HQ->Pos:State.Home;
   for(const auto* Unit:Army){
    Observation.NearbyDefenders+=W.D(*Unit).Id!=Spy&&Unit->Progress>=1&&Unit->DisarmedUntil<=W.Time&&
     FVector2D::DistSquared(Unit->Pos,Base)<=FMath::Square(BotBaseDefenseRadius);
   }
  }
  ON_SCOPE_EXIT {
   if(TeacherTemple){
    TeacherObservation=nullptr;
    Observation.After=ObserveTeacherProduction(*TeacherTemple);
    Observation.Gate=TeacherGate;
    Observation.BlockingOrder=BlockingOrder;
    RecordTeacherDecision(Observation);
   }
  };
  const auto& Age=AgeChoice();
  const bool bAgeQueued=HQ&&HQ->Jobs.ContainsByPredicate([&](const FRTSJob& J){return Catalog.Def(J.Type).Kind==TEXT("age_option");});
  const int32 WorkerCount=Workers.Num()+Count(WorkerType,true)-Count(WorkerType);
  auto& RaidTrace=State.GadiantonRaidTrace;
  const bool bTraceProduction=RaidTrace.bEnabled&&RaidTrace.FirstBandQueued<0;
  const TCHAR* RaidGate=TEXT("no_storehouse");
  const FRTSEntity* TraceStore=nullptr;
  FVector RaidResources=P.Resources;
  if(bTraceProduction){
   for(const auto* B:Buildings){
    if(W.D(*B).Id!=Store)continue;
    if(!TraceStore||B->Progress>TraceStore->Progress)TraceStore=B;
   }
   if(TraceStore)RaidGate=TraceStore->Progress<1?TEXT("storehouse_under_construction"):
    P.Age<2?TEXT("age_below_two"):!TraceStore->Jobs.IsEmpty()?TEXT("storehouse_busy"):
    Army.Num()<3?TEXT("army_below_three"):TEXT("band_queue_unreached");
  }
  ON_SCOPE_EXIT {
   if(bTraceProduction){
    const FString Job=TraceStore&&!TraceStore->Jobs.IsEmpty()?TraceStore->Jobs[0].Type:FString();
    const bool bForagingComplete=W.Has(P.Slot,Plan.Research[0]);
    const auto* Last=RaidTrace.Production.IsEmpty()?nullptr:&RaidTrace.Production.Last();
    const int32 StoreId=TraceStore?TraceStore->Id:0;
    if((!Last||Last->Gate!=RaidGate||Last->StoreJob!=Job||Last->StoreId!=StoreId||Last->Age!=P.Age||
        Last->bForagingComplete!=bForagingComplete||Last->bThreat!=(Threat!=nullptr))&&RaidTrace.Production.Num()<96){
     RaidTrace.Production.Add({RaidGate,Job,RaidResources,W.Time,P.Age,WorkerCount,Army.Num(),P.Capacity-P.Population,
      StoreId,Threat!=nullptr,bForagingComplete});
    }
   }
  };
  if(bGadianton&&P.Age>=2&&WorkerCount>=6&&!Threat&&!W.Has(P.Slot,Plan.Research[0])&&!DeferForestForaging()){
   // Wild-food research pays for itself in this opening. Reserve its price
   // before recruitment draws food down to the individual unit price again.
   for(auto* B:Buildings){
    if(W.D(*B).Id!=Store||B->Progress<1||!B->Jobs.IsEmpty())continue;
    if(!Research(Plan.Research[0])){
     if(bTraceProduction){RaidGate=TEXT("reserving_forest_foraging");TraceStore=B;}
     return;
    }
    if(bTraceProduction){RaidGate=TEXT("forest_foraging_queued");TraceStore=B;}
    break;
   }
  }
  const bool bSaveAge=P.Age==1||(!Age.Id.IsEmpty()&&Army.Num()>=Config.Wave&&Workers.Num()>=Config.Workers-2&&!Threat);
  Observation.bSavingAge=bSaveAge;
  if(HQ&&HQ->Jobs.IsEmpty()){
   if(bJaredite&&Count(TEXT("curelom"),true)+Count(TEXT("cumelom"),true)+Count(TEXT("elephant"),true)==0){
    QueueType(*HQ,TEXT("curelom"),TEXT("train"));
   }else if(WorkerCount<(P.Age==1?Config.OpeningWorkers:6)){
    QueueType(*HQ,WorkerType,TEXT("train"));
   }else if(bSaveAge&&QueueType(*HQ,Age.Id,TEXT("age"))){
    // The Seat's queue holds the paid age job until advancement completes.
   }else if(P.Age>=2&&WorkerCount<Config.Workers&&(!bSaveAge||P.Resources.X>=Age.Cost.X+70)&&
     (WorkerCount<Config.OpeningWorkers||P.Resources.X>=100||Army.Num()>=Config.Wave)){
    QueueType(*HQ,WorkerType,TEXT("train"));
   }else if(bJaredite&&P.Age>=2&&!bSaveAge&&Count(TEXT("curelom"),true)<2&&P.Resources.X>=160){
    QueueType(*HQ,TEXT("curelom"),TEXT("train"));
   }
  }

  if(P.Age<2){TeacherGate=TEXT("age_below_two");return;}
  const bool bReserve=bSaveAge&&!bAgeQueued&&HQ&&!HQ->Jobs.ContainsByPredicate([&](const FRTSJob& J){return J.Type==Age.Id;});
  Observation.bAgeReserve=bReserve;
  const bool bTrainArmy=Army.Num()<Config.Army&&(!bReserve||P.Resources.X>=Age.Cost.X+120);
  const bool bReplaceDetector=State.bCampaign&&!State.HideoutClues.IsEmpty()&&Count(Spy,true)==0;
  const bool bEmergencySlinger=State.EmergencySlingerMode!=ERTSEmergencySlingerMode::Inactive&&
   P.Faction==TEXT("nephites")&&P.Age==2&&Threat&&Army.Num()<4;
  for(auto* B:Buildings){
   if(B->Progress<1||!B->Jobs.IsEmpty())continue;
   const auto& Type=W.D(*B).Id;
   if(Type==Camp&&bReplaceDetector&&Count(Spy,true)==0&&QueueType(*B,Spy,TEXT("train")))continue;
   if(Type==Camp&&bTrainArmy){
    const int32 Spies=Count(Spy,true);
    if(Spies==0&&QueueType(*B,Spy,TEXT("train")))continue;
    if(bEmergencySlinger){
     auto& SlingerTrace=State.EmergencySlingerTrace;
     ++SlingerTrace.EligibleCamps;
     if(SlingerTrace.FirstEligible<0)SlingerTrace.FirstEligible=W.Time;
     if(State.EmergencySlingerMode==ERTSEmergencySlingerMode::Priority){
      ++SlingerTrace.Attempts;
      if(QueueType(*B,TEXT("slinger_of_stones"),TEXT("train"))){
       ++SlingerTrace.Queued;
       if(SlingerTrace.FirstQueued<0)SlingerTrace.FirstQueued=W.Time;
       continue;
      }
     }
    }
    const int32 Ranged=Count(Plan.Ranged,true);
    if(Ranged*3<Army.Num()+1&&QueueType(*B,Plan.Ranged,TEXT("train")))continue;
    if(P.Age>=3&&Count(Plan.Advanced,true)*3<Army.Num()&&QueueType(*B,Plan.Advanced,TEXT("train")))continue;
    if(QueueType(*B,Plan.Frontline,TEXT("train")))continue;
    QueueType(*B,Plan.Ranged,TEXT("train"));
   }else if(Type==Store&&bGadianton&&Army.Num()>=3&&Count(BotBandType,true)<(P.BotDifficulty==2?3:2)){
    if(bTraceProduction){RaidResources=P.Resources;TraceStore=B;}
    const bool bQueued=QueueType(*B,BotBandType,TEXT("train"));
    if(bTraceProduction){
     RaidGate=bQueued?TEXT("band_queued"):TEXT("band_queue_failed");
     if(bQueued)RaidTrace.FirstBandQueued=W.Time;
    }
   }else if(Type==Flock&&bJaredite){
    if(P.Age>=4&&Count(TEXT("elephant"),true)==0)QueueType(*B,TEXT("elephant"),TEXT("train"));
    else if(P.Age>=3&&Count(TEXT("cumelom"),true)<2)QueueType(*B,TEXT("cumelom"),TEXT("train"));
    else if(Count(TEXT("curelom"),true)<2&&P.Resources.X>=120)QueueType(*B,TEXT("curelom"),TEXT("train"));
   }
  }
  if(bReserve&&P.Resources.X<Age.Cost.X+180){TeacherGate=TEXT("saving_next_age");return;}
  if(Army.Num()>=3||P.Resources.X>=250){
   for(const TCHAR* Tech:Plan.Research)if(Research(Tech)){TeacherGate=TEXT("research_queued");BlockingOrder=Tech;return;}
   if(P.Faction==TEXT("laban")&&P.Age>=3&&Research(TEXT("laban_and_his_fifty")))return;
   if(Research(TEXT("exceedingly_industrious"))){TeacherGate=TEXT("research_queued");BlockingOrder=TEXT("exceedingly_industrious");return;}
   if(Army.Num()>=Config.Wave&&HQ){
    for(const FString& Hero:P.HeroChoices){
     if(HQ->Jobs.IsEmpty()&&W.CanTrainHero(P.Slot,Hero)&&W.Time-P.HeroDeaths.FindRef(Hero)>=90&&QueueType(*HQ,Hero,TEXT("train"))){
      TeacherGate=TEXT("hero_queued");BlockingOrder=*Hero;return;
     }
    }
   }
   const auto& SupportDef=Catalog.Def(Plan.Support);
   if(Army.Num()>=4&&Count(Plan.Support,true)<2&&Faction.Units.Contains(Plan.Support)){
    TeacherGate=TEXT("temple_busy");
    for(auto* B:Buildings){
     if(B->Progress<1||!B->Jobs.IsEmpty()||W.D(*B).Id!=SupportDef.Producer)continue;
     const FVector Before=P.Resources;
     const bool bQueued=QueueType(*B,SupportDef.Id,TEXT("train"));
     if(TeacherObservation&&State.FirstTeacherTrace.FirstQueued<0){
      auto& Trace=State.FirstTeacherTrace;
      if(Trace.FirstAttempt<0)Trace.FirstAttempt=W.Time;
      if(bQueued){TeacherGate=TEXT("teacher_queued");Trace.FirstQueued=W.Time;}
      else if(P.Age<Catalog.RequiredAge(SupportDef,P.Faction)||!Prerequisites(SupportDef))TeacherGate=TEXT("teacher_prerequisite");
      else if(!CanAfford(Catalog.Price(SupportDef,P.Faction),SupportDef.CementCost))TeacherGate=TEXT("teacher_resources");
      else if(P.Population+SupportDef.Pop>P.Capacity)TeacherGate=TEXT("teacher_population");
      else TeacherGate=TEXT("teacher_authority_rejected");
     }
     if(RTSBotSupportTraceEnabled()&&State.SupportTrainingTraceCount<40){
      ++State.SupportTrainingTraceCount;
      const FVector Cost=Catalog.Price(SupportDef,P.Faction);
      UE_LOG(LogTemp,Display,TEXT("RTS_BALANCE_SUPPORT_QUEUE time=%.1f slot=%d type=%s producer=%d queued=%d resources=%.1f,%.1f,%.1f cost=%.1f,%.1f,%.1f army=%d pop=%d/%d"),
       W.Time,P.Slot,*SupportDef.Id,B->Id,bQueued,Before.X,Before.Y,Before.Z,Cost.X,Cost.Y,Cost.Z,Army.Num(),P.Population,P.Capacity);
     }
    }
   }else TeacherGate=Army.Num()<4?TEXT("army_below_four"):TEXT("support_roster_gate");
  }
 }

 FVector2D ScoutPoint(FVector2D From,bool bMilitary) {
  FVector2D Best=From;
  int32 BestIndex=INDEX_NONE;
  double BestScore=-TNumericLimits<double>::Max();
  for(int32 Y=10;Y<160;Y+=14)for(int32 X=10;X<160;X+=14){
   const FVector2D At(X,Y);
   if(!IsLand(At,W.Map,3)||FVector2D::DistSquared(At,From)<100)continue;
   const int32 Cell=FogAt(At);
   const float* Last=State.ScoutVisits.Find(Cell);
   if(Last&&W.Time-*Last<130)continue;
   int32 Unexplored=0;
   for(const FVector2D Offset:{FVector2D(0,0),FVector2D(6,0),FVector2D(-6,0),FVector2D(0,6),FVector2D(0,-6)})if(Fog.Cells[FogAt(At+Offset)]==0)++Unexplored;
   if(!bMilitary&&Unexplored==0)continue;
   const double Distance=FVector2D::Distance(At,From);
   const double BaseDistance=FVector2D::Distance(At,State.Home);
   const double Score=Unexplored*13-Distance-(bMilitary?FMath::Abs(75-BaseDistance)*.32:BaseDistance*.7);
   if(Score>BestScore){Best=At;BestScore=Score;BestIndex=Cell;}
  }
  if(BestIndex!=INDEX_NONE)State.ScoutVisits.Add(BestIndex,W.Time);
  return Best;
 }

 bool ScoutObservedHideout(FRTSEntity& Scout) {
  if(!State.bCampaign||State.HideoutClues.IsEmpty())return false;
  // Keep the mobile detector in range when it has a sight line to a Seat the
  // army can attack. The enemy list is already filtered through player fog.
  for(const auto* E:Enemies){
   if(W.D(*E).Id!=Seat||W.Players[E->Owner].Faction!=TEXT("gadianton"))continue;
   if(FVector2D::DistSquared(Scout.Pos,E->Pos)>FMath::Square(BotSpyDetectionRange)||!W.LOS(Scout.Pos,E->Pos,Scout.Id,E->Id))continue;
   if(Scout.Order!=TEXT("hold"))Order(TEXT("hold"),{Scout.Id});
   return true;
  }
  if(State.HideoutSweepOwner==INDEX_NONE){
   if(W.Time<State.NextHideoutSweep)return false;
   double Nearest=TNumericLimits<double>::Max();
   for(const auto& Clue:State.HideoutClues){
    if(W.Players[Clue.Key].bDefeated)continue;
    const double Distance=FVector2D::DistSquared(Scout.Pos,Clue.Value.Position);
    if(Distance>=Nearest)continue;
    Nearest=Distance;
    State.HideoutSweepOwner=Clue.Key;
    State.HideoutSweepCenter=Clue.Value.Position;
   }
   if(State.HideoutSweepOwner==INDEX_NONE)return false;
   State.HideoutSweepVisited=0;
   State.HideoutSweepUntil=W.Time+90;
   State.ScoutGoalUntil=0;
  }
  if(W.Time<State.HideoutSweepUntil&&!W.Players[State.HideoutSweepOwner].bDefeated){
   if(Scout.Order==TEXT("move")&&W.Time<State.ScoutGoalUntil&&FVector2D::DistSquared(Scout.Pos,State.ScoutGoal)>4)return true;
   constexpr int32 Width=5;
   constexpr float Spacing=7;
   double Nearest=TNumericLimits<double>::Max();
   int32 Chosen=INDEX_NONE;
   FVector2D Goal=Scout.Pos;
   const int32 Middle=(Width*Width)/2;
   for(int32 Cell=0;Cell<Width*Width;++Cell){
    if(State.HideoutSweepVisited&(1u<<Cell))continue;
    const FVector2D At=State.HideoutSweepCenter+FVector2D(Cell%Width-Width/2,Cell/Width-Width/2)*Spacing;
    if(!IsLand(At,W.Map,1))continue;
    const double Distance=FVector2D::DistSquared(Scout.Pos,At);
    if(Distance<=4)continue;
    const double Score=Cell==Middle?0:Distance;
    if(Score<Nearest){Nearest=Score;Chosen=Cell;Goal=At;}
   }
   if(Chosen!=INDEX_NONE){
    State.HideoutSweepVisited|=1u<<Chosen;
    State.ScoutGoal=Goal;
    const float Travel=float(FVector2D::Distance(Scout.Pos,Goal))/FMath::Max(1.f,W.D(Scout).Speed);
    State.ScoutGoalUntil=W.Time+FMath::Clamp(10.f+Travel*1.5f,10.f,45.f);
    Order(TEXT("move"),{Scout.Id},Goal);
    return true;
   }
  }
  State.HideoutSweepOwner=INDEX_NONE;
  State.NextHideoutSweep=W.Time+45;
  State.ScoutGoalUntil=0;
  return false;
 }

 void AssignWorkers() {
  TMap<int32,int32> Assigned;
  int32 OnResource[3]={};
  bool HasResource[3]={};
  for(const auto* Site:Visible){
   const int32 Resource=ResourceIndex(W.D(*Site).Id);
   if(Resource!=INDEX_NONE&&!Workers.IsEmpty()&&W.CanAssignWork(*Workers[0],*Site))HasResource[Resource]=true;
  }
  for(auto* Unit:Workers){
   auto* Site=Visible.FindByPredicate([&](const FRTSEntity* E){return E->Id==Unit->WorkTarget;});
   if(!Site)continue;
   const int32 Resource=ResourceIndex(W.D(**Site).Id);
   if(Resource==INDEX_NONE)continue;
   if(!Unit->DropTarget&&W.Time-Unit->LastMove>18&&FVector2D::DistSquared(Unit->Pos,W.WorkPosition(*Unit,**Site))>9){
    State.RejectedWork.Add(Unit->Id,W.Time+25);
    Order(TEXT("stop"),{Unit->Id});
    continue;
   }
   ++Assigned.FindOrAdd((*Site)->Id);
   ++OnResource[Resource];
  }
  const auto& Age=AgeChoice();
  const double Wants[3]={
   140+(P.Age==1?Age.Cost.X:140),P.Age==1?140.:220.,100+(P.Age==1?Age.Cost.Z:90)
  };
  bool bSurplusOre=false;
  if(bGadianton&&State.bGadiantonFoodAssignmentForProbe&&P.Age<=2&&!Threat){
   // Reserve enough Precious Things for the paid age advance, or for the
   // first Spy, two regular fighters and Band in Age II.
   const FVector FrontlineCost=Catalog.Price(Catalog.Def(Plan.Frontline),P.Faction);
   const double FoodReserve=P.Age==1?Age.Cost.X:2*FrontlineCost.X;
   const double PreciousReserve=P.Age==1?Age.Cost.Z:2*FrontlineCost.Z+
    Catalog.Price(Catalog.Def(Spy),P.Faction).Z+Catalog.Price(Catalog.Def(BotBandType),P.Faction).Z;
   bSurplusOre=P.Resources.X<FoodReserve&&P.Resources.Z>=PreciousReserve;
  }
  bool bExploring=Workers.ContainsByPredicate([](const FRTSEntity* E){return E->Order==TEXT("move");});
  const bool bNeedsSearch=(!HasResource[0]&&P.Resources.X<100)||(!HasResource[1]&&P.Resources.Y<80)||(!HasResource[2]&&P.Resources.Z<70);
  for(auto* Unit:Workers){
   if(Unit->Order!=TEXT("idle"))continue;
   if(Unit->Cargo.SizeSquared()>1){
    FRTSEntity* Drop=nullptr;
    double BestDistance=TNumericLimits<double>::Max();
    for(auto* Site:Buildings){
     const double Distance=FVector2D::DistSquared(Unit->Pos,Site->Pos);
     if(Distance<BestDistance&&W.CanUseDropoff(*Unit,*Site)){Drop=Site;BestDistance=Distance;}
    }
    if(Drop){Order(TEXT("deliver"),{Unit->Id},Drop->Pos,Drop->Id);continue;}
   }
   if(bNeedsSearch&&!bExploring){
    const FVector2D Point=ScoutPoint(Unit->Pos,false);
    if(!Point.Equals(Unit->Pos)){Order(TEXT("move"),{Unit->Id},Point);bExploring=true;continue;}
   }
   FRTSEntity* Best=nullptr;
   int32 BestResource=INDEX_NONE;
   double BestScore=-TNumericLimits<double>::Max();
   FRTSEntity* Alternative=nullptr;
   int32 AlternativeResource=INDEX_NONE;
   double AlternativeScore=-TNumericLimits<double>::Max();
   bool bLocalWildFood=false;
   for(auto* Site:Visible){
    const auto& Def=W.D(*Site);
    const int32 Resource=ResourceIndex(Def.Id);
    if(Resource==INDEX_NONE||W.Allied(P.Slot,Site->DisguisedOwner)||!W.CanAssignWork(*Unit,*Site))continue;
    const double Distance=FVector2D::Distance(Site->Pos,Unit->Pos);
    const double Return=FVector2D::Distance(Site->Pos,HQ?HQ->Pos:State.Home);
    const float* Retry=State.RejectedWork.Find(Unit->Id);
    if(Retry&&W.Time<*Retry&&Distance>18)continue;
    const bool bPreferred=Def.Id==Mine||Def.Id==Farm||Def.Id==Flock;
    const double Need=FMath::Clamp((Wants[Resource]-P.Resources[Resource])/Wants[Resource],-.25,1.);
    const double Score=36*Need-10*OnResource[Resource]-5*Assigned.FindRef(Site->Id)-Distance*.55-Return*.36+
     (bPreferred?12:0)+(Resource==0?9:0);
    if(Score>BestScore){Best=Site;BestResource=Resource;BestScore=Score;}
    if(bSurplusOre){
     const bool bWildFood=Resource==0&&Site->Owner<0&&Return<=BotBaseDefenseRadius;
     bLocalWildFood|=bWildFood;
     if((bWildFood||Resource==1)&&Score>AlternativeScore){Alternative=Site;AlternativeResource=Resource;AlternativeScore=Score;}
    }
   }
   const bool bDivertedSurplusOre=BestResource==2&&bLocalWildFood&&Alternative;
   if(bDivertedSurplusOre){
    Best=Alternative;BestResource=AlternativeResource;
   }
   if(Best){
    Order(TEXT("gather"),{Unit->Id},Best->Pos,Best->Id);
    if(bDivertedSurplusOre&&Unit->WorkTarget==Best->Id&&State.GadiantonRaidTrace.bEnabled)++State.GadiantonRaidTrace.SurplusOreDiversions;
    ++Assigned.FindOrAdd(Best->Id);
    ++OnResource[BestResource];
    continue;
   }
   if(bExploring)continue;
   const FVector2D Point=ScoutPoint(Unit->Pos,false);
   if(!Point.Equals(Unit->Pos)){Order(TEXT("move"),{Unit->Id},Point);bExploring=true;}
  }
 }

 void ObserveRaidVisibility() {
  auto& Trace=State.GadiantonRaidTrace;
  if(!Trace.bEnabled)return;
  FRTSRaidVisibilityEvent Now;
  Now.Time=W.Time;
  Now.ThreatId=Threat?Threat->Id:0;
  const FRTSEntity* FirstBand=nullptr;
  for(const auto* Unit:Army)if(W.D(*Unit).Id==BotBandType){
   ++Now.Bands;
   if(!FirstBand)FirstBand=Unit;
  }
  const FVector2D From=FirstBand?FirstBand->Pos:State.Home;
  for(const auto* Victim:Enemies){
   if(Victim->Progress<1||Victim->Carrier||Victim->HP<=0)continue;
   const auto& Def=W.D(*Victim);
   if(IsRaidBuilding(Def.Id)){
    ++Now.Buildings;
    const double Distance=FVector2D::Distance(From,Victim->Pos);
    if(Now.NearestBuildingId==0||Distance<Now.NearestBuildingDistance){
     Now.NearestBuildingId=Victim->Id;Now.NearestBuildingDistance=Distance;
    }
   }else if(RTS::IsWorker(Def)){
    ++Now.Workers;
    // Authority-only witness for the replay. The planner uses its visible set
    // and does not use this cargo observation to make decisions.
    Now.LoadedWorkers+=Victim->Cargo.X+Victim->Cargo.Y+Victim->Cargo.Z>.001;
    const double Distance=FVector2D::Distance(From,Victim->Pos);
    if(Now.NearestWorkerId==0||Distance<Now.NearestWorkerDistance){
     Now.NearestWorkerId=Victim->Id;Now.NearestWorkerDistance=Distance;
    }
   }
  }
  if(!Trace.Visibility.IsEmpty()&&Now.SameTargets(Trace.Visibility.Last()))return;
  if(Trace.Visibility.Num()<192)Trace.Visibility.Add(Now);else ++Trace.DroppedVisibility;
 }

 const FRTSEntity* RaidGuardNear(FVector2D Point) const {
  const FRTSEntity* Closest=nullptr;
  double Nearest=TNumericLimits<double>::Max();
  for(const auto* Guard:Enemies){
   const auto& Def=W.D(*Guard);
   if(Def.Attack<=0||Guard->Progress<1||RTS::IsWorker(Def))continue;
   const double Radius=W.EffectiveAttackRange(*Guard)+Def.Radius+(Def.bBuilding?2:8);
   const double Distance=FVector2D::DistSquared(Point,Guard->Pos);
   if(Distance<Nearest&&Distance<=FMath::Square(Radius)){Closest=Guard;Nearest=Distance;}
  }
  return Closest;
 }

 bool SendRaidCargoHome(FRTSEntity& Unit) {
  const auto* Drop=W.Find(W.Dropoff(Unit));
  if(!Drop)return false;
  // Clear the repeat source so the next trip can respond to current vision.
  if(Unit.RaidSource)Order(TEXT("stop"),{Unit.Id});
  Order(TEXT("deliver"),{Unit.Id},Drop->Pos,Drop->Id);
  return Unit.Order==TEXT("deliver");
 }

 FRTSEntity* RaidTarget(const FRTSEntity& Unit,bool bAllowDistant) const {
  FRTSEntity* Best=nullptr;
  FRTSEntity* Distant=nullptr;
  double Nearest=TNumericLimits<double>::Max();
  const float Sight=W.EffectiveSight(Unit);
  for(auto* Victim:Enemies){
   const auto& Def=W.D(*Victim);
   if(Victim->Progress<1||!IsRaidBuilding(Def.Id))continue;
   const double Distance=FVector2D::DistSquared(Unit.Pos,Victim->Pos);
   const bool bNearby=Distance<=FMath::Square(Sight+Def.Radius);
   if((bNearby&&Distance>=Nearest)||(!bNearby&&(!bAllowDistant||Distant))||RaidGuardNear(Victim->Pos))continue;
   if(bNearby){
    if(W.LOS(Unit.Pos,Victim->Pos,Unit.Id,Victim->Id)){Best=Victim;Nearest=Distance;}
   }else Distant=Victim;
  }
  return Best?Best:Distant;
 }

 bool ManageOpportunisticRaid(FRTSEntity& Unit,bool bRaidSlot) {
  if(Unit.Order==TEXT("deliver")||Unit.Order==TEXT("raid_return"))return true;
  const double Cargo=Unit.Cargo.X+Unit.Cargo.Y+Unit.Cargo.Z;
  if(Cargo>.001&&(Cargo>=15||Threat||W.Time-Unit.LastDamage<1.8f||!IsRaidOrder(Unit))&&SendRaidCargoHome(Unit))return true;
  if(!bRaidSlot||Threat)return false;
  if(IsRaidOrder(Unit))return true;
  if(const auto* Target=RaidTarget(Unit,true))Order(TEXT("raid"),{Unit.Id},Target->Pos,Target->Id);
  return IsRaidOrder(Unit);
 }

 bool ManageRaidExpedition(FRTSEntity& Unit) {
  if(Unit.Order==TEXT("deliver")||Unit.Order==TEXT("raid_return"))return true;
  const double Cargo=Unit.Cargo.X+Unit.Cargo.Y+Unit.Cargo.Z;
  const auto* Guard=RaidGuardNear(Unit.Pos);
  const FRTSEntity* Current=nullptr;
  if(Unit.Order==TEXT("raid"))for(const auto* Victim:Enemies)if(Victim->Id==Unit.Target){Current=Victim;break;}
  const bool bRecentDamage=W.Time-Unit.LastDamage<2.f;
  if(Cargo>.001&&(Cargo>=15||bRecentDamage||Guard||!Current||RaidGuardNear(Current->Pos))&&SendRaidCargoHome(Unit))return true;
  if(Guard||bRecentDamage){
   if(Threat&&FVector2D::DistSquared(Unit.Pos,State.Home)<=FMath::Square(BotBaseDefenseRadius))return false;
   if(Unit.Order==TEXT("move")&&W.Time<State.RaidScoutUntil&&
      (!Guard||FVector2D::DistSquared(Unit.Rally,Guard->Pos)>FVector2D::DistSquared(Unit.Pos,Guard->Pos)))return true;
   FVector2D Retreat=State.Home;
   if(Guard){
    const FVector2D Away=Unit.Pos+(Unit.Pos-Guard->Pos).GetSafeNormal()*14;
    if(IsLand(Away,W.Map,2))Retreat=Away;
   }
   State.RaidScoutUntil=W.Time+8;
   Order(TEXT("move"),{Unit.Id},Retreat);
   return true;
  }
  if(Current&&!RaidGuardNear(Current->Pos))return true;
  if(const auto* Target=RaidTarget(Unit,false)){
   Order(TEXT("raid"),{Unit.Id},Target->Pos,Target->Id);
   return IsRaidOrder(Unit);
  }
  if(Unit.Order==TEXT("move")&&W.Time<State.RaidScoutUntil)return true;
  const FVector2D Point=ScoutPoint(Unit.Pos,true);
  if(Point.Equals(Unit.Pos))return false;
  State.RaidScoutUntil=W.Time+24;
  Order(TEXT("move"),{Unit.Id},Point);
  return true;
 }

 void ManageArmy() {
  ObserveRaidVisibility();
  TArray<int32> Force;
  FVector2D Center=FVector2D::ZeroVector;
  auto* Scout=W.Find(State.ScoutId);
  if(!Scout||Scout->Owner!=P.Slot||W.D(*Scout).Id!=Spy){
   State.ScoutId=0;
   Scout=nullptr;
   for(auto* E:Army)if(W.D(*E).Id==Spy){Scout=E;State.ScoutId=E->Id;State.ScoutGoalUntil=0;break;}
  }
  int32 Raiders=0;
  const auto RaidPolicy=bGadianton?RTSBotRaidPolicy():ERTSBotRaidPolicy::Legacy;
  for(auto* E:Army){
   if(E==Scout&&!Threat)continue;
   const auto& Def=W.D(*E);
   if(RaidPolicy==ERTSBotRaidPolicy::Opportunistic&&Def.Id==BotBandType){
    if(ManageOpportunisticRaid(*E,Raiders++<2))continue;
   }else if(RaidPolicy==ERTSBotRaidPolicy::Expedition&&Def.Id==BotBandType){
    const bool bFirstBand=Raiders++==0;
    if((bFirstBand||E->Cargo.SizeSquared()>0)&&ManageRaidExpedition(*E))continue;
   }else if(!Threat&&bGadianton&&Def.Id==BotBandType&&Raiders<2){
    ++Raiders;
    if(!IsRaidOrder(*E)){
     for(auto* Victim:Enemies){
      const FString& Type=W.D(*Victim).Id;
      if(Victim->Progress<1||!IsRaidBuilding(Type))continue;
      Order(TEXT("raid"),{E->Id},Victim->Pos,Victim->Id);break;
     }
    }
    if(IsRaidOrder(*E))continue;
   }
   Force.Add(E->Id);
   Center+=E->Pos;
   if(bSupportEnabled&&E!=Scout&&E->Progress>=1&&E->DisarmedUntil<=W.Time)Escorts.Add(E);
  }
  if(Force.Num()>0)Center/=Force.Num();else Center=State.Home;
  for(const auto* E:Escorts)EscortCenter+=E->Pos;
  if(!Escorts.IsEmpty())EscortCenter/=Escorts.Num();
  if(Threat){
   State.ArmyTarget=Threat->Id;
   State.ArmyGoal=Threat->Pos;
   State.ArmyGoalUntil=W.Time+10;
   Attack(Force,*Threat);
   return;
  }

  if(Scout&&!ScoutObservedHideout(*Scout)&&(W.Time>=State.ScoutGoalUntil||FVector2D::DistSquared(Scout->Pos,State.ScoutGoal)<16||Scout->Order==TEXT("idle")||Scout->Order==TEXT("hold"))){
   State.ScoutGoal=ScoutPoint(Scout->Pos,true);
   State.ScoutGoalUntil=W.Time+38;
   if(!State.ScoutGoal.Equals(Scout->Pos))Order(TEXT("move"),{Scout->Id},State.ScoutGoal);
  }
  if(Force.IsEmpty())return;
  if(State.bCampaign&&Force.Num()<FMath::Max(3,Config.Wave/3)){
   State.bCampaign=false;
   State.NextOffense=W.Time+40;
   State.ArmyGoalUntil=0;
  }
  if(!State.bCampaign&&W.Time>=FMath::Max(Config.FirstAttack,State.NextOffense)&&Force.Num()>=Config.Wave){
   State.bCampaign=true;
   State.ArmyGoalUntil=0;
  }
  if(!State.bCampaign){
   TArray<int32> Return;
   for(int32 Id:Force)if(FVector2D::DistSquared(W.Find(Id)->Pos,State.Home)>FMath::Square(19.0))Return.Add(Id);
   if(!Return.IsEmpty()){
    const FVector2D Rally=State.Home+(FVector2D(80,80)-State.Home).GetSafeNormal()*8;
    TArray<int32> Stragglers;
    for(int32 Id:Return){const auto* E=W.Find(Id);if(E->Order!=TEXT("attack_move")||!E->Rally.Equals(Rally,8))Stragglers.Add(Id);}
    Order(TEXT("attack_move"),Stragglers,Rally);
   }
   return;
  }

  FRTSEntity* Target=nullptr;
  double BestScore=-TNumericLimits<double>::Max();
  for(auto* E:Enemies){
   const auto& Def=W.D(*E);
   const bool bEconomic=RTS::IsWorker(Def)||Def.Id==Farm||Def.Id==Store||Def.Id==House||Def.Id==Mine;
   if(P.Faction==TEXT("nephites")&&bEconomic)continue;
   const double Distance=FVector2D::Distance(Center,E->Pos);
   if(Distance>35&&Def.Id!=Seat)continue;
   const double Score=(Def.Id==Seat?45:Def.Attack>0?18:Def.bBuilding?10:0)-Distance*.8+(E->Id==State.ArmyTarget?6:0);
   if(Score>BestScore){BestScore=Score;Target=E;}
  }
  if(Target){
   State.ArmyTarget=Target->Id;
   State.ArmyGoal=Target->Pos;
   State.ArmyGoalUntil=W.Time+10;
   Attack(Force,*Target);
   return;
  }
  if(State.ArmyTarget||W.Time>=State.ArmyGoalUntil||FVector2D::DistSquared(Center,State.ArmyGoal)<36){
   State.ArmyTarget=0;
   State.ArmyGoal=Center;
   double Best=FMath::Square(200.0);
   for(const auto& Known:State.EnemyBuildings){
    const double Distance=FVector2D::DistSquared(Center,Known.Value);
    if(Distance>36&&Distance<Best){Best=Distance;State.ArmyGoal=Known.Value;}
   }
   if(State.ArmyGoal.Equals(Center))State.ArmyGoal=ScoutPoint(Center,true);
   State.ArmyGoalUntil=W.Time+65;
  }
  TArray<int32> Advance;
  for(int32 Id:Force){
   const auto* E=W.Find(Id);
   if((E->Order!=TEXT("attack_move")||!E->Rally.Equals(State.ArmyGoal,12))&&
    FVector2D::DistSquared(E->Pos,State.ArmyGoal)>36)Advance.Add(Id);
  }
  Order(TEXT("attack_move"),Advance,State.ArmyGoal);
 }

 bool SupportExposed(const FRTSEntity& Unit,FVector2D At) const {
  for(const auto* Enemy:Enemies){
   const auto& Def=W.D(*Enemy);
   if(Def.Attack<=0||Enemy->Progress<1||Enemy->DisarmedUntil>W.Time||
      (RTS::IsWorker(Def)&&!Enemy->bDirectAttack))continue;
   const double Distance=FVector2D::Distance(At,Enemy->Pos);
   const double Reach=W.EffectiveAttackRange(*Enemy)+W.D(Unit).Radius+(Def.bBuilding?0.:3.5);
   if(Distance>Reach||!W.LOS(Enemy->Pos,At,Enemy->Id,Unit.Id))continue;
   if(Def.bBuilding||Enemy->Target==Unit.Id)return true;
   const bool bScreened=Escorts.ContainsByPredicate([&](const FRTSEntity* Guard){
    return FVector2D::DistSquared(At,Guard->Pos)<=36&&FVector2D::Distance(Enemy->Pos,Guard->Pos)+1<Distance&&
      W.LOS(Enemy->Pos,Guard->Pos,Enemy->Id,Guard->Id);
   });
   if(!bScreened)return true;
  }
  return false;
 }

 bool SafeSupportPoint(const FRTSEntity& Unit,FVector2D At) const {
  return Fog.Cells[FogAt(At)]==2&&W.Walkable(At,W.D(Unit).Radius,Unit.Id)&&!SupportExposed(Unit,At);
 }

 bool HasOtherSupportOrders(const FRTSEntity& Unit) const {
  if(Unit.PendingOrders.IsEmpty())return false;
  return Unit.Order!=TEXT("move")||Unit.PendingOrders.Num()!=1||
   Unit.PendingOrders[0].Action!=TEXT("hold")||Unit.PendingOrders[0].Type!=SupportMoveHoldTag;
 }

 void MoveSupport(FRTSEntity& Unit,FVector2D At) {
  Order(TEXT("move"),{Unit.Id},At);
  if(Unit.Order==TEXT("move")){
   // The normal queue suppresses idle conversion between arrival and the next decision.
   W.CommandForOwner(P.Slot,nullptr,TEXT("hold"),{Unit.Id},At,0,SupportMoveHoldTag,true);
  }
 }

 void PositionSupport(FRTSEntity& Unit,FVector2D At) {
  if(Unit.Pos.Equals(At,1.1)){
   if(Unit.Order!=TEXT("hold"))Order(TEXT("hold"),{Unit.Id});
  }else if(Unit.Order!=TEXT("move")||!Unit.Rally.Equals(At,1.1)){
   MoveSupport(Unit,At);
  }
 }

 bool TrySupportConversion(FRTSEntity& Unit,TSet<int32>& Claimed) {
  if(SupportExposed(Unit,Unit.Pos))return false;
  const double Range=W.ConversionRange(Unit);
  const FRTSEntity* Best=nullptr;
  FVector2D Approach=Unit.Pos;
  double BestScore=TNumericLimits<double>::Max();
  for(const auto* Enemy:Enemies){
   const double Distance=FVector2D::Distance(Unit.Pos,Enemy->Pos);
   if(Distance>Range+2.5||Claimed.Contains(Enemy->Id)||!W.LOS(Unit.Pos,Enemy->Pos,Unit.Id,Enemy->Id)||
      !W.CanConvertWith(Unit,*Enemy))continue;
   const bool bApproach=Distance>Range;
   const FVector2D At=bApproach?Enemy->Pos+(Unit.Pos-Enemy->Pos).GetSafeNormal()*(Range-.65):Unit.Pos;
   if(bApproach){
    // Approach a fixed, nearby point. The conversion order itself pursues a moving target.
    bool bClear=true;
    const int32 Steps=FMath::CeilToInt(FVector2D::Distance(Unit.Pos,At)/.5);
    for(int32 Step=1;Step<=Steps&&bClear;++Step)bClear=SafeSupportPoint(Unit,FMath::Lerp(Unit.Pos,At,double(Step)/Steps));
    if(!bClear||!W.LOS(At,Enemy->Pos,Unit.Id,Enemy->Id))continue;
   }
   const double Score=Distance+(bApproach?3.:0.)+(RTS::IsWorker(W.D(*Enemy))?1.:0.);
   if(Score<BestScore){Best=Enemy;Approach=At;BestScore=Score;}
  }
  if(!Best)return false;
  if(Approach.Equals(Unit.Pos)){
   if(Unit.Order!=TEXT("convert")||Unit.Target!=Best->Id)Order(TEXT("convert"),{Unit.Id},Best->Pos,Best->Id);
   if(Unit.Order!=TEXT("convert")||Unit.Target!=Best->Id)return false;
  }else if(Unit.Order!=TEXT("move")||!Unit.Rally.Equals(Approach,.4)){
   MoveSupport(Unit,Approach);
  }
  Claimed.Add(Best->Id);
  return true;
 }

 bool SupportLaborers(FRTSEntity& Unit) {
  struct FAura {FVector2D Pos;double Radius;int32 Id;};
  TArray<FAura> OtherAuras;
  for(const auto* Source:Visible){
   if(!W.Allied(P.Slot,Source->Owner)||Source==&Unit)continue;
   const bool bOwnConcubine=Source->Owner==P.Slot&&W.D(*Source).Id==TEXT("drunken_concubine");
   if(bOwnConcubine&&Source->Id>Unit.Id)continue; // Stable priority when two auras overlap.
   const double Radius=W.LabanAuraRadius(*Source);
   if(Radius<=0)continue;
   const bool bMoving=bOwnConcubine&&Source->Order==TEXT("move");
   OtherAuras.Add({bMoving?Source->Rally:Source->Pos,bMoving?5.:Radius,Source->Id});
  }
  TArray<const FRTSEntity*> Busy;
  for(const auto* Worker:Workers){
   if(Worker->Order!=TEXT("gather")&&Worker->Order!=TEXT("build")&&Worker->Order!=TEXT("repair"))continue;
   if(!OtherAuras.ContainsByPredicate([&](const FAura& Aura){
    return FVector2D::DistSquared(Aura.Pos,Worker->Pos)<=FMath::Square(Aura.Radius)&&W.LOS(Aura.Pos,Worker->Pos,Aura.Id,Worker->Id);
   }))Busy.Add(Worker);
  }
  FVector2D Best=Unit.Pos;
  double BestScore=0;
  auto ScorePoint=[&](FVector2D At){
   int32 Covered=0;
   for(const auto* Worker:Busy)if(FVector2D::DistSquared(At,Worker->Pos)<=25&&W.LOS(At,Worker->Pos,Unit.Id,Worker->Id))++Covered;
   const double Score=Covered*6-FVector2D::Distance(Unit.Pos,At)*.4;
   if(Score>BestScore&&SafeSupportPoint(Unit,At)){Best=At;BestScore=Score;}
  };
  ScorePoint(Unit.Pos);
  if(Unit.Order==TEXT("move"))ScorePoint(Unit.Rally);
  for(const auto* Worker:Busy)ScorePoint(Worker->Pos);
  if(BestScore<=0)return false;
  PositionSupport(Unit,Best);
  return true;
 }

 void JoinEscortOrShelter(FRTSEntity& Unit) {
  // Follow troops' actual positions, using their destination only for facing.
  if(!Escorts.IsEmpty()){
   const FVector2D Front=Threat?Threat->Pos:State.bCampaign?State.ArmyGoal:FVector2D(80,80);
   const FVector2D Forward=(Front-EscortCenter).GetSafeNormal();
   const FVector2D Rear=EscortCenter-Forward*4;
   const FRTSEntity* Guard=Escorts[0];
   for(const auto* E:Escorts)if(FVector2D::DistSquared(E->Pos,Rear)<FVector2D::DistSquared(Guard->Pos,Rear))Guard=E;
   const FVector2D Behind=Guard->Pos-Forward*3;
   if(SafeSupportPoint(Unit,Behind)){PositionSupport(Unit,Behind);return;}
  }
  if(HQ){
   const FVector2D Forward=(FVector2D(80,80)-HQ->Pos).GetSafeNormal();
   const FVector2D Side(-Forward.Y,Forward.X);
   for(const FVector2D Direction:{-Forward,Side,-Side,Forward}){
    const FVector2D Shelter=HQ->Pos+Direction*(W.D(*HQ).Radius+2);
    if(SafeSupportPoint(Unit,Shelter)){PositionSupport(Unit,Shelter);return;}
   }
  }
  PositionSupport(Unit,Unit.Pos);
 }

 void ManageSupport() {
  if(Support.IsEmpty())return;
  TSet<int32> Claimed;
  for(const auto* E:Visible){
   if(W.Allied(P.Slot,E->Owner)&&E->Channel>0&&(E->Order==TEXT("convert")||W.D(*E).Id==Rameumptom))Claimed.Add(E->Target);
  }
  const FRTSEntity* Platform=nullptr;
  if(P.Faction==TEXT("lamanites")){
   if(const auto* Complete=Buildings.FindByPredicate([&](const FRTSEntity* E){return E->Progress>=1&&W.D(*E).Id==Rameumptom;}))Platform=*Complete;
  }
  FRTSEntity* Priest=nullptr;
  if(Platform){
   double Nearest=TNumericLimits<double>::Max();
   for(auto* E:Support){
    if(W.D(*E).Id!=TEXT("priestcrafter")||E->Progress<1||HasOtherSupportOrders(*E)||!E->Evolution.IsEmpty()||E->Channel>0)continue;
    if(E->Order==TEXT("occupy")&&E->Target==Platform->Id){Priest=E;break;}
    const double Distance=FVector2D::DistSquared(Platform->Pos,E->Pos);
    const FVector2D Entrance=Platform->Pos+(E->Pos-Platform->Pos).GetSafeNormal()*(W.D(*Platform).Radius+.4);
    if(Distance<Nearest&&SafeSupportPoint(*E,Entrance)){Priest=E;Nearest=Distance;}
   }
  }
  for(auto* Unit:Support){
   if(Unit->Progress<1||HasOtherSupportOrders(*Unit)||!Unit->Evolution.IsEmpty()||
      (Unit->Order==TEXT("convert")&&Unit->Channel>0))continue;
   if(Unit==Priest){
    if(Unit->Order!=TEXT("occupy")||Unit->Target!=Platform->Id)Order(TEXT("context"),{Unit->Id},Platform->Pos,Platform->Id);
    continue;
   }
   if(TrySupportConversion(*Unit,Claimed))continue;
   if(P.Faction==TEXT("laban")&&W.D(*Unit).Id==TEXT("drunken_concubine")&&SupportLaborers(*Unit))continue;
   JoinEscortOrShelter(*Unit);
  }
 }

 void Attack(const TArray<int32>& Force,const FRTSEntity& Target) {
  TArray<int32> OrderIds;
  for(int32 Id:Force)if(auto* E=W.Find(Id)){
   if(E->Order!=TEXT("attack")||E->Target!=Target.Id){
    OrderIds.Add(Id);
    if(State.GadiantonRaidTrace.bEnabled&&Threat==&Target&&W.D(*E).Id==BotBandType&&IsRaidOrder(*E)){
     ++State.GadiantonRaidTrace.DefenseInterruptions;
     State.GadiantonRaidTrace.LoadedDefenseInterruptions+=E->Cargo.X+E->Cargo.Y+E->Cargo.Z>.001;
    }
   }
  }
  Order(TEXT("attack"),OrderIds,Target.Pos,Target.Id);
 }
};

void ARTSWorld::UpdateBots() {
 if(Phase!=1||!HasAuthority())return;
 for(auto& P:Players){
  if(!P.bBot||!P.bConnected||P.bDefeated||Time<P.Bot.NextThink)continue;
  P.Bot.NextThink=Time+Difficulties[FMath::Clamp(P.BotDifficulty,0,2)].ThinkSeconds;
  FRTSBotPlanner(*this,P).Think();
 }
}
