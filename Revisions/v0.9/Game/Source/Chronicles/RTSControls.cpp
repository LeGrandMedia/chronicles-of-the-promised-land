#include "RTSControls.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* BuildingIds[] = {
		TEXT("encampment_of_war"), TEXT("fields_of_grain"), TEXT("gathering_of_flocks"),
		TEXT("hill_of_ore"), TEXT("house_of_curious_workmanship"),
		TEXT("place_of_many_waters"), TEXT("place_of_trade"), TEXT("rameumptom"),
		TEXT("seat_of_the_people"), TEXT("storehouse_of_provisions"), TEXT("temple_of_the_land"),
		TEXT("tents_and_habitations"), TEXT("tower_of_watchmen"), TEXT("works_of_defense"), TEXT("secret_combination_lodge"), TEXT("winery"), TEXT("cement_wall"), TEXT("cement_tower")
	};
	const TCHAR* BuildingKeys[] = {
		TEXT("Q"), TEXT("W"), TEXT("E"), TEXT("R"), TEXT("T"), TEXT("U"), TEXT("I"),
		TEXT("O"), TEXT("P"), TEXT("A"), TEXT("S"), TEXT("D"), TEXT("F"), TEXT("G"), TEXT("J"), TEXT("Z"), TEXT("K"), TEXT("L")
	};

	FString ControlsFilePath()
	{
		return FPaths::ProjectSavedDir() / TEXT("Config/RTSControls.json");
	}

	bool IsModifierName(const FString& Name)
	{
		static const TSet<FString> Names = {
			TEXT("LeftControl"), TEXT("RightControl"), TEXT("LeftShift"), TEXT("RightShift"),
			TEXT("LeftAlt"), TEXT("RightAlt"), TEXT("LeftCommand"), TEXT("RightCommand")
		};
		return Names.Contains(Name);
	}

	bool IsMouseName(const FString& Name)
	{
		return Name.Contains(TEXT("MouseButton")) || Name == TEXT("MouseScrollUp") || Name == TEXT("MouseScrollDown");
	}

	bool IsForbiddenKey(const FString& Name)
	{
	return Name == TEXT("Escape") || Name == TEXT("Enter") || Name == TEXT("Tab") || Name == TEXT("BackSpace") ||
		Name == TEXT("F1") || Name == TEXT("F10") || Name == TEXT("Delete") || IsModifierName(Name) || IsMouseName(Name);
	}

	bool IsIdleConflictKey(const FString& Name)
	{
		static const TSet<FString> Names = {
			TEXT("W"), TEXT("A"), TEXT("S"), TEXT("D"), TEXT("Up"), TEXT("Down"), TEXT("Left"), TEXT("Right"),
			TEXT("B"), TEXT("C"), TEXT("E"), TEXT("H"), TEXT("P"), TEXT("R"), TEXT("Y")
		};
		return Names.Contains(Name);
	}

	bool IsKeyboardName(const FString& Name)
	{
		if (Name.Len() == 1 && ((Name[0] >= TEXT('A') && Name[0] <= TEXT('Z')) || (Name[0] >= TEXT('0') && Name[0] <= TEXT('9'))))
			return true;
		if (Name.StartsWith(TEXT("F")) && Name.Len() >= 2)
		{
			const int32 Number = FCString::Atoi(*Name.RightChop(1));
			return Number >= 1 && Number <= 24 && Name == FString::Printf(TEXT("F%d"), Number);
		}
		if (Name.StartsWith(TEXT("NumPad")))
		{
			const FString Suffix = Name.RightChop(6);
			if (Suffix.Len() == 1 && Suffix[0] >= TEXT('0') && Suffix[0] <= TEXT('9')) return true;
			static const TSet<FString> NumpadOps = {TEXT("Add"), TEXT("Subtract"), TEXT("Multiply"), TEXT("Divide"), TEXT("Decimal"), TEXT("Enter"), TEXT("Zero"), TEXT("One"), TEXT("Two"), TEXT("Three"), TEXT("Four"), TEXT("Five"), TEXT("Six"), TEXT("Seven"), TEXT("Eight"), TEXT("Nine")};
			return NumpadOps.Contains(Suffix);
		}
		static const TSet<FString> Names = {
		TEXT("Zero"), TEXT("One"), TEXT("Two"), TEXT("Three"), TEXT("Four"), TEXT("Five"), TEXT("Six"), TEXT("Seven"), TEXT("Eight"), TEXT("Nine"),
			TEXT("SpaceBar"), TEXT("BackSpace"), TEXT("CapsLock"), TEXT("ScrollLock"), TEXT("Pause"), TEXT("PrintScreen"),
			TEXT("Insert"), TEXT("Delete"), TEXT("Home"), TEXT("End"), TEXT("PageUp"), TEXT("PageDown"),
			TEXT("Up"), TEXT("Down"), TEXT("Left"), TEXT("Right"),
			TEXT("Tilde"), TEXT("Hyphen"), TEXT("Equals"), TEXT("LeftBracket"), TEXT("RightBracket"),
			TEXT("Backslash"), TEXT("Semicolon"), TEXT("Apostrophe"), TEXT("Comma"), TEXT("Period"), TEXT("Slash")
		};
		return Names.Contains(Name);
	}
}

