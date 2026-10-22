#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

bool RTSRunV08EconomyTests(ARTSWorld& World);
bool RunRTSV08ReligionTests(ARTSWorld* World);
bool RTSRunV08PresentationTests(ARTSWorld& World);
bool RTSRunV07SelectionTests(ARTSWorld& World);
bool RunRTSV07ReligionTests(ARTSWorld* World);
bool RTSRunV07EconomyTests(ARTSWorld& World);
bool RunRTSV07CoreTests(ARTSWorld* World);
bool RunRTSV05CombatTests(ARTSWorld* World);
bool RTSRunEconomyV05Tests(ARTSWorld& World);
bool RunRTSV04CoreTests(ARTSWorld* World);
bool RunRTSEconomyV04Tests(ARTSWorld* World);
bool RunRTSEconomyV03Tests(ARTSWorld* World);
bool RunRTSReligionTests(ARTSWorld* World);
bool RunRTSRallyTests(ARTSWorld* World);
bool RunRTSCustomizationTests();
bool RunRTSVisualTests();

ARTSWorld::ARTSWorld() { PrimaryActorTick.bCanEverTick=true; bReplicates=false; }
void ARTSWorld::BeginPlay() { Super::BeginPlay();FRTSCatalog::Get();bTesting=FParse::Param(FCommandLine::Get(),TEXT("RTSSelfTest")); if(FParse::Param(FCommandLine::Get(),TEXT("RTSV08EconomyTest"))){bTesting=true;const bool Ok=RTSRunV08EconomyTests(*this);FPlatformMisc::RequestExitWithStatus(!Ok,Ok?0:1);return;}if(FParse::Param(FCommandLine::Get(),TEXT("RTSV08ReligionTest"))){bTesting=true;const bool Ok=RunRTSV08ReligionTests(this);FPlatformMisc::RequestExitWithStatus(!Ok,Ok?0:1);return;}if(bTesting) {const bool Base=RunSelfTests();const bool Scenarios=RunScenarioTests();const bool Custom=RunRTSCustomizationTests();const bool Visual=RunRTSVisualTests();const bool Rally=RunRTSRallyTests(this);const bool Economy=RunRTSEconomyV03Tests(this);const bool ReligionOK=RunRTSReligionTests(this);const bool CoreV04=RunRTSV04CoreTests(this);const bool EconomyV04=RunRTSEconomyV04Tests(this);const bool CombatV05=RunRTSV05CombatTests(this);const bool EconomyV05=RTSRunEconomyV05Tests(*this);const bool ReligionV07=RunRTSV07ReligionTests(this);const bool EconomyV07=RTSRunV07EconomyTests(*this);const bool CoreV07=RunRTSV07CoreTests(this);const bool SelectionV07=RTSRunV07SelectionTests(*this);const bool EconomyV08=RTSRunV08EconomyTests(*this);const bool ReligionV08=RunRTSV08ReligionTests(this);const bool PresentationV08=RTSRunV08PresentationTests(*this);const bool Passed=EconomyV08&&ReligionV08&&PresentationV08&&SelectionV07&&ReligionV07&&EconomyV07&&CoreV07&&CombatV05&&EconomyV05&&Base&&Scenarios&&Custom&&Visual&&Rally&&Economy&&ReligionOK&&CoreV04&&EconomyV04;FPlatformMisc::RequestExitWithStatus(!Passed,Passed?0:1);return;}if(FParse::Param(FCommandLine::Get(),TEXT("RTSStressTest"))){bTesting=true;RunStressTests();FPlatformMisc::RequestExit(false);} }
const FRTSDef& ARTSWorld::D(const FRTSEntity& E) const {return FRTSCatalog::Get().Def(E.Def);}
FRTSEntity* ARTSWorld::Find(int32 Id) { auto P=Entities.Find(Id);return P?P->Get():nullptr; }
bool ARTSWorld::Allied(int32 A,int32 B) const {return Players.IsValidIndex(A)&&Players.IsValidIndex(B)&&Players[A].Team==Players[B].Team;}
bool ARTSWorld::Has(int32 O,const FString& U) const {return Players.IsValidIndex(O)&&Players[O].Upgrades.Contains(U);}
bool ARTSWorld::OwnedStructure(int32 O,const FString& Type) const {for(auto& KV:Entities) if(KV.Value->Owner==O&&KV.Value->Progress>=1&&D(*KV.Value).Id==Type) return true;return false;}
void ARTSWorld::Error(ARTSController* C,const FString& M) {if(C) C->ClientNotice(M);}
void ARTSWorld::Join(ARTSController* C) {
 int Slot=Players.Num();for(int I=0;I<Players.Num();I++)if(!Players[I].bConnected){Slot=I;break;}
 if(Slot>=4 || Phase!=0) { C->ClientNotice(TEXT("This match is full or already underway. Return to menu to join the next match.")); return; }
 FRTSPlayer P;P.Slot=Slot;P.Name=FString::Printf(TEXT("Player %d"),P.Slot+1);P.Team=P.Slot%2;P.Controller=C;P.Explored.Init(0,RTS::FogSize*RTS::FogSize); C->ServerSlot=P.Slot;if(Players.IsValidIndex(Slot))Players[Slot]=P;else Players.Add(P);Snapshot();
 UE_LOG(LogTemp,Display,TEXT("RTS_JOIN slot=%d"),C->ServerSlot);
}
void ARTSWorld::Leave(ARTSController* C) {if(!Players.IsValidIndex(C->ServerSlot)) return;auto& P=Players[C->ServerSlot];P.bConnected=false;P.Controller=nullptr;P.DisconnectedAt=Time;if(Phase==0) {P.bDefeated=true;P.bReady=false;} }
void ARTSWorld::Lobby(ARTSController* C,const FString& Name,const FString& Faction,int32 Team,bool Ready) {
 if(Phase!=0||!Players.IsValidIndex(C->ServerSlot)) return;auto& P=Players[C->ServerSlot]; if(!FRTSCatalog::Get().Faction(Faction).Id.IsEmpty()) P.Faction=Faction;P.Team=FMath::Clamp(Team,0,1);P.Name=Name.IsEmpty()?FString::Printf(TEXT("Player %d"),P.Slot+1):Name;P.bReady=Ready;Snapshot();
}
FRTSEntity* ARTSWorld::Spawn(const FString& Type,int32 OwnerSlot,FVector2D Pos,int32 Tier) {
 auto& Cat=FRTSCatalog::Get();int32 Index=Cat.Index(Type);if(Index==INDEX_NONE)return nullptr;
 auto E=MakeShared<FRTSEntity>();E->Id=NextId++;E->Owner=OwnerSlot;E->Def=Index;E->Pos=Pos;E->Destination=Pos;E->Rally=Pos+FVector2D(5,5);E->Tier=Tier;E->BaseMax=Cat.Def(Index).HP;E->MaxHP=MaxHP(*E);E->HP=E->MaxHP;E->ResourceRemaining=E->HP;if(OwnerSlot<0&&(Type==TEXT("tree")||Type==TEXT("ore")||Type==TEXT("berries")||Type==TEXT("wild_game"))){const float Supply=Fruitfulness==0?.5f:Fruitfulness==2?2.f:1.f;E->HP*=Supply;E->MaxHP*=Supply;E->ResourceRemaining=E->HP;}
 if(Players.IsValidIndex(OwnerSlot)&&Players[OwnerSlot].Faction==TEXT("gadianton")&&(Type==TEXT("seat_of_the_people")||Type==TEXT("encampment_of_war")))E->LastDamage=Time;
 Entities.Add(E->Id,E);ObstacleCount=-1;return &E.Get();
}
void ARTSWorld::Start(ARTSController* C,int32 InMap) {
 if(C&&C->ServerSlot!=0) return; if(Phase!=0) return;
 int Count=0;TSet<int32> Teams;for(auto& P:Players) if(P.bConnected) {Count++;Teams.Add(P.Team);if(!P.bReady) {Error(C,TEXT("Every connected player must be ready."));return;} }
 bSandbox=Count==1; if(!bSandbox&&Teams.Num()<2) {Error(C,TEXT("Assign players to both teams before starting."));return;}
 if(Count==0)return;Map=FMath::Clamp(InMap,0,2);Phase=1;Time=0;Winner=-1;Entities.Empty();TraderVisitAt.Empty();NextId=1;
 const FVector2D Starts[]={FVector2D(24,24),FVector2D(136,136),FVector2D(24,136),FVector2D(136,24)};
 for(auto& P:Players) { const FVector Reserves[]={FVector(150,150,100),FVector(300,300,200),FVector(1000,1000,750),FVector(5000,5000,5000)};P.Resources=Reserves[FMath::Clamp(StartingResources,0,3)];P.Cement=StartingResources==3?1000:StartingResources==2?200:0;P.Age=1;P.Favor=20;P.HeroChoices.Empty();P.KnownBuildings.Empty();P.Upgrades.Empty();P.DefeatRemaining=60;P.bDefeated=!P.bConnected;P.bLiahona=true;P.GatheredFood=0;P.FoodAwards.Empty();P.ConversionAwards.Empty();P.DefenseAwards.Empty();P.HeroDeaths.Empty();P.LastProfanity=-100;P.DisconnectedAt=-1;P.Explored.Init(0,4096);if(!P.bConnected)continue;FVector2D Start=Starts[P.Slot];
  auto StartingSeat=Spawn(TEXT("seat_of_the_people"),P.Slot,Start,RTS::IsJaredite(P.Faction)?1:0);
  for(int I=0;I<6;I++) Spawn(TEXT("industrious_laborer"),P.Slot,Start+FVector2D(-4+I*1.5f,5));
  if(RTS::IsJaredite(P.Faction))if(auto Beast=Spawn(TEXT("curelom"),P.Slot,Start+FVector2D(0,-5)))Beast->HomeBuilding=StartingSeat->Id;
 }
 // Identical finite supplies at every starting quadrant keep all openings viable.
 for(auto Start:Starts) {FVector2D Toward=FVector2D(80,80)-Start;Toward.Normalize();FVector2D Side(-Toward.Y,Toward.X);Spawn(TEXT("berries"),-1,Start+Toward*9-Side*4);Spawn(TEXT("berries"),-1,Start+Toward*9-Side*7);Spawn(TEXT("ore"),-1,Start+Toward*10+Side*7);for(int I=0;I<2;++I)Spawn(TEXT("wild_game"),-1,Start+Toward*15+Side*(5+I*3)); for(int I=0;I<9;I++)Spawn(TEXT("tree"),-1,Start-Toward*(10+I/3*3)+Side*((I%3-1.f)*3));}
 for(int I=0;I<8;I++) {FVector2D Pos(55+(I%2)*50,35+(I/2)*30);if(!RTS::WaterAt(Pos,Map)&&!RTS::RockAt(Pos,Map)) Spawn(TEXT("ore"),-1,Pos);}
 // Thinner mirrored groves leave wider lanes for large animals.
 for(int X=12;X<160;X+=12)for(int Y=12;Y<160;Y+=12){FVector2D Grove(X,Y);bool NearStart=false;for(auto Start:Starts)if(FVector2D::Distance(Grove,Start)<21)NearStart=true;if(NearStart||RTS::WaterAt(Grove,Map)||RTS::RockAt(Grove,Map))continue;
  for(int I=0;I<2;++I){FVector2D At=Grove+FVector2D((I-.5f)*3.f,I%2*3.f);if(Walkable(At,1.1f))Spawn(TEXT("tree"),-1,At);}
 }
 Population();Snapshot();UE_LOG(LogTemp,Display,TEXT("RTS_MATCH_START map=%d players=%d"),Map,Count);
}
void ARTSWorld::Tick(float Delta) {Super::Tick(Delta); if(Phase==1) {Accumulator+=FMath::Min(Delta,.5f)*GameSpeed;int N=0;while(Accumulator>=RTS::Step&&N++<10){Step(RTS::Step);Accumulator-=RTS::Step;}}SnapshotTime+=Delta;if(SnapshotTime>=.25f){SnapshotTime=0;Snapshot();}}
bool ARTSWorld::Pay(int O,FVector Cost,float Cement) {if(!Players.IsValidIndex(O))return false;auto& P=Players[O];auto& R=P.Resources;if(R.X+1e-5<Cost.X||R.Y+1e-5<Cost.Y||R.Z+1e-5<Cost.Z||P.Cement+1e-5<Cement)return false;R-=Cost;P.Cement-=Cement;return true;}
void ARTSWorld::Refund(int O,FVector Cost,float F,float Cement) {if(Players.IsValidIndex(O)){F=FMath::Clamp(F,0.f,1.f);Players[O].Resources+=Cost*F;Players[O].Cement+=Cement*F;}}
void ARTSWorld::NotifyRobbery(int32 Victim,int32 Thief) {if(!Players.IsValidIndex(Victim)||!Players.IsValidIndex(Thief))return;auto& P=Players[Victim];if(Time-P.LastRobberyNotice<8)return;P.LastRobberyNotice=Time;if(P.Controller.IsValid())P.Controller->ClientNotice(TEXT("Your resources are being stolen! Scout your drop-off points with towers and religious units."));}
float ARTSWorld::MaxHP(const FRTSEntity& E) const {
 auto& Def=D(E);float Base=Def.HP;if(E.Owner<0||!Players.IsValidIndex(E.Owner))return Base;auto& P=Players[E.Owner];
 if(Def.bBuilding) {if(Def.Id==TEXT("works_of_defense"))Base*=1+(Has(E.Owner,TEXT("heaps_of_earth"))?.1f:0)+(Has(E.Owner,TEXT("works_of_timbers"))?.2f:0)+(Has(E.Owner,TEXT("pickets"))?.2f:0);if(RTS::IsJaredite(P.Faction))Base*=(E.Tier+2)*(1+P.Favor*.003f);return Base*FMath::Max(.001f,E.Progress);}
 if(RTS::IsInfantry(Def)&&Has(E.Owner,TEXT("exceedingly_large_in_stature")))Base*=1.2f;
 if(P.Faction==TEXT("shiz")&&RTS::IsInfantry(Def))Base*=.9f;
 if(P.Faction==TEXT("nephites"))Base=RTS::FavorMaxHP(Base,P.Favor);
 if(E.bDrunkenAura)Base*=.85f;return FMath::Max(1.f,Base);
}
void ARTSWorld::Rescale(int O) {for(auto& KV:Entities) {auto& E=*KV.Value;if(E.Owner!=O||E.HP<=0)continue;float Ratio=E.HP/E.MaxHP;E.MaxHP=MaxHP(E);E.HP=Ratio*E.MaxHP;}}
void ARTSWorld::Favor(int O,float Change) {if(!Players.IsValidIndex(O))return;Players[O].Favor=FMath::Clamp(Players[O].Favor+Change,0.f,100.f);Rescale(O);}
void ARTSWorld::Population() {for(auto& P:Players){P.Population=0;P.Capacity=0;} for(auto& KV:Entities) {auto& E=*KV.Value;if(!Players.IsValidIndex(E.Owner))continue;auto& P=Players[E.Owner];P.Population+=D(E).bBuilding?0:D(E).Pop;if(D(E).bBuilding&&E.Progress>=1) P.Capacity+=D(E).Id==TEXT("seat_of_the_people")?15:D(E).Id==TEXT("tents_and_habitations")?10:0;for(auto& J:E.Jobs)P.Population+=J.Pop;}for(auto& P:Players)P.Capacity=FMath::Min(P.Capacity,200);}
void ARTSWorld::ResetOrder(FRTSEntity& E,const FString& Order) {E.Order=Order;E.Path.Empty();E.Target=0;E.Channel=0;E.bDirectAttack=false;E.WorkTarget=0;E.DropTarget=0;E.LastPath=-100;}
void ARTSWorld::LiahonaFailure(ARTSController* C) {if(!Players.IsValidIndex(C->ServerSlot))return;auto& P=Players[C->ServerSlot];P.bLiahona=false;if(Time-P.LastProfanity>=30){P.LastProfanity=Time;Favor(P.Slot,-10);}Error(C,TEXT("The Liahona has ceased to work. Submit sorry to restore it."));}
void ARTSWorld::Chat(ARTSController* C,const FString& M) {if(!Players.IsValidIndex(C->ServerSlot))return;auto& P=Players[C->ServerSlot];if(RTS::HasProfanity(M,true))LiahonaFailure(C);if(RTS::IsApology(M)&&!P.bLiahona){P.bLiahona=true;Error(C,TEXT("The Lord acknowledges your repentance."));}FString Line=P.Name+TEXT(": ")+M;for(auto& Other:Players)if(Other.Controller.IsValid())Other.Controller->ClientChat(Line);}

