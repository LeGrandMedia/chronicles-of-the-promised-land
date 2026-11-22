#include "RTSWallPlacementTests.h"

#include "RTSController.h"
#include "RTSWorld.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace {
constexpr double SliceHeight=.45;
constexpr double PositionTolerance=.001;
constexpr float StepSeconds=RTS::Step;
const FVector2D WallFixtureStart(40,40);

struct FWallFixture {
 const TCHAR* Faction;
 const TCHAR* Type;
 const TCHAR* Builder;
};

struct FWallSection {
 FString File;
 TArray<FVector2D> Points;
 double CenterClearance=TNumericLimits<double>::Max();
};

struct FWallChecks {
 TSharedRef<FJsonObject> Case=MakeShared<FJsonObject>();
 TArray<TSharedPtr<FJsonValue>> Assertions;
 int32 Passed=0;
 int32 Failed=0;

 void Check(const TCHAR* Name,bool bPassed) {
  auto Entry=MakeShared<FJsonObject>();
  Entry->SetStringField(TEXT("name"),Name);
  Entry->SetBoolField(TEXT("passed"),bPassed);
  Assertions.Add(MakeShared<FJsonValueObject>(Entry));
  if(bPassed) {
   ++Passed;
  } else {
   ++Failed;
   UE_LOG(LogTemp,Error,TEXT("RTS_WALL_FAIL %s %s"),*Case->GetStringField(TEXT("case")),Name);
  }
 }
};

TSharedPtr<FJsonObject> ReadWallJson(const FString& File) {
 FString Text;
 TSharedPtr<FJsonObject> Object;
 if(FFileHelper::LoadFileToString(Text,*File)) {
  FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object);
 }
 return Object;
}

FWallSection ReadWallSection(const FJsonObject& Manifest,const FString& Type,const FString& Faction,bool bGate) {
 FWallSection Result;
 const FString Key=Type+(bGate?TEXT(":gate"):TEXT(""));
 const auto Variants=Manifest.GetObjectField(TEXT("variants"));
 if(!Variants->TryGetStringField(Key+TEXT(":")+Faction,Result.File)&&
    !Variants->TryGetStringField(Key,Result.File)) {
  Manifest.GetObjectField(TEXT("models"))->TryGetStringField(Key,Result.File);
 }
 if(Result.File.IsEmpty())return Result;
 const auto Mesh=ReadWallJson(FPaths::ProjectContentDir()/TEXT("Data/Models")/Result.File);
 if(!Mesh)return Result;

 for(const auto& Value:Mesh->GetArrayField(TEXT("parts"))) {
  const auto Part=Value->AsObject();
  TArray<FVector> Vertices;
  for(const auto& Item:Part->GetArrayField(TEXT("vertices"))) {
   const auto& V=Item->AsArray();
   Vertices.Emplace(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());
  }
  const auto& Indices=Part->GetArrayField(TEXT("triangles"));
  for(int32 I=0;I+2<Indices.Num();I+=3) {
   double MinX=TNumericLimits<double>::Max(),MaxX=-MinX;
   auto AddPoint=[&](const FVector& P) {
    Result.Points.Emplace(P.X,P.Y);
    MinX=FMath::Min(MinX,P.X);
    MaxX=FMath::Max(MaxX,P.X);
   };
   for(int32 Edge=0;Edge<3;++Edge) {
    const FVector& A=Vertices[int32(Indices[I+Edge]->AsNumber())];
    const FVector& B=Vertices[int32(Indices[I+(Edge+1)%3]->AsNumber())];
    if(FMath::IsNearlyEqual(A.Z,SliceHeight,1.e-8))AddPoint(A);
    if((A.Z<SliceHeight&&B.Z>SliceHeight)||(B.Z<SliceHeight&&A.Z>SliceHeight)) {
     AddPoint(FMath::Lerp(A,B,(SliceHeight-A.Z)/(B.Z-A.Z)));
    }
   }
   if(MinX<=MaxX) {
    const double Offset=MinX<=0&&MaxX>=0?0:FMath::Min(FMath::Abs(MinX),FMath::Abs(MaxX));
    Result.CenterClearance=FMath::Min(Result.CenterClearance,Offset);
   }
  }
 }
 return Result;
}