FRTSControls::FRTSControls()
{
	ResetDefaults();
	Load();
}

FRTSControls& FRTSControls::Get()
{
	static FRTSControls Controls;
	return Controls;
}

const TArray<FString>& FRTSControls::ActionIds()
{
	static TArray<FString> Ids = []()
	{
		TArray<FString> Result;
		for (const TCHAR* BuildingId : BuildingIds) Result.Add(FString(TEXT("build.")) + BuildingId);
		Result.Add(TEXT("idle.next"));
		Result.Add(TEXT("idle.all"));
        Result.Add(TEXT("menu.build"));
		return Result;
	}();
	return Ids;
}

bool FRTSControls::IsKnownAction(const FString& ActionId)
{
	return ActionIds().Contains(ActionId);
}

bool FRTSControls::IsBuildAction(const FString& ActionId)
{
	return ActionId.StartsWith(TEXT("build.")) && IsKnownAction(ActionId);
}

bool FRTSControls::IsIdleAction(const FString& ActionId)
{
	return ActionId == TEXT("idle.next") || ActionId == TEXT("idle.all");
}

bool FRTSControls::IsSafeKeyboardKey(const FKey& Key)
{
	const FString Name = Key.GetFName().ToString();
	return !Name.IsEmpty() && Name != TEXT("None") && IsKeyboardName(Name) && !IsForbiddenKey(Name);
}

bool FRTSControls::IsSafeBinding(const FString& ActionId, const FBinding& Binding, FString& Reason)
{
	if (!IsKnownAction(ActionId)) { Reason = TEXT("Unknown control action."); return false; }
	if (!IsSafeKeyboardKey(Binding.Key)) { Reason = TEXT("Choose an available keyboard key."); return false; }
	const FString Name = Binding.Key.GetFName().ToString();

	if (IsIdleAction(ActionId) && !Binding.bCtrl && !Binding.bShift && !Binding.bAlt && IsIdleConflictKey(Name))
	{
		Reason = TEXT("Idle-worker shortcuts cannot replace movement or unit-order keys."); return false;
	}
	if ((IsIdleAction(ActionId)||ActionId==TEXT("menu.build")) && !Binding.bCtrl && !Binding.bShift && !Binding.bAlt &&
		(Name == TEXT("SpaceBar") || (Name == TEXT("Zero") || Name == TEXT("One") || Name == TEXT("Two") ||
		 Name == TEXT("Three") || Name == TEXT("Four") || Name == TEXT("Five") || Name == TEXT("Six") ||
		 Name == TEXT("Seven") || Name == TEXT("Eight") || Name == TEXT("Nine"))))
	{
		Reason = TEXT("Space and number keys are reserved for headquarters and control groups."); return false;
	}
	return true;
}

bool FRTSControls::SameChord(const FBinding& A, const FBinding& B)
{
	return A.Key == B.Key && A.bCtrl == B.bCtrl && A.bShift == B.bShift && A.bAlt == B.bAlt;
}

bool FRTSControls::SameContext(const FString& A, const FString& B)
{
	return (IsBuildAction(A) && IsBuildAction(B)) || ((!IsBuildAction(A)) && (!IsBuildAction(B)));
}

