#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool ARTSWorld::RunScenarioTests()
{
	TArray<TSharedPtr<FJsonValue>> Assertions;
	int32 Passed = 0, Failed = 0;
	auto Check = [&](const FString& Name, bool bCondition, const FString& Expected, const FString& Actual)
	{
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Name);
		Item->SetBoolField(TEXT("passed"), bCondition);
		Item->SetStringField(TEXT("expected"), Expected);
		Item->SetStringField(TEXT("actual"), Actual);
		Assertions.Add(MakeShared<FJsonValueObject>(Item));
		if (bCondition) ++Passed;
		else { ++Failed; UE_LOG(LogTemp, Error, TEXT("RTS_SCENARIO_TEST_FAIL %s expected=%s actual=%s"), *Name, *Expected, *Actual); }
	};
	auto Near = [](float A, float B, float Tolerance = .02f) { return FMath::Abs(A - B) <= Tolerance; };
	auto Reset = [&]()
	{
		Entities.Empty(); Players.Empty(); NextId = 1; Map = 0; Time = 0; Phase = 1; Winner = -1; bSandbox = true;
		NavigationCount=-1;NavigationTime=-100;ObstacleCount=-1;Obstacles.Empty();bDispatchingQueuedOrder=false;
	};
	ARTSController* C = GetWorld() ? GetWorld()->SpawnActor<ARTSController>() : nullptr;
	if (C) C->ServerSlot = 0;
	auto AddPlayer = [&](const FString& Faction, int32 Team = 0)
	{
		FRTSPlayer P; P.Slot = Players.Num(); P.Team = Team; P.Faction = Faction;
		P.Controller = C; P.bReady = true; P.Explored.Init(0, RTS::FogSize * RTS::FogSize); Players.Add(P);
	};
	auto Run = [&](float Seconds)
	{
		const int32 N = FMath::CeilToInt(Seconds / .1f);
		for (int32 I = 0; I < N; ++I) Step(.1f);
	};
	// Resource harvesting must move to the node, fill a real cargo load, and
	// deliver it to an owned drop-off without requiring a scripted resource grant.
	Reset(); AddPlayer(TEXT("nephites"));
	FRTSEntity* HQ = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20));
	FRTSEntity* Worker = Spawn(TEXT("industrious_laborer"), 0, FVector2D(24, 20));
	FRTSEntity* Berries = Spawn(TEXT("berries"), -1, FVector2D(31, 20));
	const FVector2D WorkerStart = Worker ? Worker->Pos : FVector2D::ZeroVector;
	if (Berries) Berries->ResourceRemaining = Berries->HP = 100;
	if (C && Worker && Berries) Command(C, TEXT("gather"), { Worker->Id }, Berries->Pos, Berries->Id, TEXT(""));
	Run(50);
	Check(TEXT("economy.gather_deliver_moves_and_deposits"), HQ && Worker && Players.Num() && Players[0].Resources.X > 300 && FVector2D::Distance(Worker->Pos, WorkerStart) > 1,
		TEXT("food delivered and worker traveled from its starting point"),
		FString::Printf(TEXT("food=%.2f cargo=%.2f pos=(%.1f,%.1f)"), Players.Num() ? Players[0].Resources.X : 0.f, Worker ? Worker->Cargo.X : -1.f, Worker ? Worker->Pos.X : -1.f, Worker ? Worker->Pos.Y : -1.f));

	// A command-created farm must remain a foundation until its worker reaches
	// it, then complete through elapsed simulation time.
	Reset(); AddPlayer(TEXT("nephites"));
	HQ = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20));
	Worker = Spawn(TEXT("industrious_laborer"), 0, FVector2D(35.5f, 20));
	if (C) Command(C, TEXT("build"), { Worker ? Worker->Id : 0 }, FVector2D(30, 20), 0, TEXT("fields_of_grain"));
	FRTSEntity* Farm = nullptr;
	for (auto& KV : Entities) if (D(*KV.Value).Id == TEXT("fields_of_grain") && KV.Value->Owner == 0) Farm = KV.Value.Get();
	const bool bFarmFoundation = Farm && Farm->Progress < 1;
	Run(75);
	Check(TEXT("construction.farm_builds_over_elapsed_time"), bFarmFoundation && Farm && Farm->Progress >= 1, TEXT("foundation progresses to complete"),
		FString::Printf(TEXT("foundation=%s progress=%.3f"), bFarmFoundation ? TEXT("yes") : TEXT("no"), Farm ? Farm->Progress : 0.f));

	// Queue prices are faction-adjusted while cancellation restores the exact
	// payment and releases reserved population.
	Reset(); AddPlayer(TEXT("lamanites"));
	Players[0].Age=2;
	HQ = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20));
	FRTSEntity* Encampment = Spawn(TEXT("encampment_of_war"), 0, FVector2D(27, 20));
	Players[0].Resources = FVector(100, 100, 100); Population();
	const FVector QueueBefore = Players[0].Resources;
	if (C && Encampment) Command(C, TEXT("train"), { Encampment->Id }, FVector2D::ZeroVector, 0, TEXT("cimeter_bearer"));
	const bool bFirstQueued = Encampment && Encampment->Jobs.Num() == 1 && Encampment->Jobs[0].Type == TEXT("cimeter_bearer");
	if (C && Encampment) Command(C, TEXT("train"), { Encampment->Id }, FVector2D::ZeroVector, 0, TEXT("cimeter_bearer"));
	const FVector Paid = QueueBefore - Players[0].Resources;
	const int32 ReservedBeforeCancel = Players[0].Population;
	const bool bTwoQueued = Encampment && Encampment->Jobs.Num() == 2;
	if (C && Encampment) Command(C, TEXT("cancel"), { Encampment->Id }, FVector2D::ZeroVector, 0, TEXT("last"));
	const FVector ExpectedAfterCancel = QueueBefore - FVector(33, 0, 17);
	Check(TEXT("production.modified_queue_price_and_refund"), bFirstQueued && bTwoQueued && Paid == FVector(66, 0, 34) && Encampment->Jobs.Num() == 1 && Encampment->Jobs[0].Type == TEXT("cimeter_bearer") && Players[0].Resources == ExpectedAfterCancel && ReservedBeforeCancel == 2 && Players[0].Population == 1,
		TEXT("two Lamanite Cimeter Bearers cost (66,0,34); cancelling queued last fully refunds it and retains first job/population"),
		FString::Printf(TEXT("two_queued=%s paid=%s jobs=%d reserved=%d resources=%s population=%d"), bTwoQueued ? TEXT("yes") : TEXT("no"), *Paid.ToString(), Encampment ? Encampment->Jobs.Num() : -1, ReservedBeforeCancel, *Players[0].Resources.ToString(), Players[0].Population));

	// Server-slot ownership is authoritative: selecting another player's worker
	// cannot move it, and faction-exclusive training cannot spend resources.
	Reset(); AddPlayer(TEXT("nephites"), 0); AddPlayer(TEXT("lamanites"), 1);
	FRTSEntity* EnemyWorker = Spawn(TEXT("industrious_laborer"), 1, FVector2D(40, 40));
	FVector2D EnemyStart = EnemyWorker ? EnemyWorker->Pos : FVector2D::ZeroVector;
	if (C && EnemyWorker) Command(C, TEXT("move"), { EnemyWorker->Id }, FVector2D(60, 60), 0, TEXT(""));
	Check(TEXT("authority.foreign_entity_order_rejected"), EnemyWorker && EnemyWorker->Pos == EnemyStart && EnemyWorker->Order == TEXT("idle"), TEXT("position and order unchanged"), EnemyWorker ? EnemyWorker->Order + TEXT(" at ") + EnemyWorker->Pos.ToString() : TEXT("missing"));
	HQ = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20));
	Encampment = Spawn(TEXT("encampment_of_war"), 0, FVector2D(27, 20));
	Players[0].Resources = FVector(300, 300, 200); const FVector ForbiddenBefore = Players[0].Resources;
	if (C && Encampment) Command(C, TEXT("train"), { Encampment->Id }, FVector2D::ZeroVector, 0, TEXT("gadianton_robber"));
	Check(TEXT("faction.forbidden_unit_rejected_without_payment"), Encampment && Encampment->Jobs.Num() == 0 && Players[0].Resources == ForbiddenBefore, TEXT("Nephites cannot queue Gadianton Robber from its actual producer; no payment"), FString::Printf(TEXT("jobs=%d resources=%s"), Encampment ? Encampment->Jobs.Num() : -1, *Players[0].Resources.ToString()));

	// Laban's elite guard limit applies across both living units and reserved queue orders.
	Reset(); AddPlayer(TEXT("laban"));
	HQ = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20));
	Encampment = Spawn(TEXT("encampment_of_war"), 0, FVector2D(27, 20));
	Players[0].Age = 4; Players[0].Upgrades.Add(TEXT("laban_and_his_fifty"));
	Players[0].Resources = FVector(10000, 1000, 10000);
	for (int32 I = 0; I < 19; ++I) Spawn(TEXT("tents_and_habitations"), 0, FVector2D(26 + (I % 5) * 5.5f, 26 + (I / 5) * 5.5f));
	int32 LivingFifty = 0;
	for (int32 I = 0; I < 49; ++I) if (Spawn(TEXT("one_of_the_fifty"), 0, FVector2D(62 + (I % 10), 48 + (I / 10)))) ++LivingFifty;
	Population();
	if (C && Encampment) Command(C, TEXT("train"), { Encampment->Id }, FVector2D::ZeroVector, 0, TEXT("one_of_the_fifty"));
	const FVector BeforeRejectedFifty = Players[0].Resources;
	const bool bFiftyQueued = Encampment && Encampment->Jobs.Num() == 1 && Encampment->Jobs[0].Type == TEXT("one_of_the_fifty");
	if (C && Encampment) Command(C, TEXT("train"), { Encampment->Id }, FVector2D::ZeroVector, 0, TEXT("one_of_the_fifty"));
	Check(TEXT("faction.laban_fifty_cap_counts_alive_and_queued"), LivingFifty == 49 && bFiftyQueued && Encampment->Jobs.Num() == 1 && Players[0].Resources == BeforeRejectedFifty,
		TEXT("49 living plus one queued reaches 50; the next queue is denied without payment"),
		FString::Printf(TEXT("living=%d queued=%d jobs=%d resources=%s"), LivingFifty, bFiftyQueued ? 1 : 0, Encampment ? Encampment->Jobs.Num() : -1, *Players[0].Resources.ToString()));

	// Humans cannot construct for a Jaredite settlement; its authorized beast can.
	Reset(); AddPlayer(TEXT("coriantumr"));
	HQ = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20), 1);
	Worker = Spawn(TEXT("industrious_laborer"), 0, FVector2D(24, 20));
	FRTSEntity* Beast = Spawn(TEXT("cumelom"), 0, FVector2D(25, 20), 1);
	if (C && Worker) Command(C, TEXT("build"), { Worker->Id }, FVector2D(28, 20), 0, TEXT("fields_of_grain"));
	int32 AfterHuman = 0; for (auto& KV : Entities) if (KV.Value->Owner == 0 && D(*KV.Value).Id == TEXT("fields_of_grain")) ++AfterHuman;
	if (C && Beast) Command(C, TEXT("build"), { Beast->Id }, FVector2D(28, 20), 0, TEXT("fields_of_grain"));
	int32 AfterBeast = 0; for (auto& KV : Entities) if (KV.Value->Owner == 0 && D(*KV.Value).Id == TEXT("fields_of_grain")) ++AfterBeast;
	Check(TEXT("jaredite.builder_beast_required"), AfterHuman == 0 && AfterBeast == 1, TEXT("worker rejected, beast foundation accepted"), FString::Printf(TEXT("after_worker=%d after_beast=%d"), AfterHuman, AfterBeast));

	// A raider diverts a known gross amount into only its own cargo. An idle
	// raider must neither create nor divert resources.
	Reset(); AddPlayer(TEXT("gadianton"), 0); AddPlayer(TEXT("nephites"), 1);
	Players[0].Favor=0;
	FRTSEntity* Raider = Spawn(TEXT("gadianton_robber"), 0, FVector2D(40, 40));
	FRTSEntity* Site = Spawn(TEXT("fields_of_grain"), 1, FVector2D(40, 40));
	float Retained = 0; bool Intercepted = false;
	if (Raider && Site) { Raider->Order = TEXT("raid"); Raider->Target = Site->Id; Raider->Channel = 3; Divert(Site->Id, 0, 100, Retained, Intercepted); }
	const float Stolen = Raider ? Raider->Cargo.X : 0;
	Check(TEXT("plunder.conserves_gross_resource"), Raider && Intercepted && Near(Retained + Stolen + 25.f, 100.f) && Near(Stolen, 30.f), TEXT("retained 45 + stolen 30 + suppressed 25 = gross 100"), FString::Printf(TEXT("retained=%.2f stolen=%.2f suppressed=%.2f"), Retained, Stolen, 100.f - Retained - Stolen));
	if (Raider) { Raider->Order = TEXT("idle"); Raider->Target = 0; Raider->Channel = 0; Raider->Cargo = FVector::ZeroVector; }
	Intercepted = false;
	Divert(Site ? Site->Id : 0, 0, 100, Retained, Intercepted);
	Check(TEXT("plunder.idle_raider_has_no_effect"), Near(Retained, 100.f) && (Raider == nullptr || Near(Raider->Cargo.X, 0)), TEXT("retained 100, stolen 0"), FString::Printf(TEXT("retained=%.2f stolen=%.2f"), Retained, Raider ? Raider->Cargo.X : 0.f));

	// Age-II Nephite conversion with Mighty Change changes the same entity's
	// owner and definition while preserving its current health fraction.
	Reset(); AddPlayer(TEXT("nephites"), 0); AddPlayer(TEXT("lamanites"), 1);Players[0].Age=2;
	Spawn(TEXT("seat_of_the_people"), 0, FVector2D(30, 40));
	Spawn(TEXT("seat_of_the_people"), 1, FVector2D(52, 40));
	FRTSEntity* Teacher = Spawn(TEXT("teacher_of_the_word"), 0, FVector2D(40, 40));
	FRTSEntity* ConvertTarget = Spawn(TEXT("man_of_moroni"), 1, FVector2D(42, 40));
	Players[0].Upgrades.Add(TEXT("mighty_change_of_heart")); Time = 60;
	if (ConvertTarget) ConvertTarget->HP *= .5f;
	const int32 OriginalId = ConvertTarget ? ConvertTarget->Id : 0;
	const int32 TargetPop = ConvertTarget ? D(*ConvertTarget).Pop : 0;
	const float OriginalRatio = ConvertTarget ? ConvertTarget->HP / ConvertTarget->MaxHP : 0;
	Population();
	const int32 OldOwnerPop = Players[1].Population;
	const int32 TeacherOwnerPop = Players[0].Population;
	if (Teacher && ConvertTarget) { Teacher->Order = TEXT("convert"); Teacher->Target = ConvertTarget->Id; }
	if (Teacher) Conversion(*Teacher, 12.f);
	FRTSEntity* Converted = Find(OriginalId);
	Check(TEXT("conversion.same_id_owner_population_and_health_ratio"), Converted && Converted->Owner == 0 && D(*Converted).Id == TEXT("ammonite_covenant_keeper") && Converted->Id == OriginalId && Players[0].Population == TeacherOwnerPop + TargetPop && Players[1].Population == OldOwnerPop - TargetPop && Players[0].Population + Players[1].Population == TeacherOwnerPop + OldOwnerPop && Near(Converted->HP / Converted->MaxHP, OriginalRatio, .03f),
		TEXT("same ID becomes allied covenant keeper; exact population transfers and health ratio is preserved at conversion"),
		FString::Printf(TEXT("id=%d owner=%d def=%s owner0pop=%d owner1pop=%d ratio=%.3f"), Converted ? Converted->Id : 0, Converted ? Converted->Owner : -1, Converted ? *D(*Converted).Id : TEXT("missing"), Players[0].Population, Players[1].Population, Converted ? Converted->HP / Converted->MaxHP : 0.f));

	// A recent Seat foundation pauses defeat long enough to finish, then the
	// ordinary sixty-second last-settlement timer applies if no Seat survives.
	Reset(); AddPlayer(TEXT("nephites"), 0); AddPlayer(TEXT("lamanites"), 1);
	FRTSEntity* Foundation = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20));
	if (Foundation) { Foundation->Progress = .001f; Foundation->LastProgress = 0; Foundation->MaxHP = MaxHP(*Foundation); Foundation->HP = Foundation->MaxHP; }
	FRTSEntity* OpponentHQ = Spawn(TEXT("seat_of_the_people"), 1, FVector2D(136, 136));
	Players[0].DefeatRemaining = 60; Players[1].DefeatRemaining = 60;
	Run(14);
	const bool bGrace = !Players[0].bDefeated && Near(Players[0].DefeatRemaining, 60.f, .05f);
	if (Foundation) Kill(Foundation->Id);
	Run(61);
	Check(TEXT("defeat.foundation_grace_and_last_seat_timer"), bGrace && Players[0].bDefeated && OpponentHQ && !Players[1].bDefeated, TEXT("recent foundation grants grace; after removal only its owner is defeated at 60s"), FString::Printf(TEXT("grace=%s defeated=%s opponent_alive=%s remaining=%.1f"), bGrace ? TEXT("yes") : TEXT("no"), Players[0].bDefeated ? TEXT("yes") : TEXT("no"), Players[1].bDefeated ? TEXT("yes") : TEXT("no"), Players[0].DefeatRemaining));

	// Starting each faction through the actual lobby start path creates its
	// headquarters and the appropriate regular/Jaredite opening roster.
	const FString Factions[] = { TEXT("nephites"), TEXT("lamanites"), TEXT("gadianton"), TEXT("coriantumr"), TEXT("shiz"), TEXT("laban") };
	for (const FString& Faction : Factions)
	{
		Reset(); Phase = 0; AddPlayer(Faction); if (C) C->ServerSlot = 0;
		if (C) Start(C, 0);
		int32 Seats = 0, Workers = 0, Beasts = 0;
		for (auto& KV : Entities) if (KV.Value->Owner == 0)
		{
			if (D(*KV.Value).Id == TEXT("seat_of_the_people")) ++Seats;
			if (D(*KV.Value).Id == TEXT("industrious_laborer")) ++Workers;
			if (D(*KV.Value).Id == TEXT("curelom")) ++Beasts;
		}
		const bool bJaredite = RTS::IsJaredite(Faction);
		Check(TEXT("start.") + Faction, C && Phase == 1 && Seats == 1 && Workers == 6 && Beasts == (bJaredite ? 1 : 0),
			TEXT("one Seat, six workers, and builder beast only for Jaredite starts"), FString::Printf(TEXT("phase=%d seats=%d workers=%d beasts=%d"), Phase, Seats, Workers, Beasts));
	}

	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("passed"), Failed == 0); Report->SetNumberField(TEXT("passed_count"), Passed); Report->SetNumberField(TEXT("failed_count"), Failed); Report->SetArrayField(TEXT("assertions"), Assertions);
	FString Json; const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Tests"); IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = Directory / TEXT("scenario-tests.json");
	if (!FFileHelper::SaveStringToFile(Json, *Path)) { ++Failed; UE_LOG(LogTemp, Error, TEXT("RTS_SCENARIO_TEST_FAIL report.write path=%s"), *Path); }
	if (Failed == 0) { UE_LOG(LogTemp, Display, TEXT("RTS_SCENARIO_TESTS passed=%d failed=0 report=%s"), Passed, *Path); }
	else { UE_LOG(LogTemp, Error, TEXT("RTS_SCENARIO_TESTS passed=%d failed=%d report=%s"), Passed, Failed, *Path); }
	return Failed == 0;
}
