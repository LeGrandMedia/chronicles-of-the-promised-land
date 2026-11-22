#include "RTSGateTests.h"
#include "RTSWorld.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

// Isolated authority movement audit. Completed, regularly spaced wall entities
// remove construction timing from the gate/grid alignment question.
bool RTSRunV09GateTraversalTests(ARTSWorld& World) {
 const auto& Catalog=FRTSCatalog::Get();
 TArray<TSharedPtr<FJsonValue>> Cases;
 int32 Passed=0,Failed=0;
 const auto Run=[&](const TCHAR* WallType,const TCHAR* UnitType,int32 Angle,float GridOffset,int32 MoverSlot,float ApproachOffset=0.f,float GateProgress=1.f,bool bGateEnabled=true) {
  World.Entities.Reset();World.Players.Reset();World.Obstacles.Reset();
  World.NextId=1;World.Map=0;World.Phase=1;World.Time=0;World.bSandbox=true;World.bFogOfWar=false;
  World.ObstacleCount=-1;World.NavigationCount=-1;World.NavigationTime=-100;
  for(int32 Slot=0;Slot<3;++Slot) {
   auto& Player=World.Players.AddDefaulted_GetRef();
   Player.Slot=Slot;Player.Team=Slot==1?1:0;Player.Age=4;
   Player.Faction=TEXT("coriantumr");Player.Resources=FVector(10000);
  }
  const auto& WallDef=Catalog.Def(WallType);
  const FVector2D Center(40+GridOffset,40+GridOffset);
  const FVector2D Along=FVector2D(1,0).GetRotated(Angle),Across(-Along.Y,Along.X);
  const float Spacing=2*WallDef.Radius+.15f;
  for(int32 Index=-3;Index<=3;++Index) {
   auto* Wall=World.Spawn(WallType,0,Center+Along*(Index*Spacing));
   Wall->Heading=FMath::DegreesToRadians(float(Angle));
   Wall->bGate=Index==0&&bGateEnabled;
   if(Index==0)Wall->Progress=GateProgress;
  }
  const FVector2D Start=Center-Across*4+Along*ApproachOffset,Goal=Center+Across*4+Along*ApproachOffset;
  auto* Unit=World.Spawn(UnitType,MoverSlot,Start);
  const auto& UnitDef=World.D(*Unit);
  Unit->Order=TEXT("move");Unit->Rally=Goal;
  bool bCenterCorridor=true,bDirectCorridor=true;
  for(int32 Sample=0;Sample<=80;++Sample) {
   bCenterCorridor&=World.Walkable(FMath::Lerp(Center-Across*4,Center+Across*4,Sample/80.),UnitDef.Radius,Unit->Id);
   bDirectCorridor&=World.Walkable(FMath::Lerp(Start,Goal,Sample/80.),UnitDef.Radius,Unit->Id);
  }
  bool bCrossed=false,bBounded=true,bReached=false;
  double CrossOffset=-1,Travelled=0;
  const double NavHalfWidth=Spacing-WallDef.Radius*.88-UnitDef.Radius;
  int32 Ticks=0;
  for(;Ticks<180;++Ticks) {
   const FVector2D Before=Unit->Pos;
   World.Time+=RTS::Step;
   bReached=World.Move(*Unit,Goal,.15f,RTS::Step);
   const double Travel=FVector2D::Distance(Before,Unit->Pos);
   Travelled+=Travel;
   bBounded&=Travel<=UnitDef.Speed*RTS::Step+.121;
   const double SideBefore=FVector2D::DotProduct(Before-Center,Across);
   const double SideAfter=FVector2D::DotProduct(Unit->Pos-Center,Across);
   if(SideBefore<0&&SideAfter>=0) {
    const FVector2D Crossing=FMath::Lerp(Before,Unit->Pos,-SideBefore/(SideAfter-SideBefore));
    CrossOffset=FMath::Abs(FVector2D::DotProduct(Crossing-Center,Along));
    bCrossed|=CrossOffset<=NavHalfWidth+.025;
   }
   if(bReached)break;
  }
  const bool bFriendly=MoverSlot!=1;
  const bool bShouldPass=bFriendly&&GateProgress>=1&&bGateEnabled;
  const bool bPass=bBounded&&(bShouldPass?(bCenterCorridor&&bCrossed&&bReached):(!bCenterCorridor&&!bCrossed));
  bPass?++Passed:++Failed;
  auto Row=MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("wall"),WallType);Row->SetStringField(TEXT("unit"),UnitType);
  Row->SetNumberField(TEXT("angle_degrees"),Angle);Row->SetNumberField(TEXT("grid_offset_m"),GridOffset);
  Row->SetNumberField(TEXT("approach_offset_m"),ApproachOffset);Row->SetBoolField(TEXT("direct_corridor_clear"),bDirectCorridor);
  Row->SetNumberField(TEXT("mover_slot"),MoverSlot);Row->SetBoolField(TEXT("allied"),bFriendly);
  Row->SetNumberField(TEXT("gate_progress"),GateProgress);Row->SetBoolField(TEXT("gate_enabled"),bGateEnabled);
  Row->SetBoolField(TEXT("passed"),bPass);Row->SetBoolField(TEXT("continuous_center_corridor_clear"),bCenterCorridor);
  Row->SetBoolField(TEXT("crossed_gate"),bCrossed);Row->SetBoolField(TEXT("reached_goal"),bReached);
  Row->SetBoolField(TEXT("bounded_movement"),bBounded);Row->SetNumberField(TEXT("crossing_offset_m"),CrossOffset);
  Row->SetNumberField(TEXT("navigation_center_half_width_m"),NavHalfWidth);
  Row->SetNumberField(TEXT("travelled_m"),Travelled);Row->SetNumberField(TEXT("elapsed_seconds"),World.Time);
  Row->SetNumberField(TEXT("remaining_distance_m"),FVector2D::Distance(Unit->Pos,Goal));
  Cases.Add(MakeShared<FJsonValueObject>(Row));
  UE_LOG(LogTemp,Display,TEXT("RTS_GATE_%s wall=%s unit=%s angle=%d offset=%.2f slot=%d corridor=%d crossed=%d reached=%d travel=%.2f"),
   bPass?TEXT("PASS"):TEXT("FAIL"),WallType,UnitType,Angle,GridOffset,MoverSlot,bCenterCorridor,bCrossed,bReached,Travelled);
 };
 for(const TCHAR* Wall:{TEXT("works_of_defense"),TEXT("cement_wall")}) {
  for(const TCHAR* Unit:{TEXT("industrious_laborer"),TEXT("elephant")})
   for(int32 Angle:{0,45,90})for(float Offset:{0.f,.5f})Run(Wall,Unit,Angle,Offset,0);
  Run(Wall,TEXT("elephant"),45,.5f,2);
  Run(Wall,TEXT("industrious_laborer"),0,0,1);
  for(const TCHAR* Unit:{TEXT("industrious_laborer"),TEXT("elephant")})
   for(int32 Angle:{0,45})Run(Wall,Unit,Angle,0,0,FString(Wall)==TEXT("cement_wall")?1.5f:2.5f);
  Run(Wall,TEXT("elephant"),45,0,1);
  for(const TCHAR* Unit:{TEXT("industrious_laborer"),TEXT("elephant")}) {
   Run(Wall,Unit,0,0,0,0,.5f);
   Run(Wall,Unit,45,.5f,0,0,1.f,false);
  }
 }
 auto Report=MakeShared<FJsonObject>();
 Report->SetNumberField(TEXT("passed"),Passed);Report->SetNumberField(TEXT("failed"),Failed);
 Report->SetStringField(TEXT("scope"),TEXT("Authority movement through completed spawned seven-segment walls at grid phases and headings; no construction, rendering, or combat claims."));
 Report->SetArrayField(TEXT("cases"),Cases);
 FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
 const FString Output=FPaths::ProjectSavedDir()/TEXT("GateTraversalTest/gate-traversal-tests.json");
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true);
 const bool bSaved=FFileHelper::SaveStringToFile(Json,*Output);
 UE_LOG(LogTemp,Display,TEXT("RTS_GATE_TRAVERSAL passed=%d failed=%d report=%s"),Passed,Failed,*Output);
 return Failed==0&&bSaved;
}
