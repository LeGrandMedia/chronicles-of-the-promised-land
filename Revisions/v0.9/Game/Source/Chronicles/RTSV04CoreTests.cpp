#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool RunRTSV04CoreTests(ARTSWorld* W) {
 if(!W||!W->GetWorld())return false;auto C=W->GetWorld()->SpawnActor<ARTSController>();if(!C)return false;
 int Passed=0,Failed=0;TArray<TSharedPtr<FJsonValue>> Results;
 auto Check=[&](const FString& Name,bool OK){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),OK);Results.Add(MakeShared<FJsonValueObject>(R));if(OK)++Passed;else{++Failed;UE_LOG(LogTemp,Error,TEXT("RTS_V04_CORE_FAIL %s"),*Name);}};
 auto Reset=[&](FString Faction){W->Entities.Empty();W->Players.Empty();W->TraderVisitAt.Empty();W->NextId=1;W->Time=100;W->Map=0;W->Phase=1;W->bSandbox=true;W->bDispatchingQueuedOrder=false;W->NavigationCount=-1;W->NavigationTime=-100;W->ObstacleCount=-1;W->Obstacles.Empty();C->ServerSlot=0;
  for(int I=0;I<2;++I){FRTSPlayer P;P.Slot=I;P.Team=I;P.Faction=I?TEXT("lamanites"):Faction;P.Age=2;P.Resources=FVector(10000);P.Favor=20;P.Controller=I?nullptr:C;P.Explored.Init(1,4096);W->Players.Add(P);W->Spawn(TEXT("seat_of_the_people"),I,I?FVector2D(136,136):FVector2D(20,20));}
 };
 auto Spawn=[&](FString Type,int Owner,FVector2D At){return W->Spawn(Type,Owner,At);};auto& Cat=FRTSCatalog::Get();
 TSet<FString> Unique;bool Valid=true;for(const auto& D:Cat.Defs){Valid&=!D.Id.IsEmpty()&&!Unique.Contains(D.Id);Unique.Add(D.Id);}Check(TEXT("catalog.unique_definition_ids"),Valid);
 for(const auto& F:Cat.Factions){Reset(F.Id);W->Players[0].Age=1;auto HQ=W->Find(1);auto OtherHQ=Spawn(TEXT("seat_of_the_people"),0,{35,20});bool AllAges=true;
  for(int A=1;A<=3;++A){const FRTSDef* Choice=nullptr;int Count=0;for(const FString& Id:F.Upgrades){const auto& D=Cat.Def(Id);if(D.Kind==TEXT("age_option")&&D.Age==A){Choice=&D;++Count;}}
   AllAges&=Count>=2&&Choice;if(!Choice)continue;for(const FString& Req:Choice->Requires)if(!W->OwnedStructure(0,Req)&&Cat.Def(Req).bBuilding)Spawn(Req,0,{20.+A*10,60});W->Queue(*HQ,Choice->Id,C);bool Queued=HQ->Jobs.Num()==1;W->Queue(*OtherHQ,Choice->Id,C);AllAges&=Queued&&OtherHQ->Jobs.IsEmpty();if(Queued){HQ->Jobs[0].Remaining=.01f;W->Production(*HQ,.1f);}AllAges&=W->Players[0].Age==A+1&&W->Players[0].HeroChoices.Contains(Choice->HeroChoice);
   const FVector Balance=W->Players[0].Resources;W->Queue(*OtherHQ,Choice->Id,C);AllAges&=OtherHQ->Jobs.IsEmpty()&&W->Players[0].Resources==Balance;
  }Check(TEXT("ages.all_three_advancements_and_global_lock.")+F.Id,AllAges&&W->Players[0].Age==4&&W->Players[0].HeroChoices.Num()==3);
 }
 Reset(TEXT("laban"));auto Concubine=Spawn(TEXT("drunken_concubine"),0,{40,40});auto Victim=Spawn(TEXT("cimeter_bearer"),1,{42,40});int VictimId=Victim->Id;Victim->HP=Victim->MaxHP*.5f;W->Religion(.1f);
 Check(TEXT("laban.aura_reduces_max_hp_preserving_health_fraction"),Victim->bDrunkenAura&&FMath::IsNearlyEqual(Victim->MaxHP,Cat.Def(TEXT("cimeter_bearer")).HP*.85f,.02f)&&FMath::IsNearlyEqual(Victim->HP/Victim->MaxHP,.5f));
 Victim->Pos={60,40};W->Religion(.1f);Check(TEXT("laban.aura_expires_without_healing_fraction"),!Victim->bDrunkenAura&&FMath::IsNearlyEqual(Victim->HP/Victim->MaxHP,.5f));
 Victim->Pos={42,40};W->Conversion(*Concubine,8);Check(TEXT("laban.seduction_creates_regular_worker"),W->Find(VictimId)->Owner==0&&W->D(*Victim).Id==TEXT("industrious_laborer"));
 Reset(TEXT("nephites"));auto Archer=Spawn(TEXT("bowman"),0,{40,40});auto Priest=Spawn(TEXT("priestcrafter"),1,{42,40});float HP=Priest->HP;W->Damage(*Priest,*Archer,20);Check(TEXT("counters.ranged_50percent_bonus_against_religion"),FMath::IsNearlyEqual(HP-Priest->HP,FMath::Max(1.f,30-Cat.Def(TEXT("priestcrafter")).Armor),.01f));
 auto Melee=Spawn(TEXT("cimeter_bearer"),1,{43,40});HP=Melee->HP;W->Damage(*Melee,*Archer,20);Check(TEXT("counters.melee_25percent_ranged_resistance"),FMath::IsNearlyEqual(HP-Melee->HP,FMath::Max(1.f,15-Cat.Def(TEXT("cimeter_bearer")).Armor),.01f));
 Reset(TEXT("lamanites"));Priest=Spawn(TEXT("priestcrafter"),0,{40,40});Melee=Spawn(TEXT("cimeter_bearer"),1,{42,40});W->Conversion(*Priest,4.9f);bool Waiting=Melee->Owner==1;W->Conversion(*Priest,.11f);Check(TEXT("counters.melee_conversion_finishes_after_five_seconds"),Waiting&&Melee->Owner==0);
 Reset(TEXT("gadianton"));auto Lawyer=Spawn(TEXT("lawyer"),0,{40,40});Victim=Spawn(TEXT("industrious_laborer"),1,{42,40});VictimId=Victim->Id;W->Conversion(*Lawyer,12);Check(TEXT("infiltration.lawyer_transfers_real_owner_and_retains_disguise"),Victim->Owner==0&&Victim->DisguisedOwner==1&&W->CanSee(1,*Victim));
 C->ServerSlot=1;W->Command(C,TEXT("move"),{VictimId},{70,70},0,TEXT(""));Check(TEXT("infiltration.former_owner_orders_ignored"),Victim->Order==TEXT("idle"));auto FormerAlly=Spawn(TEXT("cimeter_bearer"),1,{43,40});HP=Victim->HP;W->Damage(*Victim,*FormerAlly,1000);Check(TEXT("infiltration.former_owner_cannot_attack_disguise"),Victim->HP==HP);
 auto OtherForeign=Spawn(TEXT("industrious_laborer"),0,{50,40});int OtherId=OtherForeign->Id;W->Command(C,TEXT("delete"),{OtherId},{0,0},0,TEXT(""));W->Command(C,TEXT("delete"),{VictimId},{0,0},0,TEXT(""));Check(TEXT("infiltration.delete_exception_is_limited_to_former_owner"),W->Find(VictimId)==nullptr&&W->Find(OtherId)!=nullptr);
 Reset(TEXT("gadianton"));W->Players[0].Upgrades.Add(TEXT("network_of_secrets"));W->Players[0].Favor=50;bool Boundary=!W->VisiblePoint(0,{135,135});W->Players[0].Favor=50.1f;Check(TEXT("infiltration.shared_vision_requires_more_than_fifty"),Boundary&&W->VisiblePoint(0,{135,135}));W->Players[0].Favor=49;Check(TEXT("infiltration.shared_vision_is_revoked_when_stat_falls"),!W->VisiblePoint(0,{135,135}));
 W->Players[0].Upgrades.Add(TEXT("plunder_share"));W->Players[0].Favor=30;bool NoBonus=W->PlunderMultiplier(0)==1;W->Players[0].Favor=31;bool Double=W->PlunderMultiplier(0)==2;W->Players[0].Upgrades.Add(TEXT("plunder_hoard"));Check(TEXT("infiltration.raid_multiplier_threshold_and_upgrade"),NoBonus&&Double&&W->PlunderMultiplier(0)==3);
 auto Hidden=Spawn(TEXT("gadianton_robber"),0,{60,60});Hidden->LastMove=90;auto Scout=Spawn(TEXT("cimeter_bearer"),1,{64,60});Check(TEXT("stealth.stationary_band_is_hidden"),W->IsStealthed(*Hidden)&&!W->CanSee(1,*Hidden));Spawn(TEXT("tower_of_watchmen"),1,{60,66});Check(TEXT("stealth.tower_detects_band_without_judgment_seat"),W->CanSee(1,*Hidden));
 Reset(TEXT("lamanites"));auto Chariot=Spawn(TEXT("kings_chariot"),0,{40,40});auto Passenger=Spawn(TEXT("industrious_laborer"),0,{41,40});W->Command(C,TEXT("context"),{Passenger->Id},Chariot->Pos,Chariot->Id,TEXT(""));W->Step(.1f);Check(TEXT("carrier.regular_worker_boards_chariot"),Passenger->Carrier==Chariot->Id&&Chariot->Passengers.Contains(Passenger->Id));W->Ability(*Chariot,Chariot->Pos,0,C);Check(TEXT("carrier.chariot_unloads_regular_worker"),Passenger->Carrier==0&&Chariot->Passengers.IsEmpty());
 Reset(TEXT("coriantumr"));auto Elephant=Spawn(TEXT("elephant"),0,{40,40});Passenger=Spawn(TEXT("industrious_laborer"),0,{41,40});W->Command(C,TEXT("context"),{Passenger->Id},Elephant->Pos,Elephant->Id,TEXT(""));W->Step(.1f);Check(TEXT("carrier.elephant_boards_worker"),Passenger->Carrier==Elephant->Id);W->Ability(*Elephant,Elephant->Pos,0,C);Check(TEXT("carrier.elephant_unloads_worker"),Passenger->Carrier==0&&Elephant->Passengers.IsEmpty());
 Reset(TEXT("nephites"));W->Players[0].Favor=70;W->Religion(1);Check(TEXT("devotion.high_values_decay_by_more_than_one_per_second"),W->Players[0].Favor<69);
 auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("passed"),Passed);Report->SetNumberField(TEXT("failed"),Failed);Report->SetArrayField(TEXT("assertions"),Results);FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("core-v04-tests.json")));C->Destroy();UE_LOG(LogTemp,Display,TEXT("RTS_V04_CORE_TESTS passed=%d failed=%d"),Passed,Failed);return Failed==0;
}
