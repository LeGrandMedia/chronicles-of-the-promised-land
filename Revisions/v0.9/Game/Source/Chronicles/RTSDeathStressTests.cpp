#include "RTSDeathStressTests.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool RTSDeathStressEnabled() {
#if UE_BUILD_SHIPPING
 return false;
#else
 static const bool bEnabled=FParse::Param(FCommandLine::Get(),TEXT("RTSV09DeathStress"));
 return bEnabled;
#endif
}

#if !UE_BUILD_SHIPPING
#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSHUD.h"
#include "RTSScene3D.h"
#include "RTSVisualAssets.h"
#include "RTSWorld.h"
#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineGlobals.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "HardwareInfo.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "RenderTimer.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace {
enum class EDeathStage : uint8 { Waiting, Warmup, Baseline, FirstLoss, Active, Cleanup, Capture, Lifecycle, Done };
enum class EDeathPhase : uint8 { Live, FirstLoss, Fall, Collapse, Settled, Sink, Expiry, Clean };

const FVector2D DeathCameraCenter(47,47);
constexpr float DeathCameraZoom=1.65f;
constexpr double DeathPositionToleranceCm=.002;
constexpr double DeathNormalTolerance=.0002;
constexpr double DeathPoseTimeTolerance=.002;

struct FDeathGeometryWindow {
 const TCHAR* Name;
 double Begin,End;
};
constexpr FDeathGeometryWindow DeathGeometryWindows[]={
 {TEXT("falling"),.20,.80}, {TEXT("building_falling"),.90,1.50}, {TEXT("settled"),1.55,2.},
 {TEXT("early_sink"),2.10,2.55}, {TEXT("late_sink"),2.65,3.20}
};

struct FDeathGeometryErrors {
 double PositionCm=0,VertexNormal=0,FaceNormal=0,PoseTime=0;
 int32 Vertices=0,Faces=0,Colors=0;

 void Add(const FDeathGeometryErrors& Other) {
  PositionCm=FMath::Max(PositionCm,Other.PositionCm);
  VertexNormal=FMath::Max(VertexNormal,Other.VertexNormal);
  FaceNormal=FMath::Max(FaceNormal,Other.FaceNormal);
  PoseTime=FMath::Max(PoseTime,Other.PoseTime);
  Vertices+=Other.Vertices;
  Faces+=Other.Faces;
  Colors+=Other.Colors;
 }

 void Write(FJsonObject& Row) const {
  Row.SetNumberField(TEXT("max_position_error_cm"),PositionCm);
  Row.SetNumberField(TEXT("max_vertex_normal_error"),VertexNormal);
  Row.SetNumberField(TEXT("max_face_normal_error"),FaceNormal);
  Row.SetNumberField(TEXT("max_pose_time_error_s"),PoseTime);
  Row.SetNumberField(TEXT("vertices_checked"),Vertices);
  Row.SetNumberField(TEXT("triangle_normals_checked"),Faces);
  Row.SetNumberField(TEXT("vertex_colors_checked"),Colors);
 }
};

// Reconstruct the original per-vertex deformation from immutable authored data.
// This path intentionally does not use cached renderer pose terms or vertices.
FString CompareDeathMesh(const FRTSRendered3D& Rendered,const FRTSEntityView& Effect,bool bBuilding,
                        double ExpectedAge,FDeathGeometryErrors& Errors) {
 auto* Mesh=Rendered.Mesh.Get();
 const auto* Section=Mesh?Mesh->GetProcMeshSection(0):nullptr;
 const int32 FirstVertex=Rendered.IsBatchedDeath()?Rendered.BatchFirstVertex:0;
 const int32 FirstIndex=Rendered.IsBatchedDeath()?Rendered.BatchFirstIndex:0;
 if(!Section||!Rendered.Model||FirstVertex<0||FirstIndex<0||
    Section->ProcVertexBuffer.Num()<FirstVertex+Rendered.Model->VertexCount||
    Section->ProcIndexBuffer.Num()<FirstIndex+Rendered.Model->Triangles*3||
    (!Rendered.IsBatchedDeath()&&(Section->ProcVertexBuffer.Num()!=Rendered.Model->VertexCount||
      Section->ProcIndexBuffer.Num()!=Rendered.Model->Triangles*3)))return TEXT("Procedural body range is missing or has changed");
 const FTransform& Transform=Mesh->GetComponentTransform();
 const FVector Location=Transform.GetLocation();
 const FQuat ExpectedRotation=FRotator(0,FMath::RadiansToDegrees(Effect.Heading),0).Quaternion();
 const FTransform BodyTransform(ExpectedRotation,FVector(Effect.Pos,0)*100);
 if(Transform.ContainsNaN()||!Transform.GetScale3D().Equals(FVector::OneVector,DeathPositionToleranceCm)||
    (!Rendered.IsBatchedDeath()&&(!FVector2D(Location.X,Location.Y).Equals(Effect.Pos*100,DeathPositionToleranceCm)||
      !Transform.GetRotation().Equals(ExpectedRotation,DeathNormalTolerance))))return TEXT("The corpse moved horizontally, turned or rescaled");
 const auto BodyPoint=[&](const FVector& Point){return BodyTransform.InverseTransformPosition(Transform.TransformPosition(Point));};
 const auto BodyNormal=[&](const FVector& Normal){return BodyTransform.InverseTransformVectorNoScale(Transform.TransformVectorNoScale(Normal));};

 const float Age=Rendered.PoseElapsed;
 if(!FMath::IsFinite(Age))return TEXT("Nonfinite corpse pose clock");
 Errors.PoseTime=FMath::Abs(double(Age)-ExpectedAge);
 const float Fall=FMath::Clamp(Age/(bBuilding?1.5f:.8f),0.f,1.f);
 const FVector LegacySinkCm(0,0,Age>2.f?double((Age-2.f)*.6f)*100:0.);
 TArray<FVector> Reference,Normals;
 int32 Base=FirstVertex,Index=FirstIndex;
 for(int32 PartIndex=0;PartIndex<Rendered.Model->Parts.Num();++PartIndex) {
  const auto& Part=Rendered.Model->Parts[PartIndex];
  Reference.Reset(Part.Vertices.Num());
  Normals.Init(FVector::UpVector,Part.Vertices.Num());
  FLinearColor Color=Part.Color;
  if(Effect.Owner>=0&&(Part.Team||Part.Name.Contains(TEXT("team"))))Color=FMath::Lerp(Color,RTS::PlayerColor(Effect.Owner),.68f);
  if(Effect.bRemembered)Color*=.38f;else if(Effect.bStealthed)Color=FMath::Lerp(Color,FLinearColor(.16f,.27f,.29f),.4f);
  Color.A=1;
  const FColor ExpectedColor=Color.ToFColor(false);
  for(const FVector& Rest:Part.Vertices) {
   FVector Expected=Rest;
   if(bBuilding) {
    if(Effect.Progress<1)Expected.Z*=.08f+.92f*FMath::Clamp(Effect.Progress,0.f,1.f);
    const FVector Axis=PartIndex%2?FVector::XAxisVector:FVector::YAxisVector;
    Expected=FQuat(Axis,Fall*(.08f+(PartIndex%5)*.035f)).RotateVector(Expected);
    Expected.Z*=1-Fall*.85f;
    Expected.X+=FMath::Sin(PartIndex*5.73f)*Fall*.18f;
    Expected.Y+=FMath::Cos(PartIndex*3.19f)*Fall*.18f;
   } else {
    Expected=FQuat(FVector::YAxisVector,Fall*PI*.48f).RotateVector(Expected);
    Expected.Z=FMath::Max(.06,Expected.Z+.12f);
   }
   Reference.Add(Expected*100);
  }
  for(int32 VertexIndex=0;VertexIndex<Part.Vertices.Num();++VertexIndex) {
   const auto& Vertex=Section->ProcVertexBuffer[Base+VertexIndex];
   if(Vertex.Position.ContainsNaN()||Vertex.Normal.ContainsNaN())return TEXT("Nonfinite submitted corpse vertex");
   const FVector WithSink=BodyPoint(Vertex.Position);
   Errors.PositionCm=FMath::Max(Errors.PositionCm,FVector::Distance(WithSink,Reference[VertexIndex]-LegacySinkCm));
   if(Vertex.Color!=ExpectedColor)return TEXT("Submitted corpse color differs from authored/team color");
   ++Errors.Vertices;
   ++Errors.Colors;
  }
  for(int32 I=0;I<Part.Triangles.Num();I+=3,Index+=3) {
   const int32 A=Part.Triangles[I],B=Part.Triangles[I+1],C=Part.Triangles[I+2];
   // Clockwise submission uses A,C,B while the legacy normals use A,B,C.
   if(Section->ProcIndexBuffer[Index]!=uint32(Base+A)||Section->ProcIndexBuffer[Index+1]!=uint32(Base+C)||
      Section->ProcIndexBuffer[Index+2]!=uint32(Base+B))return TEXT("Submitted corpse indices or winding differ from the authored parts");
   const FVector Expected=FVector::CrossProduct(Reference[B]-Reference[A],Reference[C]-Reference[A]).GetSafeNormal();
   const auto& V=Section->ProcVertexBuffer;
   const FVector Actual=BodyNormal(FVector::CrossProduct(V[Base+B].Position-V[Base+A].Position,
                                              V[Base+C].Position-V[Base+A].Position).GetSafeNormal());
   Errors.FaceNormal=FMath::Max(Errors.FaceNormal,FVector::Distance(Actual,Expected));
   ++Errors.Faces;
   Normals[A]=Normals[B]=Normals[C]=Expected;
  }
  for(int32 VertexIndex=0;VertexIndex<Part.Vertices.Num();++VertexIndex) {
   const FVector Actual=BodyNormal(Section->ProcVertexBuffer[Base+VertexIndex].Normal);
   Errors.VertexNormal=FMath::Max(Errors.VertexNormal,FVector::Distance(Actual,Normals[VertexIndex]));
  }
  Base+=Part.Vertices.Num();
 }
 if(Errors.PositionCm>DeathPositionToleranceCm||Errors.VertexNormal>DeathNormalTolerance||
    Errors.FaceNormal>DeathNormalTolerance||Errors.PoseTime>DeathPoseTimeTolerance)
  return FString::Printf(TEXT("Legacy pose differs: position %.8f cm, vertex normal %.8f, face normal %.8f, elapsed %.8f s"),
                         Errors.PositionCm,Errors.VertexNormal,Errors.FaceNormal,Errors.PoseTime);
 return FString();
}

