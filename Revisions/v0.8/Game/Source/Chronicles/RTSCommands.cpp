#include "RTSWorld.h"
#include "RTSController.h"

static bool WallSegment(const FRTSDef& D){return D.bBuilding&&(D.Id.Contains(TEXT("wall"))||D.Id==TEXT("works_of_defense")||D.Id.Contains(TEXT("bulwark")));}
static bool RaiderUnit(const FRTSDef& D){return D.Id==TEXT("gadianton_robber")||D.Id==TEXT("secret_murderer")||D.Id==TEXT("corrupt_judge");}
static bool RaidBuilding(const FRTSDef& D){return D.Id==TEXT("seat_of_the_people")||D.Id==TEXT("fields_of_grain")||D.Id==TEXT("gathering_of_flocks")||D.Id==TEXT("hill_of_ore")||D.Id==TEXT("storehouse_of_provisions")||D.Id==TEXT("winery");}
static bool RaidVictim(const FRTSDef& D,const FRTSEntity& E){return E.Progress>=1&&!E.Carrier&&(RaidBuilding(D)||(RTS::IsWorker(D)&&E.Cargo.X+E.Cargo.Y+E.Cargo.Z>.001f));}

void ARTSWorld::Queue(FRTSEntity& E,const FString& Type,ARTSController* C) {
 if(Type==TEXT("age2")||Type==TEXT("age3")||Type==TEXT("age4")||Type.StartsWith(TEXT("age2_"))||Type.StartsWith(TEXT("age3_"))||Type.StartsWith(TEXT("age4_"))){QueueAgeChoice(E,Type,C);return;}
 if(!Players.IsValidIndex(E.Owner))return;
 auto& Cat=FRTSCatalog::Get();const auto& Def=Cat.Def(Type);auto& P=Players[E.Owner];auto& F=Cat.Faction(P.Faction);
 if(Def.Id.IsEmpty()||!D(E).bBuilding||E.Progress<1||E.Jobs.Num()>=8){Error(C,TEXT("Complete the producer first; each queue holds eight orders."));return;}
 bool GateUpgrade=Type==TEXT("gate_segment_upgrade")&&WallSegment(D(E))&&!E.bGate;
 bool Age=false,Emergency=Type==TEXT("curelom")&&D(E).Id==TEXT("seat_of_the_people")&&RTS::IsJaredite(P.Faction);
 bool BeastAtFlock=RTS::IsJaredite(P.Faction)&&D(E).Id==TEXT("gathering_of_flocks")&&(Type==TEXT("curelom")||Type==TEXT("cumelom")||Type==TEXT("elephant"));
 bool RaiderAtStore=Type==TEXT("gadianton_robber")&&P.Faction==TEXT("gadianton")&&D(E).Id==TEXT("storehouse_of_provisions");
 bool Alternate=Type==TEXT("chief_captain")&&D(E).Id==TEXT("temple_of_the_land")&&P.Faction==TEXT("lamanites");
 if((Def.Producer!=D(E).Id&&!Emergency&&!Alternate&&!BeastAtFlock&&!RaiderAtStore&&!GateUpgrade)||Def.Kind==TEXT("job")||Def.Kind==TEXT("ammonite")){Error(C,TEXT("This unit is acquired elsewhere."));return;}
 if((!Def.bUpgrade&&!F.Units.Contains(Type))||(Def.bUpgrade&&!Age&&!F.Upgrades.Contains(Type))){Error(C,TEXT("This civilization cannot recruit or research that."));return;}
 int RequiredAge=Cat.RequiredAge(Def,P.Faction);if(RTS::IsMilitary(Def))RequiredAge=FMath::Max(2,RequiredAge);
 if(P.Age<RequiredAge){Error(C,TEXT("Advance to the required age first."));return;}
 for(auto& Req:Def.Requires)if(!Has(E.Owner,Req)&&!OwnedStructure(E.Owner,Req)){Error(C,TEXT("A prerequisite is missing: ")+Cat.Def(Req).Name);return;}
 if(Age&&(P.Age!=(Type==TEXT("age2")?1:2)||(Type==TEXT("age3")&&!OwnedStructure(E.Owner,TEXT("house_of_curious_workmanship"))))){Error(C,TEXT("Mobilization requires Fortification Age and a workshop."));return;}
 if(Def.bHero&&!CanTrainHero(E.Owner,Type)){Error(C,TEXT("Choose this hero when advancing ages before recruiting them."));return;}
 if(Type==TEXT("sheep")){int Count=0;for(auto& KV:Entities)if(KV.Value->HomeBuilding==E.Id&&D(*KV.Value).Kind==TEXT("sheep"))Count++;for(auto& J:E.Jobs)if(J.Type==TEXT("sheep"))Count++;if(Count>=10){Error(C,TEXT("This flock already has ten sheep alive or queued."));return;}}
 if(Def.Kind==TEXT("beast")){bool HQ=D(E).Id==TEXT("seat_of_the_people");int Count=0;for(auto& KV:Entities){auto& Beast=*KV.Value;if(Beast.Owner==E.Owner&&D(Beast).Kind==TEXT("beast")&&(Beast.HomeBuilding==E.Id||(HQ&&Beast.HomeBuilding==0)))Count++;}for(auto& J:E.Jobs)if(Cat.Def(J.Type).Kind==TEXT("beast"))Count++;if(Count>=10){Error(C,TEXT("This producer already supports ten builder beasts alive or queued."));return;}if(Type==TEXT("elephant")){int Elephants=0;for(auto& KV:Entities){auto& Beast=*KV.Value;if(Beast.Owner==E.Owner&&D(Beast).Id==Type&&(Beast.HomeBuilding==E.Id||(HQ&&Beast.HomeBuilding==0)))Elephants++;}for(auto& J:E.Jobs)if(J.Type==Type)Elephants++;if(Elephants>=1){Error(C,TEXT("Each flock supports one elephant alive or queued."));return;}}}
 if(Def.bUpgrade&&!GateUpgrade&&Has(E.Owner,Type)){Error(C,TEXT("This research is already complete."));return;}
 for(auto& KV:Entities){auto& Other=*KV.Value;if(Other.Owner!=E.Owner)continue;if(Def.bHero&&D(Other).Id==Type){Error(C,TEXT("Only one living copy of each hero is permitted."));return;}for(auto& Job:Other.Jobs)if(Job.Type==Type&&((Def.bUpgrade&&!GateUpgrade)||Def.bHero||(GateUpgrade&&Other.Id==E.Id))){Error(C,TEXT("That hero or research is already queued."));return;}}
 if(Def.bHero&&P.HeroDeaths.Contains(Type)&&Time-P.HeroDeaths[Type]<90){Error(C,TEXT("The hero can be recruited 90 seconds after their death."));return;}
 if(Type==TEXT("stripling_warrior")||Type==TEXT("one_of_the_fifty")){int Count=0;for(auto& KV:Entities){auto& Other=*KV.Value;if(Other.Owner!=E.Owner)continue;if(D(Other).Id==Type)Count++;for(auto& Job:Other.Jobs)if(Job.Type==Type)Count++;}int Limit=Type==TEXT("stripling_warrior")?20:50;if(Count>=Limit){Error(C,FString::Printf(TEXT("Only %d of this unit may be alive or queued."),Limit));return;}}
 Population();if(!Def.bUpgrade&&P.Population+Def.Pop>FMath::Min(200,P.Capacity)){Error(C,TEXT("Population full. Build Tents and Habitations."));return;}
 FVector Cost=Cat.Price(Def,P.Faction);if(!Pay(E.Owner,Cost,Def.CementCost)){Error(C,TEXT("Not enough resources."));return;}
 FRTSJob J;J.Type=Type;J.Paid=Cost;J.CementPaid=Def.CementCost;J.Total=J.Remaining=Cat.Duration(Def,P.Faction);J.Pop=Def.bUpgrade?0:Def.Pop;E.Jobs.Add(J);Population();
}
int ARTSWorld::BeastTier(const FRTSEntity& E) const {return D(E).Id==TEXT("curelom")?1:D(E).Id==TEXT("cumelom")?2:D(E).Id==TEXT("elephant")?3:0;}
bool ARTSWorld::CanBuildSite(const FRTSEntity& B,const FRTSEntity& Site) const {
 if(!Players.IsValidIndex(B.Owner)||(!Allied(B.Owner,Site.Owner)||(B.Owner!=Site.Owner&&Site.Progress<1))||B.Carrier||B.HP<=0||!D(Site).bBuilding)return false;
 if(RTS::IsJaredite(Players[B.Owner].Faction)){int Tier=BeastTier(B);if(Tier<=0)return false;if(Site.Progress>=1)return true;return (Site.Tier==0||Tier==Site.Tier)&&D(Site).Age<=Tier+1&&(D(Site).CementCost<=0||Tier==3);}
 if(D(B).Kind==TEXT("worker")||D(B).Kind==TEXT("job"))return true;
 return D(Site).Id==TEXT("temple_of_the_land")&&Players[B.Owner].Faction==TEXT("lamanites")&&D(B).Kind==TEXT("priest")&&Has(B.Owner,TEXT("by_the_pride_of_their_hearts"));
}
void ARTSWorld::Build(FRTSEntity& B,const FString& Type,FVector2D Point,ARTSController* C) {
 if(!Players.IsValidIndex(B.Owner)||!FMath::IsFinite(Point.X)||!FMath::IsFinite(Point.Y)||B.Carrier)return;
 auto& Cat=FRTSCatalog::Get();auto& Def=Cat.Def(Type);auto& P=Players[B.Owner];bool Jared=RTS::IsJaredite(P.Faction);int Tier=BeastTier(B);
 const bool PriestBuilder=D(B).Kind==TEXT("priest")&&P.Faction==TEXT("lamanites")&&Type==TEXT("temple_of_the_land")&&Has(B.Owner,TEXT("by_the_pride_of_their_hearts"));
 if(!B.Evolution.IsEmpty()||!Def.bBuilding||!Cat.Faction(P.Faction).Buildings.Contains(Type)||(Jared?Tier==0:!((D(B).Kind==TEXT("worker"))||(D(B).Kind==TEXT("job"))||PriestBuilder))){Error(C,TEXT("Select an authorized builder for this structure."));return;}
 if(P.Faction==TEXT("gadianton")&&(Type==TEXT("fields_of_grain")||Type==TEXT("gathering_of_flocks"))){Error(C,TEXT("Conspiring Civilians obtain food from wild provisions or other settlements."));return;}
 if(Jared&&(Def.Age>Tier+1||(Def.CementCost>0&&Tier<3))){Error(C,TEXT("This structure needs a higher-tier builder beast."));return;}
 // The same group command creates one foundation; subsequent builders assist it.
 for(auto& KV:Entities){auto& Site=*KV.Value;if(Site.Owner==B.Owner&&D(Site).Id==Type&&Site.Progress<1&&CanBuildSite(B,Site)&&FVector2D::Distance(Site.Pos,Point)<.25f){ResetOrder(B,TEXT("build"));B.Target=Site.Id;B.Destination=Site.Pos;return;}}
 if(P.Age<FMath::Max(Cat.RequiredAge(Def,P.Faction),Type==TEXT("encampment_of_war")?2:1)){Error(C,TEXT("This structure needs a later age."));return;}for(auto& R:Def.Requires)if(!Has(B.Owner,R)&&!OwnedStructure(B.Owner,R)){Error(C,TEXT("Missing prerequisite: ")+Cat.Def(R).Name);return;}
 if(P.Faction==TEXT("laban")&&Type==TEXT("temple_of_the_land")){for(auto& KV:Entities)if(KV.Value->Owner==B.Owner&&D(*KV.Value).Id==Type){Error(C,TEXT("Only one Great and Spacious Building may stand or be under construction."));return;}}
 if(Type==TEXT("gathering_of_flocks")){int Count=0;for(auto& KV:Entities)if(KV.Value->Owner==B.Owner&&D(*KV.Value).Id==Type)Count++;if(Count>=(Jared&&Has(B.Owner,TEXT("enlarge_the_flocks"))?7:5)){Error(C,TEXT("The maximum number of flocks has been reached."));return;}}
 if(Type==TEXT("works_of_defense")&&!Has(B.Owner,TEXT("heaps_of_earth"))){Error(C,TEXT("Research Heaps of Earth to build defenses."));return;}
 int Deposit=0;if(Type==TEXT("hill_of_ore")){for(auto& KV:Entities)if(D(*KV.Value).Id==TEXT("ore")&&FVector2D::Distance(KV.Value->Pos,Point)<3&&KV.Value->ResourceRemaining>0){Deposit=KV.Key;Point=KV.Value->Pos;break;}if(!Deposit){Error(C,TEXT("Place the Hill of Ore over a visible ore deposit."));return;}}
 bool Dock=Type==TEXT("place_of_many_waters");if(Dock&&(Map!=2||!(FMath::Abs(Point.X-71)<3||FMath::Abs(Point.X-89)<3)||Point.Y<10||Point.Y>150)){Error(C,TEXT("Place your dock along a shoreline."));return;}
 if(!Walkable(Point,Def.Radius,Deposit,Dock)){if(!Dock||!Walkable(Point,Def.Radius,Deposit,false)){Error(C,TEXT("Cannot build here. Leave space between structures."));return;}}
 if(!Pay(B.Owner,Def.Cost,Def.CementCost)){Error(C,TEXT("Not enough resources for this structure."));return;}
 auto E=Spawn(Type,B.Owner,Point,Tier);E->Progress=.001f;E->MaxHP=MaxHP(*E);E->HP=E->MaxHP;E->Builder=B.Id;E->LastProgress=Time;E->WorkTarget=Deposit;ResetOrder(B,TEXT("build"));B.Target=E->Id;B.Destination=Point;Population();
}
void ARTSWorld::Command(ARTSController* C,const FString& Action,const TArray<int32>& Ids,FVector2D Point,int32 Target,const FString& Type,bool bQueued) {
 if(!C||!Players.IsValidIndex(C->ServerSlot))return;int O=C->ServerSlot;
 if(Action==TEXT("lobby")){if(O==0&&Phase==2){Phase=0;Entities.Empty();for(auto& P:Players){P.bReady=false;P.bDefeated=false;}Snapshot();}return;}
 if(Action==TEXT("map")&&O==0&&Phase==0){Map=FMath::Clamp(Target,0,2);Snapshot();return;}
 if(Phase!=1||Players[O].bDefeated||!FMath::IsFinite(Point.X)||!FMath::IsFinite(Point.Y))return;Point.X=FMath::Clamp(Point.X,0.,160.);Point.Y=FMath::Clamp(Point.Y,0.,160.);
 static const TSet<FString> PublicActions={TEXT("context"),TEXT("move"),TEXT("attack"),TEXT("attack_move"),TEXT("patrol"),TEXT("stop"),TEXT("hold"),TEXT("gather"),TEXT("build"),TEXT("wall_line"),TEXT("train"),TEXT("research"),TEXT("age"),TEXT("cancel"),TEXT("convert"),TEXT("raid"),TEXT("raid_move"),TEXT("storehouse_raid"),TEXT("galavant"),TEXT("market_galavant"),TEXT("deliver"),TEXT("ability"),TEXT("evolve"),TEXT("rally"),TEXT("rally_economic"),TEXT("resign"),TEXT("delete")};
 if(!PublicActions.Contains(Action))return;
 if(Action==TEXT("resign")){Players[O].bDefeated=true;TArray<int32> Dead;for(auto& KV:Entities)if(KV.Value->Owner==O)Dead.Add(KV.Key);for(auto Id:Dead)Kill(Id);return;}
 if(Action==TEXT("wall_line")){
  TArray<FString> Parts;Type.ParseIntoArray(Parts,TEXT("|"),false);if(Parts.Num()!=3||!Parts[1].IsNumeric()||!Parts[2].IsNumeric())return;
  auto& Wall=FRTSCatalog::Get().Def(Parts[0]);if(!Wall.bBuilding||(!Parts[0].Contains(TEXT("wall"))&&Parts[0]!=TEXT("works_of_defense")&&!Parts[0].Contains(TEXT("bulwark"))))return;
  FVector2D Start(FCString::Atod(*Parts[1]),FCString::Atod(*Parts[2]));if(!FMath::IsFinite(Start.X)||!FMath::IsFinite(Start.Y)||Start.X<0||Start.Y<0||Start.X>160||Start.Y>160)return;
  FVector2D Line=Point-Start;float Length=Line.Size(),Spacing=2*Wall.Radius+.15f;int Count=FMath::FloorToInt(Length/Spacing+.0001f)+1;if(Count>128)return;Line=Line.GetSafeNormal();int Issued=0;
  for(int I=0;I<Count;I++){FVector2D At=Start+Line*(I*Spacing);bool Complete=false;for(auto& KV:Entities)if(KV.Value->Owner==O&&D(*KV.Value).Id==Parts[0]&&KV.Value->Progress>=1&&KV.Value->Pos.Equals(At,.2f)){Complete=true;break;}if(Complete)continue;Command(C,TEXT("build"),Ids,At,0,Parts[0],bQueued||Issued>0);Issued++;}return;
 }
 auto T=Find(Target);if(T&&!CanSee(O,*T))return;TArray<int32> Unique;for(int Id:Ids)Unique.AddUnique(Id);int Formation=0;
 for(int Id:Unique){auto E=Find(Id);if(!E)continue;if(E->Owner!=O){if(Action==TEXT("delete")&&E->DisguisedOwner==O)Kill(E->Id);continue;}if(E->Carrier)continue;auto& Def=D(*E);int ExistingRaidSource=E->RaidSource;
  if(Action==TEXT("delete")){Kill(E->Id);continue;}
  if(Action==TEXT("attack_move")&&Def.bBuilding)continue;
  if(Action==TEXT("market_galavant")){if(Def.Id!=TEXT("place_of_trade")||E->Progress<1)continue;if(!bQueued)E->GalavantRoute.Empty();if(E->GalavantRoute.Num()<32)E->GalavantRoute.Add(Point);continue;}
  if(Action==TEXT("storehouse_raid")){if(Def.Id!=TEXT("storehouse_of_provisions")||E->Progress<1||Players[O].Faction!=TEXT("gadianton"))continue;if(T&&(T->Owner<0||Allied(O,T->Owner)||!RaidVictim(D(*T),*T)))continue;E->bRaidRallySet=true;E->RaidRallyPoint=Point;E->RaidRallyTarget=T?T->Id:0;continue;}
  if(Action==TEXT("rally_economic")){SetEconomicRally(*E,Point,Target);continue;}
  if(Action==TEXT("train")||Action==TEXT("research")||Action==TEXT("age")){int Count=Action==TEXT("train")&&bQueued?5:1;for(int I=0;I<Count;I++){int Before=E->Jobs.Num();Queue(*E,Type,C);if(E->Jobs.Num()==Before)break;}continue;}
  if(Action==TEXT("galavant")){if(Def.Kind!=TEXT("trader"))continue;if(!bQueued){E->TradeRoute.Empty();E->TradeRouteIndex=0;E->PendingOrders.Empty();ResetOrder(*E,TEXT("galavant"));}if(E->TradeRoute.Num()<32)E->TradeRoute.Add(Point);E->GalavantRoute=E->TradeRoute;E->Order=TEXT("galavant");continue;}
  if(!Def.bBuilding&&Action!=TEXT("cancel")&&Action!=TEXT("rally")){
   if(Action==TEXT("build")&&bQueued&&((E->Order!=TEXT("idle")&&E->Order!=TEXT("hold"))||!E->PendingOrders.IsEmpty())){
    if(E->PendingOrders.Num()>=128)continue;FRTSEntity Previous=*E;Build(*E,Type,Point,C);int SiteId=E->Target;bool Placed=E->Order==TEXT("build");*E=MoveTemp(Previous);auto Site=Find(SiteId);
    if(Placed&&Site&&Site->Owner==O&&D(*Site).Id==Type&&Site->Progress<1&&Site->Pos.Equals(Point,.25f)){bool Duplicate=E->Order==TEXT("build")&&E->Target==SiteId;for(auto& Q:E->PendingOrders)if(Q.Target==SiteId)Duplicate=true;if(!Duplicate){FRTSQueuedOrder Q;Q.Action=TEXT("context");Q.Point=Site->Pos;Q.Target=SiteId;E->PendingOrders.Add(Q);}}continue;
   }
   if(bQueued&&((E->Order!=TEXT("idle")&&E->Order!=TEXT("hold"))||!E->PendingOrders.IsEmpty())){bool Duplicate=false;if(Action==TEXT("build")){for(auto& Q:E->PendingOrders)if(Q.Action==Action&&Q.Type==Type&&Q.Point.Equals(Point,.2f))Duplicate=true;auto Current=Find(E->Target);if(E->Order==TEXT("build")&&Current&&D(*Current).Id==Type&&Current->Pos.Equals(Point,.2f))Duplicate=true;}if(!Duplicate&&E->PendingOrders.Num()<128){FRTSQueuedOrder Q;Q.Action=Action;Q.Type=Type;Q.Point=Point;Q.Target=Target;E->PendingOrders.Add(Q);}continue;}
   if(!bQueued&&!bDispatchingQueuedOrder)E->PendingOrders.Empty();
   E->bRaidMove=false;E->RaidSource=0;E->TradeRoute.Empty();E->GalavantRoute.Empty();
  }
  if(Action==TEXT("cancel")){
   if(!E->Evolution.IsEmpty()){Refund(O,FRTSCatalog::Get().Def(E->Evolution).Cost,1);E->Evolution.Empty();E->EvoRemaining=0;E->Order=TEXT("idle");}
   else if(Def.bBuilding&&E->Progress<1){Refund(O,Def.Cost,FMath::Clamp(1-E->Progress,0.f,1.f),Def.CementCost);Kill(E->Id);}
   else if(E->Jobs.Num()){int Index=Type==TEXT("last")?E->Jobs.Num()-1:0;if(Type.StartsWith(TEXT("queue:"))){FString S=Type.Mid(6);if(!S.IsNumeric())continue;Index=FCString::Atoi(*S);}if(!E->Jobs.IsValidIndex(Index))continue;Refund(O,E->Jobs[Index].Paid,1,E->Jobs[Index].CementPaid);E->Jobs.RemoveAt(Index);}Population();continue;
  }
  if(!E->Evolution.IsEmpty()){Error(C,TEXT("This beast is evolving. Cancel to interrupt."));continue;}
  if(Action==TEXT("build")){Build(*E,Type,Point,C);continue;}
  if(Action==TEXT("rally")&&Def.bBuilding){SetRally(*E,Point,Target);continue;}
  if(Action==TEXT("evolve")){Error(C,TEXT("Builder beasts are recruited separately in their respective ages."));continue;}
  if(Action==TEXT("ability")){Ability(*E,Point,Target,C);continue;}
  FString Order=Action;
  if(Action==TEXT("context")){
   Order=TEXT("move");if(T){if(Allied(O,T->Owner)){
    bool Builder=CanBuildSite(*E,*T);
    if(D(*T).bBuilding&&Builder&&((T->Owner==O&&T->Progress<1)||(T->Progress>=1&&T->HP<T->MaxHP)))Order=T->Progress<1?TEXT("build"):TEXT("repair");
    else if(CanUseDropoff(*E,*T)&&T->Progress>=1&&CarryCapacity(*E)>0&&E->Cargo.SizeSquared()>.001f)Order=TEXT("deliver");
    else if(T->Owner==O&&Players[O].Faction==TEXT("gadianton")&&D(*T).Id==TEXT("tents_and_habitations")&&!Def.bBuilding&&!RTS::IsCarrier(Def)&&Def.Kind!=TEXT("beast")&&Def.Kind!=TEXT("sheep")&&T->Progress>=1)Order=TEXT("board");
    else if(RTS::IsWorker(Def)&&(D(*T).Id==TEXT("fields_of_grain")||D(*T).Id==TEXT("gathering_of_flocks")||D(*T).Id==TEXT("hill_of_ore")))Order=TEXT("gather");
    else if((D(*T).Kind==TEXT("ship")&&!Def.bBuilding&&Def.Kind!=TEXT("ship")&&Def.Kind!=TEXT("beast")&&Def.Kind!=TEXT("chariot"))||((D(*T).Kind==TEXT("chariot")||D(*T).Id==TEXT("elephant"))&&!Def.bBuilding&&Def.Kind!=TEXT("beast")&&Def.Kind!=TEXT("ship")&&Def.Kind!=TEXT("chariot")))Order=TEXT("board");
    else if(Def.Kind==TEXT("trader")&&RTS::IsWorker(D(*T)))Order=TEXT("trade");
    else if(Def.Id==TEXT("priestcrafter")&&Players[O].Faction==TEXT("lamanites")&&T->Owner==O&&T->Progress>=1&&D(*T).Id==TEXT("temple_of_the_land"))Order=TEXT("occupy");
    else if(E->Cargo.X+E->Cargo.Y+E->Cargo.Z>0&&D(*T).bBuilding)Order=TEXT("deliver");
   }else if(T->Owner<0){if(RTS::IsWorker(Def)||Def.Id==TEXT("gadianton_robber")||Def.Id==TEXT("secret_murderer"))Order=TEXT("gather");}
   else if(CanAssignWork(*E,*T))Order=TEXT("gather");
   else if(CanUseDropoff(*E,*T)&&RTS::IsWorker(Def))Order=TEXT("deliver");
   else if(CanConvertWith(*E,*T))Order=TEXT("convert");
   else if(Def.Kind==TEXT("lawyer")&&(D(*T).Id==TEXT("seat_of_the_people")||D(*T).Id==TEXT("storehouse_of_provisions")))Order=TEXT("compromise");
   else if(RaiderUnit(Def)&&RaidVictim(D(*T),*T))Order=TEXT("raid");else Order=TEXT("attack");}}
  if(Def.bBuilding){if(Order==TEXT("move")||Action==TEXT("context")){if(Def.Id==TEXT("seat_of_the_people"))SetEconomicRally(*E,Point,Target);else SetRally(*E,Point,Target);}continue;}
  if(Order==TEXT("stop")||Order==TEXT("hold")){ResetOrder(*E,Order==TEXT("hold")?TEXT("hold"):TEXT("idle"));continue;}
  if(Order==TEXT("attack")&&(!T||Allied(O,T->Owner)||T->Owner<0))continue;
  if((Order==TEXT("attack")||Order==TEXT("attack_move"))&&Def.Attack<=0){Error(C,TEXT("This unit serves without attacking."));continue;}
  if(Order==TEXT("convert")&&(!T||!CanConvertWith(*E,*T))){Error(C,TEXT("Select a religious unit and an eligible enemy unit."));continue;}
  if((Order==TEXT("raid")||Order==TEXT("raid_move"))&&!RaiderUnit(Def)){Error(C,TEXT("Select a raider."));continue;}
  if(Order==TEXT("raid")&&(!T||Allied(O,T->Owner)||T->Owner<0||!RaidVictim(D(*T),*T)))continue;
  if(Order==TEXT("gather")&&(!T||!CanAssignWork(*E,*T))){Error(C,TEXT("Choose an available resource or work slot."));continue;}
  if(Order==TEXT("deliver")&&(!T||!CanUseDropoff(*E,*T)))continue;
  if(Order==TEXT("build")&&T){if(!CanBuildSite(*E,*T))continue;T->Builder=E->Id;}
  if(Order==TEXT("trade")&&(!T||Def.Kind!=TEXT("trader")||!Allied(O,T->Owner)||!RTS::IsWorker(D(*T))))continue;
  ResetOrder(*E,Order);E->Target=T?T->Id:0;E->bDirectAttack=Order==TEXT("attack");E->PatrolStart=E->Pos;
  if(Order==TEXT("convert")&&Def.Kind==TEXT("lawyer"))E->RevealedUntil=FMath::Max(E->RevealedUntil,Time+1);
  int Columns=FMath::Max(1,FMath::CeilToInt(FMath::Sqrt(float(Unique.Num())))),Rows=FMath::DivideAndRoundUp(Unique.Num(),Columns);float Spacing=FMath::Max(1.2f,Def.Radius*2+.3f);
  FVector2D Offset=Unique.Num()>1?FVector2D((Formation%Columns-(Columns-1)*.5f)*Spacing,(Formation/Columns-(Rows-1)*.5f)*Spacing):FVector2D::ZeroVector;Formation++;
  E->Destination=Point+Offset;E->Destination.X=FMath::Clamp(E->Destination.X,double(Def.Radius+.1f),double(RTS::MapSize-Def.Radius-.1f));E->Destination.Y=FMath::Clamp(E->Destination.Y,double(Def.Radius+.1f),double(RTS::MapSize-Def.Radius-.1f));
  if(Order==TEXT("move")||Order==TEXT("attack_move")||Order==TEXT("patrol")){bool Ship=Def.Kind==TEXT("ship");if(!Walkable(E->Destination,Def.Radius,E->Id,Ship)){FVector2D Center=E->Destination;bool Found=false;for(int Ring=1;Ring<=12&&!Found;Ring++)for(int I=0;I<16;I++){float A=(I+E->Id%16)*PI/8;FVector2D At=Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*(Ring*.75f);if(Walkable(At,Def.Radius,E->Id,Ship)){E->Destination=At;Found=true;break;}}}}
  E->Rally=E->Destination;
  if(Order==TEXT("gather"))E->WorkTarget=Target;
  if(Order==TEXT("deliver")){E->DropTarget=T->Id;E->TradeB=T->Id;E->WorkTarget=0;if(ExistingRaidSource>0&&!RTS::IsWorker(Def))E->RaidSource=ExistingRaidSource;}
  if(Order==TEXT("raid")){E->RaidSource=T?T->Id:0;}
  if(Order==TEXT("raid_move")){E->bRaidMove=true;E->RaidDestination=Point;}
 }
}
