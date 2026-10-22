#include "RTSControls.h"
#include "RTSNames.h"
#include "RTSTypes.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	struct FControlState
	{
		FString Id;
		FString Label;
		FKey Key;
		bool bBound = false;
		bool bCtrl = false;
		bool bShift = false;
		bool bAlt = false;
	};

	const TCHAR* TestBuildingIds[] = {
		TEXT("encampment_of_war"), TEXT("fields_of_grain"), TEXT("gathering_of_flocks"),
		TEXT("hill_of_ore"), TEXT("house_of_curious_workmanship"),
		TEXT("place_of_many_waters"), TEXT("place_of_trade"), TEXT("rameumptom"),
		TEXT("seat_of_the_people"), TEXT("storehouse_of_provisions"), TEXT("temple_of_the_land"),
		TEXT("tents_and_habitations"), TEXT("tower_of_watchmen"), TEXT("works_of_defense"), TEXT("secret_combination_lodge"), TEXT("winery"), TEXT("cement_wall"), TEXT("cement_tower")
	};
	const TCHAR* DefaultKeys[] = {
		TEXT("Q"), TEXT("W"), TEXT("E"), TEXT("R"), TEXT("T"), TEXT("U"), TEXT("I"),
		TEXT("O"), TEXT("P"), TEXT("A"), TEXT("S"), TEXT("D"), TEXT("F"), TEXT("G"), TEXT("J"), TEXT("Z"), TEXT("K"), TEXT("L")
	};

	TArray<FString> AllActionIds()
	{
		TArray<FString> Result;
		for (const TCHAR* Id : TestBuildingIds) Result.Add(FString(TEXT("build.")) + Id);
		Result.Add(TEXT("idle.next"));
		Result.Add(TEXT("idle.all"));
        Result.Add(TEXT("menu.build"));
		return Result;
	}

	FString KeyNameFromLabel(const FString& Label)
	{
		if (Label == TEXT(".")) return TEXT("Period");
		if (Label == TEXT(",")) return TEXT("Comma");
		if (Label == TEXT("/")) return TEXT("Slash");
		if (Label == TEXT("\\")) return TEXT("Backslash");
		if (Label == TEXT("[")) return TEXT("LeftBracket");
		if (Label == TEXT("]")) return TEXT("RightBracket");
		if (Label == TEXT(";")) return TEXT("Semicolon");
		if (Label == TEXT("'")) return TEXT("Apostrophe");
		if (Label == TEXT("-")) return TEXT("Hyphen");
		if (Label == TEXT("=")) return TEXT("Equals");
		if (Label == TEXT("Space")) return TEXT("SpaceBar");
		static const TMap<FString, FString> Digits = {
			{TEXT("0"), TEXT("Zero")}, {TEXT("1"), TEXT("One")}, {TEXT("2"), TEXT("Two")}, {TEXT("3"), TEXT("Three")},
			{TEXT("4"), TEXT("Four")}, {TEXT("5"), TEXT("Five")}, {TEXT("6"), TEXT("Six")}, {TEXT("7"), TEXT("Seven")},
			{TEXT("8"), TEXT("Eight")}, {TEXT("9"), TEXT("Nine")}
		};
		if (const FString* Name = Digits.Find(Label)) return *Name;
		return Label;
	}

	FControlState CaptureControl(const FRTSControls& Controls, const FString& Id)
	{
		FControlState State;
		State.Id = Id;
		State.Key = Controls.KeyFor(Id);
		State.Label = Controls.LabelFor(Id);
		State.bBound = !State.Key.GetFName().IsNone();
		if (!State.bBound) return State;
		TArray<FString> Parts;
		Controls.LabelFor(Id).ParseIntoArray(Parts, TEXT("+"), false);
		if (Parts.Num() == 0) { State.bBound = false; return State; }
		const FString KeyLabel = Parts.Last();
		State.bCtrl = Parts.Contains(TEXT("Ctrl"));
		State.bShift = Parts.Contains(TEXT("Shift"));
		State.bAlt = Parts.Contains(TEXT("Alt"));
		State.Key = FKey(FName(*KeyNameFromLabel(KeyLabel)));
		return State;
	}

	bool RestoreControls(FRTSControls& Controls, const TArray<FControlState>& Snapshot)
	{
		Controls.ResetDefaults();
	for (const FControlState& State : Snapshot) Controls.Clear(State.Id);
		bool bRestored = true;
		for (const FControlState& State : Snapshot)
		{
			if (!State.bBound) { Controls.Clear(State.Id); continue; }
			FString Notice;
			bRestored &= Controls.Rebind(State.Id, State.Key, State.bCtrl, State.bShift, State.bAlt, Notice);
		}
		for (const FControlState& State : Snapshot)
		{
			const FKey CurrentKey = Controls.KeyFor(State.Id);
			const bool bCurrentBound = !CurrentKey.GetFName().IsNone();
			bRestored &= bCurrentBound == State.bBound;
			if (State.bBound)
				bRestored &= CurrentKey == State.Key && Controls.LabelFor(State.Id) == State.Label;
		}
		return bRestored;
	}

	void AddCheck(TArray<TSharedPtr<FJsonValue>>& Assertions, int32& Passed, int32& Failed,
		const FString& Name, bool bCondition, const FString& Expected, const FString& Actual)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Name);
		Item->SetBoolField(TEXT("passed"), bCondition);
		Item->SetStringField(TEXT("expected"), Expected);
		Item->SetStringField(TEXT("actual"), Actual);
		Assertions.Add(MakeShared<FJsonValueObject>(Item.ToSharedPtr()));
		if (bCondition) ++Passed;
		else { ++Failed; UE_LOG(LogTemp, Error, TEXT("RTS_CUSTOMIZATION_TEST_FAIL %s expected=%s actual=%s"), *Name, *Expected, *Actual); }
	}

	bool WriteControlsText(const FString& Path, const FString& Text)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Text, *Path);
	}
}

