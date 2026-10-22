#include "RTSNames.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	TMap<FString, FString> GDisplayNames;
	TMap<FString, FString> GOriginalNames;
	struct FNameReplacement { FString Original, Display; };
	TArray<FNameReplacement> GReplacements;
	TMap<FString, FString> GResolvedTextCache;
	bool bNamesLoaded = false;
	bool bReplacementsDirty = true;

	void RebuildReplacements()
	{
		if (!bReplacementsDirty) return;
		bReplacementsDirty = false;
		GReplacements.Reset();
		GResolvedTextCache.Reset();
		TArray<FString> Keys;
		GOriginalNames.GetKeys(Keys);
		Keys.Sort(); // Stable resolution when two content IDs share a phrase.
		TMap<FString, FString> ByPhrase;
		for (const FString& Id : Keys)
		{
			const FString& Original = GOriginalNames[Id];
			if (Original.IsEmpty()) continue;
			const FString Display = RTSNames::Get(Id, Original);
			const FString Folded = Original.ToLower();
			if (!ByPhrase.Contains(Folded) || Display != Original) ByPhrase.Add(Folded, Display);
		}
		for (const auto& Pair : ByPhrase)
		{
			FNameReplacement Item;
			Item.Original = Pair.Key;
			Item.Display = Pair.Value;
			GReplacements.Add(MoveTemp(Item));
		}
		GReplacements.Sort([](const FNameReplacement& A, const FNameReplacement& B)
		{
			if (A.Original.Len() != B.Original.Len()) return A.Original.Len() > B.Original.Len();
			return A.Original < B.Original;
		});
	}
}

void RTSNames::Load()
{
	if (bNamesLoaded) return;
	bNamesLoaded = true;
	auto ReadNames = [](const FString& Path, bool bRequired, bool bReadOriginals)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			if (bRequired) UE_LOG(LogTemp, Warning, TEXT("RTS names: optional override file unavailable: %s"), *Path);
			return;
		}
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("RTS names: invalid JSON in %s"), *Path);
			return;
		}
		const TSharedPtr<FJsonObject>* Names = nullptr;
		if (!Root->TryGetObjectField(TEXT("names"), Names) || !Names || !Names->IsValid()) return;
		for (const auto& Pair : (*Names)->Values)
		{
			FString Value;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value) && !Pair.Key.IsEmpty() && !Value.IsEmpty())
			{
				GDisplayNames.Add(Pair.Key, Value);
			}
		}
		if (bReadOriginals)
		{
			const TSharedPtr<FJsonObject>* Originals = nullptr;
			if (Root->TryGetObjectField(TEXT("originals"), Originals) && Originals && Originals->IsValid())
			{
				for (const auto& Pair : (*Originals)->Values)
				{
					FString Value;
					if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value) && !Pair.Key.IsEmpty() && !Value.IsEmpty())
						GOriginalNames.Add(Pair.Key, Value);
				}
			}
		}
	};
	ReadNames(FPaths::ProjectContentDir() / TEXT("Data/names.json"), true, true);
	// A loose file in Saved wins over cooked defaults, so deployment-specific
	// renames do not require modifying the pak or rebuilding the executable.
	ReadNames(FPaths::ProjectSavedDir() / TEXT("names.json"), false, false);
	bReplacementsDirty = true;
}

FString RTSNames::Get(const FString& Id, const FString& Fallback)
{
	Load();
	if (const FString* Value = GDisplayNames.Find(Id)) return *Value;
	return Fallback;
}

FString RTSNames::Get(const TCHAR* Id, const FString& Fallback)
{
	return Get(FString(Id), Fallback);
}

void RTSNames::RegisterOriginal(const FString& Id, const FString& Original)
{
	Load();
	if (!Id.IsEmpty() && !Original.IsEmpty())
	{
		GOriginalNames.Add(Id, Original);
		bReplacementsDirty = true;
	}
}

FString RTSNames::ResolveText(const FString& Text)
{
	Load();
	if (Text.IsEmpty()) return Text;
	RebuildReplacements();
	if (const FString* Cached = GResolvedTextCache.Find(Text)) return *Cached;
	FString Result;
	Result.Reserve(Text.Len() + 16);
	int32 At = 0;
	while (At < Text.Len())
	{
		const FNameReplacement* Match = nullptr;
		for (const FNameReplacement& Candidate : GReplacements)
		{
			const int32 End = At + Candidate.Original.Len();
			if (End > Text.Len() || !Text.Mid(At, Candidate.Original.Len()).Equals(Candidate.Original, ESearchCase::IgnoreCase)) continue;
			const bool LeftBoundary = At == 0 || !FChar::IsAlnum(Text[At - 1]);
			const bool RightBoundary = End == Text.Len() || !FChar::IsAlnum(Text[End]);
			if (LeftBoundary && RightBoundary) { Match = &Candidate; break; }
		}
		if (Match)
		{
			Result += Match->Display;
			At += Match->Original.Len();
		}
		else Result.AppendChar(Text[At++]);
	}
	if (GResolvedTextCache.Num() >= 512) GResolvedTextCache.Reset();
	GResolvedTextCache.Add(Text, Result);
	return Result;
}
