#include "RTSWallJunctionTests.h"

#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSScene3D.h"
#include "RTSWorld.h"
#include "Algo/Reverse.h"
#include "Components/MeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace {
constexpr double JunctionSliceCm=45.;
constexpr double JunctionHeightToleranceCm=.06;
constexpr int32 JunctionFixtureOwner=1;

struct FJunctionStyle {
 const TCHAR* Faction;
 const TCHAR* Type;
 const TCHAR* Builder;
};

constexpr FJunctionStyle JunctionStyles[]={
 {TEXT("nephites"),TEXT("works_of_defense"),TEXT("industrious_laborer")},
 {TEXT("lamanites"),TEXT("works_of_defense"),TEXT("industrious_laborer")},
 {TEXT("coriantumr"),TEXT("cement_wall"),TEXT("elephant")}
};

struct FJunctionLayout {
 int32 Angle=0;
 int32 BackId=0;
 int32 CornerId=0;
 int32 NeighborId=0;
 FIntPoint Pick=FIntPoint(-1,-1);
 double CompletedHeight=0.;
 uint32 NearCompletePose=0;
};

template<class FSectionPoint>
void VisitJunctionSlice(const TArray<FVector>& Vertices,const TArray<int32>& Indices,int32 First,int32 Count,
                       const FTransform& Transform,FSectionPoint Visit) {
 for(int32 I=First;I+2<First+Count;I+=3) {
  const FVector P[]={Transform.TransformPosition(Vertices[Indices[I]]),Transform.TransformPosition(Vertices[Indices[I+1]]),
                     Transform.TransformPosition(Vertices[Indices[I+2]])};
  for(int32 Edge=0;Edge<3;++Edge) {
   const FVector& A=P[Edge];
   const FVector& B=P[(Edge+1)%3];
   if(FMath::IsNearlyEqual(A.Z,JunctionSliceCm,1.e-7))Visit(A);
   if((A.Z<JunctionSliceCm&&B.Z>JunctionSliceCm)||(B.Z<JunctionSliceCm&&A.Z>JunctionSliceCm)) {
    Visit(FMath::Lerp(A,B,(JunctionSliceCm-A.Z)/(B.Z-A.Z)));
   }
  }
 }
}

struct FJunctionSection {
 double Min=TNumericLimits<double>::Max();
 double Max=-TNumericLimits<double>::Max();

 void Add(double Distance) {Min=FMath::Min(Min,Distance);Max=FMath::Max(Max,Distance);}
 bool IsValid() const {return Min<=Max;}
};

FJunctionSection ProjectJunctionSlice(const TArray<FVector>& Vertices,const TArray<int32>& Indices,int32 First,int32 Count,
                                      const FTransform& Transform,const FVector& Axis,const FVector& Origin) {
 FJunctionSection Section;
 VisitJunctionSlice(Vertices,Indices,First,Count,Transform,[&](const FVector& P){Section.Add(FVector::DotProduct(P-Origin,Axis));});
 return Section;
}
}

class FRTSWallJunctionProbe {
 enum class EStage {Initialize,Full,Rotated,Branch,Gates,Isolated,Partial,NearComplete,Complete,Remembered,WithheldBack,Withheld};
 EStage Stage=EStage::Initialize;
 TArray<TSharedPtr<FJsonValue>> Checks;
 TArray<TSharedPtr<FJsonValue>> Screenshots;
 FJunctionLayout Layouts[3];
 int32 StyleIndex=0,Frame=0,StageFrame=0,Passed=0,Failed=0;
 int32 ExtraGateId=0;
 uint32 FullRevision=0;
 double Started=FPlatformTime::Seconds(),StageStarted=Started;
 float ExpectDrawAfter=0;
 float CameraAngle=0;
 FColor FullColor=FColor::Black;
 bool bFinished=false;

 void Check(const FString& Suffix,bool bPassed,const FString& Detail=FString()) {
  const FString Name=FString(JunctionStyles[StyleIndex].Faction)+TEXT(".")+Suffix;
  auto CheckRow=MakeShared<FJsonObject>();
  CheckRow->SetStringField(TEXT("name"),Name);
  CheckRow->SetStringField(TEXT("detail"),Detail);
  CheckRow->SetBoolField(TEXT("passed"),bPassed);
  Checks.Add(MakeShared<FJsonValueObject>(CheckRow));
  if(bPassed)++Passed;else ++Failed;
  UE_LOG(LogTemp,Display,TEXT("RTS_WALL_JUNCTION_%s %s %s"),bPassed?TEXT("PASS"):TEXT("FAIL"),*Name,*Detail);
 }

