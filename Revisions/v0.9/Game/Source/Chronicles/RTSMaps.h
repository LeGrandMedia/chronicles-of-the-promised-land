#pragma once
#include "CoreMinimal.h"

// Stable map indices travel with the authoritative match snapshot. Append new
// maps; never reorder this registry or infer geography independently on clients.
struct FRTSMapInfo {
 const TCHAR* Id;
 const TCHAR* Name;
 const TCHAR* Description;
 FLinearColor Grass,Dry,Water,Rock;
 int32 GroveSpacing;
};
namespace RTS {
 int32 MapCount();
 const FRTSMapInfo& MapInfo(int32 Map);
 bool NavigableWaterAt(FVector2D Point,int32 Map);
 bool TerrainClear(FVector2D Point,float Radius,int32 Map,bool Ship=false);
 bool DockSite(FVector2D Point,float Radius,int32 Map);
 FLinearColor TerrainColor(FVector2D Point,int32 Map);
 float TerrainHeight(FVector2D Point,int32 Map);
 bool MapGroveAt(FVector2D Point,int32 Map);
}