double ProjectedSectionGap(const FWallSection& Section,const FRTSEntity& A,const FRTSEntity& B) {
 const FVector2D Between=B.Pos-A.Pos;
 const FVector2D Axis=Between.GetSafeNormal();
 const double YawA=FMath::RadiansToDegrees(A.Heading),YawB=FMath::RadiansToDegrees(B.Heading);
 double ReachA=-TNumericLimits<double>::Max(),StartB=TNumericLimits<double>::Max();
 for(const auto& Point:Section.Points) {
  ReachA=FMath::Max(ReachA,FVector2D::DotProduct(Point.GetRotated(YawA),Axis));
  StartB=FMath::Min(StartB,FVector2D::DotProduct(Point.GetRotated(YawB),Axis));
 }
 return Between.Size()+StartB-ReachA;
}

void ResetWallWorld(ARTSWorld& World,ARTSController& Controller,const FWallFixture& Fixture) {
 World.Entities.Reset();
 World.Players.Reset(3);
 World.NextId=1;
 World.Map=0;
 World.Phase=1;
 World.Time=0;
 World.bSandbox=true;
 World.bFogOfWar=false;
 World.NavigationCount=-1;
 World.NavigationTime=-100;
 World.ObstacleCount=-1;
 World.Obstacles.Reset();
 World.bDispatchingQueuedOrder=false;
 Controller.ServerSlot=0;
 for(int32 Slot=0;Slot<3;++Slot) {
  FRTSPlayer& Player=World.Players.AddDefaulted_GetRef();
  Player.Slot=Slot;
  Player.Team=Slot==1?1:0;
  Player.Faction=Slot==0?Fixture.Faction:TEXT("nephites");
  Player.Age=4;
  Player.Resources=FVector(10000);
  Player.Cement=1000;
  if(Slot==0) {
   Player.Controller=&Controller;
   Player.Upgrades.Add(TEXT("heaps_of_earth"));
  }
 }
 World.Spawn(TEXT("seat_of_the_people"),0,{20,20});
 World.Spawn(TEXT("seat_of_the_people"),1,{140,20});
 World.Spawn(TEXT("seat_of_the_people"),2,{20,140});
 World.Spawn(TEXT("house_of_curious_workmanship"),0,{25,20});
}

TArray<FRTSEntity*> FindWalls(ARTSWorld& World,const FRTSDef& Wall) {
 TArray<FRTSEntity*> Walls;
 for(const auto& Entry:World.Entities) {
  if(Entry.Value->Owner==0&&World.D(*Entry.Value).Id==Wall.Id)Walls.Add(Entry.Value.Get());
 }
 Walls.Sort([](const FRTSEntity& A,const FRTSEntity& B){return A.Id<B.Id;});
 return Walls;
}

void IssueWallLine(ARTSWorld& World,ARTSController& Controller,int32 BuilderId,const FRTSDef& Wall,
                   const FVector2D& From,const FVector2D& To,bool bQueued) {
 const FString Type=FString::Printf(TEXT("%s|%.4f|%.4f"),*Wall.Id,From.X,From.Y);
 World.Command(&Controller,TEXT("wall_line"),{BuilderId},To,0,Type,bQueued);
}

bool QueueMatches(const FRTSEntity& Builder,const TArray<FRTSEntity*>& Walls,int32 FirstIndex) {
 if(Builder.Order!=TEXT("build")||Builder.Target!=Walls[FirstIndex]->Id||
    Builder.PendingOrders.Num()!=Walls.Num()-FirstIndex-1)return false;
 for(int32 Index=0;Index<Builder.PendingOrders.Num();++Index) {
  const auto& Job=Builder.PendingOrders[Index];
  const auto* Site=Walls[FirstIndex+Index+1];
  if(Job.Action!=TEXT("context")||Job.Target!=Site->Id||!Job.Point.Equals(Site->Pos,PositionTolerance))return false;
 }
 return true;
}

template<class FCondition>
bool AdvanceUntil(ARTSWorld& World,int32 Seconds,FCondition Condition) {
 for(int32 Tick=0;Tick<Seconds/StepSeconds&&!Condition();++Tick) {
  World.Step(StepSeconds);
 }
 return Condition();
}

bool WallsComplete(const TArray<FRTSEntity*>& Walls) {
 return !Walls.ContainsByPredicate([](const FRTSEntity* Wall){return Wall->Progress<1.f;});
}

