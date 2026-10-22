#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool RunRTSV05CombatTests(ARTSWorld* W) {
 if(!W||!W->GetWorld())return false;
 auto C=W->GetWorld()->SpawnActor<ARTSController>();if(!C)return false;
 int Passed=0,Failed=0;TArray<TSharedPtr<FJsonValue>> Checks,Matches;
 auto Check=[&](FString Name,bool OK){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),OK);Checks.Add(MakeShared<FJsonValueObject>(R));if(OK)++Passed;else{++Failed;UE_LOG(LogTemp,Error,TEXT("RTS_V05_COMBAT_FAIL %s"),*Name);}};
 auto Reset=[&](FString A,FString B){W->Entities.Empty();W->Players.Empty();W->TraderVisitAt.Empty();W->NextId=1;W->Time=100;W->Map=0;W->Phase=1;W->bSandbox=true;W->bDispatchingQueuedOrder=false;W->NavigationCount=-1;W->NavigationTime=-100;W->ObstacleCount=-1;W->Obstacles.Empty();C->ServerSlot=0;
  for(int I=0;I<2;++I){FRTSPlayer P;P.Slot=I;P.Team=I;P.Faction=I?B:A;P.Age=3;P.Resources=FVector(10000);P.Cement=10000;P.Favor=20;P.Controller=I?nullptr:C;P.Explored.Init(1,4096);W->Players.Add(P);W->Spawn(TEXT("seat_of_the_people"),I,I?FVector2D(145,145):FVector2D(15,15));for(int J=0;J<5;++J)W->Spawn(TEXT("tents_and_habitations"),I,I?FVector2D(110+J*6,145):FVector2D(15+J*6,15));}W->Population();
 };
 auto Spawn=[&](const TCHAR* Type,int OwnerSlot,FVector2D At){return W->Spawn(Type,OwnerSlot,At);};auto& Cat=FRTSCatalog::Get();
 Reset(TEXT("laban"),TEXT("lamanites"));auto Laban=Spawn(TEXT("laban"),0,{40,40});auto Concubine=Spawn(TEXT("drunken_concubine"),0,{43,40});auto Guard=Spawn(TEXT("one_of_the_fifty"),0,{42,41});auto Attacker=Spawn(TEXT("bowman"),1,{46,40});
 float HP=Concubine->HP;W->Damage(*Concubine,*Attacker,10);Check(TEXT("laban.concubine_receives_one_armor"),FMath::IsNearlyEqual(HP-Concubine->HP,14.f));
 HP=Guard->HP;W->Damage(*Guard,*Attacker,20);Check(TEXT("laban.military_no_longer_receives_armor"),FMath::IsNearlyEqual(HP-Guard->HP,10.f));
 W->Players[0].Upgrades.Add(TEXT("age2_laban_laban"));Check(TEXT("laban.age_choice_boosts_concubines_not_military"),FMath::IsNearlyEqual(W->MilitaryMultiplier(0),1.f)&&FMath::IsNearlyEqual(W->ConcubinePower(*Concubine),1.375f));
 auto Victim=Spawn(TEXT("cimeter_bearer"),1,{49.5,40});W->Religion(.1f);Check(TEXT("laban.concubine_aura_reaches_seven_metres"),Victim->bDrunkenAura);Laban->Pos={20,40};W->Religion(.1f);Check(TEXT("laban.extra_aura_range_ends_when_hero_leaves"),!Victim->bDrunkenAura);
 Laban->Pos={40,40};Victim->Pos={45,40};Concubine->Cooldown=0;Concubine->LastDamage=-100;W->ResetOrder(*Concubine,TEXT("idle"));W->Conversion(*Concubine,4.9f);bool Waiting=Victim->Owner==1;W->Conversion(*Concubine,.1f);Check(TEXT("laban.buffs_preserve_exact_five_second_seduction"),Waiting&&Victim->Owner==0);
 Reset(TEXT("nephites"),TEXT("lamanites"));auto Archer=Spawn(TEXT("bowman"),0,{40,40});auto Priest=Spawn(TEXT("priestcrafter"),1,{46,40});HP=Priest->HP;W->Combat(*Archer,.1f);Check(TEXT("combat.ranged_units_auto_acquire_enemy_religious_units"),Archer->Target==Priest->Id&&Priest->HP<HP);
 W->ResetOrder(*Priest,TEXT("convert"));Priest->Target=Archer->Id;Priest->Channel=7;W->Damage(*Priest,*Archer,1);Check(TEXT("combat.damage_interrupts_conversion_without_cooldown"),Priest->Channel==0&&Priest->Cooldown==0);
 Reset(TEXT("lamanites"),TEXT("lamanites"));auto Defender=Spawn(TEXT("cimeter_bearer"),0,{40,40});auto Enemy=Spawn(TEXT("bowman"),1,{46,40});W->Command(C,TEXT("hold"),{Defender->Id},{0,0},0,TEXT(""));W->Combat(*Defender,1);Check(TEXT("combat.hold_position_does_not_chase"),Defender->Pos.Equals(FVector2D(40,40),.01));
 W->ResetOrder(*Defender,TEXT("idle"));W->Combat(*Defender,1);Check(TEXT("combat.idle_melee_closes_on_ranged_threat"),Defender->Pos.X>40);
 Reset(TEXT("gadianton"),TEXT("lamanites"));auto Band=Spawn(TEXT("gadianton_robber"),0,{40,40});Band->LastMove=90;Archer=Spawn(TEXT("bowman"),1,{46,40});W->Combat(*Archer,.1f);Check(TEXT("combat.stationary_stealth_prevents_auto_acquisition"),Archer->Target==0);Spawn(TEXT("tower_of_watchmen"),1,{40,46});W->Combat(*Archer,.1f);Check(TEXT("combat.tower_detection_enables_ranged_attack"),Archer->Target==Band->Id&&Band->HP<Band->MaxHP);
 for(const auto& F:Cat.Factions){Reset(F.Id,TEXT("lamanites"));Spawn(F.Id==TEXT("gadianton")?TEXT("lawyer"):F.Id==TEXT("laban")?TEXT("drunken_concubine"):TEXT("priestcrafter"),0,{40,40});Spawn(F.Id==TEXT("gadianton")?TEXT("secret_combination_lodge"):TEXT("temple_of_the_land"),0,{50,40});W->Players[0].Favor=60;W->RefreshReligiousStats();Check(TEXT("religion.hover_counts_and_values.")+F.Id,W->Players[0].ReligiousUnits==1&&W->Players[0].ReligiousStructures==1&&W->Players[0].ReligionTooltip.Contains(TEXT("Generation"))&&W->Players[0].ReligionTooltip.Contains(TEXT("Decay")));}
 // Actual deterministic engagements: normal Step drives acquisition, pathing, damage,
 // conversion, auras, casualties and victory. No test-side damage or steering during fights.
 auto Fight=[&](const TCHAR* Name,const TCHAR* FactionA,const TCHAR* TypeA,int CountA,const TCHAR* FactionB,const TCHAR* TypeB,int CountB,bool Reverse=false){
  Reset(FactionA,FactionB);TArray<int32> Army;FVector CostA=FVector::ZeroVector,CostB=FVector::ZeroVector;
  for(int Side=0;Side<2;++Side){const TCHAR* Type=Side?TypeB:TypeA;const int Count=Side?CountB:CountA;for(int I=0;I<Count;++I){float X=(Side!=int(Reverse))?52.f:44.f;auto E=Spawn(Type,Side,{X+(I/6)*((Side!=int(Reverse))?2.:-2.),65.+I%6*2.});Army.Add(E->Id);(Side?CostB:CostA)+=Cat.Price(Cat.Def(Type),Side?FactionB:FactionA);}}
  W->Population();float Elapsed=0;int AliveA=CountA,AliveB=CountB,Converted=0;bool Finite=true;
  for(int Tick=0;Tick<2400&&AliveA>0&&AliveB>0;++Tick){W->Step(.1f);Elapsed+=.1f;AliveA=AliveB=0;for(int Id:Army)if(auto E=W->Find(Id)){Finite&=FMath::IsFinite(E->HP)&&FMath::IsFinite(E->Pos.X)&&FMath::IsFinite(E->Pos.Y);if(E->Owner==0)++AliveA;else ++AliveB;}}
  float HealthA=0,HealthB=0;for(int Id:Army)if(auto E=W->Find(Id)){if(E->bConverted)++Converted;(E->Owner?HealthB:HealthA)+=E->HP;}
  auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("scenario"),Name);R->SetStringField(TEXT("faction_a"),FactionA);R->SetStringField(TEXT("unit_a"),TypeA);R->SetNumberField(TEXT("start_a"),CountA);R->SetStringField(TEXT("faction_b"),FactionB);R->SetStringField(TEXT("unit_b"),TypeB);R->SetNumberField(TEXT("start_b"),CountB);R->SetNumberField(TEXT("seconds"),Elapsed);R->SetNumberField(TEXT("survivors_a"),AliveA);R->SetNumberField(TEXT("survivors_b"),AliveB);R->SetNumberField(TEXT("health_a"),HealthA);R->SetNumberField(TEXT("health_b"),HealthB);R->SetNumberField(TEXT("converted_survivors"),Converted);R->SetNumberField(TEXT("combined_resource_cost_a"),CostA.X+CostA.Y+CostA.Z);R->SetNumberField(TEXT("combined_resource_cost_b"),CostB.X+CostB.Y+CostB.Z);R->SetStringField(TEXT("result"),!AliveA?TEXT("B wins"):!AliveB?TEXT("A wins"):TEXT("240-second stalemate"));Matches.Add(MakeShared<FJsonValueObject>(R));Check(FString(TEXT("engagement.finite_state."))+Name,Finite);UE_LOG(LogTemp,Display,TEXT("RTS_COMBAT_MATCH %s %.1fs survivors=%d/%d"),Name,Elapsed,AliveA,AliveB);
 };
 Fight(TEXT("melee_vs_bows_equal_units"),TEXT("lamanites"),TEXT("cimeter_bearer"),6,TEXT("lamanites"),TEXT("bowman"),6);
 Fight(TEXT("melee_vs_bows_reversed_sides"),TEXT("lamanites"),TEXT("cimeter_bearer"),6,TEXT("lamanites"),TEXT("bowman"),6,true);
 Fight(TEXT("bows_vs_religion_equal_units"),TEXT("lamanites"),TEXT("bowman"),4,TEXT("lamanites"),TEXT("priestcrafter"),4);
 Fight(TEXT("melee_vs_religion_equal_units"),TEXT("lamanites"),TEXT("cimeter_bearer"),4,TEXT("lamanites"),TEXT("priestcrafter"),4);
 Fight(TEXT("raiders_vs_line_infantry"),TEXT("gadianton"),TEXT("gadianton_robber"),6,TEXT("lamanites"),TEXT("cimeter_bearer"),6);
 Fight(TEXT("concubines_vs_melee"),TEXT("laban"),TEXT("drunken_concubine"),4,TEXT("lamanites"),TEXT("cimeter_bearer"),4);
 Fight(TEXT("elephants_vs_melee"),TEXT("coriantumr"),TEXT("elephant"),2,TEXT("lamanites"),TEXT("cimeter_bearer"),12);
 Fight(TEXT("elite_guard_vs_melee"),TEXT("laban"),TEXT("one_of_the_fifty"),4,TEXT("lamanites"),TEXT("cimeter_bearer"),12);
 Fight(TEXT("nephite_vs_lamanite_equal_units"),TEXT("nephites"),TEXT("cimeter_bearer"),8,TEXT("lamanites"),TEXT("cimeter_bearer"),8);
 Fight(TEXT("nephite_vs_lamanite_rough_equal_cost"),TEXT("nephites"),TEXT("cimeter_bearer"),6,TEXT("lamanites"),TEXT("cimeter_bearer"),11);
 Fight(TEXT("shiz_vs_coriantumr_equal_infantry"),TEXT("shiz"),TEXT("cimeter_bearer"),8,TEXT("coriantumr"),TEXT("cimeter_bearer"),8);
 Fight(TEXT("elephant_vs_watchtower"),TEXT("coriantumr"),TEXT("elephant"),1,TEXT("lamanites"),TEXT("tower_of_watchmen"),1);
 auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("passed"),Passed);Report->SetNumberField(TEXT("failed"),Failed);Report->SetArrayField(TEXT("assertions"),Checks);Report->SetArrayField(TEXT("combat_matches"),Matches);Report->SetStringField(TEXT("method"),TEXT("Native simulation, 0.1s ticks, clear ground, 8m starting gap, Age III, initial religious stat20, no upgrades or micromanagement, 240s maximum. Results are balance observations, not assertions that every matchup is fair."));
 FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("combat-v05-tests.json")));C->Destroy();UE_LOG(LogTemp,Display,TEXT("RTS_V05_COMBAT_TESTS passed=%d failed=%d"),Passed,Failed);return Failed==0;
}
