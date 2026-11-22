#pragma once
#include "CoreMinimal.h"
class ARTSController;
class ARTSHUD;

// Run after a HUD frame, including after the actual Shift+wheel input probe.
bool VerifyRTSV08Minimap(ARTSController* Controller,ARTSHUD* HUD,FString& Failure);
