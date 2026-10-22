#include "RTSWorld.h"
#include "RTSTypes.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool ARTSWorld::RunSelfTests()
{
	TArray<TSharedPtr<FJsonValue>> Assertions;
	int32 Passed = 0;
	int32 Failed = 0;

	auto Check = [&](const FString& Name, bool bCondition, const FString& Expected, const FString& Actual)
	{
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Name);
		Item->SetBoolField(TEXT("passed"), bCondition);
		Item->SetStringField(TEXT("expected"), Expected);
		Item->SetStringField(TEXT("actual"), Actual);
		Assertions.Add(MakeShared<FJsonValueObject>(Item));
		if (bCondition)
		{
			++Passed;
		}
		else
		{
			++Failed;
			UE_LOG(LogTemp, Error, TEXT("RTS_SELF_TEST_FAIL %s expected=%s actual=%s"), *Name, *Expected, *Actual);
		}
	};
	auto Near = [](float A, float B, float Tolerance = 0.01f) { return FMath::Abs(A - B) <= Tolerance; };
	auto Number = [](float Value) { return FString::Printf(TEXT("%.3f"), Value); };
	auto Reset = [&]()
	{
		Entities.Empty();
		Players.Empty();
		NextId = 1;
		Map = 0;
		Time = 0;
		Phase = 0;
		NavigationCount=-1;NavigationTime=-100;ObstacleCount=-1;Obstacles.Empty();bDispatchingQueuedOrder=false;
	};
	auto AddPlayer = [&](const FString& Faction, int32 Team = 0)
	{
		FRTSPlayer Player;
		Player.Slot = Players.Num();
		Player.Team = Team;
		Player.Faction = Faction;
		Player.Explored.Init(0, RTS::FogSize * RTS::FogSize);
		Players.Add(Player);
	};

	FRTSCatalog& Catalog = FRTSCatalog::Get();
	FString CatalogText;TSharedPtr<FJsonObject> CatalogJSON;int ExpectedDefinitions=7;
 FFileHelper::LoadFileToString(CatalogText,*(FPaths::ProjectContentDir()/TEXT("Data/catalog.json")));const bool Parsed=FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(CatalogText),CatalogJSON);
 if(Parsed&&CatalogJSON.IsValid())for(const TCHAR* Key:{TEXT("units"),TEXT("buildings"),TEXT("upgrades")}){const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;if(CatalogJSON->TryGetArrayField(Key,Items))ExpectedDefinitions+=Items->Num();}
 Check(TEXT("catalog.definition_count"), Parsed&&Catalog.Defs.Num()==ExpectedDefinitions,FString::FromInt(ExpectedDefinitions),FString::FromInt(Catalog.Defs.Num()));
	Check(TEXT("catalog.faction_count"), Catalog.Factions.Num() == 6, TEXT("6"), FString::FromInt(Catalog.Factions.Num()));
	Check(TEXT("catalog.cimeter_bearer_base_cost"), Catalog.Def(TEXT("cimeter_bearer")).Cost == FVector(50, 0, 25), TEXT("(50,0,25)"), Catalog.Def(TEXT("cimeter_bearer")).Cost.ToString());
	Check(TEXT("catalog.nephite_cimeter_bearer_price"), Catalog.Price(Catalog.Def(TEXT("cimeter_bearer")), TEXT("nephites")) == FVector(63, 0, 32), TEXT("(63,0,32)"), Catalog.Price(Catalog.Def(TEXT("cimeter_bearer")), TEXT("nephites")).ToString());
	Check(TEXT("catalog.lamanite_cimeter_bearer_price"), Catalog.Price(Catalog.Def(TEXT("cimeter_bearer")), TEXT("lamanites")) == FVector(33, 0, 17), TEXT("(33,0,17)"), Catalog.Price(Catalog.Def(TEXT("cimeter_bearer")), TEXT("lamanites")).ToString());

	Reset();
	AddPlayer(TEXT("nephites"));
	Players[0].Resources = FVector(10, 50, 50);
	const FVector BeforeFailedPay = Players[0].Resources;
	const bool bUnaffordablePaid = Pay(0, FVector(11, 30, 40));
	Check(TEXT("economy.pay_rejects_unaffordable"), !bUnaffordablePaid, TEXT("false"), bUnaffordablePaid ? TEXT("true") : TEXT("false"));
	Check(TEXT("economy.pay_failure_is_atomic"), Players[0].Resources == BeforeFailedPay, BeforeFailedPay.ToString(), Players[0].Resources.ToString());
	const bool bAffordablePaid = Pay(0, FVector(10, 30, 40));
	Check(TEXT("economy.pay_affordable_cost"), bAffordablePaid && Players[0].Resources == FVector(0, 20, 10), TEXT("true; resources=(0,20,10)"), FString::Printf(TEXT("%s; resources=%s"), bAffordablePaid ? TEXT("true") : TEXT("false"), *Players[0].Resources.ToString()));

	Reset();
	AddPlayer(TEXT("nephites"));
	Players[0].Favor = 50;
	FRTSEntity* NephiteInfantry = Spawn(TEXT("man_of_moroni"), 0, FVector2D(20, 20));
	const float InfantryBase=Catalog.Def(TEXT("man_of_moroni")).HP;
	Check(TEXT("favor.initial_nephite_hp"), NephiteInfantry && Near(NephiteInfantry->MaxHP, InfantryBase), Number(InfantryBase), NephiteInfantry ? Number(NephiteInfantry->MaxHP) : TEXT("spawn failed"));
	if (NephiteInfantry)
	{
		NephiteInfantry->HP = NephiteInfantry->MaxHP*.5f;
		Favor(0, -50.f);
		Check(TEXT("favor.zero_endpoint_hp"), Near(NephiteInfantry->MaxHP, 1.f), Number(1.f), Number(NephiteInfantry->MaxHP));
		Check(TEXT("favor.zero_preserves_hp_fraction"), Near(NephiteInfantry->HP / NephiteInfantry->MaxHP, 0.5f), TEXT("0.500"), Number(NephiteInfantry->HP / NephiteInfantry->MaxHP));
		Favor(0, 100.f);
		Check(TEXT("favor.full_endpoint_hp"), Near(NephiteInfantry->MaxHP, InfantryBase*100.f), Number(InfantryBase*100.f), Number(NephiteInfantry->MaxHP));
		Check(TEXT("favor.full_preserves_hp_fraction"), Near(NephiteInfantry->HP / NephiteInfantry->MaxHP, 0.5f), TEXT("0.500"), Number(NephiteInfantry->HP / NephiteInfantry->MaxHP));
	}

	Reset();
	AddPlayer(TEXT("coriantumr"));
	FRTSEntity* TierOneSeat = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20), 1);
	FRTSEntity* TierTwoSeat = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(30, 20), 2);
	const float SeatBase = Catalog.Def(TEXT("seat_of_the_people")).HP;
	Check(TEXT("jaredite.tier_one_headquarters_hp"), TierOneSeat && Near(TierOneSeat->MaxHP, SeatBase * 3.f * 1.06f), Number(SeatBase * 3.f * 1.06f), TierOneSeat ? Number(TierOneSeat->MaxHP) : TEXT("spawn failed"));
	Check(TEXT("jaredite.tier_two_headquarters_hp"), TierTwoSeat && Near(TierTwoSeat->MaxHP, SeatBase * 4.f * 1.06f), Number(SeatBase * 4.f * 1.06f), TierTwoSeat ? Number(TierTwoSeat->MaxHP) : TEXT("spawn failed"));

	Reset();
	AddPlayer(TEXT("nephites"));
	FRTSEntity* Seat = Spawn(TEXT("seat_of_the_people"), 0, FVector2D(20, 20));
	FRTSEntity* Worker = Spawn(TEXT("industrious_laborer"), 0, FVector2D(25, 20));
	if (Seat)
	{
		FRTSJob QueuedJob;
		QueuedJob.Type = TEXT("man_of_moroni");
		QueuedJob.Pop = 2;
		Seat->Jobs.Add(QueuedJob);
	}
	Population();
	const int32 ExpectedReservedPopulation = (Worker ? D(*Worker).Pop : 0) + 2;
	Check(TEXT("population.queue_reserves_population"), Players[0].Population == ExpectedReservedPopulation, FString::FromInt(ExpectedReservedPopulation), FString::FromInt(Players[0].Population));
	Check(TEXT("population.completed_headquarters_capacity"), Players[0].Capacity == 15, TEXT("15"), FString::FromInt(Players[0].Capacity));
	if (Seat) Seat->Jobs.Empty();
	Population();
	Check(TEXT("population.cancelled_queue_releases_population"), Players[0].Population == (Worker ? D(*Worker).Pop : 0), FString::FromInt(Worker ? D(*Worker).Pop : 0), FString::FromInt(Players[0].Population));

	Reset();
	AddPlayer(TEXT("nephites"), 0);
	AddPlayer(TEXT("lamanites"), 1);
	FRTSEntity* FriendlyLaborer = Spawn(TEXT("industrious_laborer"), 0, FVector2D(40, 50));
	FRTSEntity* HiddenRobber = Spawn(TEXT("gadianton_robber"), 1, FVector2D(43, 50));
	const bool bHiddenWithoutDetector = HiddenRobber && !CanSee(0, *HiddenRobber);
	Check(TEXT("visibility.hidden_enemy_requires_detection"), bHiddenWithoutDetector, TEXT("hidden enemy not visible"), bHiddenWithoutDetector ? TEXT("hidden enemy not visible") : TEXT("hidden enemy visible or spawn failed"));
	FRTSEntity* Spy = Spawn(TEXT("wilderness_spy"), 0, FVector2D(42, 50));
	const bool bHiddenDetectedBySpy = HiddenRobber && Spy && CanSee(0, *HiddenRobber);
	Check(TEXT("visibility.spy_detects_hidden_enemy"), bHiddenDetectedBySpy, TEXT("hidden enemy visible to detector"), bHiddenDetectedBySpy ? TEXT("hidden enemy visible to detector") : TEXT("hidden enemy remains hidden"));
	(void)FriendlyLaborer;

	Reset();
	AddPlayer(TEXT("nephites"), 0);
	AddPlayer(TEXT("lamanites"), 1);
	Map = 0;
	FRTSEntity* Watchtower = Spawn(TEXT("tower_of_watchmen"), 0, FVector2D(72, 55));
	FRTSEntity* Enemy = Spawn(TEXT("man_of_moroni"), 1, FVector2D(88, 55));
	const bool bRockBlocksLine = !LOS(FVector2D(72, 55), FVector2D(88, 55));
	Check(TEXT("visibility.rock_blocks_line_of_sight"), bRockBlocksLine, TEXT("blocked"), bRockBlocksLine ? TEXT("blocked") : TEXT("clear"));
	const bool bBlockedEnemyNotVisible = Enemy && Watchtower && !CanSee(0, *Enemy);
	Check(TEXT("visibility.rock_hides_enemy"), bBlockedEnemyNotVisible, TEXT("enemy not visible"), bBlockedEnemyNotVisible ? TEXT("enemy not visible") : TEXT("enemy visible or spawn failed"));

	Check(TEXT("chat.profanity_detects_word_token"), RTS::HasProfanity(TEXT("a little shit!"), true), TEXT("true"), RTS::HasProfanity(TEXT("a little shit!"), true) ? TEXT("true") : TEXT("false"));
	Check(TEXT("chat.profanity_does_not_match_substring"), !RTS::HasProfanity(TEXT("classical"), true), TEXT("false"), RTS::HasProfanity(TEXT("classical"), true) ? TEXT("true") : TEXT("false"));
	Check(TEXT("chat.apology_accepts_case_and_punctuation"), RTS::IsApology(TEXT("... SORRY! ")), TEXT("true"), RTS::IsApology(TEXT("... SORRY! ")) ? TEXT("true") : TEXT("false"));
	Check(TEXT("chat.apology_requires_exact_phrase"), !RTS::IsApology(TEXT("sorry about that")), TEXT("false"), RTS::IsApology(TEXT("sorry about that")) ? TEXT("true") : TEXT("false"));

	TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetBoolField(TEXT("passed"), Failed == 0);
	Report->SetNumberField(TEXT("passed_count"), Passed);
	Report->SetNumberField(TEXT("failed_count"), Failed);
	Report->SetArrayField(TEXT("assertions"), Assertions);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
	const FString ReportDirectory = FPaths::ProjectSavedDir() / TEXT("Tests");
	IFileManager::Get().MakeDirectory(*ReportDirectory, true);
	const FString ReportPath = ReportDirectory / TEXT("self-tests.json");
	if (!FFileHelper::SaveStringToFile(Json, *ReportPath))
	{
		++Failed;
		UE_LOG(LogTemp, Error, TEXT("RTS_SELF_TEST_FAIL report.write expected=success actual=failed path=%s"), *ReportPath);
	}
	if (Failed == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("RTS_SELF_TESTS passed=%d failed=0 report=%s"), Passed, *ReportPath);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("RTS_SELF_TESTS passed=%d failed=%d report=%s"), Passed, Failed, *ReportPath);
	}
	return Failed == 0;
}