void ARTSWorld::ConfigureLobby(ARTSController* C,int32 Resources,float Speed,int32 Fertility,bool FogEnabled) {
 if(!C||C->ServerSlot!=0||Phase!=0||Resources<0||Resources>3||Fertility<0||Fertility>2||!FMath::IsFinite(Speed))return;
 const bool ValidSpeed=Speed==.75f||Speed==1.f||Speed==1.25f||Speed==1.5f||Speed==2.f;if(!ValidSpeed)return;
 if(StartingResources==Resources&&GameSpeed==Speed&&Fruitfulness==Fertility&&bFogOfWar==FogEnabled)return;
 StartingResources=Resources;GameSpeed=Speed;Fruitfulness=Fertility;bFogOfWar=FogEnabled;for(auto& P:Players)P.bReady=false;Snapshot();
}
float ARTSWorld::EffectiveSight(const FRTSEntity& E) const {
 float Sight=D(E).Sight;if(D(E).Id==TEXT("jaredite_barge")&&Has(E.Owner,TEXT("glowing_stones")))Sight=14;
 if(Players.IsValidIndex(E.Owner)&&Players[E.Owner].Faction==TEXT("lamanites")&&D(E).Id==TEXT("temple_of_the_land")&&Has(E.Owner,TEXT("lofty_rameumptom")))Sight*=3;
 return Sight;
}