const TCHAR* DeathPhaseName(EDeathPhase Phase) {
 switch(Phase){
  case EDeathPhase::Live:return TEXT("live");
  case EDeathPhase::FirstLoss:return TEXT("first_loss");
  case EDeathPhase::Fall:return TEXT("fall");
  case EDeathPhase::Collapse:return TEXT("collapse");
  case EDeathPhase::Settled:return TEXT("settled");
  case EDeathPhase::Sink:return TEXT("sink");
  case EDeathPhase::Expiry:return TEXT("expiry");
  case EDeathPhase::Clean:return TEXT("clean");
 }
 return TEXT("invalid");
}

struct FDeathFrame {
 EDeathPhase Phase;
 double Age,WallMs,GameMs,RenderMs,RHIMs,GPUMs;
 int32 ActiveEffects;
 FRTSSceneProfile Profile;

 TSharedRef<FJsonObject> ToJson() const {
  auto Row=MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("phase"),DeathPhaseName(Phase));
  Row->SetNumberField(TEXT("scene_age_s"),Age);
  Row->SetNumberField(TEXT("wall_ms"),WallMs);
  Row->SetNumberField(TEXT("game_ms"),GameMs);
  Row->SetNumberField(TEXT("render_ms"),RenderMs);
  Row->SetNumberField(TEXT("rhi_ms"),RHIMs);
  Row->SetNumberField(TEXT("gpu_ms"),GPUMs);
  Row->SetNumberField(TEXT("active_effects"),ActiveEffects);
  Row->SetNumberField(TEXT("scene_update_ms"),Profile.UpdateMs);
  Row->SetNumberField(TEXT("pose_ms"),Profile.PoseMs);
  Row->SetNumberField(TEXT("mesh_submit_ms"),Profile.MeshSubmitMs);
  Row->SetNumberField(TEXT("death_create_ms"),Profile.DeathCreateMs);
  Row->SetNumberField(TEXT("death_first_submit_ms"),Profile.DeathFirstSubmitMs);
  Row->SetNumberField(TEXT("live_retire_ms"),Profile.LiveRetireMs);
  Row->SetNumberField(TEXT("death_retire_ms"),Profile.DeathRetireMs);
  Row->SetNumberField(TEXT("death_created"),Profile.DeathCreates);
  Row->SetNumberField(TEXT("death_first_submitted"),Profile.DeathFirstSubmits);
  Row->SetNumberField(TEXT("live_retired"),Profile.LiveRetires);
  Row->SetNumberField(TEXT("death_retired"),Profile.DeathRetires);
  Row->SetNumberField(TEXT("pose_calls"),Profile.PoseCalls);
  Row->SetNumberField(TEXT("posed_cpu_vertices"),Profile.PosedVertices);
  Row->SetNumberField(TEXT("death_pose_calls"),Profile.DeathPoseCalls);
  Row->SetNumberField(TEXT("death_posed_cpu_vertices"),Profile.DeathPosedVertices);
  Row->SetNumberField(TEXT("death_batch_creates"),Profile.DeathBatchCreates);
  Row->SetNumberField(TEXT("death_batch_rebuilds"),Profile.DeathBatchRebuilds);
  Row->SetNumberField(TEXT("death_batch_updates"),Profile.DeathBatchUpdates);
  Row->SetNumberField(TEXT("death_batch_uploaded_vertices"),Profile.DeathBatchUploadedVertices);
  Row->SetNumberField(TEXT("death_batch_pose_solves"),Profile.DeathBatchPoseSolves);
  Row->SetNumberField(TEXT("death_batch_solved_vertices"),Profile.DeathBatchSolvedVertices);
  Row->SetNumberField(TEXT("death_batch_retires"),Profile.DeathBatchRetires);
  return Row;
 }
};

TSharedRef<FJsonObject> DeathDistribution(TArray<double>& Values) {
 auto Series=MakeShared<FJsonObject>();
 Series->SetNumberField(TEXT("n"),Values.Num());
 if(Values.IsEmpty())return Series;
 Values.Sort();
 const int32 Mid=Values.Num()/2;
 Series->SetNumberField(TEXT("median"),Values.Num()%2?Values[Mid]:(Values[Mid-1]+Values[Mid])*.5);
 Series->SetNumberField(TEXT("p95"),Values[FMath::CeilToInt(Values.Num()*.95)-1]);
 Series->SetNumberField(TEXT("max"),Values.Last());
 return Series;
}

double DeathCVar(const TCHAR* Name) {
 const auto* CVar=IConsoleManager::Get().FindConsoleVariable(Name);
 return CVar?CVar->GetFloat():-1.;
}
}

