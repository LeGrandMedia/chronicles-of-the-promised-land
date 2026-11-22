#include "RTSGateVisualTests.h"
#include "RTSScene3D.h"
#include "RTSController.h"
#include "RTSWorld.h"
#include "Components/MeshComponent.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace {
// Independent triangle/box SAT; deliberately conservative about beveled posts.
bool TriangleBox(FVector A,FVector B,FVector C,const FBox& Box) {
 const FVector Center=Box.GetCenter(),Extent=Box.GetExtent();A-=Center;B-=Center;C-=Center;
 const auto Separated=[&](FVector Axis) {
  if(Axis.SizeSquared()<1.e-12)return false;
  const double P=FVector::DotProduct(A,Axis),Q=FVector::DotProduct(B,Axis),R=FVector::DotProduct(C,Axis);
  const double Limit=FVector::DotProduct(Extent,Axis.GetAbs());
  return FMath::Min3(P,Q,R)>=Limit-1.e-5||FMath::Max3(P,Q,R)<=-Limit+1.e-5;
 };
 const FVector Edges[]={B-A,C-B,A-C},Axes[]={FVector(1,0,0),FVector(0,1,0),FVector(0,0,1)};
 for(FVector Axis:Axes)if(Separated(Axis))return false;
 if(Separated(FVector::CrossProduct(B-A,C-A)))return false;
 for(FVector Edge:Edges)for(FVector Axis:Axes)if(Separated(FVector::CrossProduct(Edge,Axis)))return false;
 return true;
}
}