TSharedRef<FJsonObject> BuilderState(const ARTSWorld& World,const FRTSEntity& Builder,const TArray<FRTSEntity*>& Walls) {
 auto State=MakeShared<FJsonObject>();
 State->SetNumberField(TEXT("time"),World.Time);
 State->SetNumberField(TEXT("x"),Builder.Pos.X);
 State->SetNumberField(TEXT("y"),Builder.Pos.Y);
 State->SetStringField(TEXT("order"),Builder.Order);
 State->SetNumberField(TEXT("target"),Builder.Target);
 State->SetNumberField(TEXT("queued_orders"),Builder.PendingOrders.Num());
 State->SetNumberField(TEXT("last_move_time"),Builder.LastMove);
 State->SetNumberField(TEXT("path_points"),Builder.Path.Num());

 const auto& BuilderDef=World.D(Builder);
 const FRTSEntity* Enclosing=nullptr;
 double GreatestDepth=0;
 TArray<TSharedPtr<FJsonValue>> WallDistances;
 for(const auto* Site:Walls) {
  const double Distance=FVector2D::Distance(Site->Pos,Builder.Pos);
  const double Depth=World.D(*Site).Radius*.88f+BuilderDef.Radius+.03f-Distance;
  if(Depth>GreatestDepth) {
   GreatestDepth=Depth;
   Enclosing=Site;
  }
  auto Wall=MakeShared<FJsonObject>();
  Wall->SetNumberField(TEXT("id"),Site->Id);
  Wall->SetNumberField(TEXT("distance_m"),Distance);
  Wall->SetNumberField(TEXT("escape_boundary_depth_m"),Depth);
  WallDistances.Add(MakeShared<FJsonValueObject>(Wall));
 }
 State->SetArrayField(TEXT("wall_distances"),WallDistances);
 if(!Enclosing)return State;

 State->SetNumberField(TEXT("enclosing_wall"),Enclosing->Id);
 const FVector2D Away=(Builder.Pos-Enclosing->Pos).GetSafeNormal();
 const double Distance=FVector2D::Distance(Builder.Pos,Enclosing->Pos);
 const double Travel=FMath::Min(double(BuilderDef.Speed*StepSeconds),GreatestDepth);
 TArray<TSharedPtr<FJsonValue>> Exits;
 for(const double Angle:{0.,22.5,-22.5,45.,-45.,67.5,-67.5,90.,-90.}) {
  const FVector2D Candidate=Builder.Pos+Away.GetRotated(Angle)*Travel;
  auto Check=MakeShared<FJsonObject>();
  Check->SetNumberField(TEXT("angle_degrees_from_directly_away"),Angle);
  Check->SetBoolField(TEXT("moves_outward"),FVector2D::Distance(Candidate,Enclosing->Pos)>Distance);
  Check->SetBoolField(TEXT("walkable_except_enclosing_wall"),World.Walkable(Candidate,BuilderDef.Radius,Enclosing->Id));
  Exits.Add(MakeShared<FJsonValueObject>(Check));
 }
 State->SetArrayField(TEXT("exit_candidates"),Exits);
 return State;
}

double NavigationHalfOpening(const ARTSWorld& World,const FRTSEntity& Gate,const FRTSEntity& Mover,double Spacing) {
 const float Radius=World.D(Mover).Radius;
 double LastClear=0;
 for(double Distance=.01;Distance<Spacing;Distance+=.01) {
  if(!World.Walkable(Gate.Pos+FVector2D(Distance,0),Radius,Mover.Id))break;
  LastClear=Distance;
 }
 return LastClear;
}

double MoveThroughGate(ARTSWorld& World,ARTSController& Controller,FRTSEntity& Mover,const FRTSEntity& Gate) {
 // These probes use the straight segment on the first east-west leg.
 const FVector2D Goal=Gate.Pos+FVector2D(0,4);
 Mover.Pos=Gate.Pos-FVector2D(0,4);
 Controller.ServerSlot=Mover.Owner;
 World.Command(&Controller,TEXT("move"),{Mover.Id},Goal,0,FString());
 Controller.ServerSlot=0;
 double Crossing=-1;
 for(int32 Tick=0;Tick<150&&Mover.Order==TEXT("move");++Tick) {
  const FVector2D Before=Mover.Pos;
  World.Step(StepSeconds);
  if(Before.Y<Gate.Pos.Y&&Mover.Pos.Y>=Gate.Pos.Y) {
   const double At=FMath::Lerp(Before.X,Mover.Pos.X,(Gate.Pos.Y-Before.Y)/(Mover.Pos.Y-Before.Y));
   Crossing=FMath::Abs(At-Gate.Pos.X);
  }
 }
 return Crossing;
}