 void Finish() {
  bFinished=true;
  auto Result=MakeShared<FJsonObject>();
  Result->SetNumberField(TEXT("passed"),Passed);
  Result->SetNumberField(TEXT("failed"),Failed);
  Result->SetNumberField(TEXT("measured_section_height_cm"),JunctionSliceCm);
  Result->SetArrayField(TEXT("checks"),Checks);
  Result->SetArrayField(TEXT("screenshots"),Screenshots);
  Result->SetStringField(TEXT("scope"),TEXT("Standalone rendered viewport with server-placed wall lines and authoritative filtered snapshots. Construction values are pinned for comparison."));
  FString Json;
  FJsonSerializer::Serialize(Result,TJsonWriterFactory<>::Create(&Json));
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("Tests");
  IFileManager::Get().MakeDirectory(*Folder,true);
  const bool bSaved=FFileHelper::SaveStringToFile(Json,*(Folder/TEXT("wall-junction-tests.json")));
  UE_LOG(LogTemp,Display,TEXT("RTS_WALL_JUNCTION_TEST passed=%d failed=%d saved=%d"),Passed,Failed,bSaved);
  FPlatformMisc::RequestExitWithStatus(Failed>0||!bSaved,Failed>0||!bSaved?1:0);
 }

 void SetStage(EStage Next) {
  Stage=Next;
  StageFrame=Frame;
  StageStarted=FPlatformTime::Seconds();
 }

 void SetProgress(ARTSWorld& World,FRTSEntity& Wall,float Progress) {
  Wall.Progress=Progress;
  Wall.MaxHP=World.MaxHP(Wall);
  Wall.HP=Wall.MaxHP;
 }

 bool PlaceLayout(ARTSWorld& World,ARTSController& C,const FJunctionStyle& Style,FJunctionLayout& Layout,int32 Angle,const FVector2D& Start) {
  const auto& Def=FRTSCatalog::Get().Def(Style.Type);
  const float Spacing=2*Def.Radius+.15f;
  const FVector2D Turn=Start+FVector2D(2*Spacing,0);
  const FVector2D Direction=FVector2D(1,0).GetRotated(Angle);
  auto* Builder=World.Spawn(Style.Builder,JunctionFixtureOwner,Start-FVector2D(3,2));
  if(!Builder)return false;
  auto Command=[&](const FVector2D& From,const FVector2D& To,bool bQueued) {
   const FString Payload=FString::Printf(TEXT("%s|%.4f|%.4f"),*Def.Id,From.X,From.Y);
   World.Command(&C,TEXT("wall_line"),{Builder->Id},To,0,Payload,bQueued);
  };
  const int32 First=World.NextId;
  Command(Start,Turn,false);
  Command(Turn,Turn+Direction*(2*Spacing),true);
  Layout=FJunctionLayout{};
  Layout.Angle=Angle;
  Layout.BackId=First+1;
  Layout.CornerId=First+2;
  Layout.NeighborId=First+3;
  bool bPlaced=World.NextId==First+5;
  for(int32 I=0;I<5;++I) {
   FRTSEntity* Wall=World.Find(First+I);
   if(!Wall) {bPlaced=false;continue;}
   const FVector2D Expected=I<3?Start+FVector2D(I*Spacing,0):Turn+Direction*((I-2)*Spacing);
   bPlaced&=World.D(*Wall).Id==Def.Id&&Wall->Owner==JunctionFixtureOwner&&Wall->Progress<1&&
            Wall->Pos.Equals(Expected,.001)&&FMath::IsNearlyEqual(Wall->Heading,I<3?0.f:FMath::DegreesToRadians(float(Angle)),.0001f);
   SetProgress(World,*Wall,1);
  }
  return bPlaced;
 }

 void Snapshot(ARTSWorld& World) {
  World.Time+=RTS::Step;
  World.Snapshot();
 }

 bool Initialize(ARTSWorld& World,ARTSController& C) {
  const auto& Style=JunctionStyles[StyleIndex];
  CameraAngle=0;
  World.SetActorTickEnabled(false);
  World.Entities.Reset();
  World.Obstacles.Reset();
  World.ObstacleCount=-1;
  World.NextId=1;
  World.Players.Reset();
  World.Players.AddDefaulted(2);
  World.Map=0;
  World.Phase=1;
  World.Time=0;
  World.bSandbox=true;
  World.bFogOfWar=false;
  World.bDispatchingQueuedOrder=false;
  for(int32 Slot=0;Slot<World.Players.Num();++Slot) {
   auto& Player=World.Players[Slot];
   Player.Slot=Slot;
   Player.Team=Slot;
   Player.Faction=Style.Faction;
   Player.Age=4;
   Player.Resources=FVector(10000);
   Player.Cement=1000;
  }
  World.Players[0].Controller=&C;
  World.Players[JunctionFixtureOwner].Upgrades.Add(TEXT("heaps_of_earth"));
  World.Spawn(TEXT("seat_of_the_people"),JunctionFixtureOwner,{18,20});
  World.Spawn(TEXT("house_of_curious_workmanship"),JunctionFixtureOwner,{24,20});
  C.bMenu=false;
  C.bPauseMenu=false;
  C.bHelp=false;
  C.ServerSlot=JunctionFixtureOwner;
  bool bAllPlaced=true;
  const bool bPlaced[]={PlaceLayout(World,C,Style,Layouts[0],0,{40,48}),
                       PlaceLayout(World,C,Style,Layouts[1],45,{40,61}),
                       PlaceLayout(World,C,Style,Layouts[2],90,{40,75})};
  for(int32 I=0;I<3;++I) {
   Check(FString::Printf(TEXT("%d.server_placed_five_segments"),Layouts[I].Angle),bPlaced[I]);
   bAllPlaced&=bPlaced[I];
  }
  C.ServerSlot=0;
  C.DisplayPositions.Reset();
  C.DeathAnimations.Reset();
  C.Selection.Reset();
  Snapshot(World);
  ExpectDrawAfter=C.GetWorld()->GetTimeSeconds();
  SetStage(EStage::Full);
  return bAllPlaced;
 }

