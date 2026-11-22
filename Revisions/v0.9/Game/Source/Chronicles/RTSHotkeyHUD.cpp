#include "RTSHUD.h"
#include "RTSController.h"
#include "RTSControls.h"
#include "RTSNames.h"

void ARTSHUD::DrawHotkeys()
{
	Buttons.Empty();
	HoverTip.Empty();
	Rect(0, 0, W, H, FLinearColor(0.008f, 0.018f, 0.02f, 0.82f));

	const float PW = 1100.f, PH = 710.f;
	const float X = (W - PW) * .5f, Y = (H - PH) * .5f;
	const FLinearColor Paper(.89f, .89f, .79f, 1.f);
	const FLinearColor Gold(.94f, .79f, .46f, 1.f);
	const FLinearColor Muted(.72f, .75f, .67f, 1.f);
	const FLinearColor KeyBackground(.045f, .045f, .036f, 1.f);
	const float RowText = HUDTextSize(16, 11.5f), KeyText = HUDTextSize(15, 11.5f);
	Panel(X, Y, PW, PH, true);
	Text(TEXT("Command shortcuts"), X + 40, Y + 24, HUDTextSize(27, 16), Paper);
	Text(TEXT("Select an action and press a key. Conflicting bindings swap automatically."), X + 41, Y + 63, HUDTextSize(15, 11.5f), Muted);
	Text(TEXT("Structure shortcuts work while the construction menu is open."), X + 41, Y + 88, HUDTextSize(15, 11.5f), Muted);

	TArray<const FRTSDef*> Buildings;
	const auto& Catalog=FRTSCatalog::Get();const auto* Player=PC->Me();
 const FString Faction=Player?Player->Faction:Catalog.Factions.IsValidIndex(PC->LobbyFaction)?Catalog.Factions[PC->LobbyFaction].Id:FString();
 for (const FRTSDef& Def : Catalog.Defs) if (Def.bBuilding&&Def.Id!=TEXT("judgment_seat")&&Catalog.Faction(Faction).Buildings.Contains(Def.Id)) Buildings.Add(&Def);
	Buildings.Sort([this](const FRTSDef& A, const FRTSDef& B)
	{
		const int32 NameOrder = Name(A).Compare(Name(B), ESearchCase::IgnoreCase);
		return NameOrder == 0 ? A.Id < B.Id : NameOrder < 0;
	});

	auto DrawBindingRow = [&](float BX, float BY, float BW, float BH, const FString& Label,
		const FString& ActionId, const FString& Tip)
	{
		const bool bCapturing = PC->HotkeyEditId == ActionId;
		Button(BX, BY, BW, BH, FString(), TEXT("bind_key"), ActionId, true, Tip, bCapturing);
		FString Assigned = FRTSControls::Get().LabelFor(ActionId);
		if (Assigned.EndsWith(TEXT("."))) Assigned = Assigned.LeftChop(1) + TEXT("Period");
		const FString Binding = bCapturing ? TEXT("Press a key…") : Assigned.IsEmpty() ? TEXT("Unbound") : Assigned;
		const float BindingWidth = FMath::Clamp(TextWidth(Binding, KeyText) + 20, 46.f, BW * .46f);
		const float BindingX = BX + BW - 9 - BindingWidth;
		Rect(BindingX, BY + 4, BindingWidth, BH - 8, KeyBackground);
		FitText(Label, BX + 12, BY + (BH - RowText) * .5f, BindingX - BX - 24, RowText, Paper);
		FitText(Binding, BindingX + 5, BY + (BH - KeyText) * .5f, BindingWidth - 10, KeyText, Gold, true);
	};

	const float ColW = 502.f, ColGap = 20.f, ColX[2] = { X + 38.f, X + 38.f + ColW + ColGap };
	const float RowY = Y + 130.f, RowH = 36.f, RowGap = 6.f;
	for (int32 I = 0; I < Buildings.Num(); ++I)
	{
		const int32 Column = I < 8 ? 0 : 1;
		const int32 Row = I < 8 ? I : I - 8;
		const FRTSDef& Def = *Buildings[I];
		const FString ActionId = TEXT("build.") + Def.Id;
		const FString DisplayName = Name(Def);
		DrawBindingRow(ColX[Column], RowY + Row * (RowH + RowGap), ColW, RowH, DisplayName, ActionId,
			FString(TEXT("Construction shortcut for ")) + DisplayName + TEXT(". Shortcuts apply only while the build menu is open."));
	}

	Line(FVector2D(X + 38, Y + 480), FVector2D(X + PW - 38, Y + 480), FLinearColor(.20f, .30f, .26f, 1.f));
	DrawBindingRow(X + 38, Y + 495, PW - 76, 38, TEXT("Open construction menu"), TEXT("menu.build"), TEXT("Default Q. When construction is already open, Q selects its first build card. Escape closes it."));
	DrawBindingRow(X + 38, Y + 540, PW - 76, 38, TEXT("Select next idle worker"), TEXT("idle.next"), TEXT("Select the next idle villager. This shortcut is available outside the build menu."));
	DrawBindingRow(X + 38, Y + 585, PW - 76, 38, TEXT("Select all idle workers"), TEXT("idle.all"), TEXT("Select every idle villager. This shortcut is available outside the build menu."));
	Button(X + 38, Y + 650, 220, 38, TEXT("Reset defaults"), TEXT("reset_keys"), FString(), true, TEXT("Restore the default key assignments."));
	Button(X + PW - 258, Y + 650, 220, 38, TEXT("Done"), TEXT("hotkeys"), FString(), true, TEXT("Close shortcut settings."), true);
	if (!PC->HotkeyEditId.IsEmpty()) FitText(TEXT("Esc cancels   ·   Backspace clears the binding"), X + 270, Y + 660, PW - 540, KeyText, Paper, true);
}