void TestGate(ARTSWorld& World,ARTSController& Controller,FWallChecks& Report,FRTSEntity& Gate,double Spacing,const FWallSection& GateArt) {
 const FRTSCatalog& Catalog=FRTSCatalog::Get();
 const FRTSDef& Upgrade=Catalog.Def(TEXT("gate_segment_upgrade"));
 const FVector2D OriginalCenter=Gate.Pos;
 const float OriginalHeading=Gate.Heading;
 auto& Player=World.Players[0];
 const FVector Before=Player.Resources;
 World.Command(&Controller,TEXT("research"),{Gate.Id},Gate.Pos,0,Upgrade.Id);
 Report.Check(TEXT("gate_research_queues_and_charges_once"),!Gate.bGate&&Gate.Jobs.Num()==1&&
              Gate.Jobs[0].Type==Upgrade.Id&&Player.Resources.Equals(Before-Catalog.Price(Upgrade,Player.Faction),.001));
 const bool bUpgraded=AdvanceUntil(World,30,[&]{return Gate.bGate;});
 Report.Check(TEXT("gate_research_completes_in_place"),bUpgraded&&Gate.Jobs.IsEmpty()&&
              Gate.Pos.Equals(OriginalCenter,PositionTolerance)&&Gate.Heading==OriginalHeading&&
              !World.Has(Gate.Owner,Upgrade.Id));

 FRTSEntity* Mover=World.Spawn(TEXT("industrious_laborer"),0,{36,30});
 const float Radius=World.D(*Mover).Radius;
 const double NavOpening=NavigationHalfOpening(World,Gate,*Mover,Spacing);
 const double ArtOpening=GateArt.CenterClearance;
 Report.Case->SetNumberField(TEXT("gate_aperture_m"),ArtOpening*2);
 Report.Case->SetNumberField(TEXT("gate_art_worker_center_half_width_m"),FMath::Max(0.,ArtOpening-Radius));
 Report.Case->SetNumberField(TEXT("gate_navigation_worker_center_half_width_m"),NavOpening);
 World.Entities.Remove(Mover->Id);
 constexpr const TCHAR* CenterChecks[]={TEXT("gate_center_passes_owner"),TEXT("gate_center_blocks_enemy"),TEXT("gate_center_passes_ally")};
 constexpr const TCHAR* MoveChecks[]={TEXT("owner_move_crosses_gate"),TEXT("enemy_move_stays_outside_gate"),TEXT("ally_move_crosses_gate")};
 constexpr const TCHAR* CrossingFields[]={TEXT("owner_crossing_offset_m"),TEXT("enemy_crossing_offset_m"),TEXT("ally_crossing_offset_m")};
 for(int32 Slot=0;Slot<3;++Slot) {
  Mover=World.Spawn(TEXT("industrious_laborer"),Slot,{36,30});
  const bool bFriendly=Slot!=1;
  Report.Check(CenterChecks[Slot],World.Walkable(Gate.Pos,Radius,Mover->Id)==bFriendly);
  const double Crossing=MoveThroughGate(World,Controller,*Mover,Gate);
  Report.Case->SetNumberField(CrossingFields[Slot],Crossing);
  const bool bUsedGate=Crossing>=0&&Crossing<=NavOpening+.02;
  Report.Check(MoveChecks[Slot],bUsedGate==bFriendly);
  World.Entities.Remove(Mover->Id);
 }
 Report.Check(TEXT("gate_center_still_blocks_placement"),!World.Walkable(Gate.Pos,Radius));
}

