#include "RTSRenderStressTests.h"
#include "RTSController.h"
#include "RTSHUD.h"
#include "RTSScene3D.h"
#include "RTSWorld.h"
#include "Components/MeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineGlobals.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "HardwareInfo.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "RenderTimer.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SkeletalRenderPublic.h"
#include "UnrealClient.h"

namespace {
constexpr int32 StressStructureCount=30;
constexpr int32 StressResourceCount=24;
constexpr int32 WallSampleCount=180;
constexpr int32 FirstPickCount=24;
constexpr int32 CachedPickRepeats=10;
constexpr double WarmupSeconds=2.;
constexpr int32 WarmupFrames=45;
constexpr double SnapshotInterval=.1;

enum class ERenderStressStage : uint8 { Waiting, Warmup, Frames, Picking, Capture, Done };

struct FRenderStressPixel {
 FVector2D Point;
 int32 EntityId;
};

struct FRenderStressFrame {
 double GameThreadMs,RenderThreadMs,RHIThreadMs,GpuMs;
 double SceneUpdateMs,PoseMs,MeshSubmitMs,BoundsMs,HitMs,PoseCalls,PosedVertices;
};

TSharedRef<FJsonObject> StressDistribution(TArray<double> Samples) {
 auto Result=MakeShared<FJsonObject>();
 Result->SetNumberField(TEXT("count"),Samples.Num());
 if(Samples.IsEmpty())return Result;
 Samples.Sort();
 const int32 Middle=Samples.Num()/2;
 const double Median=Samples.Num()%2?Samples[Middle]:(Samples[Middle-1]+Samples[Middle])*.5;
 double Total=0;
 for(double Sample:Samples)Total+=Sample;
 Result->SetNumberField(TEXT("median"),Median);
 Result->SetNumberField(TEXT("p95"),Samples[FMath::CeilToInt(Samples.Num()*.95)-1]);
 Result->SetNumberField(TEXT("max"),Samples.Last());
 Result->SetNumberField(TEXT("mean"),Total/Samples.Num());
 return Result;
}

double ReadStressCVar(const TCHAR* Name) {
 const IConsoleVariable* CVar=IConsoleManager::Get().FindConsoleVariable(Name);
 return CVar?CVar->GetFloat():-1.;
}

void AddStressSeries(FJsonObject& Output,const TCHAR* Name,const TArray<FRenderStressFrame>& Frames,
 double FRenderStressFrame::* Field,bool bEngineCounter=false) {
 TArray<double> Values;
 TArray<TSharedPtr<FJsonValue>> Raw;
 Values.Reserve(Frames.Num());
 Raw.Reserve(Frames.Num());
 for(const FRenderStressFrame& Frame:Frames) {
  const double Value=Frame.*Field;
  Raw.Add(MakeShared<FJsonValueNumber>(Value));
  if(!bEngineCounter||Value>0)Values.Add(Value);
 }
 const auto Series=StressDistribution(MoveTemp(Values));
 Series->SetArrayField(TEXT("samples"),Raw);
 Output.SetObjectField(Name,Series);
}
}

