#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/** Local, persistent keyboard preferences. Bindings are keyed by stable action IDs, never display text. */
class CHRONICLES_API FRTSControls
{
public:
	static FRTSControls& Get();

	FKey KeyFor(const FString& ActionId) const;
	FString LabelFor(const FString& ActionId) const;
	FString Match(const FKey& Key, bool bCtrl, bool bShift, bool bAlt, const FString& Context = FString()) const;
	bool Rebind(const FString& ActionId, const FKey& Key, bool bCtrl, bool bShift, bool bAlt, FString& Notice);
	void Clear(const FString& ActionId);
	void ResetDefaults();
	void Save() const;
	void Load();

private:
	struct FBinding
	{
		FKey Key;
		bool bCtrl = false;
		bool bShift = false;
		bool bAlt = false;
	};

	FRTSControls();
	TMap<FString, FBinding> Bindings;
	static const TArray<FString>& ActionIds();
	static bool IsKnownAction(const FString& ActionId);
	static bool IsBuildAction(const FString& ActionId);
	static bool IsIdleAction(const FString& ActionId);
	static bool IsSafeKeyboardKey(const FKey& Key);
	static bool IsSafeBinding(const FString& ActionId, const FBinding& Binding, FString& Reason);
	static bool SameChord(const FBinding& A, const FBinding& B);
	static bool SameContext(const FString& A, const FString& B);
	static FString DisplayKey(const FKey& Key);
};
