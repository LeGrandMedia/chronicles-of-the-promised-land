#pragma once

#include "CoreMinimal.h"

// User-facing text overrides keyed by stable content IDs. Gameplay rules must
// continue to use definition/faction/resource IDs, never these display strings.
namespace RTSNames
{
	// Reads Content/Data/names.json once, on first lookup. Missing keys return
	// the supplied fallback so a partial override file is safe.
	void Load();
	FString Get(const FString& Id, const FString& Fallback = FString());
	FString Get(const TCHAR* Id, const FString& Fallback = FString());
	// Adds the canonical, untranslated phrase used by existing help/notices.
	// Call before applying an override when a label comes from the catalog.
	void RegisterOriginal(const FString& Id, const FString& Original);
	// Resolves registered canonical phrases in display text with one
	// non-cascading, whole-word pass. Do not call on IDs or rule expressions.
	FString ResolveText(const FString& Text);
}