void RunWallCase(ARTSWorld& World,ARTSController& Controller,const FWallFixture& Fixture,
                 const FWallSection& Art,const FWallSection& GateArt,int32 Degrees,bool bCompleteCorner,FWallChecks& Report) {
 Report.Case->SetStringField(TEXT("case"),FString::Printf(TEXT("%s-%d-%s"),Fixture.Faction,Degrees,bCompleteCorner?TEXT("complete"):TEXT("foundation")));
 Report.Case->SetStringField(TEXT("wall_model"),Art.File);
 Report.Case->SetStringField(TEXT("gate_model"),GateArt.File);
 Report.Check(TEXT("authored_wall_and_gate_cross_sections_load"),!Art.Points.IsEmpty()&&!GateArt.Points.IsEmpty()&&GateArt.CenterClearance>0);
 const FRTSDef& Wall=FRTSCatalog::Get().Def(Fixture.Type);
 const float Spacing=2*Wall.Radius+.15f;
 const FVector2D Corner=WallFixtureStart+FVector2D(2*Spacing,0);
 const FVector2D Direction=FVector2D(1,0).GetRotated(Degrees);
 ResetWallWorld(World,Controller,Fixture);
 FRTSEntity* Builder=World.Spawn(Fixture.Builder,0,WallFixtureStart-FVector2D(4,3));
 auto& Player=World.Players[0];
 FVector Before=Player.Resources;
 float CementBefore=Player.Cement;
 IssueWallLine(World,Controller,Builder->Id,Wall,WallFixtureStart,Corner,false);
 auto Walls=FindWalls(World,Wall);
 Report.Check(TEXT("first_leg_creates_and_queues_three_foundations"),Walls.Num()==3&&QueueMatches(*Builder,Walls,0)&&
              !Walls.ContainsByPredicate([](const FRTSEntity* Site){return Site->Progress>=1.f;}));
 Report.Check(TEXT("first_leg_charges_three_segments"),Player.Resources.Equals(Before-Wall.Cost*3,.001)&&
              FMath::IsNearlyEqual(Player.Cement,CementBefore-Wall.CementCost*3,.001f));
 if(Walls.Num()!=3)return;
 const int32 CornerId=Walls.Last()->Id;
 if(bCompleteCorner) {
  AdvanceUntil(World,100,[&]{return WallsComplete(Walls);});
  World.Command(&Controller,TEXT("stop"),{Builder->Id},Builder->Pos,0,FString());
 }
 Report.Check(TEXT("corner_starts_in_requested_state"),(Walls.Last()->Progress>=1)==bCompleteCorner);
 Report.Case->SetObjectField(TEXT("builder_before_queued_leg"),BuilderState(World,*Builder,Walls));
 Before=Player.Resources;
 CementBefore=Player.Cement;
 IssueWallLine(World,Controller,Builder->Id,Wall,Corner,Corner+Direction*(2*Spacing),true);
 Walls=FindWalls(World,Wall);
 Report.Check(TEXT("queued_leg_preserves_corner_id_and_new_order_targets"),Walls.Num()==5&&Walls[2]->Id==CornerId&&
              QueueMatches(*Builder,Walls,bCompleteCorner?3:0));
 Report.Check(TEXT("queued_leg_charges_only_two_new_segments"),Player.Resources.Equals(Before-Wall.Cost*2,.001)&&
              FMath::IsNearlyEqual(Player.Cement,CementBefore-Wall.CementCost*2,.001f));
 if(Walls.Num()!=5)return;
 Report.Case->SetObjectField(TEXT("builder_after_queued_leg"),BuilderState(World,*Builder,Walls));
 bool bSnapped=true;
 TArray<TSharedPtr<FJsonValue>> Centers;
 for(int32 I=0;I<Walls.Num();++I) {
  const FVector2D Expected=I<3?WallFixtureStart+FVector2D(I*Spacing,0):Corner+Direction*((I-2)*Spacing);
  const double Heading=FMath::RadiansToDegrees(Walls[I]->Heading);
  bSnapped&=Walls[I]->Pos.Equals(Expected,PositionTolerance)&&FMath::IsNearlyEqual(Heading,I<3?0.:double(Degrees),.001);
  auto Center=MakeShared<FJsonObject>();
  Center->SetNumberField(TEXT("id"),Walls[I]->Id);
  Center->SetNumberField(TEXT("x"),Walls[I]->Pos.X);
  Center->SetNumberField(TEXT("y"),Walls[I]->Pos.Y);
  Center->SetNumberField(TEXT("heading_degrees"),Heading);
  Centers.Add(MakeShared<FJsonValueObject>(Center));
 }
 Report.Case->SetArrayField(TEXT("authoritative_centers"),Centers);
 Report.Check(TEXT("authoritative_centers_and_headings_snap_without_turning_corner"),bSnapped);
 const bool bBuilt=AdvanceUntil(World,120,[&]{return WallsComplete(Walls);});
 Report.Check(TEXT("actual_simulation_finishes_both_wall_legs"),bBuilt);
 Report.Case->SetObjectField(TEXT("builder_after_construction"),BuilderState(World,*Builder,Walls));
 Report.Case->SetNumberField(TEXT("simulation_seconds_before_gate"),World.Time);
 TArray<TSharedPtr<FJsonValue>> Progress;
 for(const auto* Site:Walls)Progress.Add(MakeShared<FJsonValueNumber>(Site->Progress));
 Report.Case->SetArrayField(TEXT("segment_progress"),Progress);

 FRTSEntity* Laborer=World.Spawn(TEXT("industrious_laborer"),0,{36,30});
 const FVector2D Seam=(Walls[2]->Pos+Walls[3]->Pos)*.5;
 Report.Check(TEXT("worker_collision_closes_wall_join"),!World.Walkable(Seam,World.D(*Laborer).Radius,Laborer->Id));
 World.Entities.Remove(Laborer->Id);
 if(!Art.Points.IsEmpty()) {
  const double Gap=ProjectedSectionGap(Art,*Walls[2],*Walls[3]);
  Report.Case->SetNumberField(TEXT("authored_join_projected_gap_m"),Gap);
  Report.Case->SetBoolField(TEXT("authored_presentation_gap"),Degrees>0&&Gap>.01);
 }
 if(Degrees==90&&bBuilt&&!GateArt.Points.IsEmpty()) {
  TestGate(World,Controller,Report,*Walls[1],Spacing,GateArt);
 }
}
} // namespace