class FRTSRenderStressProbe {
 ERenderStressStage Stage=ERenderStressStage::Waiting;
 TArray<TSharedPtr<FJsonValue>> Cases;
 TArray<FVector2D> UnitPositions;
 TArray<double> FrameTimes,FirstPicks;
 TArray<FRenderStressFrame> FrameDiagnostics;
 FString FactionId,ScreenshotRelative;
 TSharedPtr<FJsonObject> CurrentResult;
 int32 CaseIndex=0,MobileCount=200,SnapshotIndex=0,WarmedFrames=0,SuccessfulFirstPicks=0,FirstPickAttempts=0;
 double Started=FPlatformTime::Seconds(),StageStarted=Started,PreviousFrame=0.,FixtureStarted=0.,WarmupDuration=0.;
 int32 ViewportWidth=0,ViewportHeight=0;
 int32 RequestedCase=INDEX_NONE;
 bool bSingleCase=false;
 const bool bGpuRequested=!FParse::Param(FCommandLine::Get(),TEXT("RTSProceduralMeshes"));
 bool bFailed=false;
 FString Failure;
 const FString ScreenshotDirectory=TEXT("Screenshots/RenderStress-")+FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));

 void Finish() {
  Stage=ERenderStressStage::Done;
  auto Report=MakeShared<FJsonObject>();
  Report->SetNumberField(TEXT("schema_version"),3);
  Report->SetBoolField(TEXT("passed"),!bFailed);
  Report->SetStringField(TEXT("scope"),TEXT("Rendered client presentation from deterministic synthetic snapshots. Authoritative simulation, pathfinding, bots, network and population limits are outside this probe."));
  Report->SetStringField(TEXT("failure"),Failure);
  Report->SetArrayField(TEXT("cases"),Cases);
  Report->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Started);
  Report->SetNumberField(TEXT("requested_case_index"),RequestedCase);
  Report->SetBoolField(TEXT("gpu_skinning_requested"),bGpuRequested);
  Report->SetStringField(TEXT("engine_timing_clock"),TEXT("Latest available UE stat unit 32-bit cycle counters at each scene tick. Pipeline stages may refer to different frames and are not additive. Zero engine readings are retained in raw samples and excluded from distributions."));
  Report->SetStringField(TEXT("scene_timing_clock"),TEXT("Previous completed game-thread scene and HUD frame. Scene update includes pose time, pose includes mesh submission, and hit includes its bounds work. These scopes are not additive."));
  Report->SetStringField(TEXT("vertex_counter_scope"),TEXT("Posed vertices in the scene diagnostic count CPU geometry work; viewport authored and buffer vertices describe displayed mesh data for both backends."));
  Report->SetStringField(TEXT("screenshot_directory"),ScreenshotDirectory);

  auto Config=MakeShared<FJsonObject>();
  Config->SetStringField(TEXT("platform"),FPlatformProperties::PlatformName());
  Config->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
  Config->SetStringField(TEXT("cpu"),FPlatformMisc::GetCPUBrand());
  Config->SetStringField(TEXT("gpu"),FPlatformMisc::GetPrimaryGPUBrand());
  Config->SetStringField(TEXT("rhi"),FHardwareInfo::GetHardwareInfo(NAME_RHI));
  Config->SetNumberField(TEXT("ram_gib"),double(FPlatformMemory::GetConstants().TotalPhysical)/(1024.*1024.*1024.));
  Config->SetNumberField(TEXT("width"),ViewportWidth);
  Config->SetNumberField(TEXT("height"),ViewportHeight);
  Config->SetNumberField(TEXT("vsync"),ReadStressCVar(TEXT("r.VSync")));
  Config->SetNumberField(TEXT("max_fps"),ReadStressCVar(TEXT("t.MaxFPS")));
  Config->SetNumberField(TEXT("screen_percentage"),ReadStressCVar(TEXT("r.ScreenPercentage")));
  Config->SetNumberField(TEXT("shadow_quality"),ReadStressCVar(TEXT("sg.ShadowQuality")));
  Config->SetBoolField(TEXT("render_offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
  Config->SetBoolField(TEXT("frame_smoothing_disabled"),GEngine&&GEngine->bForceDisableFrameRateSmoothing);
  Config->SetNumberField(TEXT("snapshot_hz"),1./SnapshotInterval);
  Report->SetObjectField(TEXT("config"),Config);

  const FString Directory=FPaths::ProjectSavedDir()/TEXT("Tests");
  IFileManager::Get().MakeDirectory(*Directory,true);
  FString Json;
  FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  const bool bSaved=FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("v09-render-stress.json")));
  const bool bPassed=!bFailed&&bSaved;
  UE_LOG(LogTemp,Display,TEXT("RTS_RENDER_STRESS passed=%d cases=%d detail=%s"),bPassed,Cases.Num(),*Failure);
  FPlatformMisc::RequestExitWithStatus(!bPassed,bPassed?0:1);
 }

 void Fail(const FString& Reason) {
  bFailed=true;
  Failure=Reason;
  Finish();
 }

 void Advance(ERenderStressStage Next,double Now) {
  Stage=Next;
  StageStarted=Now;
 }

 void DisableFrameCaps() {
  GEngine->bForceDisableFrameRateSmoothing=true;
  GEngine->bUseFixedFrameRate=false;
  for(const TCHAR* Name:{TEXT("r.VSync"),TEXT("t.MaxFPS")}) {
   if(IConsoleVariable* CVar=IConsoleManager::Get().FindConsoleVariable(Name))CVar->Set(0,ECVF_SetByCode);
  }
 }

 bool Fixture(ARTSController& C,double Now) {
  const auto& Catalog=FRTSCatalog::Get();
  const bool bExtreme=CaseIndex>=Catalog.Factions.Num();
  const FRTSFaction& Faction=bExtreme?Catalog.Faction(TEXT("nephites")):Catalog.Factions[CaseIndex];
  FactionId=Faction.Id;
  MobileCount=bExtreme?800:200;

  TArray<int32> UnitDefs,BuildingDefs,ResourceDefs;
  for(const FString& Id:Faction.Units) {
   const int32 DefIndex=Catalog.Index(Id);
   if(DefIndex<0)continue;
   const FRTSDef& Def=Catalog.Def(DefIndex);
   if(!Def.bBuilding&&!Def.bUpgrade&&Def.Speed>0&&Def.Kind!=TEXT("ship"))UnitDefs.Add(DefIndex);
  }
  for(const FString& Id:Faction.Buildings) {
   const int32 DefIndex=Catalog.Index(Id);
   if(DefIndex>=0&&Catalog.Def(DefIndex).bBuilding)BuildingDefs.Add(DefIndex);
  }
  for(const TCHAR* Id:{TEXT("tree"),TEXT("ore"),TEXT("berries"),TEXT("wild_game")}) {
   const int32 DefIndex=Catalog.Index(Id);
   if(DefIndex>=0)ResourceDefs.Add(DefIndex);
  }
  if(UnitDefs.IsEmpty()||BuildingDefs.IsEmpty()||ResourceDefs.IsEmpty()||C.Players.IsEmpty())return false;

  C.Entities.Reset(MobileCount+StressStructureCount+StressResourceCount);
  C.DisplayPositions.Reset();
  C.DeathAnimations.Reset();
  C.Selection.Reset();
  C.ActiveSubgroup.Reset();
  C.PendingAction.Reset();
  C.PendingType.Reset();
  C.bMenu=false;
  C.bHelp=false;
  C.bPauseMenu=false;
  C.bBuildMenu=false;
  C.bReducedMotion=false;
  C.bSoundEnabled=false;
  C.Phase=1;
  C.Map=0;
  C.MatchTime=0;
  C.CameraCenter=FVector2D(80,80);
  C.CameraRotation=0;
  C.Zoom=1.65f;
  C.Fog.Init(2,RTS::FogSize*RTS::FogSize);
  C.LocalSlot=C.Players[0].Slot;
  C.Players[0].Age=4;
  C.Players[0].Faction=FactionId;
  C.Players[0].Population=MobileCount;
  C.Players[0].Capacity=MobileCount;
  C.Players[0].Favor=50;

  const auto Add=[&](int32 DefIndex,double X,double Y,int32 EntityOwner) {
   const FRTSDef& Def=Catalog.Def(DefIndex);
   FRTSEntityView Entity;
   Entity.Id=10000+CaseIndex*2000+C.Entities.Num()+1;
   Entity.Owner=EntityOwner;
   Entity.Def=DefIndex;
   Entity.Pos=C.ScreenToWorld(FVector2D(C.ViewSize.X*X,C.ViewSize.Y*Y));
   Entity.HP=Entity.MaxHP=Def.HP;
   Entity.Status=TEXT("idle");
   Entity.Heading=float((C.Entities.Num()%8)*PI/4);
   C.DisplayPositions.Add(Entity.Id,Entity.Pos);
   C.Entities.Add(MoveTemp(Entity));
  };

  UnitPositions.Reset(MobileCount);
  const int32 UnitColumns=bExtreme?40:20;
  const int32 UnitRows=MobileCount/UnitColumns;
  for(int32 I=0;I<MobileCount;++I) {
   const double X=.12+.76*(I%UnitColumns+.5)/UnitColumns;
   const double Y=.29+.39*(I/UnitColumns+.5)/UnitRows;
   Add(UnitDefs[I%UnitDefs.Num()],X,Y,C.LocalSlot);
   UnitPositions.Add(C.Entities.Last().Pos);
  }
  for(int32 I=0;I<StressStructureCount;++I) {
   const double X=.13+.74*(I%15+.5)/15.;
   const double Y=I<15?.16:.235;
   Add(BuildingDefs[I%BuildingDefs.Num()],X,Y,C.LocalSlot);
  }
  for(int32 I=0;I<StressResourceCount;++I) {
   const int32 Column=I/2;
   const double X=(I%2==0?.025:.925)+.012*(Column%5);
   const double Y=.28+.38*(Column+.5)/12.;
   Add(ResourceDefs[I%ResourceDefs.Num()],X,Y,-1);
  }
  C.Selection.Add(C.Entities[MobileCount].Id);
  FixtureStarted=Now;
  SnapshotIndex=0;
  WarmedFrames=0;
  SuccessfulFirstPicks=0;
  FirstPickAttempts=0;
  PreviousFrame=0.;
  FrameTimes.Reset(WallSampleCount);
  FrameDiagnostics.Reset(WallSampleCount);
  FirstPicks.Reset(FirstPickCount);
  Advance(ERenderStressStage::Warmup,Now);
  return true;
 }

 void Animate(ARTSController& C,double Now) {
  const int32 NextSnapshot=FMath::FloorToInt((Now-FixtureStarted)/SnapshotInterval);
  if(NextSnapshot==SnapshotIndex)return;
  SnapshotIndex=NextSnapshot;
  C.MatchTime=float(SnapshotIndex*SnapshotInterval);
  for(int32 I=0;I<MobileCount;++I) {
   const double Phase=SnapshotIndex*.12+(I%11)*.28;
   FRTSEntityView& Unit=C.Entities[I];
   Unit.Pos=UnitPositions[I]+FVector2D(FMath::Cos(Phase),FMath::Sin(Phase))*1.3;
   Unit.Heading=float(Phase+PI*.5);
   Unit.Status=TEXT("move");
  }
 }

 void CaptureDiagnostics(const ARTSScene3D& Scene) {
  const FRTSSceneProfile& Profile=Scene.FrameProfile();
  FrameDiagnostics.Add({
   FPlatformTime::ToMilliseconds(GGameThreadTime),
   FPlatformTime::ToMilliseconds(GRenderThreadTime),
   FPlatformTime::ToMilliseconds(GRHIThreadTime),
   FPlatformTime::ToMilliseconds(GGPUFrameTime),
   Profile.UpdateMs,Profile.PoseMs,Profile.MeshSubmitMs,Profile.BoundsMs,Profile.HitMs,
   double(Profile.PoseCalls),double(Profile.PosedVertices)
  });
 }

 int32 TimePick(const ARTSController& C,FVector2D Pixel,TArray<double>& Samples,bool bContext=false) const {
  const double Before=FPlatformTime::Seconds();
  const int32 Hit=C.EntityUnder(Pixel,bContext);
  Samples.Add((FPlatformTime::Seconds()-Before)*1000.);
  return Hit;
 }

 bool CanPick(const ARTSController& C,const ARTSHUD& HUD,FVector2D Pixel) const {
  return Pixel.X>=0&&Pixel.X<C.ViewSize.X&&!HUD.BlocksWorld(Pixel);
 }

 void FirstPick(ARTSScene3D& Scene,const ARTSController& C,const ARTSHUD& HUD) {
  // Some rapidly posed mesh centers land behind the HUD or outside the valid
  // gameplay pixels. Advance independently of how many samples were accepted.
  const int32 Index=(FirstPickAttempts++*53+9)%MobileCount;
  const FBox2D Bounds=Scene.Bounds(C.Entities[Index]);
  if(!Bounds.bIsValid)return;
  const FVector2D Pixel=Bounds.GetCenter().RoundToVector();
  if(!CanPick(C,HUD,Pixel))return;
  SuccessfulFirstPicks+=TimePick(C,Pixel,FirstPicks)!=0?1:0;
 }

 void VerifiedPixels(ARTSScene3D& Scene,const ARTSController& C,const ARTSHUD& HUD,TArray<FRenderStressPixel>& Hits,TArray<FRenderStressPixel>& Misses) const {
  const double Offsets[]={.5,.35,.65,.2,.8};
  for(int32 I=0;I<C.Entities.Num()&&Hits.Num()<24;++I) {
   const int32 Index=(I*47+11)%C.Entities.Num();
   const FRTSEntityView& Entity=C.Entities[Index];
   const FBox2D Bounds=Scene.Bounds(Entity);
   if(!Bounds.bIsValid)continue;
   bool bFound=false;
   for(double Y:Offsets) {
    for(double X:Offsets) {
     const FVector2D Pixel=(Bounds.Min+Bounds.GetSize()*FVector2D(X,Y)).RoundToVector();
     if(CanPick(C,HUD,Pixel)&&C.EntityUnder(Pixel)==Entity.Id) {
      Hits.Add({Pixel,Entity.Id});bFound=true;break;
     }
    }
    if(bFound)break;
   }
  }
  for(int32 Y=0;Y<20&&Misses.Num()<16;++Y) {
   for(int32 X=0;X<24&&Misses.Num()<16;++X) {
    const FVector2D Pixel=(C.ViewSize*FVector2D(.01+.042*X,.085+.032*Y)).RoundToVector();
    if(CanPick(C,HUD,Pixel)&&C.EntityUnder(Pixel)==0)Misses.Add({Pixel,0});
   }
  }
 }

 void MeasureScene(ARTSScene3D& Scene,const ARTSController& C,const ARTSHUD& HUD,double Now) {
  TArray<FRenderStressPixel> Hits,Misses;
  VerifiedPixels(Scene,C,HUD,Hits,Misses);
  TArray<double> HitTimes,MissTimes,ContextTimes;
  HitTimes.Reserve(Hits.Num()*CachedPickRepeats);
  MissTimes.Reserve(Misses.Num()*CachedPickRepeats);
  ContextTimes.Reserve(Hits.Num()*CachedPickRepeats);
  int32 PickMismatches=0,ContextHits=0;
  for(int32 Repeat=0;Repeat<CachedPickRepeats;++Repeat) {
   for(const FRenderStressPixel& Target:Hits) {
    PickMismatches+=TimePick(C,Target.Point,HitTimes)!=Target.EntityId?1:0;
    ContextHits+=TimePick(C,Target.Point,ContextTimes,true)!=0?1:0;
   }
   for(const FRenderStressPixel& Terrain:Misses)PickMismatches+=TimePick(C,Terrain.Point,MissTimes)!=0?1:0;
  }

  int32 VisibleMobile=0,VisibleStructures=0,VisibleResources=0,ExpectedGpu=0,ActiveGpu=0,InvalidSkin=0;
  int64 ViewportTriangles=0,AuthoredVertices=0,BufferVertices=0,GpuBufferVertices=0;
  const FBox2D Viewport(FVector2D::ZeroVector,C.ViewSize);
  for(int32 I=0;I<C.Entities.Num();++I) {
   const FRTSEntityView& Entity=C.Entities[I];
   const FRTSRendered3D* Rendered=Scene.Rendered.Find(Entity.Id);
   const UMeshComponent* Component=Rendered?Rendered->Component():nullptr;
   if(!Rendered||!Rendered->Model||!Component||!Component->IsVisible()||!Scene.Bounds(Entity).Intersect(Viewport))continue;
   const FRTSModel3D& Model=*Rendered->Model;
   ViewportTriangles+=Model.Triangles;
   AuthoredVertices+=Model.VertexCount;
   if(bGpuRequested&&!Model.AnimationStyle.IsEmpty())++ExpectedGpu;
   if(const UPoseableMeshComponent* Skin=Rendered->Skin.Get()) {
    const FSkeletalMeshRenderData* RenderData=Skin->GetSkeletalMeshRenderData();
    if(!Skin->IsRegistered()||!Skin->IsRenderStateCreated()||Skin->bRenderStatic||!Skin->MeshObject||
       Skin->MeshObject->IsCPUSkinned()||!RenderData||!RenderData->LODRenderData.IsValidIndex(0)) {
     ++InvalidSkin;
    } else {
     ++ActiveGpu;
     const int64 SkinVertices=RenderData->LODRenderData[0].GetNumVertices();
     BufferVertices+=SkinVertices;
     GpuBufferVertices+=SkinVertices;
    }
   } else BufferVertices+=Model.VertexCount;
   if(I<MobileCount)++VisibleMobile;
   else if(I<MobileCount+StressStructureCount)++VisibleStructures;
   else ++VisibleResources;
  }
  const bool bBackendValid=InvalidSkin==0&&(bGpuRequested?ExpectedGpu>=MobileCount&&ActiveGpu==ExpectedGpu&&GpuBufferVertices>0:ActiveGpu==0);
  const bool bValid=bBackendValid&&Hits.Num()>=12&&Misses.Num()>=8&&PickMismatches==0&&ContextHits>0&&
   VisibleMobile==MobileCount&&VisibleStructures==StressStructureCount&&VisibleResources==StressResourceCount&&ViewportTriangles>0;
  bFailed|=!bValid;
  if(!bValid)Failure=FString::Printf(TEXT("Fixture %s failed visibility, physical-pixel picking or skinning validation. Visible %d/%d/%d, expected GPU %d, actual GPU %d, invalid GPU %d."),
   *FactionId,VisibleMobile,VisibleStructures,VisibleResources,ExpectedGpu,ActiveGpu,InvalidSkin);
  CurrentResult=MakeShared<FJsonObject>();
  CurrentResult->SetBoolField(TEXT("measurements_valid"),bValid);
  CurrentResult->SetNumberField(TEXT("case_index"),CaseIndex);
  CurrentResult->SetStringField(TEXT("faction"),FactionId);
  CurrentResult->SetNumberField(TEXT("mobile_units"),MobileCount);
  CurrentResult->SetNumberField(TEXT("structures"),StressStructureCount);
  CurrentResult->SetNumberField(TEXT("resources"),StressResourceCount);
  CurrentResult->SetNumberField(TEXT("warmup_frames"),WarmedFrames);
  CurrentResult->SetNumberField(TEXT("warmup_seconds"),WarmupDuration);
  CurrentResult->SetObjectField(TEXT("wall_frame_ms"),StressDistribution(FrameTimes));
  TArray<TSharedPtr<FJsonValue>> RawFrames;
  RawFrames.Reserve(FrameTimes.Num());
  for(double Milliseconds:FrameTimes)RawFrames.Add(MakeShared<FJsonValueNumber>(Milliseconds));
  CurrentResult->SetArrayField(TEXT("wall_frame_samples_ms"),RawFrames);
  auto Diagnostics=MakeShared<FJsonObject>();
  AddStressSeries(*Diagnostics,TEXT("game_thread_ms"),FrameDiagnostics,&FRenderStressFrame::GameThreadMs,true);
  AddStressSeries(*Diagnostics,TEXT("render_thread_ms"),FrameDiagnostics,&FRenderStressFrame::RenderThreadMs,true);
  AddStressSeries(*Diagnostics,TEXT("rhi_thread_ms"),FrameDiagnostics,&FRenderStressFrame::RHIThreadMs,true);
  AddStressSeries(*Diagnostics,TEXT("gpu_ms"),FrameDiagnostics,&FRenderStressFrame::GpuMs,true);
  AddStressSeries(*Diagnostics,TEXT("scene_update_ms"),FrameDiagnostics,&FRenderStressFrame::SceneUpdateMs);
  AddStressSeries(*Diagnostics,TEXT("pose_ms"),FrameDiagnostics,&FRenderStressFrame::PoseMs);
  AddStressSeries(*Diagnostics,TEXT("mesh_submit_ms"),FrameDiagnostics,&FRenderStressFrame::MeshSubmitMs);
  AddStressSeries(*Diagnostics,TEXT("bounds_ms"),FrameDiagnostics,&FRenderStressFrame::BoundsMs);
  AddStressSeries(*Diagnostics,TEXT("hit_ms"),FrameDiagnostics,&FRenderStressFrame::HitMs);
  AddStressSeries(*Diagnostics,TEXT("pose_calls"),FrameDiagnostics,&FRenderStressFrame::PoseCalls);
  AddStressSeries(*Diagnostics,TEXT("posed_vertices"),FrameDiagnostics,&FRenderStressFrame::PosedVertices);
  CurrentResult->SetObjectField(TEXT("diagnostics"),Diagnostics);
  CurrentResult->SetNumberField(TEXT("visible_mobile_units"),VisibleMobile);
  CurrentResult->SetNumberField(TEXT("visible_structures"),VisibleStructures);
  CurrentResult->SetNumberField(TEXT("visible_resources"),VisibleResources);
  CurrentResult->SetNumberField(TEXT("viewport_entity_triangles"),double(ViewportTriangles));
  CurrentResult->SetNumberField(TEXT("viewport_authored_vertices"),double(AuthoredVertices));
  CurrentResult->SetNumberField(TEXT("viewport_mesh_buffer_vertices"),double(BufferVertices));
  CurrentResult->SetNumberField(TEXT("viewport_gpu_mesh_buffer_vertices"),double(GpuBufferVertices));
  CurrentResult->SetNumberField(TEXT("expected_gpu_components"),ExpectedGpu);
  CurrentResult->SetNumberField(TEXT("actual_gpu_components"),ActiveGpu);
  CurrentResult->SetNumberField(TEXT("invalid_gpu_components"),InvalidSkin);
  CurrentResult->SetNumberField(TEXT("scene_visible_entity_triangles"),Scene.RenderedTriangles());
  CurrentResult->SetNumberField(TEXT("ground_triangles"),Scene.GroundTriangles.Num()/3);
  CurrentResult->SetNumberField(TEXT("first_pick_hits"),SuccessfulFirstPicks);
  CurrentResult->SetNumberField(TEXT("first_pick_attempts"),FirstPickAttempts);
  CurrentResult->SetObjectField(TEXT("first_pick_per_frame_ms"),StressDistribution(FirstPicks));
  CurrentResult->SetObjectField(TEXT("verified_integer_entity_pick_ms"),StressDistribution(MoveTemp(HitTimes)));
  CurrentResult->SetObjectField(TEXT("verified_integer_terrain_miss_ms"),StressDistribution(MoveTemp(MissTimes)));
  CurrentResult->SetObjectField(TEXT("integer_context_pick_ms"),StressDistribution(MoveTemp(ContextTimes)));
  CurrentResult->SetNumberField(TEXT("context_entity_hits"),ContextHits);
  CurrentResult->SetNumberField(TEXT("pick_mismatches"),PickMismatches);
  const FPlatformMemoryStats Memory=FPlatformMemory::GetStats();
  CurrentResult->SetNumberField(TEXT("process_resident_mib"),double(Memory.UsedPhysical)/(1024.*1024.));

  ScreenshotRelative=ScreenshotDirectory/FString::Printf(TEXT("%s-%d.png"),*FactionId,MobileCount);
  CurrentResult->SetStringField(TEXT("screenshot"),ScreenshotRelative);
  const FString Absolute=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/ScreenshotRelative);
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Absolute),true);
  FScreenshotRequest::RequestScreenshot(Absolute,false,false);
  UE_LOG(LogTemp,Display,TEXT("RTS_RENDER_STRESS_CASE faction=%s units=%d viewport_triangles=%lld sampled_frames=%d gpu_components=%d/%d gpu_buffer_vertices=%lld valid=%d"),
   *FactionId,MobileCount,ViewportTriangles,FrameTimes.Num(),ActiveGpu,ExpectedGpu,GpuBufferVertices,bValid);
  Advance(ERenderStressStage::Capture,Now);
 }

