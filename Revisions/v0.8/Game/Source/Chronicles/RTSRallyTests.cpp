#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Native regression coverage for production-building rally orders. Kept as a
// separate opt-in harness so these fixtures do not affect normal matches.
bool RunRTSRallyTests(ARTSWorld* W)
{
	if (!W) return false;
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
		else { ++Failed; UE_LOG(LogTemp, Error, TEXT("RTS_RALLY_TEST_FAIL %s expected=%s actual=%s"), *Name, *Expected, *Actual); }
	};
	auto Reset = [&]()
	{
		W->Entities.Empty(); W->Players.Empty(); W->NextId = 1; W->Map = 0; W->Time = 0; W->Phase = 1; W->Winner = -1; W->bSandbox = true;
		W->NavigationTime = -100; W->NavigationCount = -1; W->ObstacleCount = -1; W->Obstacles.Empty();
	};
	ARTSController* C = W->GetWorld() ? W->GetWorld()->SpawnActor<ARTSController>() : nullptr;
	if (C) C->ServerSlot = 0;
	auto AddPlayer = [&](const FString& Faction)
	{
		FRTSPlayer P; P.Slot = 0; P.Team = 0; P.Faction = Faction; P.Controller = C; P.bConnected = true;
		P.Explored.Init(0, RTS::FogSize * RTS::FogSize); W->Players.Add(P);
		if (C) C->ServerSlot = 0;
	};
	auto Complete = [&](const FString& Id, FVector2D Pos)
	{
		FRTSEntity* E = W->Spawn(Id, 0, Pos);
		if (E) { E->Progress = 1; E->MaxHP = W->MaxHP(*E); E->HP = E->MaxHP; }
		return E;
	};
	const FVector2D RallyPoint(60, 60);
	const FString ExpectedProducerIds[] = {
		TEXT("encampment_of_war"), TEXT("gathering_of_flocks"), TEXT("secret_combination_lodge"), TEXT("place_of_many_waters"), TEXT("winery"),
		TEXT("place_of_trade"), TEXT("storehouse_of_provisions"), TEXT("seat_of_the_people"), TEXT("temple_of_the_land")
	};
	{
		const auto& Cat = FRTSCatalog::Get();
		TArray<FString> Actual, Expected;
		for (const FRTSDef& Def : Cat.Defs) if (Def.bBuilding && Cat.IsUnitProducer(Def)) Actual.Add(Def.Id);
		for (const FString& Id : ExpectedProducerIds) Expected.Add(Id);
		Actual.Sort(); Expected.Sort();
		Check(TEXT("catalog.exact_unit_producer_buildings"), Actual == Expected, FString::Join(Expected, TEXT(",")), FString::Join(Actual, TEXT(",")));
	}

	// Every producer is exercised through Queue -> elapsed production -> spawn.
	// The fixture uses each civilization's actual available unit at that site.
	struct FProducerCase { const TCHAR* Faction; const TCHAR* Building; const TCHAR* Unit; int32 Age; const TCHAR* Upgrade; const TCHAR* ExtraBuilding; };
	const FProducerCase Cases[] = {
		{TEXT("laban"), TEXT("winery"), TEXT("drunken_concubine"), 2, TEXT(""), TEXT("")},
		{TEXT("lamanites"), TEXT("encampment_of_war"), TEXT("cimeter_bearer"), 2, TEXT(""), TEXT("")},
		{TEXT("coriantumr"), TEXT("gathering_of_flocks"), TEXT("sheep"), 2, TEXT(""), TEXT("")},
		{TEXT("gadianton"), TEXT("secret_combination_lodge"), TEXT("lawyer"), 2, TEXT(""), TEXT("")},
		{TEXT("coriantumr"), TEXT("place_of_many_waters"), TEXT("jaredite_barge"), 2, TEXT(""), TEXT("")},
		{TEXT("nephites"), TEXT("place_of_trade"), TEXT("trader_in_precious_things"), 2, TEXT(""), TEXT("")},
		{TEXT("gadianton"), TEXT("storehouse_of_provisions"), TEXT("gadianton_robber"), 2, TEXT(""), TEXT("")},
		{TEXT("nephites"), TEXT("seat_of_the_people"), TEXT("industrious_laborer"), 1, TEXT(""), TEXT("")},
		{TEXT("nephites"), TEXT("temple_of_the_land"), TEXT("teacher_of_the_word"), 2, TEXT(""), TEXT("")}
	};
	int32 BuiltAndTrained = 0;
	TArray<FString> Detail;
	for (const FProducerCase& Test : Cases)
	{
		Reset(); AddPlayer(Test.Faction); W->Players[0].Age = Test.Age; W->Players[0].Resources = FVector(10000, 10000, 10000);
		if (FString(Test.Building) == TEXT("place_of_many_waters")) W->Map = 2;
		if (FCString::Strlen(Test.Upgrade)) W->Players[0].Upgrades.Add(Test.Upgrade);
		Complete(TEXT("seat_of_the_people"), FVector2D(20, 20));
		if (FCString::Strlen(Test.ExtraBuilding)) Complete(Test.ExtraBuilding, FVector2D(30, 20));
		const FVector2D ProducerPos = FString(Test.Building) == TEXT("place_of_many_waters") ? FVector2D(71, 20) : FVector2D(45, 45);
		FRTSEntity* Producer = Complete(Test.Building, ProducerPos);
		FRTSEntity* Target = nullptr;
		if (FCString::Strlen(Test.Unit) && FString(Test.Unit) == TEXT("industrious_laborer")) Target = W->Spawn(TEXT("berries"), -1, FVector2D(48, 45));
		if (Producer) { if(FString(Test.Building)==TEXT("seat_of_the_people"))W->SetEconomicRally(*Producer,RallyPoint,Target?Target->Id:0);else W->SetRally(*Producer, RallyPoint, Target ? Target->Id : 0); }
		if (Producer && C) W->Queue(*Producer, Test.Unit, C);
		const bool bQueued = Producer && Producer->Jobs.Num() == 1 && Producer->Jobs[0].Type == Test.Unit;
		if (bQueued) Producer->Jobs[0].Remaining = .1f;
		if (Producer && bQueued) W->Production(*Producer, .1f);
		FRTSEntity* Trained = nullptr;
		for (const auto& KV : W->Entities) if (W->D(*KV.Value).Id == Test.Unit && KV.Value->Owner == 0) Trained = KV.Value.Get();
		const bool bPassed = Producer && FRTSCatalog::Get().IsUnitProducer(W->D(*Producer)) && bQueued && Trained && Producer->Jobs.IsEmpty() &&
			(FString(Test.Unit)==TEXT("sheep") ? (Trained->HomeBuilding==Producer->Id&&Trained->Order==TEXT("move")&&Trained->Rally==RallyPoint) :
			(Trained->Destination == RallyPoint && Trained->Rally == RallyPoint &&
			(Trained->Order == TEXT("move") || (Trained->Order == TEXT("gather") && Trained->WorkTarget == (Target ? Target->Id : 0)))));
		if (bPassed) ++BuiltAndTrained;
		Detail.Add(FString::Printf(TEXT("%s/%s=%s(order=%s)"), Test.Building, Test.Unit, bPassed ? TEXT("pass") : TEXT("fail"), Trained ? *Trained->Order : TEXT("missing")));
	}
	Check(TEXT("production.all_eight_buildings_train_and_apply_rally"), BuiltAndTrained == UE_ARRAY_COUNT(Cases), TEXT("8 producers queue and spawn their valid unit at the selected rally point"), FString::Join(Detail, TEXT("; ")));

	// HQ rally-to-resource must make the new laborer gather and eventually
	// deliver to its owned HQ, rather than merely copying a waypoint.
	Reset(); AddPlayer(TEXT("nephites")); W->Players[0].Resources = FVector(10000, 10000, 10000);
	FRTSEntity* HQ = Complete(TEXT("seat_of_the_people"), FVector2D(20, 20));
	FRTSEntity* Berries = W->Spawn(TEXT("berries"), -1, FVector2D(30, 20));
	if (Berries) Berries->ResourceRemaining = Berries->HP = 100;
	if (HQ && Berries) W->SetEconomicRally(*HQ, Berries->Pos, Berries->Id);
	if (HQ && C) W->Queue(*HQ, TEXT("industrious_laborer"), C);
	if (HQ && HQ->Jobs.Num()) HQ->Jobs[0].Remaining = .1f;
	if (HQ && HQ->Jobs.Num()) W->Production(*HQ, .1f);
	FRTSEntity* RallyWorker = nullptr;
	for (const auto& KV : W->Entities) if (W->D(*KV.Value).Id == TEXT("industrious_laborer") && KV.Value->Owner == 0) RallyWorker = KV.Value.Get();
	const FVector StartResources = W->Players[0].Resources;
	if (RallyWorker) for (int32 I = 0; I < 400; ++I) W->Step(.1f);
	Check(TEXT("hq.worker_rally_gathers_and_delivers"), HQ && RallyWorker && RallyWorker->Order == TEXT("gather") && RallyWorker->WorkTarget == (Berries ? Berries->Id : 0) &&
		W->Players[0].Resources.X > StartResources.X,
		TEXT("trained Laborer is assigned to the resource and at least one cargo delivery reaches HQ"),
		FString::Printf(TEXT("worker=%s target=%d food_delta=%.2f cargo=%.2f"), RallyWorker ? *RallyWorker->Order : TEXT("missing"), RallyWorker ? RallyWorker->WorkTarget : 0,
			W->Players.Num() ? W->Players[0].Resources.X - StartResources.X : 0.f, RallyWorker ? RallyWorker->Cargo.X : -1.f));

	// A combat recruit inherits a plain move rally, and actually walks there.
	Reset(); AddPlayer(TEXT("lamanites")); W->Players[0].Age=2; W->Players[0].Resources = FVector(10000, 10000, 10000);
	HQ = Complete(TEXT("seat_of_the_people"), FVector2D(20, 20));
	FRTSEntity* War = Complete(TEXT("encampment_of_war"), FVector2D(45, 45));
	if (War) W->SetRally(*War, FVector2D(65, 45), 0);
	if (War && C) W->Queue(*War, TEXT("cimeter_bearer"), C);
	if (War && War->Jobs.Num()) War->Jobs[0].Remaining = .1f;
	if (War && War->Jobs.Num()) W->Production(*War, .1f);
 	FRTSEntity* Melee = nullptr; for (const auto& KV : W->Entities) if (W->D(*KV.Value).Id == TEXT("cimeter_bearer")) Melee = KV.Value.Get();
	const FVector2D MeleeStart = Melee ? Melee->Pos : FVector2D::ZeroVector;
	if (Melee) for (int32 I = 0; I < 100; ++I) W->Step(.1f);
	Check(TEXT("production.nonworker_rally_moves_without_gathering"), HQ && War && Melee && Melee->Rally == FVector2D(65, 45) && Melee->Order == TEXT("idle") &&
		Melee->WorkTarget == 0 && FVector2D::Distance(MeleeStart, Melee->Pos) > 10.f && FVector2D::Distance(Melee->Pos, FVector2D(65, 45)) <= .6f,
		TEXT("non-worker reaches the selected rally point and never receives a gather job"),
		FString::Printf(TEXT("order=%s worktarget=%d displacement=%.2f rally=%s position=%s distance_to_rally=%.2f"), Melee ? *Melee->Order : TEXT("missing"), Melee ? Melee->WorkTarget : 0,
			Melee ? FVector2D::Distance(MeleeStart, Melee->Pos) : 0.f, Melee ? *Melee->Rally.ToString() : TEXT("missing"), Melee ? *Melee->Pos.ToString() : TEXT("missing"),
			Melee ? FVector2D::Distance(Melee->Pos, FVector2D(65, 45)) : 999.f));

	// A destroyed or stale target falls back to the rally coordinate.
	Reset(); AddPlayer(TEXT("nephites")); W->Players[0].Resources = FVector(10000, 10000, 10000);
	HQ = Complete(TEXT("seat_of_the_people"), FVector2D(20, 20));
	if (HQ) W->SetEconomicRally(*HQ, FVector2D(52, 52), 987654);
	if (HQ && C) W->Queue(*HQ, TEXT("industrious_laborer"), C);
	if (HQ && HQ->Jobs.Num()) HQ->Jobs[0].Remaining = .1f;
	if (HQ && HQ->Jobs.Num()) W->Production(*HQ, .1f);
	RallyWorker = nullptr; for (const auto& KV : W->Entities) if (W->D(*KV.Value).Id == TEXT("industrious_laborer")) RallyWorker = KV.Value.Get();
	Check(TEXT("hq.stale_resource_target_falls_back_to_move"), RallyWorker && RallyWorker->Order == TEXT("move") && RallyWorker->WorkTarget == 0 && RallyWorker->Target == 0 && RallyWorker->Destination == FVector2D(52, 52),
		TEXT("worker moves to rally point without an invalid gather target"), RallyWorker ? FString::Printf(TEXT("order=%s target=%d work=%d destination=%s"), *RallyWorker->Order, RallyWorker->Target, RallyWorker->WorkTarget, *RallyWorker->Destination.ToString()) : TEXT("missing"));

	// A full worksite keeps the rally coordinate but declines an eleventh gatherer.
	Reset(); AddPlayer(TEXT("nephites")); W->Players[0].Resources = FVector(10000, 10000, 10000);
	HQ = Complete(TEXT("seat_of_the_people"), FVector2D(20, 20));
	FRTSEntity* Farm = Complete(TEXT("fields_of_grain"), FVector2D(50, 50));
	if (Farm) for (int32 I = 0; I < 10; ++I) { FRTSEntity* Worker = W->Spawn(TEXT("industrious_laborer"), 0, FVector2D(42 + I * 1.5f, 48)); if (Worker) { Worker->WorkTarget = Farm->Id; Worker->Target = Farm->Id; Worker->Order = TEXT("gather"); } }
	if (HQ && Farm) W->SetEconomicRally(*HQ, Farm->Pos, Farm->Id);
	if (HQ && C) W->Queue(*HQ, TEXT("industrious_laborer"), C);
	if (HQ && HQ->Jobs.Num()) HQ->Jobs[0].Remaining = .1f;
	if (HQ && HQ->Jobs.Num()) W->Production(*HQ, .1f);
	RallyWorker = nullptr; for (const auto& KV : W->Entities) if (W->D(*KV.Value).Id == TEXT("industrious_laborer") && (!RallyWorker || KV.Key > RallyWorker->Id)) RallyWorker = KV.Value.Get();
	Check(TEXT("hq.full_worksite_falls_back_to_move"), Farm && RallyWorker && RallyWorker->Order == TEXT("move") && RallyWorker->WorkTarget == 0 && RallyWorker->Destination == Farm->Pos,
		TEXT("eleventh worker moves to full field instead of being assigned beyond site capacity"), RallyWorker ? FString::Printf(TEXT("order=%s work=%d destination=%s"), *RallyWorker->Order, RallyWorker->WorkTarget, *RallyWorker->Destination.ToString()) : TEXT("missing"));

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("passed"), Passed);
	Root->SetNumberField(TEXT("failed"), Failed);
	Root->SetArrayField(TEXT("assertions"), Assertions);
	FString JSON; FJsonSerializer::Serialize(Root.ToSharedRef(), TJsonWriterFactory<>::Create(&JSON));
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("Tests");
	IFileManager::Get().MakeDirectory(*Dir, true);
	FFileHelper::SaveStringToFile(JSON, *(Dir / TEXT("rally-tests.json")));
	UE_LOG(LogTemp, Display, TEXT("RTS_RALLY_TESTS passed=%d failed=%d"), Passed, Failed);
	return Failed == 0;
}