 const FRTSWallJoint3D* JointFor(const ARTSScene3D& Scene,const FJunctionLayout& Layout) const {
  return Scene.WallJointEntries.FindByPredicate([&](const FRTSWallJoint3D& Joint){
   return Joint.EntityId==Layout.CornerId&&Joint.NeighborId==Layout.NeighborId;
  });
 }

 bool CornerUsesReplacement(const ARTSScene3D& Scene,const FJunctionLayout& Layout) const {
  const auto* Corner=Scene.Rendered.Find(Layout.CornerId);
  return Corner&&Corner->Component()&&!Corner->Component()->IsVisible()&&Scene.TrimmedWallCorners.Contains(Layout.CornerId);
 }

 void CheckSection(ARTSScene3D& Scene,const ARTSController& C,FJunctionLayout& Layout) {
  const FString Label=FString::Printf(TEXT("%d.section"),Layout.Angle);
  const auto* Corner=Scene.Rendered.Find(Layout.CornerId);
  const auto* Neighbor=Scene.Rendered.Find(Layout.NeighborId);
  const auto* Joint=JointFor(Scene,Layout);
  const auto* ViewCorner=C.FindEntity(Layout.CornerId);
  const auto* ViewNeighbor=C.FindEntity(Layout.NeighborId);
  if(!Corner||!Neighbor||!Corner->Component()||!Neighbor->Component()||!ViewCorner||!ViewNeighbor) {
   Check(Label,false,TEXT("Server placements missing from rendered snapshot"));
   return;
  }
  const FVector Axis((ViewNeighbor->Pos-ViewCorner->Pos).GetSafeNormal(),0),Origin=FVector(ViewCorner->Pos,0)*100.;
  const auto A=ProjectJunctionSlice(Corner->Vertices,Corner->Triangles,0,Corner->Triangles.Num(),Corner->Component()->GetComponentTransform(),Axis,Origin);
  const auto B=ProjectJunctionSlice(Neighbor->Vertices,Neighbor->Triangles,0,Neighbor->Triangles.Num(),Neighbor->Component()->GetComponentTransform(),Axis,Origin);
  const double AuthoredGap=A.IsValid()&&B.IsValid()?B.Min-A.Max:TNumericLimits<double>::Max();
  if(Layout.Angle==0) {
   Check(Label,AuthoredGap<1.&&!Joint&&Corner->Component()->IsVisible()&&!Scene.TrimmedWallCorners.Contains(Layout.CornerId),
         FString::Printf(TEXT("authored gap %.3f cm, added triangles %d"),AuthoredGap,Joint?Joint->NumTriangles:0));
   return;
  }
  if(!Joint) {Check(Label,false,FString::Printf(TEXT("authored gap %.3f cm, junction missing"),AuthoredGap));return;}
  const auto Bridge=ProjectJunctionSlice(Scene.WallJointVertices,Scene.WallJointTriangles,Joint->FirstTriangle*3,Joint->NumTriangles*3,
                                         FTransform::Identity,Axis,Origin);
  const auto* Back=Scene.Rendered.Find(Layout.BackId);
  const auto* ViewBack=C.FindEntity(Layout.BackId);
  double Remaining=TNumericLimits<double>::Max();
  if(Bridge.IsValid()&&Back&&Back->Component()&&ViewBack) {
   const FVector BackAxis((ViewBack->Pos-ViewCorner->Pos).GetSafeNormal(),0);
   const auto BackFace=ProjectJunctionSlice(Back->Vertices,Back->Triangles,0,Back->Triangles.Num(),Back->Component()->GetComponentTransform(),BackAxis,Origin);
   const auto BackBridge=ProjectJunctionSlice(Scene.WallJointVertices,Scene.WallJointTriangles,Joint->FirstTriangle*3,Joint->NumTriangles*3,
                                             FTransform::Identity,BackAxis,Origin);
   if(BackFace.IsValid()&&BackBridge.IsValid())Remaining=FMath::Max(B.Min-Bridge.Max,BackFace.Min-BackBridge.Max);
  }
  Layout.CompletedHeight=Joint->Bounds.Max.Z;
  Check(Label,A.IsValid()&&B.IsValid()&&AuthoredGap>5.&&Remaining<=.1&&!Joint->Remembered&&Joint->NumTriangles>0&&CornerUsesReplacement(Scene,Layout),
        FString::Printf(TEXT("authored gap %.3f cm, largest remaining projected separation %.4f cm, triangles %d"),AuthoredGap,Remaining,Joint->NumTriangles));
 }