class FRTSDeathStressProbe {
 EDeathStage Stage=EDeathStage::Waiting;
 double Started=FPlatformTime::Seconds(),StageStarted=Started,PreviousWall=0,PreviousWorld=0,LossStart=0;
 double KillMs=0,SnapshotMs=0;
 int32 UnitCount=100,BuildingCount=4,StageFrames=0,HiddenUnit=0,MemoryBuilding=0,ScoutId=0,SecondSurvivor=0;
 int32 LiveSkinCount=0,ExpectedEffects=0,ActiveTriangles=0,CleanFrames=0;
 FIntPoint ViewSize=FIntPoint::ZeroValue;
 bool bCapture=false,bVerifyGeometry=false,bLifecycle=false,bLifeCameraOff=false,bLossCaptureRequested=false,bSkipInterval=false,bObservedMotion=false,bSawDraw=false;
 int32 LifeBurst=0,LifeAction=0,LifeFrames=0,LifeEvictions=0;
 TArray<int32> LifeExpected;
 TMap<int32,float> LifeStarts;
 TSet<int32> LifeKilled;
 TArray<TSharedPtr<FJsonValue>> LifeChecks;
 FDeathGeometryErrors LifeErrors;
 bool bLifePartialExpiry=false;
 FVector2D UnitPick=FVector2D::ZeroVector,BuildingPick=FVector2D::ZeroVector;
 TArray<int32> Units,Buildings,Deaths;
 TArray<TWeakObjectPtr<UMeshComponent>> DeathMeshes;
 TArray<FDeathFrame> Frames;
 TArray<TSharedPtr<FJsonValue>> Screenshots;
 TArray<TSharedPtr<FJsonValue>> GeometrySamples;
 FDeathGeometryErrors GeometryErrors;
 int32 NextGeometryWindow=0;
 TSharedRef<FJsonObject> Checkpoints=MakeShared<FJsonObject>();
 TSharedRef<FJsonObject> Memory=MakeShared<FJsonObject>();
 FString Failure;
 FString CapturePending;
 FString CaptureDirectory;
 FRTSVisualPoseMetrics FirstUnitPose,FirstBuildingPose;

 FString CaseFile(const TCHAR* Suffix) const {
  return FString::Printf(TEXT("death-stress-%d%s"),UnitCount,Suffix);
 }

 void Checkpoint(const TCHAR* Name,int32 Rendered) {
  Checkpoints->SetNumberField(Name,Rendered);
  Memory->SetNumberField(Name,double(FPlatformMemory::GetStats().UsedPhysical)/(1024.*1024.));
 }