public:
 void Run(ARTSScene3D& Scene,ARTSController& C) {
  if(Stage==ERenderStressStage::Done)return;
  const double Now=FPlatformTime::Seconds();
  if(Now-Started>360.) {
   Fail(FString::Printf(TEXT("Timed out in stage %d, faction %s, measured frames %d, first picks %d."),int32(Stage),*FactionId,FrameTimes.Num(),FirstPicks.Num()));
   return;
  }
  const ARTSHUD* HUD=Cast<ARTSHUD>(C.GetHUD());
  if(!C.Me()||!HUD)return;
  // The uncapped packaged viewport can receive incidental mouse edge scrolling.
  // Hold the fixture viewpoint steady without changing any game setting.
  C.CameraCenter=FVector2D(80,80);
  C.CameraRotation=0;
  C.Zoom=1.65f;

  switch(Stage) {
   case ERenderStressStage::Waiting: {
    const int32 Factions=FRTSCatalog::Get().Factions.Num();
    if(Factions!=6){Fail(TEXT("The performance probe requires all six catalog factions."));return;}
    const UGameViewportClient* Client=C.GetWorld()->GetGameViewport();
    if(!Client||!Client->Viewport)return;
    const FIntPoint Size=Client->Viewport->GetSizeXY();
    if(Size.X<=0||Size.Y<=0)return;
    ViewportWidth=Size.X;ViewportHeight=Size.Y;
    C.ViewSize=FVector2D(Size);
    bSingleCase=FParse::Value(FCommandLine::Get(),TEXT("RTSRenderStressCase="),RequestedCase);
    if(bSingleCase) {
     if(RequestedCase<0||RequestedCase>Factions){Fail(TEXT("RTSRenderStressCase must be an index from 0 to 6."));return;}
     CaseIndex=RequestedCase;
    }
    for(TActorIterator<ARTSWorld> It(C.GetWorld());It;++It)It->SetActorTickEnabled(false);
    DisableFrameCaps();
    if(!Fixture(C,Now))Fail(TEXT("Could not load the first catalog fixture."));
    break;
   }
   case ERenderStressStage::Warmup:
    Animate(C,Now);
    if(++WarmedFrames>=WarmupFrames&&Now-StageStarted>=WarmupSeconds) {
     WarmupDuration=Now-StageStarted;
     PreviousFrame=Now;
     Advance(ERenderStressStage::Frames,Now);
    }
    break;
   case ERenderStressStage::Frames:
    FrameTimes.Add((Now-PreviousFrame)*1000.);
    CaptureDiagnostics(Scene);
    PreviousFrame=Now;
    Animate(C,Now);
    if(FrameTimes.Num()==WallSampleCount)Advance(ERenderStressStage::Picking,Now);
    break;
   case ERenderStressStage::Picking:
    Animate(C,Now);
    FirstPick(Scene,C,*HUD);
    if(FirstPicks.Num()==FirstPickCount)MeasureScene(Scene,C,*HUD,Now);
    else if(FirstPickAttempts>=FirstPickCount*8||Now-StageStarted>15.)Fail(FString::Printf(TEXT("Only %d/%d first picks accepted after %d candidates, faction=%s, phase=%d, menu=%d, entities=%d, viewport=%s."),
     FirstPicks.Num(),FirstPickCount,FirstPickAttempts,*FactionId,C.Phase,C.bMenu,C.Entities.Num(),*C.ViewSize.ToString()));
    break;
   case ERenderStressStage::Capture: {
    const bool bCaptured=IFileManager::Get().FileSize(*(FPaths::ProjectSavedDir()/ScreenshotRelative))>0;
    if(!bCaptured&&Now-StageStarted<4.)return;
    CurrentResult->SetBoolField(TEXT("screenshot_saved"),bCaptured);
    Cases.Add(MakeShared<FJsonValueObject>(CurrentResult));
    if(!bCaptured){Fail(TEXT("Rendered screenshot was unavailable."));return;}
    if(bSingleCase||++CaseIndex>FRTSCatalog::Get().Factions.Num()){Finish();return;}
    if(!Fixture(C,Now))Fail(TEXT("Could not load the next catalog fixture."));
    break;
   }
   case ERenderStressStage::Done: break;
  }
 }
};

void RunRTSRenderStressProbe(ARTSScene3D* Scene,ARTSController* Controller) {
 static const bool bEnabled=FParse::Param(FCommandLine::Get(),TEXT("RTSV09RenderStress"));
 if(!bEnabled)return;
 static FRTSRenderStressProbe Probe;
 Probe.Run(*Scene,*Controller);
}