 void CheckRightAngleStub(ARTSScene3D& Scene,const ARTSController& C) {
  const auto& Layout=Layouts[2];
  const auto* Corner=Scene.Rendered.Find(Layout.CornerId);
  const auto* ViewCorner=C.FindEntity(Layout.CornerId);
  const auto* ViewBack=C.FindEntity(Layout.BackId);
  const auto* Joint=JointFor(Scene,Layout);
  if(!Corner||!Corner->Component()||!ViewCorner||!ViewBack||!Joint) {Check(TEXT("90.miter_removes_unused_straight_stub"),false);return;}
  const FVector Origin=FVector(ViewCorner->Pos,0)*100.;
  const FVector StubAxis((ViewCorner->Pos-ViewBack->Pos).GetSafeNormal(),0);
  const FTransform Transform=Corner->Component()->GetComponentTransform();
  const auto Authored=ProjectJunctionSlice(Corner->Vertices,Corner->Triangles,0,Corner->Triangles.Num(),Transform,StubAxis,Origin);
  const auto Trimmed=ProjectJunctionSlice(Scene.WallJointVertices,Scene.WallJointTriangles,Joint->FirstTriangle*3,Joint->NumTriangles*3,
                                          FTransform::Identity,StubAxis,Origin);
  if(!Authored.IsValid()||!Trimmed.IsValid()) {Check(TEXT("90.miter_removes_unused_straight_stub"),false,TEXT("Missing 45 cm cross-section"));return;}
  const double Removed=Authored.Max-Trimmed.Max;

  // Past the outgoing leg's width, these replacement faces belong to the retained incoming leg.
  TArray<FIntPoint> IncomingPixels;
  for(int32 I=Joint->FirstTriangle*3;I<(Joint->FirstTriangle+Joint->NumTriangles)*3;I+=3) {
   const FVector Position=(Scene.WallJointVertices[Scene.WallJointTriangles[I]]+
                           Scene.WallJointVertices[Scene.WallJointTriangles[I+1]]+
                           Scene.WallJointVertices[Scene.WallJointTriangles[I+2]])/3.;
   if(FVector::DotProduct(Position-Origin,StubAxis)>-50.)continue;
   const FIntPoint Pixel=Scene.Project(Position/100.).IntPoint();
   if(Scene.Hit(FVector2D(Pixel))==Layout.CornerId)IncomingPixels.AddUnique(Pixel);
  }

  // Find pixels on the original unused tip, then compare real picking with the replacement restored.
  auto Entries=MoveTemp(Scene.WallJointEntries);
  const bool bWasTrimmed=Scene.TrimmedWallCorners.Remove(Layout.CornerId)>0;
  const bool bSameIncomingPick=IncomingPixels.ContainsByPredicate([&](const FIntPoint& Pixel){return Scene.Hit(FVector2D(Pixel))==Layout.CornerId;});
  TArray<FIntPoint> FormerTipPixels;
  for(int32 I=0;I+2<Corner->Triangles.Num();I+=3) {
   const FVector Local=(Corner->Vertices[Corner->Triangles[I]]+Corner->Vertices[Corner->Triangles[I+1]]+Corner->Vertices[Corner->Triangles[I+2]])/3.;
   const FVector Position=Transform.TransformPosition(Local);
   if(FVector::DotProduct(Position-Origin,StubAxis)<Trimmed.Max+20.)continue;
   const FVector2D Point=Scene.Project(Position/100.);
   const FIntPoint Pixel(FMath::RoundToInt(Point.X),FMath::RoundToInt(Point.Y));
   if(Scene.Hit(FVector2D(Pixel))==Layout.CornerId)FormerTipPixels.AddUnique(Pixel);
  }
  if(bWasTrimmed)Scene.TrimmedWallCorners.Add(Layout.CornerId);
  Scene.WallJointEntries=MoveTemp(Entries);
  const bool bNoLongerPickable=FormerTipPixels.ContainsByPredicate([&](const FIntPoint& Pixel){return Scene.Hit(FVector2D(Pixel))==0;});
  Check(TEXT("90.miter_removes_unused_straight_stub"),Removed>60.&&Trimmed.Max<45.&&bNoLongerPickable&&CornerUsesReplacement(Scene,Layout),
        FString::Printf(TEXT("visible stub reach reduced by %.3f cm to %.3f cm; %d original tip pixels tested"),Removed,Trimmed.Max,FormerTipPixels.Num()));
  Check(TEXT("90.replacement_incoming_leg_preserves_corner_pick"),bWasTrimmed&&bSameIncomingPick,
        FString::Printf(TEXT("%d integer pixels on retained incoming faces compared against the original wall"),IncomingPixels.Num()));
 }