 void Finish() {
  Stage=EDeathStage::Done;
  auto Report=MakeShared<FJsonObject>();
  Report->SetBoolField(TEXT("passed"),Failure.IsEmpty());
  Report->SetStringField(TEXT("failure"),Failure);
  Report->SetStringField(TEXT("scope"),TEXT("Development standalone rendered authority Kill and filtered snapshots, with simulation tick disabled. No transport, AI, or live-match frame-rate claim."));
  Report->SetStringField(TEXT("clocks"),TEXT("Wall intervals and prior completed scene tick. UE stat unit engine counters can lag different frames, zero readings are excluded from summaries. Scene update includes pose, pose includes mesh submission, and initial death submission is a subset of mesh submission."));
  Report->SetStringField(TEXT("first_loss_wall_scope"),TEXT("The first loss wall interval includes the authority kill loop and filtered snapshot. The scene profile begins after the fixture hook and excludes both operations."));
  Report->SetStringField(TEXT("memory_scope"),TEXT("Process resident MiB at the named boundaries. Component registration and renderer keys separately establish cleanup."));
  Report->SetBoolField(TEXT("capture_only"),bCapture);
  Report->SetBoolField(TEXT("verify_geometry"),bVerifyGeometry);
  Report->SetBoolField(TEXT("lifecycle_only"),bLifecycle);
  Report->SetBoolField(TEXT("batch_unit_deaths"),RTSBatchUnitDeathsEnabled());
  Report->SetNumberField(TEXT("physical_death_components"),DeathMeshes.Num());
  Report->SetNumberField(TEXT("requested_visible_unit_losses"),UnitCount);
  Report->SetNumberField(TEXT("requested_visible_building_losses"),BuildingCount);
  Report->SetNumberField(TEXT("effect_cap"),RTSDeath::PendingLimit);
  Report->SetNumberField(TEXT("expected_local_effects"),ExpectedEffects);
  Report->SetNumberField(TEXT("expected_local_evictions"),UnitCount+BuildingCount-ExpectedEffects);
  Report->SetNumberField(TEXT("initial_live_skeletal_meshes"),LiveSkinCount);
  Report->SetNumberField(TEXT("effect_triangles"),ActiveTriangles);
  if(!bLifecycle){
   Report->SetBoolField(TEXT("representative_poses_changed"),bObservedMotion);
   Report->SetBoolField(TEXT("representative_effects_drew"),bSawDraw);
  }
  if(!bVerifyGeometry&&!bLifecycle) {
   Report->SetNumberField(TEXT("authority_kill_ms"),KillMs);
   Report->SetNumberField(TEXT("post_loss_snapshot_ms"),SnapshotMs);
  }
  Report->SetObjectField(TEXT("logical_rendered_entity_count"),Checkpoints);
  Report->SetObjectField(TEXT("process_resident_mib"),Memory);
  Report->SetArrayField(TEXT("screenshots"),Screenshots);

  auto Config=MakeShared<FJsonObject>();
  Config->SetStringField(TEXT("platform"),FPlatformProperties::PlatformName());
  Config->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
  Config->SetStringField(TEXT("cpu"),FPlatformMisc::GetCPUBrand());
  Config->SetStringField(TEXT("gpu"),FPlatformMisc::GetPrimaryGPUBrand());
  Config->SetStringField(TEXT("rhi"),FHardwareInfo::GetHardwareInfo(NAME_RHI));
  Config->SetNumberField(TEXT("width"),ViewSize.X);
  Config->SetNumberField(TEXT("height"),ViewSize.Y);
  Config->SetNumberField(TEXT("vsync"),DeathCVar(TEXT("r.VSync")));
  Config->SetNumberField(TEXT("max_fps"),DeathCVar(TEXT("t.MaxFPS")));
  Config->SetNumberField(TEXT("screen_percentage"),DeathCVar(TEXT("r.ScreenPercentage")));
  Config->SetNumberField(TEXT("shadow_quality"),DeathCVar(TEXT("sg.ShadowQuality")));
  Config->SetBoolField(TEXT("frame_smoothing_disabled"),GEngine&&GEngine->bForceDisableFrameRateSmoothing);
  Config->SetBoolField(TEXT("offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
  Config->SetBoolField(TEXT("gpu_skinning_requested"),!FParse::Param(FCommandLine::Get(),TEXT("RTSProceduralMeshes")));
  Report->SetObjectField(TEXT("config"),Config);

  TArray<TSharedPtr<FJsonValue>> Raw;
  Raw.Reserve(Frames.Num());
  for(const auto& Frame:Frames)Raw.Add(MakeShared<FJsonValueObject>(Frame.ToJson()));
  Report->SetArrayField(TEXT("frames"),Raw);
  if(!bCapture&&!bVerifyGeometry&&!bLifecycle) {
   auto Phases=MakeShared<FJsonObject>();
   for(const EDeathPhase Phase:{EDeathPhase::Live,EDeathPhase::FirstLoss,EDeathPhase::Fall,EDeathPhase::Collapse,
                                EDeathPhase::Settled,EDeathPhase::Sink,EDeathPhase::Expiry,EDeathPhase::Clean}) {
    auto Stats=MakeShared<FJsonObject>();
    const auto Add=[&](const TCHAR* Name,auto Read,bool bSkipZero=false){
     TArray<double> Values;
     for(const auto& Frame:Frames)if(Frame.Phase==Phase){
      const double Value=Read(Frame);
      if(!bSkipZero||Value>0.)Values.Add(Value);
     }
     Stats->SetObjectField(Name,DeathDistribution(Values));
    };
    Add(TEXT("wall_ms"),[](const auto& F){return F.WallMs;});
    Add(TEXT("game_ms"),[](const auto& F){return F.GameMs;},true);
    Add(TEXT("render_ms"),[](const auto& F){return F.RenderMs;},true);
    Add(TEXT("rhi_ms"),[](const auto& F){return F.RHIMs;},true);
    Add(TEXT("gpu_ms"),[](const auto& F){return F.GPUMs;},true);
    Add(TEXT("scene_update_ms"),[](const auto& F){return F.Profile.UpdateMs;});
    Add(TEXT("pose_ms"),[](const auto& F){return F.Profile.PoseMs;});
    Add(TEXT("mesh_submit_ms"),[](const auto& F){return F.Profile.MeshSubmitMs;});
    Phases->SetObjectField(DeathPhaseName(Phase),Stats);
   }
   Report->SetObjectField(TEXT("phases"),Phases);
  }
  if(bVerifyGeometry) {
   auto Geometry=MakeShared<FJsonObject>();
   Geometry->SetNumberField(TEXT("position_tolerance_cm"),DeathPositionToleranceCm);
   Geometry->SetNumberField(TEXT("normal_tolerance"),DeathNormalTolerance);
   Geometry->SetNumberField(TEXT("pose_time_tolerance_s"),DeathPoseTimeTolerance);
   Geometry->SetNumberField(TEXT("windows_passed"),NextGeometryWindow);
   Geometry->SetNumberField(TEXT("windows_required"),UE_ARRAY_COUNT(DeathGeometryWindows));
   Geometry->SetArrayField(TEXT("samples"),GeometrySamples);
   GeometryErrors.Write(*Geometry);
   Report->SetObjectField(TEXT("geometry"),Geometry);
  }
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Tests");
  IFileManager::Get().MakeDirectory(*Directory,true);
  if(bLifecycle){
   auto Life=MakeShared<FJsonObject>();
   Life->SetNumberField(TEXT("bursts"),LifeBurst);
   Life->SetNumberField(TEXT("fifo_evictions"),LifeEvictions);
   Life->SetNumberField(TEXT("state_checks"),LifeFrames);
   Life->SetBoolField(TEXT("partial_expiry_observed"),bLifePartialExpiry);
   Life->SetArrayField(TEXT("checkpoints"),LifeChecks);
   LifeErrors.Write(*Life);
   Report->SetObjectField(TEXT("lifecycle"),Life);
  }
  const TCHAR* Suffix=bLifecycle?TEXT("-lifecycle.json"):bVerifyGeometry?TEXT("-geometry.json"):bCapture?TEXT("-capture.json"):TEXT(".json");
  const FString Path=Directory/CaseFile(Suffix);
  FString Json;
  FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  const bool bSaved=FFileHelper::SaveStringToFile(Json,*Path),bPassed=bSaved&&Failure.IsEmpty();
  UE_LOG(LogTemp,Display,TEXT("RTS_DEATH_STRESS passed=%d units=%d buildings=%d effects=%d frames=%d capture=%d verify_geometry=%d report=%s failure=%s"),
         bPassed,UnitCount,BuildingCount,DeathMeshes.Num(),Frames.Num(),bCapture,bVerifyGeometry,*Path,*Failure);
  FPlatformMisc::RequestExitWithStatus(false,bPassed?0:1);
 }

 void Fail(const FString& Reason) {Failure=Reason;Finish();}
 void Advance(EDeathStage Next,double Now) {Stage=Next;StageStarted=Now;StageFrames=0;}

 void PinCamera(ARTSController& C) const {
  C.CameraCenter=DeathCameraCenter;
  if(bLifeCameraOff)C.CameraCenter+=FVector2D(300,300);
  C.CameraRotation=0;
  C.Zoom=DeathCameraZoom;
  if(ViewSize.X>0)C.ViewSize=FVector2D(ViewSize.X,ViewSize.Y);
 }

 bool Initialize(ARTSWorld& World,ARTSController& C) {
  World.SetActorTickEnabled(false);
  World.Entities.Reset();
  World.Obstacles.Reset();
  World.ObstacleCount=-1;
  World.Players.Reset(2);
  World.NextId=1;
  World.Map=0;
  World.Time=10;
  World.Phase=1;
  World.bFogOfWar=true;
  World.bSandbox=true;
  for(int32 I=0;I<2;++I) {
   auto& P=World.Players.AddDefaulted_GetRef();
   P.Slot=P.Team=I;
   P.Faction=TEXT("nephites");
   P.Age=4;
   P.Explored.Init(0,RTS::FogSize*RTS::FogSize);
   if(I==0)P.Controller=&C;
  }
  C.ServerSlot=0;
  C.bMenu=false;
  C.bSoundEnabled=false;
  C.bReducedMotion=false;
  C.DisplayPositions.Reset();
  C.Selection.Reset();
  C.DeathAnimations.Reset();

  auto* Scout=World.Spawn(TEXT("wilderness_spy"),0,{125,128.75});
  const auto* Hidden=World.Spawn(TEXT("bowman"),1,{128.75,126.25});
  const auto* Remembered=World.Spawn(TEXT("tents_and_habitations"),1,{128.75,131.25});
  if(!Scout||!Hidden||!Remembered){Fail(TEXT("Could not spawn fog sentinels"));return false;}
  ScoutId=Scout->Id;
  HiddenUnit=Hidden->Id;
  MemoryBuilding=Remembered->Id;
  World.Snapshot();
  if(!C.FindEntity(HiddenUnit)||!C.FindEntity(MemoryBuilding)){
   Fail(TEXT("Scouting snapshot did not establish both sentinel observations"));return false;
  }
  PinCamera(C);
  const auto Place=[&](const TCHAR* Type,int32 Owner,double X,double Y) {
   return World.Spawn(Type,Owner,C.ScreenToWorld(FVector2D(ViewSize.X*X,ViewSize.Y*Y)));
  };
  Scout->Pos=C.ScreenToWorld(FVector2D(ViewSize.X*.86,ViewSize.Y*.54));
  const auto* Survivor=Place(TEXT("man_of_moroni"),0,.105,.48);
  if(!Survivor){Fail(TEXT("Could not spawn an on-screen survivor"));return false;}
  SecondSurvivor=Survivor->Id;

  const TCHAR* UnitTypes[]={TEXT("man_of_moroni"),TEXT("bowman"),TEXT("man_of_the_sword"),TEXT("teacher_of_the_word")};
  const TCHAR* DiverseTypes[]={TEXT("curelom"),TEXT("kings_chariot"),TEXT("hagoths_vessel"),TEXT("wild_game")};
  const TCHAR* BuildingTypes[]={TEXT("seat_of_the_people"),TEXT("encampment_of_war"),TEXT("storehouse_of_provisions"),TEXT("tents_and_habitations")};
  const int32 GridSide=UnitCount==100?10:20;
  for(int32 I=0;I<UnitCount-1;++I) {
   const int32 DiverseIndex=I-(UnitCount-5);
   const bool Diverse=bVerifyGeometry&&DiverseIndex>=0;
   const bool Wildlife=Diverse&&DiverseIndex==3;
   auto* Unit=Place(Diverse?DiverseTypes[DiverseIndex]:UnitTypes[I%UE_ARRAY_COUNT(UnitTypes)],Wildlife?-1:0,
    Wildlife?.90:.17+.58*(I%GridSide+.5)/GridSide,Wildlife?.54:.34+.30*(I/GridSide+.5)/GridSide);
   if(!Unit){Fail(TEXT("Could not spawn the casualty formation"));return false;}
   Unit->Heading=float(I%8)*PI/4.f;
   Units.Add(Unit->Id);
  }
  const auto* VisibleEnemy=Place(TEXT("man_of_moroni"),1,.86,.47);
  if(!VisibleEnemy){Fail(TEXT("Could not spawn the visible enemy casualty"));return false;}
  Units.Add(VisibleEnemy->Id);
  for(int32 I=0;I<BuildingCount;++I) {
   auto* Building=Place(BuildingTypes[I%UE_ARRAY_COUNT(BuildingTypes)],0,.18+.64*(I+.5)/BuildingCount,.245);
   if(!Building){Fail(TEXT("Could not spawn destruction targets"));return false;}
   if(bVerifyGeometry&&I==BuildingCount/2){
    Building->Progress=.44f;
    Building->HP=Building->MaxHP=World.MaxHP(*Building);
   }
   Buildings.Add(Building->Id);
  }
  Deaths=Units;
  Deaths.Append(Buildings);
  ExpectedEffects=FMath::Min(Deaths.Num(),RTSDeath::PendingLimit);
  World.Population();
  World.Snapshot();
  const auto* MemoryView=C.FindEntity(MemoryBuilding);
  if(C.FindEntity(HiddenUnit)||!MemoryView||!MemoryView->bRemembered||!C.DeathAnimations.IsEmpty()) {
   Fail(TEXT("Scout movement must withhold the live unit and remember the building without producing death effects"));return false;
  }
  if(!C.FindEntity(Units.Last())||!World.CanSee(0,*VisibleEnemy)) {
   Fail(TEXT("The enemy casualty is not observable after the scout movement"));return false;
  }
  C.Selection={Units[0],Buildings.Last(),SecondSurvivor};
  return true;
 }

 bool ExactPick(ARTSScene3D& Scene,const ARTSController& C,int32 Id,FVector2D& Pixel) {
  const auto* View=C.FindEntity(Id);
  if(!View)return false;
  const FBox2D Bounds=Scene.Bounds(*View);
  if(!Bounds.bIsValid)return false;
  const auto* HUD=Cast<ARTSHUD>(C.GetHUD());
  if(!HUD)return false;
  const int32 MinX=FMath::Max(0,FMath::CeilToInt(Bounds.Min.X)),MaxX=FMath::Min(ViewSize.X-1,FMath::FloorToInt(Bounds.Max.X));
  const int32 MinY=FMath::Max(0,FMath::CeilToInt(Bounds.Min.Y)),MaxY=FMath::Min(ViewSize.Y-1,FMath::FloorToInt(Bounds.Max.Y));
  for(int32 Y=MinY;Y<=MaxY;Y+=2)for(int32 X=MinX;X<=MaxX;X+=2) {
   const FVector2D Candidate(X,Y);
   if(!HUD->BlocksWorld(Candidate)&&C.EntityUnder(Candidate,false)==Id){Pixel=Candidate;return true;}
  }
  return false;
 }

 bool ValidateLive(ARTSScene3D& Scene,const ARTSController& C) {
  for(int32 Id:Deaths) {
   const auto* R=Scene.Rendered.Find(Id);
   if(!C.FindEntity(Id)||!R||!R->Component()||!R->Component()->IsRegistered()||!R->Component()->IsVisible()) {
    Fail(FString::Printf(TEXT("Visible pre-loss entity %d had no registered visible mesh"),Id));return false;
   }
   if(R->Skin.IsValid())++LiveSkinCount;
  }
  const bool bGpuRequested=!FParse::Param(FCommandLine::Get(),TEXT("RTSProceduralMeshes"));
  if((bGpuRequested&&LiveSkinCount!=UnitCount)||(!bGpuRequested&&LiveSkinCount!=0)){
   Fail(FString::Printf(TEXT("Pre-loss skeletal mesh count %d, expected %d"),LiveSkinCount,bGpuRequested?UnitCount:0));return false;
  }
  if(!ExactPick(Scene,C,Units.Last(),UnitPick)||!ExactPick(Scene,C,Buildings.Last(),BuildingPick)) {
   Fail(TEXT("Could not verify a living unit and building at gameplay pixels before loss"));return false;
  }
  Checkpoint(TEXT("live"),Scene.RenderedCount());
  return true;
 }

 void Trigger(ARTSWorld& World,ARTSController& C) {
  const double Before=FPlatformTime::Seconds();
  for(int32 Id:Deaths)World.Kill(Id);
  KillMs=(FPlatformTime::Seconds()-Before)*1000.;
  // Kill both concealed sentinels after the burst so an erroneous event would
  // displace a valid retained effect and fail the exact FIFO assertion.
  World.Kill(HiddenUnit);
  World.Kill(MemoryBuilding);
  const double SnapshotStarted=FPlatformTime::Seconds();
  World.Snapshot();
  SnapshotMs=(FPlatformTime::Seconds()-SnapshotStarted)*1000.;
  LossStart=C.GetWorld()->GetRealTimeSeconds();
 }

 bool ValidateFirstLoss(ARTSScene3D& Scene,const ARTSController& C,ARTSWorld& World) {
  const auto& Profile=Scene.FrameProfile();
  const int32 Physical=Scene.PhysicalDeathComponentCount();
  const bool Batched=RTSBatchUnitDeathsEnabled();
  const bool CountsOK=Batched?
   Physical>BuildingCount&&Physical<ExpectedEffects&&Profile.DeathBatchCreates>0&&Profile.DeathBatchPoseSolves>0:
   Physical==ExpectedEffects&&Profile.DeathPoseCalls==ExpectedEffects;
  if(C.DeathAnimations.Num()!=ExpectedEffects||Profile.DeathCreates!=Physical||Profile.DeathFirstSubmits!=Physical||
     Profile.LiveRetires!=Deaths.Num()||!CountsOK||!C.PendingDeaths.IsEmpty()) {
   Fail(FString::Printf(TEXT("First loss counts were FX=%d creates=%d submittals=%d pose_calls=%d live_retirements=%d pending=%d"),
    C.DeathAnimations.Num(),Profile.DeathCreates,Profile.DeathFirstSubmits,Profile.DeathPoseCalls,Profile.LiveRetires,C.PendingDeaths.Num()));
   return false;
  }
  const int32 Evicted=Deaths.Num()-ExpectedEffects;
  for(int32 I=0;I<Deaths.Num();++I) {
   const int32 Id=Deaths[I];
   const auto* R=Scene.Rendered.Find(-Id);
   if(World.Find(Id)||C.FindEntity(Id)||Scene.Rendered.Contains(Id)||(I<Evicted&&R)) {
    Fail(FString::Printf(TEXT("Removed/evicted ID %d survived in authority, snapshot, or renderer"),Id));return false;
   }
   if(I<Evicted)continue;
   const auto& Effect=C.DeathAnimations[I-Evicted].Entity;
   if(Effect.Id!=Id||!R||!R->Mesh.IsValid()||R->Skin.IsValid()||!R->Component()->IsRegistered()||!R->Component()->IsVisible()||!R->Model||
      (!R->IsBatchedDeath()&&R->Vertices.IsEmpty())||Effect.HP!=0||Effect.Target!=0||!Effect.Cargo.IsNearlyZero()||!Effect.Queue.IsEmpty()||!Effect.PassengerIds.IsEmpty()){
    Fail(FString::Printf(TEXT("Loss ID %d failed FIFO, mesh, or sanitized presentation invariants"),Id));return false;
   }
   DeathMeshes.AddUnique(TWeakObjectPtr<UMeshComponent>(R->Component()));
   ActiveTriangles+=R->Model->Triangles;
  }
  if(DeathMeshes.Num()!=Physical){Fail(TEXT("Physical death components differ from the owned logical ranges"));return false;}
  const auto* MemoryView=C.FindEntity(MemoryBuilding);
  if(World.Find(HiddenUnit)||World.Find(MemoryBuilding)||C.FindEntity(HiddenUnit)||!MemoryView||!MemoryView->bRemembered||
     Scene.Rendered.Contains(-HiddenUnit)||Scene.Rendered.Contains(-MemoryBuilding)) {
   Fail(TEXT("Killing concealed sentinels leaked their death or removed the remembered building"));return false;
  }
  if(C.Selection.Num()!=1||C.Selection[0]!=SecondSurvivor||C.EntityUnder(UnitPick,false)!=0||C.EntityUnder(BuildingPick,false)!=0){
   Fail(TEXT("The casualty selection or former exact-pick pixels were not cleared"));return false;
  }
  if(!Scene.GetPoseMetrics(-Units.Last(),FirstUnitPose)||!Scene.GetPoseMetrics(-Buildings.Last(),FirstBuildingPose)||
     FirstUnitPose.State!=TEXT("death")||FirstBuildingPose.State!=TEXT("destroy")) {
   Fail(TEXT("Initial death and destruction poses missing"));return false;
  }
  Checkpoint(TEXT("first_loss"),Scene.RenderedCount());
  return true;
 }

 void ObserveVisibleEffects(ARTSScene3D& Scene,const ARTSController& C) {
  const auto* Unit=Scene.Rendered.Find(-Units.Last());
  const auto* Building=Scene.Rendered.Find(-Buildings.Last());
  if(!Unit||!Building)return;
  const double WorldTime=C.GetWorld()->GetTimeSeconds();
  const auto RecentlyDrawn=[&](const FRTSRendered3D& R) {
   const double LastDraw=R.Component()->GetLastRenderTimeOnScreen();
   return LastDraw>0.&&WorldTime-LastDraw<.5;
  };
  bSawDraw|=RecentlyDrawn(*Unit)&&RecentlyDrawn(*Building);
  if(!bObservedMotion) {
   FRTSVisualPoseMetrics UnitPose,BuildingPose;
   bObservedMotion=Scene.GetPoseMetrics(-Units.Last(),UnitPose)&&Scene.GetPoseMetrics(-Buildings.Last(),BuildingPose)&&
    UnitPose.PoseHash!=FirstUnitPose.PoseHash&&BuildingPose.PoseHash!=FirstBuildingPose.PoseHash;
  }
 }

 bool VerifyGeometry(const ARTSScene3D& Scene,const ARTSController& C,double Age) {
  if(NextGeometryWindow>=int32(UE_ARRAY_COUNT(DeathGeometryWindows)))return true;
  const auto& Window=DeathGeometryWindows[NextGeometryWindow];
  if(Age<Window.Begin)return true;
  if(Age>=Window.End) {
   Fail(FString::Printf(TEXT("No completed geometry frame for %s before %.3f s"),Window.Name,Window.End));return false;
  }

  auto Sample=MakeShared<FJsonObject>();
  Sample->SetStringField(TEXT("phase"),Window.Name);
  Sample->SetNumberField(TEXT("scene_age_s"),Age);
  TArray<TSharedPtr<FJsonValue>> Models;
  TSet<FString> SampledModels;
  TSet<FString> SampledDefs;
  FDeathGeometryErrors WindowErrors;
  int32 UnitModels=0,FullBuildings=0,PartialBuildings=0;
  FString Error;
  for(const auto& Death:C.DeathAnimations) {
   const auto& Effect=Death.Entity;
   const auto* Rendered=Scene.Rendered.Find(-Effect.Id);
   if(!Rendered){Error=FString::Printf(TEXT("No rendered corpse for effect %d"),Effect.Id);break;}
   const FString SampleKey=FString::Printf(TEXT("%s:%.3f:%d:%.5f"),*Rendered->Key,Effect.Progress,Effect.Owner,Effect.Heading);
   if(SampledModels.Contains(SampleKey))continue;
   SampledModels.Add(SampleKey);
   const bool bBuilding=FRTSCatalog::Get().Def(Effect.Def).bBuilding;
   SampledDefs.Add(FRTSCatalog::Get().Def(Effect.Def).Id);
   if(!bBuilding)++UnitModels;
   else if(Effect.Progress<1.f)++PartialBuildings;
   else ++FullBuildings;

   FDeathGeometryErrors ModelErrors;
   const FString MeshError=CompareDeathMesh(*Rendered,Effect,bBuilding,Age,ModelErrors);
   WindowErrors.Add(ModelErrors);
   auto Model=MakeShared<FJsonObject>();
   Model->SetStringField(TEXT("model"),Rendered->Key);
   Model->SetNumberField(TEXT("entity_id"),Effect.Id);
   Model->SetBoolField(TEXT("building"),bBuilding);
   Model->SetNumberField(TEXT("progress"),Effect.Progress);
   Model->SetNumberField(TEXT("rendered_pose_age_s"),Rendered->PoseElapsed);
   ModelErrors.Write(*Model);
   Models.Add(MakeShared<FJsonValueObject>(Model));
   if(!MeshError.IsEmpty()){Error=SampleKey+TEXT(" | ")+MeshError;break;}
  }
  Sample->SetNumberField(TEXT("unit_meshes"),UnitModels);
  Sample->SetNumberField(TEXT("completed_building_meshes"),FullBuildings);
  Sample->SetNumberField(TEXT("partial_building_meshes"),PartialBuildings);
  Sample->SetArrayField(TEXT("models"),Models);
  WindowErrors.Write(*Sample);
  GeometrySamples.Add(MakeShared<FJsonValueObject>(Sample));
  GeometryErrors.Add(WindowErrors);
  if(Error.IsEmpty()&&(UnitModels<4||FullBuildings<3||PartialBuildings!=1))
   Error=TEXT("Missing baseline unit, completed building, or partial building mesh coverage");
  for(const TCHAR* Id:{TEXT("curelom"),TEXT("kings_chariot"),TEXT("hagoths_vessel"),TEXT("wild_game")})
   if(Error.IsEmpty()&&!SampledDefs.Contains(Id))Error=FString(TEXT("Missing diverse death geometry: "))+Id;
  if(!Error.IsEmpty()){
   Fail(FString::Printf(TEXT("Geometry %s at %.3f s: %s"),Window.Name,Age,*Error));return false;
  }
  ++NextGeometryWindow;
  return true;
 }

 bool ValidateCleanup(ARTSScene3D& Scene,const ARTSController& C) {
  if(Scene.FrameProfile().DeathRetires!=DeathMeshes.Num()||Scene.FrameProfile().DeathPoseCalls!=0||
     Scene.PhysicalDeathComponentCount()!=0||!C.DeathAnimations.IsEmpty()||Scene.RenderedCount()!=C.Entities.Num()){
   Fail(TEXT("The expiry tick did not retire the entire retained burst"));return false;
  }
  for(int32 Id:Deaths)if(Scene.Rendered.Contains(-Id)){Fail(TEXT("A negative render key survived expiry"));return false;}
  for(const auto& Handle:DeathMeshes)if(const auto* Component=Handle.Get())if(Component->IsRegistered()) {
   Fail(TEXT("A death component remained registered after expiry"));return false;
  }
  for(int32 Id:{ScoutId,SecondSurvivor,MemoryBuilding}){
   const auto* R=Scene.Rendered.Find(Id);
   if(!C.FindEntity(Id)||!R||!R->Component()||!R->Component()->IsRegistered()){
    Fail(TEXT("A live survivor or remembered building was removed during cleanup"));return false;
   }
  }
  if(!bObservedMotion||!bSawDraw) {
   Fail(TEXT("No representative changing unit/building poses and actual on-screen draws were observed"));return false;
  }
  Checkpoint(TEXT("expiry"),Scene.RenderedCount());
  return true;
 }

 void LifecycleBurst(ARTSWorld& World,ARTSController& C) {
  const int32 Begin=LifeBurst*128,End=LifeBurst==2?Deaths.Num():Begin+128;
  const float StartedAt=C.GetWorld()->GetRealTimeSeconds();
  for(int32 I=Begin;I<End;++I){
   const int32 Id=Deaths[I];
   LifeStarts.Add(Id,StartedAt);LifeKilled.Add(Id);
   LifeExpected.Add(Id);
   if(LifeExpected.Num()>RTSDeath::PendingLimit){LifeExpected.RemoveAt(0);++LifeEvictions;}
   World.Kill(Id);
  }
  ++LifeBurst;
  if(LifeBurst==3){World.Kill(HiddenUnit);World.Kill(MemoryBuilding);}
  World.Snapshot();
 }

 bool LifecycleCheck(ARTSScene3D& Scene,const ARTSController& C,ARTSWorld& World,double CompletedAt,
                     const TCHAR* Label=nullptr,bool Geometry=false) {
  const int32 Before=LifeExpected.Num();
  LifeExpected.RemoveAll([&](int32 Id){return float(CompletedAt)-LifeStarts.FindChecked(Id)>3.2f;});
  if(Before>LifeExpected.Num()&&!LifeExpected.IsEmpty())bLifePartialExpiry=true;
  if(C.DeathAnimations.Num()!=LifeExpected.Num()||LifeExpected.Num()>RTSDeath::PendingLimit){
   Fail(TEXT("Lifecycle effect count differs from independent FIFO/expiry reference"));return false;
  }
  struct FRange {int32 FirstVertex,Vertices,FirstIndex,Indices;bool Batched;};
  TMap<UProceduralMeshComponent*,TArray<FRange>> Ranges;
  for(int32 I=0;I<LifeExpected.Num();++I){
   const int32 Id=LifeExpected[I];const auto& Effect=C.DeathAnimations[I].Entity;
   const auto* R=Scene.Rendered.Find(-Id);
   if(Effect.Id!=Id||!R||!R->Model||!R->Mesh.IsValid()||!R->Mesh->IsRegistered()||R->Skin.IsValid()){
    Fail(TEXT("Lifecycle FIFO member has no registered presentation range"));return false;
   }
   if(R->Mesh->IsVisible()==bLifeCameraOff){Fail(TEXT("Lifecycle camera visibility differs from the renderer"));return false;}
   const auto* Section=R->Mesh->GetProcMeshSection(0);
   if(!Section){Fail(TEXT("Lifecycle mesh section is missing"));return false;}
   const int32 V=R->IsBatchedDeath()?R->BatchFirstVertex:0,T=R->IsBatchedDeath()?R->BatchFirstIndex:0;
   Ranges.FindOrAdd(R->Mesh.Get()).Add({V,R->Model->VertexCount,T,R->Model->Triangles*3,R->IsBatchedDeath()});
   DeathMeshes.AddUnique(TWeakObjectPtr<UMeshComponent>(R->Component()));
   if(Geometry){
    FDeathGeometryErrors Errors;
    const double Age=C.bReducedMotion?2.5:float(CompletedAt)-LifeStarts.FindChecked(Id);
    const FString Error=CompareDeathMesh(*R,Effect,FRTSCatalog::Get().Def(Effect.Def).bBuilding,Age,Errors);
    LifeErrors.Add(Errors);
    if(!Error.IsEmpty()){Fail(FString::Printf(TEXT("Lifecycle %s, ID %d: %s"),Label?Label:TEXT("frame"),Id,*Error));return false;}
   }
  }
  if(Scene.PhysicalDeathComponentCount()!=Ranges.Num()){
   Fail(TEXT("Lifecycle has an orphaned physical death component"));return false;
  }
  for(auto& Pair:Ranges){
   const auto* Section=Pair.Key->GetProcMeshSection(0);auto& BodyRanges=Pair.Value;
   if(BodyRanges[0].Batched&&BodyRanges.Num()>24){Fail(TEXT("Unit-death batch exceeded its 24-body bound"));return false;}
   if(BodyRanges[0].Batched&&Section->ProcVertexBuffer.Num()>65535){Fail(TEXT("Unit-death batch exceeded its vertex bound"));return false;}
   BodyRanges.Sort([](const FRange& A,const FRange& B){return A.FirstVertex<B.FirstVertex;});
   int32 End=0;
   for(const auto& Range:BodyRanges){if(Range.FirstVertex!=End){Fail(TEXT("Death body vertex ranges overlap or leave stale geometry"));return false;}End+=Range.Vertices;}
   if(End!=Section->ProcVertexBuffer.Num()){Fail(TEXT("Death batch retains unowned vertices"));return false;}
   BodyRanges.Sort([](const FRange& A,const FRange& B){return A.FirstIndex<B.FirstIndex;});End=0;
   for(const auto& Range:BodyRanges){if(Range.FirstIndex!=End){Fail(TEXT("Death body index ranges overlap or leave stale geometry"));return false;}End+=Range.Indices;}
   if(End!=Section->ProcIndexBuffer.Num()){Fail(TEXT("Death batch retains unowned triangles"));return false;}
  }
  for(int32 Id:LifeKilled){
   if(World.Find(Id)||C.FindEntity(Id)||C.Selection.Contains(Id)||Scene.Rendered.Contains(Id)||
      (Scene.Rendered.Contains(-Id)&&!LifeExpected.Contains(Id))){Fail(TEXT("Killed or evicted lifecycle ID survived outside its allowed presentation range"));return false;}
  }
  if(Scene.RenderedCount()!=C.Entities.Num()+LifeExpected.Num()){
   Fail(TEXT("Lifecycle logical render keys differ from filtered entities and retained effects"));return false;
  }
  if(LifeBurst==3){
   const auto* MemoryView=C.FindEntity(MemoryBuilding);
   if(C.FindEntity(HiddenUnit)||!MemoryView||!MemoryView->bRemembered||Scene.Rendered.Contains(-HiddenUnit)||Scene.Rendered.Contains(-MemoryBuilding)){
    Fail(TEXT("Lifecycle concealed deaths leaked through snapshot or render state"));return false;
   }
  }
  ++LifeFrames;
  if(Label){
   auto Check=MakeShared<FJsonObject>();Check->SetStringField(TEXT("name"),Label);
   Check->SetNumberField(TEXT("age_s"),CompletedAt-LossStart);
   Check->SetNumberField(TEXT("logical_effects"),LifeExpected.Num());
   Check->SetNumberField(TEXT("physical_components"),Ranges.Num());
   Check->SetBoolField(TEXT("geometry_checked"),Geometry);
   LifeChecks.Add(MakeShared<FJsonValueObject>(Check));
  }
  return true;
 }

 void RunLifecycle(ARTSScene3D& Scene,ARTSController& C,ARTSWorld& World,double CompletedAt) {
  const double Age=CompletedAt-LossStart;
  if(!LifecycleCheck(Scene,C,World,CompletedAt))return;
  if(LifeBurst<3&&Age>=(LifeBurst==1?.30:.65)){
   LifecycleBurst(World,C);return;
  }
  // Transition only after checking the previously completed scene. The next
  // scene tick sees these settings and is validated on the following call.
  if(LifeAction==0&&Age>=.20){
   if(!LifecycleCheck(Scene,C,World,CompletedAt,TEXT("first_fall"),true))return;
   ++LifeAction;
  }else if(LifeAction==1&&LifeBurst>=2&&Age>=.42){
   if(!LifecycleCheck(Scene,C,World,CompletedAt,TEXT("second_cohort"),true))return;
   bLifeCameraOff=true;PinCamera(C);++LifeAction;
  }else if(LifeAction==2&&LifeBurst==3&&Age>=.80){
   if(!LifecycleCheck(Scene,C,World,CompletedAt,TEXT("hidden_after_fifo_eviction")))return;
   bLifeCameraOff=false;PinCamera(C);++LifeAction;
  }else if(LifeAction==3&&Age>=.90){
   if(!LifecycleCheck(Scene,C,World,CompletedAt,TEXT("visible_reentry"),true))return;
   C.bReducedMotion=true;++LifeAction;
  }else if(LifeAction==4&&Age>=1.05){
   if(!LifecycleCheck(Scene,C,World,CompletedAt,TEXT("reduced_motion"),true))return;
   C.bReducedMotion=false;++LifeAction;
  }else if(LifeAction==5&&Age>=1.20){
   if(!LifecycleCheck(Scene,C,World,CompletedAt,TEXT("motion_restored_mid_fall"),true))return;
   ++LifeAction;
  }else if(LifeAction==6&&Age>=2.85){
   if(!LifecycleCheck(Scene,C,World,CompletedAt,TEXT("mixed_age_sinking"),true))return;
   ++LifeAction;
  }
  if(LifeBurst==3&&LifeExpected.IsEmpty()){
   if(LifeAction!=7||!bLifePartialExpiry||LifeEvictions!=Deaths.Num()-RTSDeath::PendingLimit){
    Fail(TEXT("Lifecycle completed without all transition, eviction, and partial-expiry checks"));return;
   }
   if(!LifecycleCheck(Scene,C,World,CompletedAt,TEXT("complete_expiry")))return;
   for(const auto& Handle:DeathMeshes)if(Handle.IsValid()&&Handle->IsRegistered()){
    Fail(TEXT("Lifecycle effect component remained registered after final expiry"));return;
   }
   if(C.Selection.Num()!=1||C.Selection[0]!=SecondSurvivor||C.EntityUnder(UnitPick,false)!=0||C.EntityUnder(BuildingPick,false)!=0){
    Fail(TEXT("Lifecycle casualty selection/picking remained active after final expiry"));return;
   }
   Finish();
  }
 }

 void Sample(EDeathPhase Phase,const ARTSScene3D& Scene,const ARTSController& C,double Age,double WallMs,bool bExclude) {
  if(bCapture||bVerifyGeometry||bLifecycle||bExclude)return;
  Frames.Add({Phase,Age,WallMs,FPlatformTime::ToMilliseconds(GGameThreadTime),FPlatformTime::ToMilliseconds(GRenderThreadTime),
   FPlatformTime::ToMilliseconds(GRHIThreadTime),FPlatformTime::ToMilliseconds(GGPUFrameTime),C.DeathAnimations.Num(),Scene.FrameProfile()});
 }

 void RequestCapture(const TCHAR* Name) {
  CapturePending=CaptureDirectory/FString(Name)+TEXT(".png");
  Screenshots.Add(MakeShared<FJsonValueString>(CapturePending));
  const FString Absolute=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/CapturePending);
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Absolute),true);
  FScreenshotRequest::RequestScreenshot(Absolute,false,false);
 }

 bool CaptureWritten() {
  if(CapturePending.IsEmpty())return true;
  if(IFileManager::Get().FileSize(*(FPaths::ProjectSavedDir()/CapturePending))<=0)return false;
  CapturePending.Reset();
  return true;
 }

