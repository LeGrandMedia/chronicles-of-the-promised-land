#pragma once
#include "CoreMinimal.h"
#include "RTSTypes.generated.h"

// Simulation uses meters. The presentation may choose any scale.
struct FRTSDef {
 FString Id, Name, Kind, Producer, Description, Ability, Icon, Hotkey, HeroChoice, Perk;
 FString SelectLine,SelectAudio;
 TMap<FString,FString> FactionDescriptions;
 TMap<FString,int32> FactionAges;
 TMap<FString,float> EconomyBonuses;
 float HP=100, Attack=0, Interval=1.5f, Range=1, Speed=0, Sight=10, Armor=0, Seconds=1, Radius=.45f;
 int32 Pop=0, Age=1;
 FVector Cost=FVector::ZeroVector;
 float CementCost=0;
 TArray<FString> Requires;
 bool bBuilding=false, bUpgrade=false, bHero=false;
};
struct FRTSFaction {
 FString Id, Name, Description;
 TArray<FString> Units, Buildings, Upgrades;
 FLinearColor Color=FLinearColor::White;
};
class FRTSCatalog {
public:
 TArray<FRTSDef> Defs;
 TArray<FRTSFaction> Factions;
 static FRTSCatalog& Get();
 void Load();
 int32 Index(const FString& Id) const;
 const FRTSDef& Def(int32 Index) const;
 const FRTSDef& Def(const FString& Id) const;
 const FRTSFaction& Faction(const FString& Id) const;
 FVector Price(const FRTSDef& D, const FString& Faction) const;
 float Duration(const FRTSDef& D, const FString& Faction) const;
 int32 RequiredAge(const FRTSDef& D,const FString& Faction) const {const int32* Age=D.FactionAges.Find(Faction);return Age?*Age:D.Age;}
 FString DisplayName(const FRTSDef& D,const FString& Faction) const;
 FString Description(const FRTSDef& D,const FString& Faction) const;
 bool IsUnitProducer(const FRTSDef& Building) const {
  if(!Building.bBuilding)return false;
  for(const auto& Unit:Defs)if(!Unit.bUpgrade&&!Unit.bBuilding&&Unit.Producer==Building.Id&&Unit.Kind!=TEXT("job")&&Unit.Kind!=TEXT("ammonite"))return true;
  return false;
 }
};

// Death delivery uses its own small wire type. A full entity snapshot carries
// private arrays and is far too large for a reliable RPC per casualty.
USTRUCT()
struct FRTSDeathEvent {
 GENERATED_BODY()
 UPROPERTY() int32 Id=0;
 UPROPERTY() int32 Owner=-1;
 UPROPERTY() int32 Def=0;
 UPROPERTY() FVector2D Pos=FVector2D::ZeroVector;
 UPROPERTY() float Heading=0;
 UPROPERTY() float Progress=1;
 UPROPERTY() bool bGate=false;
 bool NetSerialize(FArchive& Ar,class UPackageMap*,bool& bOutSuccess) {
  float X=float(Pos.X),Y=float(Pos.Y);uint8 Gate=bGate?1:0;
  Ar<<Id<<Owner<<Def<<X<<Y<<Heading<<Progress;Ar.SerializeBits(&Gate,1);
  if(Ar.IsLoading()){Pos=FVector2D(X,Y);bGate=Gate!=0;}
  bOutSuccess=!Ar.IsError();return true;
 }
};
template<> struct TStructOpsTypeTraits<FRTSDeathEvent> : TStructOpsTypeTraitsBase2<FRTSDeathEvent> {
 enum {WithNetSerializer=true};
};
namespace RTSDeath {
 constexpr int32 EventsPerBatch=16;
 constexpr int32 BatchesPerFlush=4;
 constexpr int32 PendingLimit=256;
}