 FIntPoint ExactPixel(ARTSScene3D& Scene,const FJunctionLayout& Layout,const FRTSWallJoint3D& Joint) {
  TSet<FIntPoint> Tried;
  TArray<FIntPoint> Candidates;
  for(int32 I=Joint.FirstTriangle*3;I<(Joint.FirstTriangle+Joint.NumTriangles)*3;I+=3) {
   const FVector A=Scene.WallJointVertices[Scene.WallJointTriangles[I]];
   const FVector B=Scene.WallJointVertices[Scene.WallJointTriangles[I+1]];
   const FVector D=Scene.WallJointVertices[Scene.WallJointTriangles[I+2]];
   const FVector2D Pixel=Scene.Project((A+B+D)/300.);
   const FIntPoint Center(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));
   for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y) {
    const FIntPoint Candidate=Center+FIntPoint(X,Y);
    if(Tried.Contains(Candidate))continue;
    Tried.Add(Candidate);
    if(Scene.Hit(FVector2D(Candidate))==Layout.CornerId)Candidates.Add(Candidate);
   }
  }
  // Query the same real base meshes with the additive triangles disabled for this control.
  auto Entries=MoveTemp(Scene.WallJointEntries);
  const bool bWasTrimmed=Scene.TrimmedWallCorners.Remove(Layout.CornerId)>0;
  FIntPoint Result(-1,-1);
  for(const FIntPoint Pixel:Candidates)if(Scene.Hit(FVector2D(Pixel))==0) {Result=Pixel;break;}
  if(bWasTrimmed)Scene.TrimmedWallCorners.Add(Layout.CornerId);
  Scene.WallJointEntries=MoveTemp(Entries);
  return Result;
 }

 void CheckPicks(ARTSScene3D& Scene,const ARTSController& C,const TCHAR* View) {
  for(int32 I=1;I<3;++I) {
   auto& Layout=Layouts[I];
   const auto* Joint=JointFor(Scene,Layout);
   const auto* Corner=C.FindEntity(Layout.CornerId);
   Layout.Pick=Joint&&Corner?ExactPixel(Scene,Layout,*Joint):FIntPoint(-1,-1);
   const FVector2D Pixel(Layout.Pick);
   const bool bHit=Corner&&Layout.Pick.X>=0&&Scene.Bounds(*Corner).IsInsideOrOn(Pixel)&&Scene.Hit(Pixel)==Corner->Id;
   Check(FString::Printf(TEXT("%d.%s.exact_integer_pick"),Layout.Angle,View),bHit,
         FString::Printf(TEXT("pixel %s, real corner ID %d"),*Layout.Pick.ToString(),Layout.CornerId));
  }
 }

 void CheckSubmittedMesh(ARTSScene3D& Scene) {
  const auto* Section=Scene.WallJoints->GetProcMeshSection(0);
  bool bMatches=Section&&Section->bSectionVisible&&Scene.WallJoints->IsVisible()&&
                Section->ProcVertexBuffer.Num()==Scene.WallJointVertices.Num()&&Section->ProcIndexBuffer.Num()==Scene.WallJointTriangles.Num();
  if(bMatches) {
   for(int32 I=0;I<Section->ProcIndexBuffer.Num();++I)bMatches&=int32(Section->ProcIndexBuffer[I])==Scene.WallJointTriangles[I];
   for(int32 I=0;I<Section->ProcVertexBuffer.Num();++I)bMatches&=Section->ProcVertexBuffer[I].Position.Equals(Scene.WallJointVertices[I],.001);
  }
  Check(TEXT("submitted_mesh_matches_pick_geometry"),bMatches&&Scene.WallJointEntries.Num()==2&&Scene.WallJoints->IsRenderStateCreated()&&
        Scene.WallJoints->GetCollisionEnabled()==ECollisionEnabled::NoCollision,
        FString::Printf(TEXT("%d junctions, %d triangles"),Scene.WallJointEntries.Num(),Scene.WallJointTriangles.Num()/3));
  if(Section&&!Section->ProcVertexBuffer.IsEmpty())FullColor=Section->ProcVertexBuffer[0].Color;
 }

 bool UpgradeGate(ARTSWorld& World,ARTSController& C,int32 Id) {
  FRTSEntity* Wall=World.Find(Id);
  if(!Wall)return false;
  C.ServerSlot=JunctionFixtureOwner;
  World.Command(&C,TEXT("research"),{Id},Wall->Pos,0,TEXT("gate_segment_upgrade"));
  C.ServerSlot=0;
  if(Wall->Jobs.Num()!=1)return false;
  World.Production(*Wall,Wall->Jobs[0].Remaining+RTS::Step);
  return Wall->bGate;
 }

 bool AddThirdGate(ARTSWorld& World,ARTSController& C,const FJunctionLayout& Layout) {
  const auto* Corner=World.Find(Layout.CornerId);
  const auto* Back=World.Find(Layout.BackId);
  if(!Corner||!Back)return false;
  // Add a third branch on the continuation of the incoming line at the same snap spacing.
  auto* Extra=World.Spawn(World.D(*Corner).Id,Corner->Owner,Corner->Pos+(Corner->Pos-Back->Pos),Corner->Tier);
  if(!Extra)return false;
  ExtraGateId=Extra->Id;
  Extra->Heading=Corner->Heading;
  return UpgradeGate(World,C,ExtraGateId);
 }

 void CheckGateSection(ARTSScene3D& Scene,int32 Id,const TCHAR* Side) {
  const auto* Gate=Scene.Rendered.Find(Id);
  double MinPositive=TNumericLimits<double>::Max(),MaxNegative=-TNumericLimits<double>::Max();
  if(Gate)VisitJunctionSlice(Gate->Vertices,Gate->Triangles,0,Gate->Triangles.Num(),FTransform::Identity,[&](const FVector& P) {
   if(P.X>=0)MinPositive=FMath::Min(MinPositive,P.X);
   if(P.X<=0)MaxNegative=FMath::Max(MaxNegative,P.X);
  });
  const double Limit=TNumericLimits<double>::Max();
  const double Opening=MinPositive==Limit||MaxNegative==-Limit?0.:MinPositive-MaxNegative;
  const bool bNoJoin=Scene.WallJointEntries.IsEmpty()&&Scene.WallJoints->GetNumSections()==0;
  Check(FString::Printf(TEXT("gate_%s_keeps_opening"),Side),Gate&&Gate->Key.Contains(TEXT(":gate"))&&bNoJoin&&Opening>100.,
        FString::Printf(TEXT("%.3f cm rendered opening, junction count %d"),Opening,Scene.WallJointEntries.Num()));
 }

 void ChangeProgress(ARTSWorld& World,float CornerValue,float NeighborValue) {
  for(int32 I=1;I<3;++I) {
   SetProgress(World,*World.Find(Layouts[I].CornerId),CornerValue);
   SetProgress(World,*World.Find(Layouts[I].NeighborId),NeighborValue);
  }
  Snapshot(World);
 }

 void CheckProgress(ARTSScene3D& Scene,float CornerValue,float NeighborValue,const TCHAR* Phase) {
  auto Scale=[](float Value){return .08+.92*Value;};
  for(int32 I=1;I<3;++I) {
   auto& Layout=Layouts[I];
   const auto* Joint=JointFor(Scene,Layout);
   const auto* Corner=Scene.Rendered.Find(Layout.CornerId);
   const auto* Neighbor=Scene.Rendered.Find(Layout.NeighborId);
   auto HeightMatches=[&](const FRTSRendered3D* Wall,float Value) {
    if(!Wall||!Wall->Model||Wall->Vertices.IsEmpty())return false;
    double Top=0;
    for(const auto& V:Wall->Vertices)Top=FMath::Max(Top,V.Z);
    return FMath::IsNearlyEqual(Top,Wall->Model->Bounds.Max.Z*100.*Scale(Value),JunctionHeightToleranceCm)&&Wall->Progress==Value;
   };
   const double JointTop=Joint?Joint->Bounds.Max.Z:0.;
   const double Expected=Layout.CompletedHeight*Scale(FMath::Min(CornerValue,NeighborValue));
   const bool bFinalPose=NeighborValue!=1|| (Neighbor&&Neighbor->PoseRevision>Layout.NearCompletePose);
   const bool bFull=CornerValue==1&&NeighborValue==1;
   const bool bExpectedCorner=bFull?CornerUsesReplacement(Scene,Layout):
       Corner&&Corner->Component()&&Corner->Component()->IsVisible()&&!Scene.TrimmedWallCorners.Contains(Layout.CornerId);
   Check(FString::Printf(TEXT("%d.%s.base_and_junction_height"),Layout.Angle,Phase),Joint&&Layout.CompletedHeight>50.&&
         FMath::IsNearlyEqual(JointTop,Expected,JunctionHeightToleranceCm)&&HeightMatches(Corner,CornerValue)&&HeightMatches(Neighbor,NeighborValue)&&bFinalPose&&bExpectedCorner,
         FString::Printf(TEXT("joint height %.4f cm, expected %.4f; corner %.2f, neighbor %.2f"),JointTop,Expected,CornerValue,NeighborValue));
   if(Neighbor&&NeighborValue==.99f)Layout.NearCompletePose=Neighbor->PoseRevision;
  }
 }

 bool SuppressesPriorPicks(ARTSScene3D& Scene) const {
  for(int32 I=1;I<3;++I) {
   const auto& Layout=Layouts[I];
   if(Layout.Pick.X<0||Scene.Hit(FVector2D(Layout.Pick))==Layout.CornerId)return false;
  }
  return true;
 }

 void CheckRemembered(ARTSScene3D& Scene,const ARTSController& C) {
  bool bRemembered=Scene.WallJointEntries.Num()==2;
  for(int32 I=1;I<3;++I) {
   const auto* Corner=C.FindEntity(Layouts[I].CornerId);
   const auto* Neighbor=C.FindEntity(Layouts[I].NeighborId);
   const auto* Joint=JointFor(Scene,Layouts[I]);
   bRemembered&=Corner&&Neighbor&&Corner->bRemembered&&Neighbor->bRemembered&&Joint&&Joint->Remembered&&CornerUsesReplacement(Scene,Layouts[I]);
  }
  const auto* Section=Scene.WallJoints->GetProcMeshSection(0);
  bool bTinted=false;
  if(Section&&!Section->ProcVertexBuffer.IsEmpty()) {
   const FColor New=Section->ProcVertexBuffer[0].Color;
   bTinted=FullColor.R+FullColor.G+FullColor.B>0&&FMath::Abs(New.R-FMath::RoundToInt(FullColor.R*.38))<=1&&
           FMath::Abs(New.G-FMath::RoundToInt(FullColor.G*.38))<=1&&FMath::Abs(New.B-FMath::RoundToInt(FullColor.B*.38))<=1;
  }
  Check(TEXT("remembered_snapshot_dims_and_disables_picks"),bRemembered&&bTinted&&SuppressesPriorPicks(Scene));
 }

