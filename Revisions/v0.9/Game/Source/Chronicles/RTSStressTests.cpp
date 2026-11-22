#include "RTSWorld.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void ARTSWorld::RunStressTests()
{
	Entities.Empty(); Players.Empty(); NextId = 1; Map = 0; Time = 0; Phase = 1; Winner = -1;
	bSandbox = true; bTesting = true;

	const FString Factions[] = { TEXT("nephites"), TEXT("lamanites"), TEXT("coriantumr"), TEXT("shiz") };
	const FVector2D Bases[] = { FVector2D(20, 20), FVector2D(140, 140), FVector2D(20, 140), FVector2D(140, 20) };
	const FVector2D ArmyOrigins[] = { FVector2D(54, 64), FVector2D(90, 64), FVector2D(54, 86), FVector2D(90, 86) };
	const FVector2D RallyPoints[] = { FVector2D(101, 69.175f), FVector2D(65, 69.175f), FVector2D(101, 91.175f), FVector2D(65, 91.175f) };
	TArray<TArray<int32>> InitialIds;
	InitialIds.SetNum(4);
	bool bSetupValid = true;
	const FRTSDef& TentDef = FRTSCatalog::Get().Def(TEXT("tents_and_habitations"));

	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		FRTSPlayer Player;
		Player.Slot = Slot; Player.Team = Slot % 2; Player.Faction = Factions[Slot];
		Player.bReady = true; Player.bConnected = true; Player.Explored.Init(0, RTS::FogSize * RTS::FogSize);
		Players.Add(Player);

		FRTSEntity* Seat = Spawn(TEXT("seat_of_the_people"), Slot, Bases[Slot], RTS::IsJaredite(Factions[Slot]) ? 1 : 0);
		bSetupValid &= Seat != nullptr;
		// The Seat supplies 15 capacity; nineteen standard Tents add 190,
		// yielding an exact 200 cap after the normal population pass.
		int32 Tents = 0;
		const float SX = Bases[Slot].X < RTS::MapSize * .5f ? 1.f : -1.f;
		const float SY = Bases[Slot].Y < RTS::MapSize * .5f ? 1.f : -1.f;
		for (int32 Row = 0; Row < 5 && Tents < 19; ++Row)		for (int32 Col = 0; Col < 5 && Tents < 19; ++Col)
		{
			const FVector2D At = Bases[Slot] + FVector2D(SX * (6.f + Col * 5.5f), SY * (6.f + Row * 5.5f));
			if (!Walkable(At, TentDef.Radius, 0, false)) continue;
			if (!Spawn(TEXT("tents_and_habitations"), Slot, At)) { bSetupValid = false; continue; }
			++Tents;
		}
		bSetupValid &= Tents == 19;

		// Two hundred canonical pop-1 melee infantry reach the 200-pop cap.
		for (int32 I = 0; I < 200; ++I)
		{
			const int32 Row = I / 20, Col = I % 20;
			const FVector2D At = ArmyOrigins[Slot] + FVector2D(Col * 1.15f, Row * 1.15f);
			FRTSEntity* Unit = Spawn(TEXT("man_of_the_sword"), Slot, At);
			if (!Unit || D(*Unit).Pop != 1 || D(*Unit).Attack <= 0) { bSetupValid = false; continue; }
			Unit->Order = TEXT("attack_move");
			Unit->Rally = RallyPoints[Slot];
			InitialIds[Slot].Add(Unit->Id);
		}
	}
	Population();
	for (const FRTSPlayer& Player : Players) bSetupValid &= Player.Population == 200 && Player.Capacity == 200;

	TArray<double> StepMilliseconds;
	StepMilliseconds.Reserve(100);
	const double Started = FPlatformTime::Seconds();
	for (int32 I = 0; I < 100; ++I)
	{
		const double Before = FPlatformTime::Seconds();
		Step(.1f);
		StepMilliseconds.Add((FPlatformTime::Seconds() - Before) * 1000.0);
	}
	const double WallSeconds = FPlatformTime::Seconds() - Started;
	StepMilliseconds.Sort();
	const double MedianMs = StepMilliseconds.Num() == 100 ? (StepMilliseconds[49] + StepMilliseconds[50]) * .5 : 0;
	const double MaximumMs = StepMilliseconds.Num() ? StepMilliseconds.Last() : 0;

	TArray<TSharedPtr<FJsonValue>> PlayerReports;
	int32 TotalMoved = 0, TotalLiveCombat = 0;
	bool bInvariantValid = bSetupValid && StepMilliseconds.Num() == 100 && FMath::IsNearlyEqual(Time, 10.f, .02f);
	for (int32 Slot = 0; Slot < 4; ++Slot)
	{
		int32 Moved = 0, LiveCombat = 0;
		for (int32 I = 0; I < InitialIds[Slot].Num(); ++I)
		{
			if (FRTSEntity* Unit = Find(InitialIds[Slot][I]))
			{
				++LiveCombat;
				const FVector2D Start = ArmyOrigins[Slot] + FVector2D((I % 20) * 1.15f, (I / 20) * 1.15f);
				if (FVector2D::Distance(Unit->Pos, Start) > .25f) ++Moved;
				if (!FMath::IsFinite(Unit->HP) || !FMath::IsFinite(Unit->Pos.X) || !FMath::IsFinite(Unit->Pos.Y) || Unit->HP <= 0 || Unit->Owner != Slot) bInvariantValid = false;
			}
		}
		TotalMoved += Moved; TotalLiveCombat += LiveCombat;
		bInvariantValid &= LiveCombat <= 200 && Players.IsValidIndex(Slot) && Players[Slot].Population >= 0 && Players[Slot].Capacity == 200;
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("slot"), Slot); Item->SetNumberField(TEXT("team"), Players[Slot].Team); Item->SetStringField(TEXT("faction"), Players[Slot].Faction);
		Item->SetNumberField(TEXT("initial_population"), 200); Item->SetNumberField(TEXT("capacity"), Players[Slot].Capacity); Item->SetNumberField(TEXT("final_population"), Players[Slot].Population);
		Item->SetNumberField(TEXT("initial_combat_units"), InitialIds[Slot].Num()); Item->SetNumberField(TEXT("live_combat_units"), LiveCombat); Item->SetNumberField(TEXT("moved_combat_units"), Moved);
		PlayerReports.Add(MakeShared<FJsonValueObject>(Item));
	}
	bInvariantValid &= TotalLiveCombat <= 800 && TotalMoved > 0 && FMath::IsFinite(float(MedianMs)) && FMath::IsFinite(float(MaximumMs));

	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("passed"), bInvariantValid);
	Report->SetStringField(TEXT("test"), TEXT("four_player_200_population_combat_10s"));
	Report->SetNumberField(TEXT("simulated_seconds"), Time); Report->SetNumberField(TEXT("step_count"), StepMilliseconds.Num());
	Report->SetNumberField(TEXT("step_wall_ms_median"), MedianMs); Report->SetNumberField(TEXT("step_wall_ms_max"), MaximumMs); Report->SetNumberField(TEXT("wall_seconds_total"), WallSeconds);
	Report->SetNumberField(TEXT("moved_combat_units_total"), TotalMoved); Report->SetNumberField(TEXT("live_combat_units_total"), TotalLiveCombat); Report->SetArrayField(TEXT("players"), PlayerReports);
	FString Json; const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Tests"); IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = Directory / TEXT("stress-tests.json");
	if (!FFileHelper::SaveStringToFile(Json, *Path)) bInvariantValid = false;
	if (bInvariantValid) { UE_LOG(LogTemp, Display, TEXT("RTS_STRESS_TEST passed players=4 pop=200 each seconds=%.1f median_ms=%.3f max_ms=%.3f moved=%d live=%d report=%s"), Time, MedianMs, MaximumMs, TotalMoved, TotalLiveCombat, *Path); }
	else { UE_LOG(LogTemp, Error, TEXT("RTS_STRESS_TEST failed setup_or_simulation_invariant report=%s"), *Path); FPlatformMisc::RequestExitWithStatus(true, 1); }
}
