#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool RunRTSV08ReligionTests(ARTSWorld* W) {
 if(!W||!W->GetWorld())return false;
 auto C=W->GetWorld()->SpawnActor<ARTSController>();if(!C)return false;C->ServerSlot=0;
 int Passed=0,Failed=0;TArray<TSharedPtr<FJsonValue>> Results;
 auto Check=[&](const FString& Name,bool OK){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),OK);Results.Add(MakeShared<FJsonValueObject>(R));if(OK)++Passed;else{++Failed;UE_LOG(LogTemp,Error,TEXT("RTS_V08_RELIGION_FAIL %s"),*Name);}};
 auto Near=[](float A,float B){return FMath::IsNearlyEqual(A,B,.001f);};
 auto Reset=[&](const FString& Faction){W->Entities.Empty();W->Players.Empty();W->TraderVisitAt.Empty();W->NextId=1;W->Time=100;W->Map=0;W->Phase=1;W->Winner=-1;W->bSandbox=W->bTesting=true;W->bFogOfWar=true;W->bDispatchingQueuedOrder=false;W->NavigationCount=-1;W->NavigationTime=-100;W->ObstacleCount=-1;W->Obstacles.Empty();C->ServerSlot=0;
  for(int I=0;I<2;++I){FRTSPlayer P;P.Slot=I;P.Team=I;P.Faction=I?TEXT("lamanites"):Faction;P.Age=4;P.Favor=50;P.Resources=FVector(10000);P.Cement=10000;P.Controller=I?nullptr:C;P.Explored.Init(1,4096);W->Players.Add(P);W->Spawn(TEXT("seat_of_the_people"),I,I?FVector2D(140,140):FVector2D(20,20));}
 };
 auto Spawn=[&](const TCHAR* Type,int Owner,FVector2D At){return W->Spawn(Type,Owner,At);};
 auto HQ=[&](){for(const auto& KV:W->Entities)if(KV.Value->Owner==0&&W->D(*KV.Value).Id==TEXT("seat_of_the_people"))return KV.Value.Get();return static_cast<FRTSEntity*>(nullptr);};
 auto FillPopulation=[&](){W->Population();FRTSJob Job;Job.Type=TEXT("industrious_laborer");Job.Pop=W->Players[0].Capacity-W->Players[0].Population;Job.Remaining=Job.Total=1000;HQ()->Jobs.Add(Job);W->Population();};
 auto MoreCapacity=[&](){for(int I=0;I<5;++I)Spawn(TEXT("tents_and_habitations"),0,{20.+6*I,10});};

 // The old curve applies to every Nephite unit class and preserves fractional health.
 Reset(TEXT("nephites"));
 for(const TCHAR* Type:{TEXT("industrious_laborer"),TEXT("man_of_moroni"),TEXT("teancum"),TEXT("ammonite_covenant_keeper"),TEXT("jaredite_barge")}){
  auto Unit=Spawn(Type,0,{40,40});const float Base=W->D(*Unit).HP;Unit->HP=Unit->MaxHP*.37f;
  for(float Favor:{0.f,50.f,75.f,100.f}){W->Favor(0,Favor-W->Players[0].Favor);const float Expected=Favor==0?1:Favor==50?Base:Favor==75?Base*10:Base*100;
   Check(FString::Printf(TEXT("favor.%s.at_%.0f"),Type,Favor),Near(Unit->MaxHP,Expected)&&Near(Unit->HP/Unit->MaxHP,.37f)&&Unit->HP>0);
  }
 }
 const float BuildingHP=HQ()->MaxHP;W->Favor(0,-100);Check(TEXT("favor.buildings_excluded"),Near(HQ()->MaxHP,BuildingHP));

 Reset(TEXT("nephites"));auto Teacher=Spawn(TEXT("teacher_of_the_word"),0,{40,40});auto Target=Spawn(TEXT("industrious_laborer"),1,{42,40});W->Time=59.99f;
 W->Command(C,TEXT("convert"),{Teacher->Id},Target->Pos,Target->Id,TEXT(""));W->Conversion(*Teacher,5);
 Check(TEXT("conversion.sixty_seconds_hostility_rejects_early_command_and_auto"),Teacher->Order==TEXT("idle")&&Target->Owner==1&&!W->IsConvertible(*Target,0));
 W->Time=60;W->Command(C,TEXT("convert"),{Teacher->Id},Target->Pos,Target->Id,TEXT(""));W->Conversion(*Teacher,5);
 Check(TEXT("conversion.sixty_seconds_boundary_accepts_innate_conversion"),Target->Owner==0&&W->D(*Target).Kind==TEXT("ammonite"));

 Reset(TEXT("nephites"));Teacher=Spawn(TEXT("teacher_of_the_word"),0,{40,40});Target=Spawn(TEXT("industrious_laborer"),1,{42,40});FillPopulation();
 W->Command(C,TEXT("convert"),{Teacher->Id},Target->Pos,Target->Id,TEXT(""));W->Conversion(*Teacher,5);
 Check(TEXT("conversion.population_required_at_command_and_auto_start"),Teacher->Order==TEXT("idle")&&Teacher->Channel==0&&Target->Owner==1);
 HQ()->Jobs.Empty();W->Conversion(*Teacher,4.9f);Check(TEXT("conversion.population_available_starts_channel"),Near(Teacher->Channel,4.9f));FillPopulation();W->Conversion(*Teacher,.1f);
 Check(TEXT("conversion.population_lost_before_completion_leaves_target_unchanged"),Target->Owner==1&&!Target->bConverted&&Teacher->Channel==0);
 HQ()->Jobs.Empty();Target->HP*=.4f;const int32 TargetId=Target->Id;const float Ratio=Target->HP/Target->MaxHP;W->Conversion(*Teacher,5);
 Check(TEXT("conversion.capacity_restored_preserves_identity_and_health_fraction"),Target->Owner==0&&Target->Id==TargetId&&Near(Target->HP/Target->MaxHP,Ratio));

 Reset(TEXT("nephites"));Teacher=Spawn(TEXT("teacher_of_the_word"),0,{40,40});auto PopulationTeacher=Spawn(TEXT("teacher_of_the_word"),0,{60,40});Target=Spawn(TEXT("industrious_laborer"),1,{42,40});auto PopulationTarget=Spawn(TEXT("industrious_laborer"),1,{62,40});W->Population();FRTSJob Reserved;Reserved.Type=TEXT("industrious_laborer");Reserved.Pop=W->Players[0].Capacity-W->Players[0].Population-FRTSCatalog::Get().Def(TEXT("ammonite_covenant_keeper")).Pop;Reserved.Remaining=Reserved.Total=1000;HQ()->Jobs.Add(Reserved);
 W->Conversion(*Teacher,4.9f);W->Conversion(*PopulationTeacher,4.9f);W->Conversion(*Teacher,.1f);W->Conversion(*PopulationTeacher,.1f);W->Population();
 Check(TEXT("conversion.concurrent_channels_never_overbook_population"),Target->Owner==0&&PopulationTarget->Owner==1&&PopulationTeacher->Channel==0&&W->Players[0].Population==W->Players[0].Capacity);

 Reset(TEXT("nephites"));MoreCapacity();Teacher=Spawn(TEXT("teacher_of_the_word"),0,{40,40});auto OtherTeacher=Spawn(TEXT("teacher_of_the_word"),0,{60,40});Target=Spawn(TEXT("industrious_laborer"),1,{42,40});auto OtherTarget=Spawn(TEXT("industrious_laborer"),1,{62,40});
 for(int I=0;I<11;++I)Spawn(TEXT("ammonite_covenant_keeper"),0,{70.+I,80});
 W->Conversion(*Teacher,4.9f);W->Conversion(*OtherTeacher,4.9f);W->Conversion(*Teacher,.1f);W->Conversion(*OtherTeacher,.1f);
 Check(TEXT("conversion.concurrent_ammonite_channels_never_exceed_twelve"),Target->Owner==0&&OtherTarget->Owner==1&&OtherTeacher->Channel==0&&!W->CanConvertWith(*OtherTeacher,*OtherTarget));

 Reset(TEXT("nephites"));Teacher=Spawn(TEXT("teacher_of_the_word"),0,{40,40});OtherTeacher=Spawn(TEXT("teacher_of_the_word"),0,{40,41});Target=Spawn(TEXT("industrious_laborer"),1,{42,40});W->Conversion(*Teacher,2);W->Conversion(*OtherTeacher,2);
 Check(TEXT("conversion.only_one_effective_channel_per_target"),Near(Teacher->Channel,2)&&OtherTeacher->Channel==0&&Target->Owner==1);
 W->Damage(*Target,*Teacher,1);W->Conversion(*Teacher,2.9f);Check(TEXT("conversion.target_damage_does_not_reset_channel"),Near(Teacher->Channel,4.9f)&&Target->Owner==1);
 W->Command(C,TEXT("move"),{Teacher->Id},{40,35},0,TEXT(""));Check(TEXT("conversion.movement_order_resets_channel"),Teacher->Order==TEXT("move")&&Teacher->Channel==0&&Target->Owner==1);

 Reset(TEXT("gadianton"));auto Lawyer=Spawn(TEXT("lawyer"),0,{40,40});Target=Spawn(TEXT("industrious_laborer"),1,{42,40});Check(TEXT("bribe.fixture_lawyer_initially_stealthed"),W->IsStealthed(*Lawyer));
 W->Command(C,TEXT("convert"),{Lawyer->Id},Target->Pos,Target->Id,TEXT(""));Check(TEXT("bribe.command_immediately_leaves_stealth"),Lawyer->Order==TEXT("convert")&&!W->IsStealthed(*Lawyer));
 for(int I=0;I<49;++I)W->Step(.1f);Check(TEXT("bribe.channel_exposed_without_target_attacking"),Target->Owner==1&&Lawyer->Channel>4.8f&&!W->IsStealthed(*Lawyer));
 W->Step(.1f);W->Step(.1f);Check(TEXT("bribe.completion_retains_class_disguise_and_recovery"),Target->Owner==0&&W->D(*Target).Id==TEXT("industrious_laborer")&&Target->DisguisedOwner==1&&!W->IsStealthed(*Lawyer));
 W->Time+=1.1f;Check(TEXT("bribe.brief_recovery_then_returns_to_stealth"),W->IsStealthed(*Lawyer));
 Target=Spawn(TEXT("industrious_laborer"),1,{42,41});W->Command(C,TEXT("convert"),{Lawyer->Id},Target->Pos,Target->Id,TEXT(""));W->Step(.1f);W->Command(C,TEXT("stop"),{Lawyer->Id},Lawyer->Pos,0,TEXT(""));
 Check(TEXT("bribe.interruption_also_keeps_brief_exposure"),!W->IsStealthed(*Lawyer)&&Target->Owner==1);

 Reset(TEXT("laban"));auto Winery=Spawn(TEXT("winery"),0,{40,40});auto Spacious=Spawn(TEXT("temple_of_the_land"),0,{60,40});auto Concubine=Spawn(TEXT("drunken_concubine"),0,{45,40});auto Laborer=Spawn(TEXT("industrious_laborer"),0,{46,40});
 Concubine->Order=TEXT("move");Concubine->Cooldown=50;Check(TEXT("laban.concubine_aura_innate_during_other_orders"),Near(W->LaborerAuraMultiplier(*Laborer),1.2f)&&Near(W->LabanAuraRadius(*Concubine),5));
 Check(TEXT("laban.winery_and_spacious_baseline_drunkenness_ratio"),Near(W->ReligiousGeneration(*Winery),.5f)&&Near(W->ReligiousGeneration(*Spacious),2.5f));
 W->Players[0].Upgrades.Add(TEXT("winery_drunkness"));Check(TEXT("laban.fermented_reserves_preserves_fivefold_ratio"),Near(W->ReligiousGeneration(*Winery),.6f)&&Near(W->ReligiousGeneration(*Spacious),3));
 Laborer->Pos={64,40};Check(TEXT("laban.spacious_building_also_gives_laborer_aura"),Near(W->LaborerAuraMultiplier(*Laborer),1.2f)&&Near(W->LabanAuraRadius(*Spacious),8));
 Target=Spawn(TEXT("cimeter_bearer"),1,{64,41});Target->HP*=.4f;W->Religion(.1f);Check(TEXT("laban.spacious_preserves_enemy_weakening_and_health_fraction"),Target->bDrunkenAura&&Near(Target->HP/Target->MaxHP,.4f)&&Near(W->AttackSpeedMultiplier(*Target),(1+W->Players[1].Favor*.003f)*.75f));
 Spacious->Progress=.5f;Check(TEXT("laban.foundation_has_no_aura_or_generation"),W->LabanAuraRadius(*Spacious)==0&&W->ReligiousGeneration(*Spacious)==0&&Near(W->LaborerAuraMultiplier(*Laborer),1));

 Reset(TEXT("lamanites"));auto Priest=Spawn(TEXT("priestcrafter"),0,{40,40});auto Soldier=Spawn(TEXT("cimeter_bearer"),0,{44,40});const float BaseAttack=W->D(*Soldier).Attack;
 Check(TEXT("priestcrafter.outside_attack_aura_restored"),Near(W->AttackStrength(*Soldier),BaseAttack*1.05f));auto Platform=Spawn(TEXT("temple_of_the_land"),0,{60,60});Priest->Pos=Platform->Pos-FVector2D(W->D(*Platform).Radius+.4f,0);Soldier->Pos={52,60};
 W->Command(C,TEXT("context"),{Priest->Id},Platform->Pos,Platform->Id,TEXT(""));W->Step(.1f);
 Check(TEXT("priestcrafter.context_occupancy_enables_stronger_aura"),Priest->Order==TEXT("occupy")&&Near(W->AttackStrength(*Soldier),BaseAttack*1.15f));
 auto SecondPriest=Spawn(TEXT("priestcrafter"),0,Priest->Pos+FVector2D(0,1));W->Command(C,TEXT("context"),{SecondPriest->Id},Platform->Pos,Platform->Id,TEXT(""));W->Step(.1f);
 Check(TEXT("priestcrafter.only_one_occupant_strongest_aura_does_not_stack"),Priest->Order==TEXT("occupy")&&SecondPriest->Order!=TEXT("occupy")&&Near(W->AttackStrength(*Soldier),BaseAttack*1.15f));

 Reset(TEXT("nephites"));auto Teancum=Spawn(TEXT("teancum"),0,{40,40});W->Ability(*Teancum,Teancum->Pos,0,C);const float Attack=W->D(*Teancum).Attack;
 Check(TEXT("teancum.night_raid_ten_seconds_stealth_forty_bonus_fortyfive_cooldown"),Teancum->Cooldown==45&&Near(Teancum->BuffUntil,W->Time+10)&&W->IsStealthed(*Teancum)&&Near(W->AttackStrength(*Teancum),Attack+40));
 Target=Spawn(TEXT("cimeter_bearer"),1,{41,40});Target->HP=Target->MaxHP=10000;W->Command(C,TEXT("attack"),{Teancum->Id},Target->Pos,Target->Id,TEXT(""));W->Combat(*Teancum,.1f);
 Check(TEXT("teancum.first_attack_consumes_night_raid_bonus"),Near(10000-Target->HP,Attack+40-W->D(*Target).Armor)&&Teancum->BuffUntil==0&&Near(W->AttackStrength(*Teancum),Attack));

 auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("passed"),Passed);Report->SetNumberField(TEXT("failed"),Failed);Report->SetArrayField(TEXT("assertions"),Results);FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("religion-v08-tests.json")));C->Destroy();UE_LOG(LogTemp,Display,TEXT("RTS_V08_RELIGION_TESTS passed=%d failed=%d"),Passed,Failed);return Failed==0;
}