public:
 void Run(ARTSScene3D& Scene,ARTSController& C) {
  if(bFinished||!C.Me())return;
  ++Frame;
  auto* Mode=C.GetWorld()->GetAuthGameMode<ARTSGameMode>();
  auto* World=Mode?Mode->Simulation:nullptr;
  if(!World||FPlatformTime::Seconds()-Started>90.) {Check(TEXT("fixture_finished"),false,TEXT("Missing authority or timed out"));Finish();return;}
  C.CameraCenter=FVector2D(46,62.5);
  C.CameraRotation=CameraAngle;
  C.Zoom=2.8f;
  if(Stage==EStage::Initialize) {
   if(!Initialize(*World,C))Finish();
   return;
  }
  if(Frame-StageFrame<3)return;
  switch(Stage) {
   case EStage::Full: {
    const bool bDrawn=Scene.WallJoints->GetLastRenderTimeOnScreen()>=ExpectDrawAfter;
    if(!bDrawn&&FPlatformTime::Seconds()-StageStarted<3.)return;
    Check(TEXT("junction_component_drawn"),bDrawn);
    for(auto& Layout:Layouts)CheckSection(Scene,C,Layout);
    CheckRightAngleStub(Scene,C);
    CheckSubmittedMesh(Scene);
    CheckPicks(Scene,C,TEXT("standard_camera"));
    const FString File=FString::Printf(TEXT("wall-junction-%s.png"),JunctionStyles[StyleIndex].Faction);
    FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/File),false,false);
    Screenshots.Add(MakeShared<FJsonValueString>(File));
    FullRevision=Scene.WallJointRevision;
    Algo::Reverse(C.Entities);
    CameraAngle=PI*.32f;
    SetStage(EStage::Rotated);
    break;
   }
   case EStage::Rotated: {
    Check(TEXT("rotation_and_snapshot_order_reuse_geometry"),Scene.WallJointRevision==FullRevision&&Scene.WallJointEntries.Num()==2&&
          CornerUsesReplacement(Scene,Layouts[1])&&CornerUsesReplacement(Scene,Layouts[2]));
    CheckPicks(Scene,C,TEXT("rotated_camera"));
    Check(TEXT("third_branch_upgrades_to_gate"),AddThirdGate(*World,C,Layouts[2]));
    Snapshot(*World);
    SetStage(EStage::Branch);
    break;
   }
   case EStage::Branch: {
    const auto* Extra=C.FindEntity(ExtraGateId);
    const auto* Corner=Scene.Rendered.Find(Layouts[2].CornerId);
    Check(TEXT("third_gate_branch_retains_full_corner"),Extra&&Extra->bGate&&CornerUsesReplacement(Scene,Layouts[1])&&
          Corner&&Corner->Component()&&Corner->Component()->IsVisible()&&!Scene.TrimmedWallCorners.Contains(Layouts[2].CornerId)&&
          Scene.WallJointEntries.Num()==2&&JointFor(Scene,Layouts[2]));
    World->Entities.Remove(ExtraGateId);
    ExtraGateId=0;
    const bool bCorner=UpgradeGate(*World,C,Layouts[1].CornerId);
    const bool bNeighbor=UpgradeGate(*World,C,Layouts[2].NeighborId);
    Check(TEXT("gate_jobs_complete_on_authority"),bCorner&&bNeighbor);
    Snapshot(*World);
    Check(TEXT("new_gate_snapshot_immediately_suppresses_stale_joint_picks"),SuppressesPriorPicks(Scene));
    SetStage(EStage::Gates);
    break;
   }
   case EStage::Gates: {
    CheckGateSection(Scene,Layouts[1].CornerId,TEXT("corner"));
    CheckGateSection(Scene,Layouts[2].NeighborId,TEXT("neighbor"));
    World->Find(Layouts[1].CornerId)->bGate=false;
    World->Find(Layouts[2].NeighborId)->bGate=false;
    World->Find(Layouts[1].NeighborId)->Owner=0;
    auto* Other=World->Find(Layouts[2].NeighborId);
    Other->Def=FRTSCatalog::Get().Index(World->D(*Other).Id==TEXT("cement_wall")?TEXT("works_of_defense"):TEXT("cement_wall"));
    Snapshot(*World);
    SetStage(EStage::Isolated);
    break;
   }
   case EStage::Isolated: {
    const auto* DifferentOwner=C.FindEntity(Layouts[1].NeighborId);
    const auto* DifferentType=C.FindEntity(Layouts[2].NeighborId);
    const auto* Corner=C.FindEntity(Layouts[2].CornerId);
    Check(TEXT("different_owner_and_type_stay_separate"),DifferentOwner&&DifferentOwner->Owner==0&&DifferentType&&Corner&&DifferentType->Def!=Corner->Def&&
          Scene.WallJointEntries.IsEmpty()&&Scene.WallJoints->GetNumSections()==0);
    World->Find(Layouts[1].NeighborId)->Owner=JunctionFixtureOwner;
    World->Find(Layouts[2].NeighborId)->Def=FRTSCatalog::Get().Index(JunctionStyles[StyleIndex].Type);
    ChangeProgress(*World,.99f,.35f);
    SetStage(EStage::Partial);
    break;
   }
   case EStage::Partial:
    CheckProgress(Scene,.99f,.35f,TEXT("partial"));
    ChangeProgress(*World,.99f,.99f);
    SetStage(EStage::NearComplete);
    break;
   case EStage::NearComplete:
    CheckProgress(Scene,.99f,.99f,TEXT("near_complete"));
    ChangeProgress(*World,1.f,1.f);
    SetStage(EStage::Complete);
    break;
   case EStage::Complete:
    CheckProgress(Scene,1.f,1.f,TEXT("complete"));
    CheckPicks(Scene,C,TEXT("completed_camera"));
    World->bFogOfWar=true;
    Snapshot(*World);
    Check(TEXT("new_fog_snapshot_immediately_suppresses_stale_joint_picks"),SuppressesPriorPicks(Scene));
    SetStage(EStage::Remembered);
    break;
   case EStage::Remembered:
    CheckRemembered(Scene,C);
    for(int32 I=1;I<3;++I)World->Players[0].KnownBuildings.Remove(Layouts[I].BackId);
    Snapshot(*World);
    SetStage(EStage::WithheldBack);
    break;
   case EStage::WithheldBack: {
    bool bLocalFallback=Scene.WallJointEntries.Num()==2&&Scene.TrimmedWallCorners.IsEmpty();
    for(int32 I=1;I<3;++I) {
     const auto& Layout=Layouts[I];
     const auto* Corner=Scene.Rendered.Find(Layout.CornerId);
     const auto* Joint=JointFor(Scene,Layout);
     bLocalFallback&=World->Find(Layout.BackId)&&!C.FindEntity(Layout.BackId)&&!Scene.Rendered.Contains(Layout.BackId)&&
                     Corner&&Corner->Component()&&Corner->Component()->IsVisible()&&Joint&&Joint->Remembered;
    }
    Check(TEXT("withheld_back_wall_restores_untrimmed_corner"),bLocalFallback&&SuppressesPriorPicks(Scene));
    for(int32 I=1;I<3;++I)World->Players[0].KnownBuildings.Remove(Layouts[I].NeighborId);
    Snapshot(*World);
    SetStage(EStage::Withheld);
    break;
   }
   case EStage::Withheld: {
    bool bFiltered=true;
    for(int32 I=1;I<3;++I)bFiltered&=World->Find(Layouts[I].NeighborId)&&!C.FindEntity(Layouts[I].NeighborId);
    Check(TEXT("withheld_neighbors_remove_client_junction_mesh_and_bounds"),bFiltered&&Scene.WallJointEntries.IsEmpty()&&Scene.WallJointVertices.IsEmpty()&&
          Scene.WallJointTriangles.IsEmpty()&&Scene.WallJointBounds.IsEmpty()&&Scene.WallJoints->GetNumSections()==0);
    if(StyleIndex+1>=UE_ARRAY_COUNT(JunctionStyles))Finish();
    else {++StyleIndex;SetStage(EStage::Initialize);}
    break;
   }
   case EStage::Initialize: break;
  }
 }
};

void RunRTSWallJunctionProbe(ARTSScene3D* Scene,ARTSController* Controller) {
 static const bool bEnabled=FParse::Param(FCommandLine::Get(),TEXT("RTSV09WallJunctionTest"));
 if(!bEnabled)return;
 static FRTSWallJunctionProbe Probe;
 Probe.Run(*Scene,*Controller);
}