FKey FRTSControls::KeyFor(const FString& ActionId) const
{
	if (const FBinding* Binding = Bindings.Find(ActionId)) return Binding->Key;
	return FKey();
}

FString FRTSControls::DisplayKey(const FKey& Key)
{
	const FString Name = Key.GetFName().ToString();
	if (Name == TEXT("Period")) return TEXT(".");
	if (Name == TEXT("Comma")) return TEXT(",");
	if (Name == TEXT("Slash")) return TEXT("/");
	if (Name == TEXT("Backslash")) return TEXT("\\");
	if (Name == TEXT("LeftBracket")) return TEXT("[");
	if (Name == TEXT("RightBracket")) return TEXT("]");
	if (Name == TEXT("Semicolon")) return TEXT(";");
	if (Name == TEXT("Apostrophe")) return TEXT("'");
	if (Name == TEXT("Hyphen")) return TEXT("-");
	if (Name == TEXT("Equals")) return TEXT("=");
	if (Name == TEXT("SpaceBar")) return TEXT("Space");
	static const TMap<FString, FString> Digits = {
		{TEXT("Zero"), TEXT("0")}, {TEXT("One"), TEXT("1")}, {TEXT("Two"), TEXT("2")}, {TEXT("Three"), TEXT("3")},
		{TEXT("Four"), TEXT("4")}, {TEXT("Five"), TEXT("5")}, {TEXT("Six"), TEXT("6")}, {TEXT("Seven"), TEXT("7")},
		{TEXT("Eight"), TEXT("8")}, {TEXT("Nine"), TEXT("9")}
	};
	if (const FString* Digit = Digits.Find(Name)) return *Digit;
	return Name;
}

FString FRTSControls::LabelFor(const FString& ActionId) const
{
	const FBinding* Binding = Bindings.Find(ActionId);
	if (!Binding) return FString();
	TArray<FString> Parts;
	if (Binding->bCtrl) Parts.Add(TEXT("Ctrl"));
	if (Binding->bShift) Parts.Add(TEXT("Shift"));
	if (Binding->bAlt) Parts.Add(TEXT("Alt"));
	Parts.Add(DisplayKey(Binding->Key));
	return FString::Join(Parts, TEXT("+"));
}

FString FRTSControls::Match(const FKey& Key, bool bCtrl, bool bShift, bool bAlt, const FString& Context) const
{
	FBinding Probe; Probe.Key = Key; Probe.bCtrl = bCtrl; Probe.bShift = bShift; Probe.bAlt = bAlt;
	for (const FString& Id : ActionIds())
	{
		if (!Context.IsEmpty() && !Id.StartsWith(Context)) continue;
		if (const FBinding* Binding = Bindings.Find(Id))
			if (SameChord(*Binding, Probe)) return Id;
	}
	return FString();
}

bool FRTSControls::Rebind(const FString& ActionId, const FKey& Key, bool bCtrl, bool bShift, bool bAlt, FString& Notice)
{
	FBinding NewBinding; NewBinding.Key = Key; NewBinding.bCtrl = bCtrl; NewBinding.bShift = bShift; NewBinding.bAlt = bAlt;
	if (!IsSafeBinding(ActionId, NewBinding, Notice)) return false;

	FString ConflictId;
	for (const FString& OtherId : ActionIds())
	{
		if (OtherId == ActionId || !SameContext(ActionId, OtherId)) continue;
		if (const FBinding* Other = Bindings.Find(OtherId))
			if (SameChord(*Other, NewBinding)) { ConflictId = OtherId; break; }
	}

	if (!ConflictId.IsEmpty())
	{
		if (const FBinding* Previous = Bindings.Find(ActionId)) Bindings.Add(ConflictId, *Previous);
		else Bindings.Remove(ConflictId);
	}
	Bindings.Add(ActionId, NewBinding);
	Notice = TEXT("Hotkey saved. Any conflicting shortcut was swapped.");
	return true;
}

void FRTSControls::Clear(const FString& ActionId)
{
	if (IsKnownAction(ActionId)) Bindings.Remove(ActionId);
}