class FRTSGateVisualProbe {
 TArray<TSharedPtr<FJsonValue>> Inputs,Results,Shots;
 int32 CaseIndex=0,SampleIndex=0,Frame=0,ChangedAt=0,Checks=0,Clips=0,Failures=0;
 bool Started=false,Done=false,Prepared=false,Captured=false;
 double Began=FPlatformTime::Seconds();
 FVector2D Center,Along,Across;
 FString FirstClip;
 TSharedPtr<FJsonObject> Current;
 FVector Local(const FVector& World) const {
  const FVector2D Delta(World.X-Center.X,World.Y-Center.Y);
  return FVector(FVector2D::DotProduct(Delta,Along),FVector2D::DotProduct(Delta,Across),World.Z);
 }
 void Finish() {
  Done=true;auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Failures==0&&Results.Num()>0);
  Report->SetNumberField(TEXT("failed"),Failures);Report->SetArrayField(TEXT("cases"),Results);Report->SetArrayField(TEXT("screenshots"),Shots);
  Report->SetStringField(TEXT("scope"),TEXT("Actual client posed meshes replay authority gate trajectories; conservative triangle versus each wall-part box. Screenshots sample crossings, not every frame. Sampled every authority tick; exact authority positions omit client interpolation. No combat or moving-obstacle claim."));
  Report->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Began);
  FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  const FString File=FPaths::ProjectSavedDir()/TEXT("Tests/v09-gate-visual.json");IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
  const bool Saved=FFileHelper::SaveStringToFile(Json,*File);const bool Pass=Saved&&Failures==0&&Results.Num()>0;FPlatformMisc::RequestExitWithStatus(!Pass,Pass?0:1);
 }
 bool Eligible(const TSharedPtr<FJsonObject>& Row) const {
  bool ProbeOnly=false;if(Row->TryGetBoolField(TEXT("probe_only"),ProbeOnly)&&ProbeOnly)return false;
  const FString Type=Row->GetStringField(TEXT("unit"));
  return (Type==TEXT("elephant")||Type==TEXT("kings_chariot"))&&Row->GetBoolField(TEXT("allied"))&&
   Row->GetBoolField(TEXT("gate_enabled"))&&Row->GetNumberField(TEXT("gate_progress"))>=1;
 }
 void PlaceSample(ARTSController* C) {
  const auto& Samples=Current->GetArrayField(TEXT("trajectory"));const auto Point=Samples[SampleIndex]->AsObject();
  auto& Unit=C->Entities.Last();Unit.Pos=FVector2D(Point->GetNumberField(TEXT("x")),Point->GetNumberField(TEXT("y")));
  Unit.Heading=Point->GetNumberField(TEXT("heading"));Unit.Status=TEXT("moving");C->DisplayPositions.Add(Unit.Id,Unit.Pos);C->MatchTime=Point->GetNumberField(TEXT("time"));
 }
 bool Prepare(ARTSController* C) {
  while(Inputs.IsValidIndex(CaseIndex)&&!Eligible(Inputs[CaseIndex]->AsObject()))++CaseIndex;
  if(!Inputs.IsValidIndex(CaseIndex)){Finish();return false;}
  Current=Inputs[CaseIndex]->AsObject();SampleIndex=0;Checks=Clips=0;Captured=false;FirstClip.Empty();
  if(!Current->GetBoolField(TEXT("passed"))||!Current->GetBoolField(TEXT("reached_goal"))) {
   auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("authority_case"),CaseIndex);Row->SetBoolField(TEXT("passed"),false);
   Row->SetStringField(TEXT("first_intersection"),TEXT("Authority trajectory failed; no visual pass can certify it."));Results.Add(MakeShared<FJsonValueObject>(Row));++Failures;++CaseIndex;return false;
  }
  const double Offset=Current->GetNumberField(TEXT("grid_offset_m")),Angle=FMath::DegreesToRadians(Current->GetNumberField(TEXT("angle_degrees")));
  Center=FVector2D(40+Offset,40+Offset);Along=FVector2D(FMath::Cos(Angle),FMath::Sin(Angle));Across=FVector2D(-Along.Y,Along.X);
  const FString Wall=Current->GetStringField(TEXT("wall"));const auto& Cat=FRTSCatalog::Get();const double Spacing=Cat.Def(Wall).Radius*2+.15;
  C->Entities.Reset();C->DisplayPositions.Reset();C->DeathAnimations.Reset();C->Selection.Reset();C->Players[0].Faction=TEXT("coriantumr");C->Players[0].Age=4;
  C->bMenu=C->bPauseMenu=C->bHelp=false;C->Phase=1;C->Map=0;C->LocalSlot=0;C->Fog.Init(2,64*64);
  const auto Add=[&](FString Type,FVector2D Pos,bool Gate=false) {
   FRTSEntityView View;View.Id=C->Entities.Num()+1;View.Def=Cat.Index(Type);View.Owner=0;View.Pos=Pos;View.Heading=Angle;
   View.HP=View.MaxHP=Cat.Def(Type).HP;View.bGate=Gate;View.Progress=1;View.Status=TEXT("idle");C->Entities.Add(View);C->DisplayPositions.Add(View.Id,Pos);
  };
  const int32 GateCount=Current->GetNumberField(TEXT("gate_count"));
  for(int32 Index=-3;Index<=3;++Index)Add(Wall,Center+Along*(Index*Spacing),Index>=0&&Index<GateCount);
  if(Current->GetBoolField(TEXT("blocked_approach")))Add(TEXT("tree"),Center-Across*2.8);
  Add(Current->GetStringField(TEXT("unit")),Center);C->Selection={C->Entities.Last().Id};
  C->CameraCenter=Center;C->CameraRotation=-Angle;C->Zoom=2.4;
  PlaceSample(C);ChangedAt=Frame;Prepared=true;return true;
 }
 void Audit(ARTSScene3D* Scene,ARTSController* C) {
  auto* Unit=Scene->Rendered.Find(C->Entities.Last().Id);
  if(!Unit||!Unit->Component()||!Unit->Model){++Clips;FirstClip=TEXT("Missing rendered mover");return;}
  Scene->EnsureGeometry(*Unit);if(Unit->Vertices.IsEmpty()||Unit->Triangles.IsEmpty()){++Clips;FirstClip=TEXT("Empty mover geometry");return;}TArray<FVector> Points;Points.Reserve(Unit->Vertices.Num());FBox UnitBox(ForceInit);
  for(FVector V:Unit->Vertices){const FVector P=Local(Unit->Component()->GetComponentTransform().TransformPosition(V)/100.);Points.Add(P);UnitBox+=P;}
  ++Checks;
  for(const auto& E:C->Entities) {
   if(!FRTSCatalog::Get().Def(E.Def).bBuilding)continue;
   const auto* R=Scene->Rendered.Find(E.Id);if(!R||!R->Component()||!R->Model){++Clips;FirstClip=TEXT("Missing rendered wall");return;}
   Scene->EnsureGeometry(*R);int32 Base=0;
   for(const auto& Part:R->Model->Parts) {
    FBox Box(ForceInit);
    for(int32 I=0;I<Part.Vertices.Num();++I)Box+=Local(R->Component()->GetComponentTransform().TransformPosition(R->Vertices[Base+I])/100.);
    Base+=Part.Vertices.Num();if(!Box.Intersect(UnitBox))continue;
    for(int32 I=0;I+2<Unit->Triangles.Num();I+=3) {
     const FVector A=Points[Unit->Triangles[I]],B=Points[Unit->Triangles[I+1]],D=Points[Unit->Triangles[I+2]];
     if(TriangleBox(A,B,D,Box)) {
      ++Clips;if(FirstClip.IsEmpty())FirstClip=FString::Printf(TEXT("sample %d, wall %d part %s, mover triangle %d"),SampleIndex,E.Id,*Part.Name,I/3);
      return;
     }
    }
   }
  }
 }
