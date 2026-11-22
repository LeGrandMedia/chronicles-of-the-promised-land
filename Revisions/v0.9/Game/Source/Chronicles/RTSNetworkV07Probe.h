#pragma once
#include "CoreMinimal.h"
class ARTSController;
class ARTSWorld;
// Test-only observation around the real command RPC; no ordinary match changes.
void RTSNetworkV07OrderAudit(ARTSController* Controller,ARTSWorld* World,const FString& Action,const TArray<int32>& Ids,const FString& Type,bool bAfter);
