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
 const auto Run=[&](const TCHAR* WallType,const TCHAR* UnitType,int32 Angle,float GridOffset,int32 MoverSlot,float ApproachOffset=0.f,float GateProgress=1.f,bool bGateEnabled=true,bool bBlockedApproach=false,int32 GateCount=1,int32 Retarget=0) {
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
   Wall->bGate=Index>=0&&Index<GateCount&&bGateEnabled;
   if(Index==0)Wall->Progress=GateProgress;
  }
  const FVector2D Start=Center-Across*(bBlockedApproach?6.5:4.)+Along*ApproachOffset;
  FVector2D Goal=Center+Across*4+Along*ApproachOffset;bool Retargeted=false;
  auto* Unit=World.Spawn(UnitType,MoverSlot,Start);
  if(bBlockedApproach)World.Spawn(TEXT("tree"),-1,Center-Across*2.8);
  const auto& UnitDef=World.D(*Unit);
  Unit->Order=TEXT("move");Unit->Rally=Goal;
  bool bCenterCorridor=true,bDirectCorridor=true;
  for(int32 Sample=0;Sample<=80;++Sample) {
   bCenterCorridor&=World.Walkable(FMath::Lerp(Center-Across*4,Center+Across*4,Sample/80.),UnitDef.Radius,Unit->Id);
   bDirectCorridor&=World.Walkable(FMath::Lerp(Start,Goal,Sample/80.),UnitDef.Radius,Unit->Id);
  }
  bool bCrossed=false,bBounded=true,bReached=false;
  double CrossOffset=-1,Travelled=0;
  const double NavHalfWidth=WallDef.GatePassage.X-UnitDef.PassageHalfSize.X;
  TArray<TSharedPtr<FJsonValue>> Trajectory;
  int32 Ticks=0;
  for(;Ticks<(bBlockedApproach?600:180);++Ticks) {
   const FVector2D Before=Unit->Pos;
   World.Time+=RTS::Step;
   bReached=World.Move(*Unit,Goal,.15f,RTS::Step);
   const double Travel=FVector2D::Distance(Before,Unit->Pos);
   Travelled+=Travel;
   bBounded&=Travel<=UnitDef.Speed*RTS::Step+.121;
   {
    auto Sample=MakeShared<FJsonObject>();Sample->SetNumberField(TEXT("x"),Unit->Pos.X);Sample->SetNumberField(TEXT("y"),Unit->Pos.Y);
    Sample->SetNumberField(TEXT("heading"),Unit->Heading);Sample->SetNumberField(TEXT("time"),World.Time);
    Trajectory.Add(MakeShared<FJsonValueObject>(Sample));
   }
   const double SideBefore=FVector2D::DotProduct(Before-Center,Across);
   const double SideAfter=FVector2D::DotProduct(Unit->Pos-Center,Across);
   if(SideBefore<0&&SideAfter>=0) {
    const FVector2D Crossing=FMath::Lerp(Before,Unit->Pos,-SideBefore/(SideAfter-SideBefore));
    CrossOffset=FMath::Abs(FVector2D::DotProduct(Crossing-Center,Along));
    bCrossed|=CrossOffset<=NavHalfWidth+.025;
   }
   if(Retarget&&!Retargeted&&SideAfter>=0) {Goal=Retarget==1?Start:Center+Along*5+Across*4;Retargeted=true;bReached=false;Unit->Path.Empty();Unit->LastPath=-100;}
   if(bReached)break;
  }
  const bool bFriendly=MoverSlot!=1;
  const bool bShouldPass=bFriendly&&GateProgress>=1&&bGateEnabled;
  const bool bPass=(!Retarget||Retargeted)&&bBounded&&(bShouldPass?(bBlockedApproach?bReached:(bCenterCorridor&&bCrossed&&bReached)):(!bCenterCorridor&&!bCrossed));
  bPass?++Passed:++Failed;
  auto Row=MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("wall"),WallType);Row->SetStringField(TEXT("unit"),UnitType);
  Row->SetNumberField(TEXT("angle_degrees"),Angle);Row->SetNumberField(TEXT("grid_offset_m"),GridOffset);
  Row->SetNumberField(TEXT("approach_offset_m"),ApproachOffset);Row->SetBoolField(TEXT("direct_corridor_clear"),bDirectCorridor);
  Row->SetNumberField(TEXT("mover_slot"),MoverSlot);Row->SetBoolField(TEXT("allied"),bFriendly);
  Row->SetNumberField(TEXT("gate_progress"),GateProgress);Row->SetBoolField(TEXT("gate_enabled"),bGateEnabled);
  Row->SetBoolField(TEXT("blocked_approach"),bBlockedApproach);Row->SetArrayField(TEXT("trajectory"),Trajectory);
  Row->SetNumberField(TEXT("gate_count"),GateCount);Row->SetNumberField(TEXT("retarget_mode"),Retarget);
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
  for(const TCHAR* Unit:{TEXT("industrious_laborer"),TEXT("elephant"),TEXT("kings_chariot")})
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
  for(const TCHAR* Unit:{TEXT("elephant"),TEXT("kings_chariot")})for(int32 Angle:{0,45,90}) {
   // Old legal offsets clipped posts; the new route must align or go around.
   Run(Wall,Unit,Angle,.5f,0,FString(Wall)==TEXT("cement_wall")?.80f:1.90f);
   Run(Wall,Unit,Angle,.5f,0,0,1.f,true,true);
   Run(Wall,Unit,Angle,.5f,0,0,1.f,true,false,3);
   Run(Wall,Unit,Angle,.5f,0,0,1.f,true,false,1,1);
   Run(Wall,Unit,Angle,.5f,0,0,1.f,true,false,1,2);
  }
 }
 // A portal cannot attract an unrelated clear move across its infinite plane,
 // nor create an invisible long corridor that blocks parallel travel.
 for(int32 Scenario=0;Scenario<2;++Scenario) {
  World.Entities.Reset();World.Obstacles.Reset();World.ObstacleCount=-1;World.NavigationCount=-1;World.NavigationTime=-100;World.Time=0;
  auto* Gate=World.Spawn(TEXT("cement_wall"),0,{40,40});Gate->bGate=true;Gate->Heading=0;
  const FVector2D Start=Scenario==0?FVector2D(60,36):FVector2D(40,37.5),Goal=Scenario==0?FVector2D(60,44):FVector2D(43,37.5);
  auto* Mover=World.Spawn(TEXT("elephant"),0,Start);Mover->Heading=Scenario==0?PI/2:0;
  bool Reached=false;double Travel=0;
  for(int32 Tick=0;Tick<160&&!Reached;++Tick){const FVector2D Before=Mover->Pos;World.Time+=RTS::Step;Reached=World.Move(*Mover,Goal,.1,RTS::Step);Travel+=FVector2D::Distance(Before,Mover->Pos);}
  const bool Pass=Reached&&Travel<=FVector2D::Distance(Start,Goal)+.6;
  Pass?++Passed:++Failed;auto Row=MakeShared<FJsonObject>();Row->SetBoolField(TEXT("probe_only"),true);Row->SetBoolField(TEXT("passed"),Pass);
  Row->SetStringField(TEXT("name"),Scenario==0?TEXT("remote_gate_does_not_attract_clear_move"):TEXT("parallel_body_clearance_has_no_artificial_corridor"));
  Row->SetNumberField(TEXT("travelled_m"),Travel);Row->SetBoolField(TEXT("reached_goal"),Reached);Cases.Add(MakeShared<FJsonValueObject>(Row));
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