USTRUCT()
struct FRTSEntityView {
 GENERATED_BODY()
 UPROPERTY() int32 Id=0;
 UPROPERTY() int32 Owner=-1;
 UPROPERTY() int32 Def=0;
 UPROPERTY() FVector2D Pos=FVector2D::ZeroVector;
 UPROPERTY() float HP=100;
 UPROPERTY() float MaxHP=100;
 UPROPERTY() float Progress=1;
 UPROPERTY() float Heading=0;
 UPROPERTY() float Cooldown=0;
 UPROPERTY() FVector Cargo=FVector::ZeroVector;
 UPROPERTY() FString Status;
 UPROPERTY() TArray<FString> Queue;
 UPROPERTY() float QueueProgress=0;
 UPROPERTY() int32 Target=0;
 UPROPERTY() int32 Tier=0;
 UPROPERTY() bool bAttacking=false;
 UPROPERTY() bool bRallySet=false;
 UPROPERTY() FVector2D RallyPoint=FVector2D::ZeroVector;
 UPROPERTY() int32 RallyTarget=0;
 UPROPERTY() bool bEconomicRallySet=false;
 UPROPERTY() FVector2D EconomicRallyPoint=FVector2D::ZeroVector;
 UPROPERTY() int32 EconomicRallyTarget=0;
 UPROPERTY() TArray<FVector2D> GalavantRoute;
 UPROPERTY() bool bRaidRallySet=false;
 UPROPERTY() FVector2D RaidRallyPoint=FVector2D::ZeroVector;
 UPROPERTY() int32 RaidRallyTarget=0;
 UPROPERTY() float AuraRadius=0;
 UPROPERTY() int32 WorkersAssigned=0;
 UPROPERTY() bool bRemembered=false;
 UPROPERTY() float ResourceEventTime=-100;
 UPROPERTY() FVector ResourceEvent=FVector::ZeroVector;
 UPROPERTY() float SpiritEvent=0;
 UPROPERTY() float CementEvent=0;
 UPROPERTY() FVector ResourceRate=FVector::ZeroVector;
 UPROPERTY() float CementRate=0;
 UPROPERTY() float SpiritRate=0;
 UPROPERTY() float EffectiveRange=0;
 UPROPERTY() float EffectiveSight=0;
 UPROPERTY() float MinimumRange=0;
 UPROPERTY() float SplashRadius=0;
 UPROPERTY() float AttackRate=0;
 UPROPERTY() float EffectiveAttack=0;
 UPROPERTY() FString DamageModifiers;
 UPROPERTY() bool bGate=false;
 UPROPERTY() bool bConverted=false;
 UPROPERTY() float AttackEventTime=-100;
 UPROPERTY() float ConversionProgress=0;
 UPROPERTY() float ProstrateRemaining=0;
 UPROPERTY() bool bStealthed=false;
 UPROPERTY() TArray<int32> PassengerDefs;
 UPROPERTY() TArray<int32> PassengerIds;
};
USTRUCT()
struct FRTSPlayerView {
 GENERATED_BODY()
 UPROPERTY() int32 LobbyResources=1;
 UPROPERTY() int32 LobbyFruitfulness=1;
 UPROPERTY() float LobbySpeed=1;
 UPROPERTY() bool bLobbyFog=true;
 UPROPERTY() int32 Slot=0;
 UPROPERTY() int32 Team=0;
 UPROPERTY() FString Name;
 UPROPERTY() FString Faction=TEXT("nephites");
 UPROPERTY() FVector Resources=FVector(300,300,200);
 UPROPERTY() float Cement=0;
 UPROPERTY() int32 Age=1;
 UPROPERTY() float Favor=20;
 UPROPERTY() int32 ReligiousUnits=0;
 UPROPERTY() int32 ReligiousStructures=0;
 UPROPERTY() FString ReligionTooltip;
 UPROPERTY() int32 Population=0;
 UPROPERTY() int32 Capacity=15;
 UPROPERTY() bool bReady=false;
 UPROPERTY() bool bDefeated=false;
 UPROPERTY() bool bConnected=true;
 UPROPERTY() bool bLiahona=true;
 UPROPERTY() float DefeatRemaining=60;
 UPROPERTY() TArray<FString> Upgrades;
 UPROPERTY() TArray<FString> HeroChoices;
};

namespace RTS {
 constexpr float MapSize=160.f;
 constexpr int32 FogSize=64;
 constexpr float Step=.1f;
 inline float FavorMaxHP(float Base,float Favor) {const float F=FMath::Clamp(Favor,0.f,100.f);return FMath::Max(1.f,FMath::RoundToFloat(F<=50?1+(Base-1)*FMath::Square(F/50):Base*FMath::Pow(100.f,(F-50)/50)));}
 inline bool IsJaredite(const FString& F) { return F==TEXT("coriantumr") || F==TEXT("shiz"); }
 inline bool IsWorker(const FRTSDef& D) { return D.Kind==TEXT("worker") || D.Kind==TEXT("job") || D.Kind==TEXT("ammonite"); }
 inline bool IsReligious(const FRTSDef& D) { return D.Id==TEXT("teacher_of_the_word")||D.Kind==TEXT("priest")||D.Kind==TEXT("lawyer"); }
 inline bool IsMilitary(const FRTSDef& D) { return !D.bBuilding&&!D.bUpgrade&&D.Attack>0&&!IsWorker(D)&&D.Kind!=TEXT("beast")&&D.Kind!=TEXT("sheep"); }
 inline bool IsInfantry(const FRTSDef& D) { return D.Kind==TEXT("infantry") || D.Kind==TEXT("ranged"); }
 inline bool IsCarrier(const FRTSDef& D) { return D.Kind==TEXT("chariot")||D.Kind==TEXT("ship")||D.Id==TEXT("elephant")||D.Id==TEXT("tents_and_habitations"); }
 inline int32 CarrierCapacity(const FRTSDef& D) { return D.Id==TEXT("tents_and_habitations")?5:D.Kind==TEXT("chariot")?6:D.Id==TEXT("jaredite_barge")?12:10; }
 inline bool IsMelee(const FRTSDef& D) { return !D.bBuilding&&D.Attack>0&&D.Range<=2&&!IsWorker(D)&&!IsReligious(D); }
 inline bool IsRanged(const FRTSDef& D) { return !D.bBuilding&&D.Attack>0&&D.Range>2; }
 inline FLinearColor PlayerColor(int32 Slot) { const FLinearColor C[]={FLinearColor(.18f,.65f,1.f),FLinearColor(1.f,.28f,.19f),FLinearColor(.36f,.9f,.53f),FLinearColor(.85f,.5f,1.f)}; return C[FMath::Clamp(Slot,0,3)]; }
 // 0 = unexplored, 1 = explored, 2 = visible. Server supplies this mask.
 bool WaterAt(FVector2D P, int32 Map);
 bool RockAt(FVector2D P, int32 Map);
 FString NormalizeChat(const FString& Text);
 bool HasProfanity(const FString& Text,bool bSubmitted);
 bool IsApology(const FString& Text);
}