public:
 void Run(ARTSScene3D* Scene,ARTSController* C) {
  if(Done||!C->Me())return;++Frame;
  if(FPlatformTime::Seconds()-Began>180){++Failures;Finish();return;}
  if(!Started) {
   Started=true;for(TActorIterator<ARTSWorld> It(C->GetWorld());It;++It)It->SetActorTickEnabled(false);
   FString Json;TSharedPtr<FJsonObject> Report;
   if(!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectSavedDir()/TEXT("GateTraversalTest/gate-traversal-tests.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Report)||!Report.IsValid()){++Failures;Finish();return;}
   Inputs=Report->GetArrayField(TEXT("cases"));
  }
  if(!Prepared){Prepare(C);return;}
  C->CameraCenter=Center;C->CameraRotation=-FMath::DegreesToRadians(Current->GetNumberField(TEXT("angle_degrees")));C->Zoom=2.4;
  if(Frame-ChangedAt<(SampleIndex==0?3:1))return;
  Audit(Scene,C);
  const FVector2D At=C->Entities.Last().Pos-Center;
  if(!Captured&&FMath::Abs(FVector2D::DotProduct(At,Across))<.3&&Current->GetNumberField(TEXT("approach_offset_m"))==0) {
   const FString Name=FString::Printf(TEXT("gate-visual-%03d.png"),CaseIndex);
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name),false,false);Shots.Add(MakeShared<FJsonValueString>(Name));Captured=true;
  }
  if(++SampleIndex>=Current->GetArrayField(TEXT("trajectory")).Num()) {
   auto Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("authority_case"),CaseIndex);Row->SetStringField(TEXT("wall"),Current->GetStringField(TEXT("wall")));
   Row->SetStringField(TEXT("unit"),Current->GetStringField(TEXT("unit")));Row->SetNumberField(TEXT("pose_samples"),Checks);Row->SetNumberField(TEXT("intersecting_samples"),Clips);
   const bool Pass=Clips==0&&Checks>0&&Current->GetBoolField(TEXT("passed"))&&Current->GetBoolField(TEXT("reached_goal"));Row->SetBoolField(TEXT("passed"),Pass);Row->SetStringField(TEXT("first_intersection"),FirstClip);Results.Add(MakeShared<FJsonValueObject>(Row));Failures+=Pass?0:1;
   UE_LOG(LogTemp,Display,TEXT("RTS_GATE_VISUAL case=%d samples=%d intersections=%d %s"),CaseIndex,Checks,Clips,*FirstClip);
   ++CaseIndex;Prepared=false;return;
  }
  PlaceSample(C);ChangedAt=Frame;
 }
};

void RunRTSGateVisualProbe(ARTSScene3D* Scene,ARTSController* Controller) {
 if(!FParse::Param(FCommandLine::Get(),TEXT("RTSV09GateVisualTest")))return;
 static FRTSGateVisualProbe Probe;Probe.Run(Scene,Controller);
}
