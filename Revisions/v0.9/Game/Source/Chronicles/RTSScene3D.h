#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTSTypes.h"
#include "RTSScene3D.generated.h"

class ARTSController;
class UCameraComponent;
class UProceduralMeshComponent;
class UMeshComponent;
class UPoseableMeshComponent;
class USkeletalMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FRTSVisualPoseMetrics;
bool RTSBatchUnitDeathsEnabled();
struct FRTSModelPart {
 FString Name,Bone;
 TArray<FVector> Vertices,Normals;
 TArray<int32> Triangles;
 FBox Bounds=FBox(EForceInit::ForceInit);
 FLinearColor Color=FLinearColor::White;
 bool Team=false;
};
struct FRTSModelRig {FVector Pivot=FVector::ZeroVector;FString Parent;};
struct FRTSModelSocket {FVector Position=FVector::ZeroVector;FString Bone;};
struct FRTSModel3D {
 FString Id,AnimationStyle,Equipment;
 TArray<FRTSModelPart> Parts;
 TMap<FString,FRTSModelRig> Rig;
 TMap<FString,FRTSModelSocket> Sockets;
 FBox Bounds=FBox(EForceInit::ForceInit);
 int32 Triangles=0,VertexCount=0;
 TWeakObjectPtr<USkeletalMesh> SkeletalMesh;
 TArray<FString> BoneNames;
 TArray<int32> BoneParents;
 TArray<FTransform> ReferenceBones;
};
struct FRTSRendered3D {
 TWeakObjectPtr<UProceduralMeshComponent> Mesh;
 TWeakObjectPtr<UPoseableMeshComponent> Skin;
 UMeshComponent* Component() const;
 bool IsBatchedDeath() const {return BatchFirstVertex!=INDEX_NONE;}
 // Offsets into the shared component's section 0 vertex and flat index buffers.
 int32 BatchFirstVertex=INDEX_NONE,BatchFirstIndex=INDEX_NONE;
 float BatchHeading=0;
 TSharedPtr<FRTSModel3D> Model;
 FString Key;
 mutable TArray<FVector> Vertices;
 TArray<FVector> Normals;
 mutable TArray<int32> Triangles;
 TArray<FTransform> PartTransforms,BoneTransforms;
 TArray<FVector> BoundsPoints;
 FVector StringOffset=FVector::ZeroVector,StringLow=FVector::ZeroVector,StringAxis=FVector::ZeroVector;
 float Bob=0;
 mutable uint32 GeometryRevision=MAX_uint32;
 TArray<FColor> Colors;
 float LastAnimation=-100;
 float NextAnimation=-100;
 float DeathFall=-1;
 FVector LastPosition=FVector::ZeroVector;
 FVector2D PreviousPosition=FVector2D::ZeroVector;
 float MoveWeight=0,WalkPhase=0,TravelDistance=0;
 uint32 PoseRevision=0;
 FString PoseState;
 float StateStarted=0,PoseElapsed=0;
 float LastAttackEvent=-100;
 float AttackStarted=-100;
 bool WasAttacking=false;
 TMap<FString,FVector> PosedSockets;
 mutable uint32 BoundsRevision=MAX_uint32;
 mutable float BoundsAngle=MAX_flt;
 mutable FBox2D ProjectedOffsets=FBox2D(EForceInit::ForceInit);
 bool Remembered=false;
 bool Stealthed=false;
 int32 Owner=-2;
 float Progress=-1;
};

struct FRTSProjectile3D {
 FVector Start=FVector::ZeroVector,End=FVector::ZeroVector;
 float Started=0,Duration=.4f;
 int32 Source=0,Target=0;
 bool Stone=false;
};
struct FRTSSceneProfile {
 double UpdateMs=0,PoseMs=0,MeshSubmitMs=0,BoundsMs=0,HitMs=0;
 int32 PoseCalls=0,PosedVertices=0;
 double DeathCreateMs=0,DeathFirstSubmitMs=0,LiveRetireMs=0,DeathRetireMs=0;
 int32 DeathCreates=0,DeathFirstSubmits=0,LiveRetires=0,DeathRetires=0;
 int32 DeathPoseCalls=0,DeathPosedVertices=0;
 int32 DeathBatchCreates=0,DeathBatchRebuilds=0,DeathBatchUpdates=0,DeathBatchUploadedVertices=0;
 int32 DeathBatchPoseSolves=0,DeathBatchSolvedVertices=0,DeathBatchRetires=0;
};

struct FRTSDeathBatchKey3D {
 FIntPoint Tile;
 float Started;
 int32 Chunk;
 bool operator==(const FRTSDeathBatchKey3D& Other) const {
  return Tile==Other.Tile&&Started==Other.Started&&Chunk==Other.Chunk;
 }
 friend uint32 GetTypeHash(const FRTSDeathBatchKey3D& Key) {
  return HashCombine(HashCombine(GetTypeHash(Key.Tile),GetTypeHash(Key.Started)),GetTypeHash(Key.Chunk));
 }
};

struct FRTSDeathBatch3D {
 TWeakObjectPtr<UProceduralMeshComponent> Mesh;
 TArray<int32> Members;
 float Fall=-1;
};

