#include "RTSWorld.h"
#include "RTSController.h"

void ARTSWorld::Ability(FRTSEntity& E, FVector2D Point, int32 Target, ARTSController* C)
{
    if (!C || !Players.IsValidIndex(C->ServerSlot) || E.Owner != C->ServerSlot || E.HP <= 0.f)
    {
        Error(C, TEXT("That unit cannot receive your ability order."));
        return;
    }
    if (E.Cooldown > 0.f)
    {
        Error(C, TEXT("That ability is still recharging."));
        return;
    }

    const FString Id = D(E).Id;
    auto EnemyTarget = [this, &E](int32 TargetId, float Range, FRTSEntity*& Out) -> bool
    {
        Out = Find(TargetId);
        return Out && Out->HP > 0.f && Out->Owner >= 0 && !Allied(E.Owner, Out->Owner)
            && CanSee(E.Owner, *Out)
            && FVector2D::Distance(E.Pos, Out->Pos) <= Range
            && LOS(E.Pos, Out->Pos, E.Id, Out->Id);
    };
    auto IsResourceSite = [this](const FRTSEntity& T) -> bool
    {
        if (!D(T).bBuilding || T.Progress < 1.f) return false;
        const FString& B = D(T).Id;
        return B == TEXT("seat_of_the_people") || B == TEXT("fields_of_grain")
            || B == TEXT("gathering_of_flocks") || B == TEXT("hill_of_ore")
            || B == TEXT("storehouse_of_provisions") || B == TEXT("place_of_trade");
    };
    auto IsMilitarySite = [this](const FRTSEntity& T) -> bool
    {
        if (!D(T).bBuilding || T.Progress < 1.f) return false;
        const FString& B = D(T).Id;
        return B == TEXT("encampment_of_war") || B == TEXT("tower_of_watchmen")
            || B == TEXT("works_of_defense");
    };
    auto Activate = [this, &E](float Cooldown, float Duration = 0.f)
    {
        E.Cooldown = Cooldown;
        if (Duration > 0.f) E.BuffUntil = Time + Duration;
    };

    if (D(E).Kind == TEXT("ammonite"))
    {
        if(!Has(E.Owner,TEXT("mighty_change_of_heart"))){Error(C,TEXT("Research Mighty Change of Heart to unlock Prostrate."));return;}
        ResetOrder(E,TEXT("hold"));
        Activate(12.f,5.f);
        return;
    }

    if (Id == TEXT("ammon"))
    {
        FRTSEntity* T = nullptr;
        if (!EnemyTarget(Target, 2.f, T) || D(*T).bBuilding || D(*T).Attack <= 0.f || D(*T).bHero)
        {
            Error(C, TEXT("Ammon must target a visible nonhero enemy within 2m."));
            return;
        }
        E.Target = T->Id;
        T->DisarmedUntil = Time + 4.f;
        if (!E.DisarmTargets.Contains(T->Id))
        {
            E.DisarmTargets.Add(T->Id);
            E.BonusAttack = FMath::Min(20.f, E.BonusAttack + 2.f);
        }
        E.BuffUntil = Time + 4.f;
        E.Order = TEXT("disarm");
        Activate(15.f);
        return;
    }

    if (Id == TEXT("amalickiah"))
    {
        FRTSEntity* T = nullptr;
        if (!EnemyTarget(Target, 6.f, T) || !IsMilitarySite(*T))
        {
            Error(C, TEXT("Dissension requires a visible enemy military building within 6m."));
            return;
        }
        E.Target = T->Id;
        E.Channel = 3.f;
        E.Order = TEXT("dissension");
        E.BuffUntil = Time + 9.f; // 3s channel, then 6s queue pause.
        Activate(45.f);
        return;
    }

    if (Id == TEXT("captain_moroni"))
    {
        if (FVector2D::Distance(E.Pos, Point) > 4.f || !Walkable(Point, .45f, E.Id, false))
        {
            Error(C, TEXT("Plant the banner on walkable ground within 4m."));
            return;
        }
        E.BuffPoint = Point;
        E.Target = 0;
        E.Order = TEXT("title_of_liberty");
        Activate(60.f, 30.f);
        return;
    }

    if (Id == TEXT("chief_captain"))
    {
        if (Players[E.Owner].Faction != TEXT("lamanites") || !Has(E.Owner, TEXT("fight_like_dragons")))
        {
            Error(C, TEXT("Fight Like Dragons must be researched by the Lamanites first."));
            return;
        }
        E.Target = 0;
        E.Order = TEXT("fight_like_dragons");
        Activate(60.f, 10.f);
        return;
    }

    if (Id == TEXT("gadianton"))
    {
        E.Target = 0;
        E.Order = TEXT("secret_combination");
        Activate(60.f, 20.f);
        return;
    }

    if (Id == TEXT("giddianhi"))
    {
        FRTSEntity* T = nullptr;
        if (!EnemyTarget(Target, 8.f, T) || !IsResourceSite(*T))
        {
            Error(C, TEXT("Demand Surrender requires a visible enemy resource building within 8m."));
            return;
        }
        E.Target = T->Id;
        E.Order = TEXT("demand_surrender");
        T->SuppressedUntil = Time + 15.f;
        Activate(60.f, 15.f);
        return;
    }

    if (Id == TEXT("helaman"))
    {
        E.Target = 0;
        E.Order = TEXT("firm_and_undaunted");
        Activate(45.f, 15.f);
        return;
    }

    if (Id == TEXT("kishkumen"))
    {
        FRTSEntity* T = nullptr;
        if (!EnemyTarget(Target, 2.f, T) || (!D(*T).bHero && D(*T).Id != TEXT("corrupt_judge")))
        {
            Error(C, TEXT("Assassination requires a visible enemy hero or Judge within 2m."));
            return;
        }
        E.Target = T->Id;
        Damage(*T, E, 60.f);
        Activate(45.f);
        return;
    }

    if (Id == TEXT("lamoni"))
    {
        E.Target = 0;
        E.Order = TEXT("royal_muster");
        Activate(60.f, 20.f);
        return;
    }

    if (Id == TEXT("samuel_the_lamanite"))
    {
        FRTSEntity* Wall = Find(Target);
        if (!Wall || Wall->Owner != E.Owner || D(*Wall).Id != TEXT("works_of_defense")
            || Wall->Progress < 1.f || FVector2D::Distance(Wall->Pos, Point) > 2.5f
            || FVector2D::Distance(E.Pos, Point) > 1.5f || !Walkable(Point, .45f, E.Id, false))
        {
            Error(C, TEXT("Samuel must channel from walkable ground beside an owned wall platform."));
            return;
        }
        E.Target = Wall->Id;
        E.BuffPoint = Point;
        E.Channel = 1.f;
        E.Order = TEXT("sermon");
        E.BuffUntil = Time + 86400.f; // sustained while Order remains sermon; orders cancel it.
        return;
    }

    if (Id == TEXT("teancum"))
    {
        E.Target = 0;
        E.Order = TEXT("night_raid");
        Activate(45.f, 10.f);
        return;
    }

    if (Id == TEXT("corrupt_judge"))
    {
        FRTSEntity* T = nullptr;
        if (!EnemyTarget(Target, 6.f, T) || !IsResourceSite(*T))
        {
            Error(C, TEXT("Channel Injunction requires a visible enemy economic building within 6m."));
            return;
        }
        for (const auto& Pair : Entities)
        {
            const FRTSEntity& Other = *Pair.Value;
            if (Other.Id != E.Id && Other.HP > 0.f && D(Other).Id == TEXT("corrupt_judge")
                && (Other.Order == TEXT("injunction") || Other.Order == TEXT("raid")) && Other.Target == T->Id)
            {
                Error(C, TEXT("That building is already under injunction."));
                return;
            }
        }
        E.Target = T->Id;
        E.Channel = 1.f;
        E.Order = TEXT("injunction");
        return;
    }

    if (RTS::IsCarrier(D(E)))
    {
        if (E.Passengers.IsEmpty())
        {
            Error(C, TEXT("There are no passengers to unload."));
            return;
        }
        if (FVector2D::Distance(E.Pos, Point) > 5.f)
        {
            Error(C, TEXT("Choose walkable land within 5m of the carrier."));
            return;
        }
        TArray<FVector2D> LandingPoints;
        bool bAllSpotsFound = true;
        for (int32 PassengerId : E.Passengers)
        {
            FRTSEntity* Passenger = Find(PassengerId);
            if (!Passenger || Passenger->Owner != E.Owner || Passenger->Carrier != E.Id)
            {
                bAllSpotsFound = false;
                break;
            }
            bool bFound = false;
            // A Hideout remains an obstacle during unloading. Mobile carriers
            // can be ignored, while a building's passengers must land outside it.
            const bool BuildingCarrier = D(E).bBuilding;
            const int32 IgnoreObstacle = BuildingCarrier ? Passenger->Id : E.Id;
            const float FirstRing = BuildingCarrier ? D(E).Radius + D(*Passenger).Radius + .3f : 0.f;
            const float Rings[] = {FirstRing, FirstRing+.7f, FirstRing+1.4f, FirstRing+2.1f, FirstRing+2.8f, FirstRing+3.5f, FirstRing+4.2f};
            for (float Radius : Rings)
            {
                const int32 Samples = Radius == 0.f ? 1 : 12;
                for (int32 Sample = 0; Sample < Samples; ++Sample)
                {
                    const float Angle = 2.f * PI * float(Sample) / float(Samples);
                    FVector2D Candidate = Point + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
                    if (FVector2D::Distance(Candidate, E.Pos) > 5.f
                        || !Walkable(Candidate, D(*Passenger).Radius, IgnoreObstacle, false)) continue;
                    bool bSeparated = true;
                    for (int32 I = 0; I < LandingPoints.Num(); ++I)
                    {
                        FRTSEntity* PriorPassenger = Find(E.Passengers[I]);
                        const float Need = D(*Passenger).Radius + (PriorPassenger ? D(*PriorPassenger).Radius : .45f) + .15f;
                        if (FVector2D::Distance(Candidate, LandingPoints[I]) < Need) { bSeparated = false; break; }
                    }
                    if (bSeparated) { LandingPoints.Add(Candidate); bFound = true; break; }
                }
                if (bFound) break;
            }
            if (!bFound) { bAllSpotsFound = false; break; }
        }
        if (!bAllSpotsFound)
        {
            Error(C, TEXT("There is not enough walkable shore space to unload every passenger."));
            return;
        }
        for (int32 I = 0; I < E.Passengers.Num(); ++I)
        {
            if (FRTSEntity* Passenger = Find(E.Passengers[I]))
            {
                Passenger->Carrier = 0;
                Passenger->Pos = LandingPoints[I];
                Passenger->Destination = LandingPoints[I];
                ResetOrder(*Passenger,TEXT("idle"));
            }
        }
        E.Passengers.Empty();
        return;
    }

    Error(C, TEXT("That unit has no active ability."));
}
