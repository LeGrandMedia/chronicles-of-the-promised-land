#pragma once
#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "RTSPresentation.generated.h"

// Routes OS text input (including Unicode) to the Canvas text fields.
UCLASS()
class CHRONICLES_API URTSViewport : public UGameViewportClient {
 GENERATED_BODY()
public:
 virtual void LostFocus(FViewport* InViewport) override;
 virtual bool InputChar(FViewport* InViewport,int32 ControllerId,TCHAR Character) override;
};
