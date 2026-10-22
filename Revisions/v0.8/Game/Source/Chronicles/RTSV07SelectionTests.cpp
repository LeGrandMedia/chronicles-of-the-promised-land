#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool RTSRunV07SelectionTests(ARTSWorld& World){
 if(!World.GetWorld())return false;auto C=World.GetWorld()->SpawnActor<ARTSController>();if(!C)return false;
 int Passed=0,Failed=0;TArray<TSharedPtr<FJsonValue>> Assertions;
 auto Check=[&](const TCHAR* Name,bool Good){auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("name"),Name);A->SetBoolField(TEXT("passed"),Good);Assertions.Add(MakeShared<FJsonValueObject>(A));if(Good)Passed++;else{Failed++;UE_LOG(LogTemp,Error,TEXT("RTS_V07_SELECTION_FAIL %s"),Name);}};
 auto Reset=[&](FString Faction){C->Entities.Empty();C->Selection.Empty();C->Players.Empty();C->ActiveSubgroup.Empty();C->LocalSlot=0;C->Phase=1;C->SelectionPage=C->ActionPage=0;FRTSPlayerView P;P.Slot=0;P.Faction=Faction;P.Age=1;P.Resources=FVector(10000);P.Cement=1000;C->Players.Add(P);};
 auto Add=[&](const FString& Type){FRTSEntityView E;E.Id=C->Entities.Num()+1;E.Def=FRTSCatalog::Get().Index(Type);Check(*FString::Printf(TEXT("fixture.catalog_definition_exists.%s"),*Type),E.Def!=INDEX_NONE);if(E.Def==INDEX_NONE)return 0;E.Owner=0;E.Status=TEXT("idle");C->Entities.Add(E);C->Selection={E.Id};return E.Id;};
 for(const FString& Faction:{FString(TEXT("nephites")),FString(TEXT("lamanites")),FString(TEXT("coriantumr")),FString(TEXT("shiz")),FString(TEXT("laban")),FString(TEXT("gadianton"))}){
  Reset(Faction);Add(Faction==TEXT("nephites")?TEXT("teacher_of_the_word"):Faction==TEXT("laban")?TEXT("drunken_concubine"):Faction==TEXT("gadianton")?TEXT("lawyer"):TEXT("priestcrafter"));Check(TEXT("convert.innate_order_in_age_one_without_research"),C->SupportsOrder(TEXT("convert")));
 }
 Reset(TEXT("nephites"));Add(TEXT("ammonite_covenant_keeper"));Check(TEXT("prostrate.locked_before_mighty_change"),!C->SupportsOrder(TEXT("ability")));C->Players[0].Upgrades.Add(TEXT("mighty_change_of_heart"));Check(TEXT("prostrate.unlocked_after_research"),C->SupportsOrder(TEXT("ability")));
 Reset(TEXT("gadianton"));Add(TEXT("tents_and_habitations"));Check(TEXT("hideout.completed_gadianton_house_can_unload"),C->SupportsOrder(TEXT("ability")));C->Entities[0].Progress=.25f;Check(TEXT("hideout.foundation_has_no_unload"),!C->SupportsOrder(TEXT("ability")));C->Entities[0].Progress=1;C->Players[0].Faction=TEXT("nephites");Check(TEXT("hideout.other_civilization_house_is_not_carrier_ui"),!C->SupportsOrder(TEXT("ability")));
 Reset(TEXT("coriantumr"));int Beast=Add(TEXT("curelom")),Worker=Add(TEXT("industrious_laborer")),Trader=Add(TEXT("trader_in_precious_things"));auto Idle=C->IdleVillagers();Check(TEXT("idle.builders_beasts_excluded_real_workers_retained"),!Idle.Contains(Beast)&&Idle.Contains(Worker)&&Idle.Contains(Trader));
 C->Selection={Beast,Worker,Trader};C->UIAction(TEXT("selection_unit"),FString::FromInt(Worker));Check(TEXT("portrait.individual_selection_preserves_entity_identity"),C->Selection.Num()==1&&C->Selection[0]==Worker&&C->ActiveSubgroup.IsEmpty());C->UIAction(TEXT("selection_unit"),TEXT("99999"));Check(TEXT("portrait.stale_entity_cannot_enter_selection"),C->Selection.Num()==1&&C->Selection[0]==Worker);
 Reset(TEXT("gadianton"));Add(TEXT("industrious_laborer"));Check(TEXT("construction.gadianton_menu_keeps_food_buildings_unavailable"),!C->CanConstruct(FRTSCatalog::Get().Def(TEXT("fields_of_grain")))&&!C->CanConstruct(FRTSCatalog::Get().Def(TEXT("gathering_of_flocks")))&&C->CanConstruct(FRTSCatalog::Get().Def(TEXT("storehouse_of_provisions"))));
 Reset(TEXT("lamanites"));Add(TEXT("priestcrafter"));Check(TEXT("construction.priest_not_builder_before_pride"),!C->HasSelectedBuilder());C->Players[0].Upgrades.Add(TEXT("by_the_pride_of_their_hearts"));Check(TEXT("construction.pride_priest_can_open_temple_build_menu"),C->HasSelectedBuilder()&&C->CanConstruct(FRTSCatalog::Get().Def(TEXT("temple_of_the_land")))&&!C->CanConstruct(FRTSCatalog::Get().Def(TEXT("fields_of_grain"))));
 auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("passed"),Passed);Report->SetNumberField(TEXT("failed"),Failed);Report->SetArrayField(TEXT("assertions"),Assertions);FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("v07-selection-tests.json")));C->Destroy();UE_LOG(LogTemp,Display,TEXT("RTS_V07_SELECTION_TESTS passed=%d failed=%d"),Passed,Failed);return Failed==0;
}
