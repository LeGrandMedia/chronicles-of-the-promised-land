#pragma once
#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "RTSTypes.h"

class UCanvas;
class UTexture2D;

// All dimensions are logical pixels at 1600x900 and zoom 1. Pivot and hitbox
// coordinates are normalized within the displayed image, independent of PNG size.
struct FRTSVisualMetadata {
 FVector2D Size=FVector2D(40,48);
 FVector2D Pivot=FVector2D(.5,.95);
 FVector2D ContextHitMin=FVector2D(.02,.02),ContextHitMax=FVector2D(.98,1);
 FVector2D HitMin=FVector2D(.1,.1),HitMax=FVector2D(.9,1);
 FLinearColor Color=FLinearColor::White;
 bool bPlayerTint=false;
};

// Metrics describe the actual deforming mesh, enabling deterministic pose and capture QA.
struct FRTSVisualPoseMetrics {
 FString State,Rig;
 float Elapsed=0,NonRigidDisplacement=0,MaximumOffset=0;
 uint32 PoseHash=0;
 int32 Triangles=0;
};

// Owns runtime-imported PNGs and retains them safely across garbage collection.
// Get() reads Content/Data/visuals.json once. Reload() explicitly refreshes edits.
class CHRONICLES_API FRTSVisualAssets final : public FGCObject {
public:
 static FRTSVisualAssets& Get();
 bool TryDraw(UCanvas* Canvas,const FRTSEntityView& Entity,const FRTSDef& Def,
  FVector2D LogicalScreenPoint,float Zoom,float UIScale,FLinearColor PlayerColor,float Time,float Opacity=1.f,const FString& Faction=FString());
 // Returns false unless this type has at least one successfully imported image.
 // Output is in actual viewport pixels, matching controller mouse coordinates.
 bool GetHitBounds(const FString& TypeId,FVector2D LogicalScreenPoint,float Zoom,float UIScale,FBox2D& OutPixelBounds,bool bContext=false,const FString& Faction=FString()) const;
 // Transparent sprite corners must not hide a nearby unit from a left click.
 bool OpaqueAt(const FString& TypeId,FVector2D Pixel,FVector2D LogicalScreenPoint,float Zoom,float UIScale,const FString& Faction=FString(),int32 EntityId=0) const;
 bool TryDrawIcon(UCanvas* Canvas,const FRTSDef& Def,FVector2D Pixel,float Size,FLinearColor Tint,const FString& Faction=FString());
 bool TryDrawIconId(UCanvas* Canvas,const FString& TypeId,FVector2D Pixel,float Size,FLinearColor Tint,const FString& Faction=FString()) const;
 static float WorldScale(const FRTSDef& Def);
 static FVector2D PoseVertex(const FRTSDef& Def,const FString& State,FVector2D UV,float Elapsed,float Seed=0);
 static FRTSVisualPoseMetrics MeasurePose(const FRTSDef& Def,const FString& State,float Elapsed,float Seed=0);
 bool GetPoseMetrics(int32 EntityId,FRTSVisualPoseMetrics& Out) const;
 static FString WorkState(const FRTSEntityView& Entity,const FRTSDef* Target=nullptr);
 bool GetMetadata(const FString& TypeId,FRTSVisualMetadata& OutMetadata,const FString& Faction=FString()) const;
 bool HasRenderable(const FString& TypeId,const FString& Faction=FString()) const;
 void Reload();
 int32 GetLoadedTextureCount() const;
 int64 GetResidentTextureBytes() const;
 const FString& GetManifestPath() const {return ManifestPath;}
 virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
 virtual FString GetReferencerName() const override {return TEXT("FRTSVisualAssets");}

private:
 struct FFrame {
  FString SourcePath;
  UTexture2D* Texture=nullptr;
  UTexture2D* TeamMask=nullptr;
  FVector4 UV=FVector4(0,0,1,1);
 };
 struct FAnimation {TArray<FFrame> Frames;float FPS=6;bool bLoop=true;};
 struct FVisual {
  FRTSVisualMetadata Meta;
  FFrame Base;
  TMap<FString,FAnimation> Animations;
  bool bRenderable=false;
 };
 struct FMotion {
  FVector2D Position=FVector2D::ZeroVector;
  float LastMove=-100,StateStarted=0,LastTime=-100,AttackStarted=-100,LastAttackEvent=-100,PoseElapsed=0;
  bool bWasAttacking=false;
  FRTSVisualPoseMetrics Metrics;
  FString State;
  bool bSeen=false;
 };
 TMap<FString,FVisual> Visuals;
 TMap<FString,UTexture2D*> Textures;
 struct FAlphaMask {int32 Width=0,Height=0;TArray<uint8> Pixels;};
 TMap<UTexture2D*,FAlphaMask> AlphaMasks;
 TMap<FString,FFrame> Icons;
 TMap<FString,TMap<FString,FFrame>> FactionIcons;
 TMap<FString,TMap<FString,FVisual*>> FactionVisuals;
 TMap<int32,FMotion> Motion;
 FString ManifestPath,ArtDirectory;
 FRTSVisualAssets();
 UTexture2D* LoadTexture(const FString& RelativePath,int32 MaxDimension=512);
 FVisual* FindVisual(const FString& TypeId,const FString& Faction) const;
 const FFrame* FindIcon(const FString& TypeId,const FString& Faction) const;
 const FFrame* ReferenceFrame(const FVisual& Visual) const;
 FVector2D FittedSize(const FFrame& Frame,FVector2D Box) const;
 void CacheIcons();
 const FFrame* ChooseFrame(FVisual& Visual,const FRTSEntityView& Entity,float Time);
};