public:
 void Run(ARTSScene3D& Scene,ARTSController& C) {
  if(Stage==EDeathStage::Done)return;
  const double Now=FPlatformTime::Seconds(),WorldTime=C.GetWorld()->GetRealTimeSeconds();
  if(Now-Started>90.){Fail(FString::Printf(TEXT("Timed out in stage %d"),int32(Stage)));return;}
  const double WallMs=(Now-PreviousWall)*1000.,CompletedAt=PreviousWorld,Age=CompletedAt-LossStart;
  PreviousWall=Now;
  PreviousWorld=WorldTime;
  const bool bExclude=bSkipInterval;
  bSkipInterval=false;
  PinCamera(C);
  ++StageFrames;
  const auto* Mode=C.GetWorld()->GetAuthGameMode<ARTSGameMode>();
  if(!Mode||!Mode->Simulation||!C.Me())return;
  auto& World=*Mode->Simulation;
  switch(Stage){
   case EDeathStage::Waiting: {
    if(C.Phase!=0||World.Phase!=0||FParse::Param(FCommandLine::Get(),TEXT("RTSV09RenderStress"))) {
     Fail(TEXT("The standalone loss probe requires a fresh lobby and exclusive fixture flags"));return;
    }
    const auto* Client=C.GetWorld()->GetGameViewport();
    if(!Client||!Client->Viewport||Client->Viewport->GetSizeXY().GetMin()<=0)return;
    ViewSize=Client->Viewport->GetSizeXY();
    if(FParse::Value(FCommandLine::Get(),TEXT("RTSDeathStressUnits="),UnitCount)&&UnitCount!=100&&UnitCount!=400){
     Fail(TEXT("RTSDeathStressUnits must be 100 or 400"));return;
    }
    BuildingCount=UnitCount==400?8:4;
    bCapture=FParse::Param(FCommandLine::Get(),TEXT("RTSDeathStressCapture"));
    bVerifyGeometry=FParse::Param(FCommandLine::Get(),TEXT("RTSDeathStressVerifyGeometry"));
    bLifecycle=FParse::Param(FCommandLine::Get(),TEXT("RTSDeathStressLifecycle"));
    if(int32(bCapture)+int32(bVerifyGeometry)+int32(bLifecycle)>1) {
     Fail(TEXT("Geometry, lifecycle, and screenshot verification require separate runs"));return;
    }
    if(bLifecycle&&UnitCount!=400){
     Fail(TEXT("Lifecycle verification requires RTSDeathStressUnits=400"));return;
    }
    CaptureDirectory=TEXT("Screenshots")/(CaseFile(TEXT("-"))+FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")));
    GEngine->bForceDisableFrameRateSmoothing=true;
    GEngine->bUseFixedFrameRate=false;
    for(const TCHAR* Name:{TEXT("r.VSync"),TEXT("t.MaxFPS")})
     if(auto* Var=IConsoleManager::Get().FindConsoleVariable(Name))Var->Set(0,ECVF_SetByCode);
    if(Initialize(World,C))Advance(EDeathStage::Warmup,Now);
    break;
   }
   case EDeathStage::Warmup:
    if(StageFrames<45||Now-StageStarted<2.)return;
    if(!ValidateLive(Scene,C))return;
    if(bLifecycle){
     LossStart=WorldTime;LifecycleBurst(World,C);
     Advance(EDeathStage::Lifecycle,Now);return;
    }
    if(bCapture)RequestCapture(TEXT("live"));
    bSkipInterval=true;
    Advance(EDeathStage::Baseline,Now);
    break;
   case EDeathStage::Baseline:
    Sample(EDeathPhase::Live,Scene,C,-1.,WallMs,bExclude);
    if(bCapture) {
     if(!CaptureWritten())return;
    } else if(StageFrames<30||Now-StageStarted<.65)return;
    Trigger(World,C);
    Advance(EDeathStage::FirstLoss,Now);
    break;
   case EDeathStage::FirstLoss:
    Sample(EDeathPhase::FirstLoss,Scene,C,Age,WallMs,false);
    if(!ValidateFirstLoss(Scene,C,World))return;
    bSkipInterval=true;
    Advance(EDeathStage::Active,Now);
    break;
   case EDeathStage::Active: {
    if(C.DeathAnimations.IsEmpty()) {
     Sample(EDeathPhase::Expiry,Scene,C,Age,WallMs,false);
     if(bVerifyGeometry&&NextGeometryWindow!=int32(UE_ARRAY_COUNT(DeathGeometryWindows))){
      Fail(FString::Printf(TEXT("Expired before geometry window %s was sampled"),DeathGeometryWindows[NextGeometryWindow].Name));return;
     }
     if(!ValidateCleanup(Scene,C))return;
     if(bCapture) {
      if(!CaptureWritten()||!bLossCaptureRequested){Fail(TEXT("The active loss screenshot did not complete before expiry"));return;}
      RequestCapture(TEXT("expired"));
     }
     bSkipInterval=true;
     Advance(EDeathStage::Cleanup,Now);
     return;
    }
    if(C.DeathAnimations.Num()!=ExpectedEffects){Fail(TEXT("The simultaneous effect count changed before expiry"));return;}
    if(bVerifyGeometry&&!VerifyGeometry(Scene,C,Age))return;
    const EDeathPhase Phase=Age<.8?EDeathPhase::Fall:Age<1.5?EDeathPhase::Collapse:Age<2.?EDeathPhase::Settled:EDeathPhase::Sink;
    Sample(Phase,Scene,C,Age,WallMs,bExclude);
    if(Age>=.25&&(!bSawDraw||!bObservedMotion)) {
     ObserveVisibleEffects(Scene,C);
     bSkipInterval=true;
    }
    if(bCapture&&!bLossCaptureRequested&&Age>=.48&&bSawDraw&&bObservedMotion) {
     RequestCapture(TEXT("collapse"));
     bLossCaptureRequested=true;
    }
    break;
   }
   case EDeathStage::Cleanup:
    Sample(EDeathPhase::Clean,Scene,C,Age,WallMs,bExclude);
    if(!bExclude)++CleanFrames;
    if(CleanFrames<30||Now-StageStarted<.5)return;
    Checkpoint(TEXT("clean"),Scene.RenderedCount());
    Advance(EDeathStage::Capture,Now);
    break;
   case EDeathStage::Capture:
    if(!CaptureWritten()) {
     if(Now-StageStarted>4.)Fail(TEXT("The cleanup screenshot did not finish writing"));
     return;
    }
    Finish();
    break;
   case EDeathStage::Lifecycle:
    RunLifecycle(Scene,C,World,CompletedAt);
    break;
   case EDeathStage::Done:break;
  }
 }
};
#endif

void RunRTSDeathStressProbe(ARTSScene3D* Scene,ARTSController* Controller) {
#if !UE_BUILD_SHIPPING
 if(!RTSDeathStressEnabled())return;
 if(Controller->GetNetMode()!=NM_Standalone) {
  static bool bLogged=false;
  if(!bLogged)UE_LOG(LogTemp,Error,TEXT("RTSV09DeathStress is only available in a fresh standalone game."));
  bLogged=true;
  return;
 }
 static FRTSDeathStressProbe Probe;
 Probe.Run(*Scene,*Controller);
#endif
}