struct FRTSWallSnapshot3D {
 int32 Id=0,Owner=-1,Def=0;
 FVector2D Pos=FVector2D::ZeroVector;
 float Heading=0,Progress=1;
 bool Remembered=false,Gate=false;
 FString ModelKey;
 bool operator==(const FRTSWallSnapshot3D& Other) const {
  return Id==Other.Id&&Owner==Other.Owner&&Def==Other.Def&&Pos==Other.Pos&&Heading==Other.Heading&&
   Progress==Other.Progress&&Remembered==Other.Remembered&&Gate==Other.Gate&&ModelKey==Other.ModelKey;
 }
};
struct FRTSWallJoint3D {
 int32 EntityId=0,NeighborId=0,FirstTriangle=0,NumTriangles=0;
 bool Remembered=false;
 FBox Bounds=FBox(EForceInit::ForceInit);
};

// Client-only presentation. Receives filtered snapshots; never reads server entities.
UCLASS()
class CHRONICLES_API ARTSScene3D : public AActor {
 GENERATED_BODY()
public:
 ARTSScene3D();
 virtual void BeginPlay() override;
 void Update(ARTSController* Controller,float Delta);
 FBox2D Bounds(const FRTSEntityView& E,bool Context=false) const;
 int32 Hit(FVector2D Pixel,bool Context=false) const;
 int32 SelectionHit(FVector2D Pixel,double TouchPadding=0) const;
 bool IntersectsSelection(const FRTSEntityView& E,const FBox2D& Box) const;
 FVector2D Project(FVector WorldMeters) const;
 bool Ready() const {return bReady;}
 int32 ModelCount() const {return ModelPaths.Num();}
 int32 RenderedCount() const {return Rendered.Num();}
 int32 PhysicalDeathComponentCount() const;
 int32 RenderedTriangles() const;
 bool GetPoseMetrics(int32 EntityId,FRTSVisualPoseMetrics& Out) const;
 const FRTSSceneProfile& FrameProfile() const {return Profile;}
 UPROPERTY() UCameraComponent* Camera=nullptr;
 UPROPERTY() UProceduralMeshComponent* Ground=nullptr;
 UPROPERTY() UProceduralMeshComponent* Projectiles=nullptr;
 UPROPERTY() UProceduralMeshComponent* WallJoints=nullptr;
 UPROPERTY() UMaterialInterface* Surface=nullptr;
 UPROPERTY() UMaterialInterface* SkinSurface=nullptr;
 UPROPERTY() TArray<USkeletalMesh*> SkinAssets;
 UPROPERTY() TMap<int32,UMaterialInstanceDynamic*> SkinMaterials;
private:
 friend class FRTSSceneProbe;
 friend class FRTSRenderStressProbe;
 friend class FRTSDeathStressProbe;
 friend class FRTSWallJunctionProbe;
 friend class FRTSDistrictProbe;
 struct FPickCandidate {
  const FRTSEntityView* Entity;
  const FRTSRendered3D* Rendered;
  FBox2D Bounds;
 };
 using FPickCandidates=TArray<FPickCandidate,TInlineAllocator<16>>;
 void GatherPickCandidates(FVector2D Pixel,double Padding,FPickCandidates& Candidates) const;
 int32 TracePick(FVector2D Pixel,const FPickCandidates& Candidates) const;
 int32 PickWithPadding(FVector2D Pixel,double Padding,bool bOwnedUnitsOnly) const;
 bool TerrainBlocksPick(const FVector& Impact,const FVector& Direction) const;
 TWeakObjectPtr<ARTSController> PC;
 TMap<FString,FString> ModelPaths;
 TMap<FString,TSharedPtr<FRTSModel3D>> Models;
 TMap<int32,FRTSRendered3D> Rendered;
 TMap<FRTSDeathBatchKey3D,FRTSDeathBatch3D> UnitDeathBatches;
 TArray<FVector> GroundVertices,GroundNormals;
 TArray<int32> GroundTriangles;
 TArray<FLinearColor> GroundColors;
 TArray<uint8> LastFog;
 TArray<FRTSProjectile3D> Shots;
 TArray<FRTSWallSnapshot3D> WallSnapshots;
 TArray<FRTSWallJoint3D> WallJointEntries;
 TArray<FVector> WallJointVertices;
 TArray<int32> WallJointTriangles;
 TMap<int32,FBox> WallJointBounds;
 TSet<int32> TrimmedWallCorners;
 uint32 WallJointRevision=0;
 int32 GroundMap=-1;
 double GroundTopZ=0.;
 bool bReady=false;
 float LastGroundUpdate=-100;
 float FrameDelta=1.f/60.f;
 mutable FRTSSceneProfile Profile;
 TSharedPtr<FRTSModel3D> Model(const FRTSEntityView& E,FString& Key);
 void Terrain();
 void Pose(FRTSRendered3D& Entry,const FRTSEntityView& E,float Now,bool Rebuild,float DeathAge=-1);
 void EnsureGeometry(const FRTSRendered3D& Entry) const;
 UMaterialInterface* SkinMaterial(const FRTSEntityView& E);
 void Entity(const FRTSEntityView& E,float Now,float DeathAge=-1);
 void UpdateUnitDeathBatches(float Now,TSet<int32>& Live);
 void HideUnitDeathBatches(float Now);
 void RetireUnitDeathBatch(FRTSDeathBatch3D& Batch);
 void SpawnProjectile(const FRTSEntityView& E,const FRTSRendered3D& R,float Now);
 void DrawProjectiles(float Now);
 void UpdateWallJoints();
 int32 HitWallJoints(FVector2D Pixel,const FVector& Origin,const FVector& Direction,double& Closest) const;
 void IncludeWallJointBounds(int32 EntityId,FBox2D& Result) const;
};