void FRTSControls::ResetDefaults()
{
	Bindings.Empty();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BuildingIds); ++Index)
	{
		FBinding Binding; Binding.Key = FKey(FName(BuildingKeys[Index]));
		Bindings.Add(FString(TEXT("build.")) + BuildingIds[Index], Binding);
	}
	FBinding NextIdle; NextIdle.Key = FKey(EKeys::Period);
	Bindings.Add(TEXT("idle.next"), NextIdle);
	FBinding AllIdle; AllIdle.Key = FKey(EKeys::Period); AllIdle.bCtrl = true;
	Bindings.Add(TEXT("idle.all"), AllIdle);
    FBinding BuildMenu;BuildMenu.Key=EKeys::Q;Bindings.Add(TEXT("menu.build"),BuildMenu);
}

void FRTSControls::Save() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FString& Id : ActionIds())
	{
		const FBinding* Binding = Bindings.Find(Id);
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("action"), Id);
		Item->SetStringField(TEXT("key"), Binding ? Binding->Key.GetFName().ToString() : FString());
		Item->SetBoolField(TEXT("ctrl"), Binding && Binding->bCtrl);
		Item->SetBoolField(TEXT("shift"), Binding && Binding->bShift);
		Item->SetBoolField(TEXT("alt"), Binding && Binding->bAlt);
		Values.Add(MakeShared<FJsonValueObject>(Item.ToSharedPtr()));
	}
	Root->SetArrayField(TEXT("bindings"), Values);
	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);
	const FString Path = ControlsFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Text, *Path))
		UE_LOG(LogTemp, Warning, TEXT("Could not save local RTS controls to %s"), *Path);
}

void FRTSControls::Load()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *ControlsFilePath())) return;

	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Ignoring invalid local RTS controls JSON."));
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (!Root->TryGetArrayField(TEXT("bindings"), Values) || !Values) return;

	TMap<FString, FBinding> Candidate = Bindings;
	TSet<FString> SeenActions;
	for (const TSharedPtr<FJsonValue>& Value : *Values)
	{
		if (!Value.IsValid() || Value->Type != EJson::Object) continue;
		const TSharedPtr<FJsonObject> Item = Value->AsObject();
		FString Action, KeyName;
		if (!Item->TryGetStringField(TEXT("action"), Action) || !Item->TryGetStringField(TEXT("key"), KeyName) ||
			SeenActions.Contains(Action) || !IsKnownAction(Action)) continue;
		SeenActions.Add(Action);
		if (KeyName.IsEmpty()) { Candidate.Remove(Action); continue; }
		FBinding Binding; Binding.Key = FKey(FName(*KeyName));
		Item->TryGetBoolField(TEXT("ctrl"), Binding.bCtrl);
		Item->TryGetBoolField(TEXT("shift"), Binding.bShift);
		Item->TryGetBoolField(TEXT("alt"), Binding.bAlt);
		FString Reason;
		if (IsSafeBinding(Action, Binding, Reason)) Candidate.Add(Action, Binding);
	}

	// Restore conflicting actions to their pre-load bindings until the whole
	// set is unique. A single pass can create a new collision when a reverted
	// action falls back onto a chord occupied by a third saved action.
	for (int32 Pass = 0; Pass < ActionIds().Num(); ++Pass)
	{
		TSet<FString> Revert;
		for (int32 I = 0; I < ActionIds().Num(); ++I)
		{
			const FString& AId = ActionIds()[I];
			const FBinding* A = Candidate.Find(AId);
			if (!A) continue;
			for (int32 J = I + 1; J < ActionIds().Num(); ++J)
			{
				const FString& BId = ActionIds()[J];
				if (!SameContext(AId, BId)) continue;
				const FBinding* B = Candidate.Find(BId);
				if (B && SameChord(*A, *B)) { Revert.Add(AId); Revert.Add(BId); }
			}
		}
		if (Revert.IsEmpty()) break;
		for (const FString& Id : Revert)
		{
			if (const FBinding* Original = Bindings.Find(Id)) Candidate.Add(Id, *Original);
			else Candidate.Remove(Id);
			UE_LOG(LogTemp, Warning, TEXT("Ignoring conflicting saved control binding for %s"), *Id);
		}
	}
	Bindings = MoveTemp(Candidate);
}