bool RTSRunV09WallPlacementTests(ARTSWorld& World) {
 auto* Controller=World.GetWorld()->SpawnActor<ARTSController>();
 const auto Manifest=ReadWallJson(FPaths::ProjectContentDir()/TEXT("Data/Models/models.json"));
 if(!Controller||!Manifest)return false;
 const FWallFixture Fixtures[]={
  {TEXT("nephites"),TEXT("works_of_defense"),TEXT("industrious_laborer")},
  {TEXT("lamanites"),TEXT("works_of_defense"),TEXT("industrious_laborer")},
  {TEXT("coriantumr"),TEXT("cement_wall"),TEXT("elephant")}
 };
 auto Result=MakeShared<FJsonObject>();
 TArray<TSharedPtr<FJsonValue>> Cases;
 int32 Passed=0,Failed=0;
 for(const auto& Fixture:Fixtures) {
  const auto Art=ReadWallSection(*Manifest,Fixture.Type,Fixture.Faction,false);
  const auto GateArt=ReadWallSection(*Manifest,Fixture.Type,Fixture.Faction,true);
  for(const int32 Degrees:{0,45,90}) {
   for(const bool bCompleteCorner:{false,true}) {
    FWallChecks Report;
    RunWallCase(World,*Controller,Fixture,Art,GateArt,Degrees,bCompleteCorner,Report);
    Passed+=Report.Passed;
    Failed+=Report.Failed;
    Report.Case->SetArrayField(TEXT("assertions"),Report.Assertions);
    Cases.Add(MakeShared<FJsonValueObject>(Report.Case));
   }
  }
 }
 Result->SetNumberField(TEXT("passed"),Passed);
 Result->SetNumberField(TEXT("failed"),Failed);
 Result->SetNumberField(TEXT("authored_slice_height_m"),SliceHeight);
 Result->SetStringField(TEXT("geometry_scope"),TEXT("Base asset sections transformed by authoritative centers and headings, before connector rendering."));
 Result->SetArrayField(TEXT("cases"),Cases);
 FString Json;
 FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
 const FString Output=FPaths::ProjectSavedDir()/TEXT("WallPlacementTest/wall-placement-tests.json");
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true);
 const bool bSaved=FFileHelper::SaveStringToFile(Json,*Output);
 UE_LOG(LogTemp,Display,TEXT("RTS_WALL_PLACEMENT passed=%d failed=%d report=%s"),Passed,Failed,*Output);
 return Failed==0&&bSaved;
}