/** Native smoke/regression suite; safe to call from a development-only server test entry point. */
bool RunRTSCustomizationTests()
{
	TArray<TSharedPtr<FJsonValue>> Assertions;
	int32 Passed = 0, Failed = 0;
	auto Check = [&](const FString& Name, bool bCondition, const FString& Expected, const FString& Actual)
	{
		AddCheck(Assertions, Passed, Failed, Name, bCondition, Expected, Actual);
	};

	FRTSControls& Controls = FRTSControls::Get();
	const TArray<FString> ActionIds = AllActionIds();
	TArray<FControlState> OriginalState;
	for (const FString& Id : ActionIds) OriginalState.Add(CaptureControl(Controls, Id));

	const FString ControlsPath = FPaths::ProjectSavedDir() / TEXT("Config/RTSControls.json");
	const bool bHadControlsFile = IFileManager::Get().FileExists(*ControlsPath);
	TArray<uint8> OriginalBytes;
	const bool bCouldBackup = !bHadControlsFile || FFileHelper::LoadFileToArray(OriginalBytes, *ControlsPath);
	Check(TEXT("controls.original_file_backed_up"), bCouldBackup, TEXT("existing bytes preserved or file absent"),
		bHadControlsFile ? (bCouldBackup ? FString::Printf(TEXT("%d bytes"), OriginalBytes.Num()) : TEXT("read failed")) : TEXT("file absent"));

	if (bCouldBackup)
	{
		Controls.ResetDefaults();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(TestBuildingIds); ++Index)
		{
			const FString Id = FString(TEXT("build.")) + TestBuildingIds[Index];
			const FKey ExpectedKey{FName(DefaultKeys[Index])};
			const FKey ActualKey = Controls.KeyFor(Id);
			const FString Match = Controls.Match(ExpectedKey, false, false, false, TEXT("build."));
			Check(TEXT("controls.default_build.") + FString::FromInt(Index + 1),
				ActualKey == ExpectedKey && Match == Id, Id, Match + TEXT(" / ") + ActualKey.GetFName().ToString());
		}
        Check(TEXT("controls.default_construction_menu_q"),Controls.LabelFor(TEXT("menu.build"))==TEXT("Q")&&Controls.Match(EKeys::Q,false,false,false,TEXT("menu."))==TEXT("menu.build")&&Controls.Match(EKeys::Q,false,false,false,TEXT("build."))==TEXT("build.encampment_of_war"),TEXT("Q opens construction; Q remains first build card in build context"),Controls.LabelFor(TEXT("menu.build")));
		Check(TEXT("controls.default_idle_next"), Controls.LabelFor(TEXT("idle.next")) == TEXT("." ) &&
			Controls.Match(FKey(EKeys::Period), false, false, false, TEXT("idle.")) == TEXT("idle.next"),
			TEXT("Period -> idle.next"), Controls.LabelFor(TEXT("idle.next")));
		Check(TEXT("controls.default_idle_all"), Controls.LabelFor(TEXT("idle.all")) == TEXT("Ctrl+." ) &&
			Controls.Match(FKey(EKeys::Period), true, false, false, TEXT("idle.")) == TEXT("idle.all"),
			TEXT("Ctrl+Period -> idle.all"), Controls.LabelFor(TEXT("idle.all")));
		Check(TEXT("controls.context_separates_defaults"), Controls.Match(FKey(FName(TEXT("Q"))), false, false, false, TEXT("idle." )).IsEmpty(),
			TEXT("build key not returned in idle context"), Controls.Match(FKey(FName(TEXT("Q"))), false, false, false, TEXT("idle.")));

		FString Notice;
		const bool bIdleRebind = Controls.Rebind(TEXT("idle.next"), FKey(FName(TEXT("Q"))), false, false, false, Notice);
		Check(TEXT("controls.cross_context_rebind"), bIdleRebind &&
			Controls.Match(FKey(FName(TEXT("Q"))), false, false, false, TEXT("idle.")) == TEXT("idle.next") &&
			Controls.Match(FKey(FName(TEXT("Q"))), false, false, false, TEXT("build.")) == TEXT("build.encampment_of_war"),
			TEXT("same chord resolves by context"), Notice);

		Controls.ResetDefaults();
		Notice.Empty();
		const bool bSwapped = Controls.Rebind(TEXT("build.fields_of_grain"), FKey(FName(TEXT("E"))), false, false, false, Notice);
		Check(TEXT("controls.conflict_swaps_chords"), bSwapped && Controls.LabelFor(TEXT("build.fields_of_grain")) == TEXT("E") &&
			Controls.LabelFor(TEXT("build.gathering_of_flocks")) == TEXT("W"),
			TEXT("fields=E and gathering=W"), Controls.LabelFor(TEXT("build.fields_of_grain")) + TEXT(" / ") + Controls.LabelFor(TEXT("build.gathering_of_flocks")));
		Check(TEXT("controls.notice_is_player_facing"), Notice == TEXT("Hotkey saved. Any conflicting shortcut was swapped.") &&
			!Notice.Contains(TEXT("build.")), TEXT("generic readable notice"), Notice);

		Controls.ResetDefaults();
		Controls.Clear(TEXT("build.encampment_of_war"));
		Notice.Empty();
		const bool bUnboundSwap = Controls.Rebind(TEXT("build.encampment_of_war"), FKey(FName(TEXT("W"))), false, false, false, Notice);
		Check(TEXT("controls.unbound_action_clears_displaced"), bUnboundSwap &&
			Controls.LabelFor(TEXT("build.encampment_of_war")) == TEXT("W") && Controls.KeyFor(TEXT("build.fields_of_grain")).GetFName().IsNone(),
			TEXT("encampment=W, fields unbound"), Controls.LabelFor(TEXT("build.encampment_of_war")) + TEXT(" / ") + Controls.LabelFor(TEXT("build.fields_of_grain")));

		Notice.Empty();
		Check(TEXT("controls.reject_backspace"), !Controls.Rebind(TEXT("build.encampment_of_war"), FKey(EKeys::BackSpace), false, false, false, Notice),
			TEXT("rejected"), Notice);
		Notice.Empty();
		Check(TEXT("controls.reject_idle_space"), !Controls.Rebind(TEXT("idle.next"), FKey(EKeys::SpaceBar), false, false, false, Notice),
			TEXT("rejected"), Notice);
		Notice.Empty();
		Check(TEXT("controls.reject_idle_number"), !Controls.Rebind(TEXT("idle.all"), FKey(EKeys::One), false, false, false, Notice),
			TEXT("rejected"), Notice);

		Notice.Empty();
        Check(TEXT("controls.reject_delete_reserved"), !Controls.Rebind(TEXT("build.encampment_of_war"), EKeys::Delete, false, false, false, Notice), TEXT("Delete reserved for hold-to-delete"), Notice);
        Notice.Empty();const bool MenuRebound=Controls.Rebind(TEXT("menu.build"),EKeys::B,false,false,false,Notice);
        Check(TEXT("controls.construction_menu_editable"),MenuRebound&&Controls.Match(EKeys::B,false,false,false,TEXT("menu."))==TEXT("menu.build"),TEXT("construction menu can be rebound to B"),Notice);
		Check(TEXT("controls.numpad_supported"), Controls.Rebind(TEXT("build.place_of_trade"), EKeys::NumPadOne, false, false, false, Notice), TEXT("numeric keypad accepted"), Notice);
		Controls.ResetDefaults();
		Notice.Empty();
		const bool bCustomSaved = Controls.Rebind(TEXT("build.place_of_trade"), FKey(FName(TEXT("Z"))), true, true, false, Notice);
		Controls.Clear(TEXT("build.storehouse_of_provisions"));
		Controls.Save();
		Controls.ResetDefaults();
		Controls.Load();
		Check(TEXT("controls.save_load_chord"), bCustomSaved && Controls.LabelFor(TEXT("build.place_of_trade")) == TEXT("Ctrl+Shift+Z"),
			TEXT("Ctrl+Shift+Z"), Controls.LabelFor(TEXT("build.place_of_trade")));
		Check(TEXT("controls.save_load_clear"), Controls.KeyFor(TEXT("build.storehouse_of_provisions")).GetFName().IsNone(),
			TEXT("unbound"), Controls.LabelFor(TEXT("build.storehouse_of_provisions")));

		Controls.ResetDefaults();
		Check(TEXT("controls.malformed_json_written"), WriteControlsText(ControlsPath, TEXT("{")), TEXT("test fixture written"), ControlsPath);
		Controls.Load();
		Check(TEXT("controls.malformed_json_uses_defaults"), Controls.LabelFor(TEXT("build.encampment_of_war")) == TEXT("Q") &&
			Controls.LabelFor(TEXT("idle.next")) == TEXT("."), TEXT("defaults retained"), Controls.LabelFor(TEXT("build.encampment_of_war")));

		const FString InvalidMappings = TEXT("{\"version\":1,\"bindings\":[")
			TEXT("{\"action\":\"build.encampment_of_war\",\"key\":\"W\"},")
			TEXT("{\"action\":\"build.fields_of_grain\",\"key\":\"Z\"},")
			TEXT("{\"action\":\"build.gathering_of_flocks\",\"key\":\"Z\"},")
			TEXT("{\"action\":\"build.tower_of_watchmen\",\"key\":\"LeftMouseButton\"},")
			TEXT("{\"action\":\"build.not_a_building\",\"key\":\"F24\"},")
			TEXT("{\"action\":\"idle.next\",\"key\":\"SpaceBar\"}]}");
		const bool bInvalidWritten = WriteControlsText(ControlsPath, InvalidMappings);
		Controls.ResetDefaults();
		Controls.Load();
		Check(TEXT("controls.invalid_mappings_fixture_written"), bInvalidWritten, TEXT("test fixture written"), ControlsPath);
		Check(TEXT("controls.unknown_and_unsafe_entries_ignored"),
			Controls.LabelFor(TEXT("build.encampment_of_war")) == TEXT("Q") &&
			Controls.LabelFor(TEXT("build.fields_of_grain")) == TEXT("W") &&
			Controls.LabelFor(TEXT("build.gathering_of_flocks")) == TEXT("E") &&
			Controls.LabelFor(TEXT("idle.next")) == TEXT("."),
			TEXT("safe defaults retained"), Controls.LabelFor(TEXT("build.encampment_of_war")) + TEXT(" / ") +
			Controls.LabelFor(TEXT("build.fields_of_grain")) + TEXT(" / ") + Controls.LabelFor(TEXT("build.gathering_of_flocks")) +
			TEXT(" / ") + Controls.LabelFor(TEXT("idle.next")));
		Check(TEXT("controls.chained_saved_conflicts_restore_unique_defaults"),
			Controls.Match(FKey(FName(TEXT("Q"))), false, false, false, TEXT("build.")) == TEXT("build.encampment_of_war") &&
			Controls.Match(FKey(FName(TEXT("W"))), false, false, false, TEXT("build.")) == TEXT("build.fields_of_grain") &&
			Controls.Match(FKey(FName(TEXT("E"))), false, false, false, TEXT("build.")) == TEXT("build.gathering_of_flocks"),
			TEXT("Q/W/E remain unique after chained collisions"),
			Controls.Match(FKey(FName(TEXT("Q"))), false, false, false, TEXT("build.")) + TEXT(" / ") +
			Controls.Match(FKey(FName(TEXT("W"))), false, false, false, TEXT("build.")) + TEXT(" / ") +
			Controls.Match(FKey(FName(TEXT("E"))), false, false, false, TEXT("build.")));
		Check(TEXT("controls.unknown_action_not_matchable"),
			Controls.Match(FKey(FName(TEXT("F24"))), false, false, false, TEXT("build." )).IsEmpty(),
			TEXT("no action"), Controls.Match(FKey(FName(TEXT("F24"))), false, false, false, TEXT("build.")));

		const bool bMemoryRestored = RestoreControls(Controls, OriginalState);
		Check(TEXT("controls.singleton_state_restored"), bMemoryRestored, TEXT("original in-memory bindings"),
			bMemoryRestored ? TEXT("restored") : TEXT("restore mismatch"));
		bool bFileRestored = true;
		if (bHadControlsFile)
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(ControlsPath), true);
			bFileRestored = FFileHelper::SaveArrayToFile(OriginalBytes, *ControlsPath);
			TArray<uint8> RestoredBytes;
			bFileRestored &= FFileHelper::LoadFileToArray(RestoredBytes, *ControlsPath) && RestoredBytes == OriginalBytes;
		}
		else if (IFileManager::Get().FileExists(*ControlsPath))
		{
			bFileRestored = IFileManager::Get().Delete(*ControlsPath, false, true);
		}
		Check(TEXT("controls.original_file_bytes_restored"), bFileRestored,
			bHadControlsFile ? FString::Printf(TEXT("exactly %d original bytes"), OriginalBytes.Num()) : TEXT("file remains absent"),
			bFileRestored ? TEXT("restored") : TEXT("restore failed"));
	}

	const FString WealthName = RTSNames::Get(TEXT("resource.wealth"), TEXT("Precious Things"));
	Check(TEXT("names.resource_wealth_default"), WealthName == TEXT("Precious Things"), TEXT("Precious Things"), WealthName);
	const FRTSCatalog& Catalog = FRTSCatalog::Get();
	Check(TEXT("names.resource_metadata_id_preserved"), Catalog.Def(TEXT("ore")).Id == TEXT("ore"), TEXT("ore"), Catalog.Def(TEXT("ore")).Id);
	Check(TEXT("names.unit_metadata_id_preserved"), Catalog.Def(TEXT("man_of_the_sword")).Id == TEXT("man_of_the_sword"),
		TEXT("man_of_the_sword"), Catalog.Def(TEXT("man_of_the_sword")).Id);
	Check(TEXT("names.faction_metadata_id_preserved"), Catalog.Faction(TEXT("nephites")).Id == TEXT("nephites"),
		TEXT("nephites"), Catalog.Faction(TEXT("nephites")).Id);

 const auto& Lamanites=Catalog.Faction(TEXT("lamanites"));
 Check(TEXT("v05.lamanite_temple_research_whitelist"),Lamanites.Upgrades.Contains(TEXT("order_of_the_nehors"))&&Lamanites.Upgrades.Contains(TEXT("by_the_pride_of_their_hearts")),TEXT("both temple research IDs in Lamanite civilization whitelist"),FString::Printf(TEXT("Order %d / Pride %d"),Lamanites.Upgrades.Contains(TEXT("order_of_the_nehors"))?1:0,Lamanites.Upgrades.Contains(TEXT("by_the_pride_of_their_hearts"))?1:0));
 const auto& Order=Catalog.Def(TEXT("order_of_the_nehors"));const auto& Pride=Catalog.Def(TEXT("by_the_pride_of_their_hearts"));
 Check(TEXT("v05.lamanite_temple_research_producers"),Order.Producer==TEXT("temple_of_the_land")&&Order.Age==2&&Pride.Producer==TEXT("temple_of_the_land")&&Pride.Age==3&&Pride.Requires.Contains(Order.Id),TEXT("Rameumptom Order Age II / Pride Age III after Order"),Order.Producer+TEXT(" / ")+Pride.Producer);
 const auto& Laborer=Catalog.Def(TEXT("industrious_laborer"));
 const FString Gentile=Catalog.DisplayName(Laborer,TEXT("laban")),Civilian=Catalog.DisplayName(Laborer,TEXT("gadianton"));
 Check(TEXT("v04.faction_laborer_names"),Gentile.Contains(TEXT("Gentile"))&&Civilian.Contains(TEXT("Conspiring")),TEXT("Laban Henpecked Gentile / Gadianton Conspiring Civilian"),Gentile+TEXT(" / ")+Civilian);
 const FString Crib=Catalog.DisplayName(Catalog.Def(TEXT("seat_of_the_people")),TEXT("laban"));
 Check(TEXT("v04.laban_headquarters_name"),Crib.Contains(TEXT("Crib")),TEXT("faction-specific Laban headquarters"),Crib);
 const FString Band=Catalog.DisplayName(Catalog.Def(TEXT("gadianton_robber")),TEXT("gadianton"));
 Check(TEXT("v04.band_of_kishkumen_name"),Band.Contains(TEXT("Kishkumen")),TEXT("Band of Kishkumen"),Band);
 const auto& Curelom=Catalog.Def(TEXT("curelom"));const auto& Cumelom=Catalog.Def(TEXT("cumelom"));const auto& Elephant=Catalog.Def(TEXT("elephant"));
 Check(TEXT("v04.independent_beast_progression"),Curelom.Age==1&&Cumelom.Age==3&&Elephant.Age==4&&Curelom.Producer==TEXT("seat_of_the_people")&&Cumelom.Producer==TEXT("gathering_of_flocks")&&Elephant.Producer==TEXT("gathering_of_flocks"),TEXT("Curelom HQ I, Cumelom Flocks III, Elephant Flocks IV"),FString::Printf(TEXT("ages %d/%d/%d"),Curelom.Age,Cumelom.Age,Elephant.Age));
 Check(TEXT("v07.band_age_two_storehouse_training"),Catalog.Def(TEXT("gadianton_robber")).Age==2&&Catalog.Def(TEXT("gadianton_robber")).Producer==TEXT("storehouse_of_provisions"),TEXT("Age II storehouse Band recruitment"),Catalog.Def(TEXT("gadianton_robber")).Producer);
 for(const auto& Faction:Catalog.Factions) {
  Check(TEXT("v04.judgment_removed.")+Faction.Id,!Faction.Buildings.Contains(TEXT("judgment_seat")),TEXT("Judgment Seat absent from civilization"),Faction.Id);
  bool FullProgression=true;
  for(int32 Age=1;Age<=3;Age++){int32 Found=0;for(const FString& Id:Faction.Upgrades){const auto& Def=Catalog.Def(Id);if(Def.Kind==TEXT("age_option")&&Def.Age==Age&&Def.Producer==TEXT("seat_of_the_people"))++Found;}FullProgression&=Found>=2;}
  Check(TEXT("v04.four_age_choice_progression.")+Faction.Id,FullProgression,TEXT("hero choices advance Ages I-II, II-III and III-IV"),Faction.Id);
  if(!RTS::IsJaredite(Faction.Id))Check(TEXT("v04.cement_is_jaredite_only.")+Faction.Id,!Faction.Buildings.Contains(TEXT("cement_wall"))&&!Faction.Buildings.Contains(TEXT("cement_tower")),TEXT("Cement defenses excluded"),Faction.Id);
 }
 bool MilitaryAgeLocks=true;for(const auto& Def:Catalog.Defs)if(RTS::IsMilitary(Def))MilitaryAgeLocks&=Def.Age>=2&&Def.Age<=4;
 Check(TEXT("v07.all_military_units_age_locked"),MilitaryAgeLocks,TEXT("all military unavailable in Age I and available by Age IV"),MilitaryAgeLocks?TEXT("valid"):TEXT("invalid military age"));
 bool ExpandedChoices=true;for(const TCHAR* FactionId:{TEXT("nephites"),TEXT("lamanites")}){bool Expanded=false;for(int32 Age=1;Age<=3;++Age){int32 Count=0;for(const auto& Id:Catalog.Faction(FactionId).Upgrades)if(Catalog.Def(Id).Kind==TEXT("age_option")&&Catalog.Def(Id).Age==Age)++Count;Expanded|=Count>2;}ExpandedChoices&=Expanded;}
 Check(TEXT("v07.larger_civilizations_have_expanded_hero_choices"),ExpandedChoices,TEXT("Nephites and Lamanites each have a tier with more than two choices"),ExpandedChoices?TEXT("expanded"):TEXT("missing choices"));
 Check(TEXT("v04.cement_defense_costs"),Catalog.Def(TEXT("cement_wall")).CementCost>0&&Catalog.Def(TEXT("cement_tower")).CementCost>0,TEXT("Cement defenses carry explicit resource costs"),FString::Printf(TEXT("wall %.0f tower %.0f"),Catalog.Def(TEXT("cement_wall")).CementCost,Catalog.Def(TEXT("cement_tower")).CementCost));
 Check(TEXT("v04.carrier_capacities"),RTS::IsCarrier(Catalog.Def(TEXT("kings_chariot")))&&RTS::CarrierCapacity(Catalog.Def(TEXT("kings_chariot")))==6&&RTS::IsCarrier(Elephant)&&RTS::CarrierCapacity(Elephant)==10,TEXT("chariot six population / elephant ten"),FString::Printf(TEXT("%d / %d"),RTS::CarrierCapacity(Catalog.Def(TEXT("kings_chariot"))),RTS::CarrierCapacity(Elephant)));

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("passed"), Passed);
	Root->SetNumberField(TEXT("failed"), Failed);
	Root->SetNumberField(TEXT("total"), Passed + Failed);
	Root->SetArrayField(TEXT("assertions"), Assertions);
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	const FString ResultsPath = FPaths::ProjectSavedDir() / TEXT("Tests/customization-tests.json");
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ResultsPath), true);
	const bool bReportWritten = FFileHelper::SaveStringToFile(Output, *ResultsPath);
	if (!bReportWritten) UE_LOG(LogTemp, Error, TEXT("Could not write customization test report to %s"), *ResultsPath);
	if (Failed == 0 && bReportWritten)
	{
		UE_LOG(LogTemp, Display, TEXT("RTS_CUSTOMIZATION_TESTS PASS passed=%d failed=%d report=%s"), Passed, Failed, *ResultsPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RTS_CUSTOMIZATION_TESTS FAIL passed=%d failed=%d report=%s"), Passed, Failed, *ResultsPath);
	}
	return Failed == 0 && bReportWritten;
}
