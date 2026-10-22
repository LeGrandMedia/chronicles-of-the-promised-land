#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RTSGameMode.generated.h"
class ARTSWorld;
UCLASS()
class CHRONICLES_API ARTSGameMode : public AGameModeBase {
 GENERATED_BODY()
public:
 ARTSGameMode();
 virtual void BeginPlay() override;
 virtual void PostLogin(APlayerController* NewPlayer) override;
 virtual void Logout(AController* Exiting) override;
 UPROPERTY() ARTSWorld* Simulation=nullptr;
};
