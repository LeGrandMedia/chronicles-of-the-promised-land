#include "RTSScene3D.h"
#include "RTSController.h"
#include "RTSDeathStressTests.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"

bool RTSBatchUnitDeathsEnabled() {
 static const bool bEnabled=FParse::Param(FCommandLine::Get(),TEXT("RTSBatchUnitDeaths"));
 return bEnabled;
}

namespace {
constexpr double DeathBatchTileMeters=20.;
constexpr int32 MaxBodiesPerDeathBatch=24;
constexpr int32 MaxVerticesPerDeathBatch=65535;

FName DeathBatchComponentTag() {
 static const FName Tag(TEXT("RTSUnitDeathBatch"));
 return Tag;
}

struct FDeathBatchTimer {
 double& Value;
 double Started;
 FDeathBatchTimer(double& In,bool bMeasure):Value(In),Started(bMeasure?FPlatformTime::Seconds():0.){}
 ~FDeathBatchTimer(){if(Started>0.)Value+=(FPlatformTime::Seconds()-Started)*1000.;}
};

struct FDeathBatchCursor {
 int32 Chunk=0,Bodies=0,Vertices=0;
};

struct FDeathBatchInput {
 TArray<const FRTSEntityView*> Effects;
 int32 Vertices=0,Indices=0;
 bool bVisible=false,bModelOrStyleChanged=false;
};

struct FDeathUnitPoseKey {
 const FRTSModel3D* Model;
 float Fall;
 bool operator==(const FDeathUnitPoseKey& Other) const {return Model==Other.Model&&Fall==Other.Fall;}
 friend uint32 GetTypeHash(const FDeathUnitPoseKey& Key) {
  return HashCombine(PointerHash(Key.Model),GetTypeHash(Key.Fall));
 }
};

struct FDeathUnitPose {
 TArray<FVector> Vertices,Normals;
};

struct FDeathBatchBuffers {
 TArray<FVector> Vertices,Normals;
 TArray<int32> Indices;
 TArray<FColor> Colors;

 void Reset(int32 VertexCount,int32 IndexCount,bool bTopologyChanged) {
  Vertices.Reset(VertexCount);
  Normals.Reset(VertexCount);
  Indices.Reset(bTopologyChanged?IndexCount:0);
  Colors.Reset(bTopologyChanged?VertexCount:0);
 }
};

void SolveDeathUnitPose(const FRTSModel3D& Model,float Fall,FDeathUnitPose& Pose) {
 Pose.Vertices.Reserve(Model.VertexCount);
 Pose.Normals.Init(FVector::UpVector,Model.VertexCount);
 const FQuat Rotation(FVector::YAxisVector,Fall*PI*.48f);
 for(const auto& Part:Model.Parts) {
  const int32 Base=Pose.Vertices.Num();
  for(const FVector& Rest:Part.Vertices) {
   FVector Vertex=Rotation.RotateVector(Rest);
   Vertex.Z=FMath::Max(.06,Vertex.Z+.12f);
   Pose.Vertices.Add(Vertex*100.);
  }
  for(int32 I=0;I<Part.Triangles.Num();I+=3) {
   const int32 A=Base+Part.Triangles[I],B=Base+Part.Triangles[I+1],C=Base+Part.Triangles[I+2];
   const FVector Normal=FVector::CrossProduct(Pose.Vertices[B]-Pose.Vertices[A],
                                              Pose.Vertices[C]-Pose.Vertices[A]).GetSafeNormal();
   Pose.Normals[A]=Pose.Normals[B]=Pose.Normals[C]=Normal;
  }
 }
}

void AppendDeathBody(const FRTSEntityView& Effect,const FRTSModel3D& Model,const FDeathUnitPose& Pose,
                     const FVector2D& Origin,bool bTopologyChanged,FDeathBatchBuffers& Buffers) {
 const FQuat Heading=FRotator(0,FMath::RadiansToDegrees(Effect.Heading),0).Quaternion();
 const FVector Forward=Heading.GetAxisX();
 const auto Rotate=[Forward](const FVector& Point) {
  return FVector(Point.X*Forward.X-Point.Y*Forward.Y,Point.X*Forward.Y+Point.Y*Forward.X,Point.Z);
 };
 const FVector Offset(100.*(Effect.Pos-Origin),0);
 int32 Base=Buffers.Vertices.Num();
 for(int32 I=0;I<Pose.Vertices.Num();++I) {
  Buffers.Vertices.Add(Rotate(Pose.Vertices[I])+Offset);
  Buffers.Normals.Add(Rotate(Pose.Normals[I]));
 }
 if(!bTopologyChanged)return;
 for(const auto& Part:Model.Parts) {
  FLinearColor Tint=Part.Color;
  if(Effect.Owner>=0&&(Part.Team||Part.Name.Contains(TEXT("team"))))
   Tint=FMath::Lerp(Tint,RTS::PlayerColor(Effect.Owner),.68f);
  if(Effect.bRemembered)Tint*=.38f;
  else if(Effect.bStealthed)Tint=FMath::Lerp(Tint,FLinearColor(.16f,.27f,.29f),.4f);
  Tint.A=1;
  const FColor Color=Tint.ToFColor(false);
  for(int32 I=0;I<Part.Vertices.Num();++I)Buffers.Colors.Add(Color);
  for(int32 I=0;I<Part.Triangles.Num();I+=3) {
   Buffers.Indices.Add(Base+Part.Triangles[I]);
   Buffers.Indices.Add(Base+Part.Triangles[I+2]);
   Buffers.Indices.Add(Base+Part.Triangles[I+1]);
  }
  Base+=Part.Vertices.Num();
 }
}

bool HasSameDeathBatchMembers(const FRTSDeathBatch3D& Batch,const FDeathBatchInput& Input) {
 if(Batch.Members.Num()!=Input.Effects.Num())return false;
 for(int32 I=0;I<Batch.Members.Num();++I)if(Batch.Members[I]!=Input.Effects[I]->Id)return false;
 return true;
}
}

int32 ARTSScene3D::PhysicalDeathComponentCount() const {
 int32 Count=0;
 // Count registered batch components even if a stale component has lost
 // both its batch inventory entry and its logical effect references.
 const FName BatchTag=DeathBatchComponentTag();
 for(const UActorComponent* Component:GetInstanceComponents())
  if(Component&&Component->IsRegistered()&&Component->ComponentHasTag(BatchTag))++Count;
 for(const auto& Item:Rendered)if(Item.Key<0&&!Item.Value.IsBatchedDeath())
  if(auto* Mesh=Item.Value.Mesh.Get())if(Mesh->IsRegistered())++Count;
 return Count;
}

void ARTSScene3D::RetireUnitDeathBatch(FRTSDeathBatch3D& Batch) {
 if(auto* Mesh=Batch.Mesh.Get()) {
  const bool bMeasure=RTSDeathStressEnabled();
  FDeathBatchTimer RetireTimer(Profile.DeathRetireMs,bMeasure);
  if(bMeasure){++Profile.DeathRetires;++Profile.DeathBatchRetires;}
  Mesh->DestroyComponent();
 }
}

void ARTSScene3D::HideUnitDeathBatches(float Now) {
 if(UnitDeathBatches.IsEmpty())return;
 PC->DeathAnimations.RemoveAll([&](const ARTSController::FDeathAnimation& Death){return Now-Death.Started>3.2f;});
 TSet<int32> Present;
 Present.Reserve(PC->DeathAnimations.Num());
 for(const auto& Death:PC->DeathAnimations)Present.Add(Death.Entity.Id);
 for(auto It=UnitDeathBatches.CreateIterator();It;++It) {
  auto& Batch=It.Value();
  if(!Batch.Members.ContainsByPredicate([&](int32 Id){return Present.Contains(Id);})) {
   RetireUnitDeathBatch(Batch);
   It.RemoveCurrent();
  } else if(auto* Mesh=Batch.Mesh.Get())Mesh->SetVisibility(false);
 }
 for(auto It=Rendered.CreateIterator();It;++It)
  if(It.Value().IsBatchedDeath()&&!Present.Contains(-It.Key()))It.RemoveCurrent();
}

void ARTSScene3D::UpdateUnitDeathBatches(float Now,TSet<int32>& Live) {
 const bool bMeasure=RTSDeathStressEnabled();
 TMap<FRTSDeathBatchKey3D,FDeathBatchInput> Inputs;
 TMap<FRTSDeathBatchKey3D,FDeathBatchCursor> Cursors;
 for(const auto& Death:PC->DeathAnimations) {
  const auto& Effect=Death.Entity;
  if(FRTSCatalog::Get().Def(Effect.Def).bBuilding)continue;
  FString ModelKey;
  auto ModelData=Model(Effect,ModelKey);
  if(!ModelData)continue;

  const FIntPoint Tile(FMath::FloorToInt(Effect.Pos.X/DeathBatchTileMeters),FMath::FloorToInt(Effect.Pos.Y/DeathBatchTileMeters));
  FRTSDeathBatchKey3D Key{Tile,Death.Started,0};
  auto& Cursor=Cursors.FindOrAdd(Key);
  if(Cursor.Bodies>=MaxBodiesPerDeathBatch||
     (Cursor.Bodies>0&&Cursor.Vertices+ModelData->VertexCount>MaxVerticesPerDeathBatch)) {
   ++Cursor.Chunk;
   Cursor.Bodies=0;
   Cursor.Vertices=0;
  }
  Key.Chunk=Cursor.Chunk;
  ++Cursor.Bodies;
  Cursor.Vertices+=ModelData->VertexCount;
  auto& Input=Inputs.FindOrAdd(Key);
  Input.Effects.Add(&Effect);
  Input.Vertices+=ModelData->VertexCount;
  Input.Indices+=ModelData->Triangles*3;

  const int32 RenderId=-Effect.Id;
  Live.Add(RenderId);
  auto& Entry=Rendered.FindOrAdd(RenderId);
  Input.bModelOrStyleChanged|=Entry.Key!=ModelKey||Entry.Owner!=Effect.Owner||
                             Entry.Remembered!=Effect.bRemembered||Entry.Stealthed!=Effect.bStealthed||
                             Entry.PreviousPosition!=Effect.Pos||Entry.BatchHeading!=Effect.Heading;
  Entry.Key=MoveTemp(ModelKey);
  Entry.Model=MoveTemp(ModelData);
  Entry.Owner=Effect.Owner;
  Entry.Remembered=Effect.bRemembered;
  Entry.Stealthed=Effect.bStealthed;
  Entry.PreviousPosition=Effect.Pos;
  Entry.BatchHeading=Effect.Heading;
  Entry.Progress=Effect.Progress;

  if(!Input.bVisible) {
   const FVector2D Screen=PC->WorldToScreen(Effect.Pos);
   Input.bVisible=Screen.X>-300&&Screen.X<PC->ViewSize.X+300&&Screen.Y>-100&&Screen.Y<PC->ViewSize.Y+300;
  }
 }

 for(auto It=UnitDeathBatches.CreateIterator();It;++It)if(!Inputs.Contains(It.Key())) {
  RetireUnitDeathBatch(It.Value());
  It.RemoveCurrent();
 }

 // Synchronized deaths share the exact fall ratio. The cache exists only
 // during this update, so dead models or cohorts cannot accumulate entries.
 TMap<FDeathUnitPoseKey,FDeathUnitPose> Poses;
 FDeathBatchBuffers Buffers;
 for(const auto& Pair:Inputs) {
  const auto& Key=Pair.Key;
  const auto& Input=Pair.Value;
  auto& Batch=UnitDeathBatches.FindOrAdd(Key);
  auto* Mesh=Batch.Mesh.Get();
  const bool bNewMesh=!Mesh;
  if(bNewMesh) {
   FDeathBatchTimer CreateTimer(Profile.DeathCreateMs,bMeasure);
   if(bMeasure){++Profile.DeathCreates;++Profile.DeathBatchCreates;}
   Mesh=NewObject<UProceduralMeshComponent>(this);
   Batch.Mesh=Mesh;
   Mesh->ComponentTags.Add(DeathBatchComponentTag());
   AddInstanceComponent(Mesh);
   Mesh->SetupAttachment(RootComponent);
   Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
   Mesh->SetCastShadow(true);
   Mesh->SetMaterial(0,Surface);
   Mesh->RegisterComponent();
  }

  const bool bRebuild=bNewMesh||Input.bModelOrStyleChanged||!HasSameDeathBatchMembers(Batch,Input);
  const float Age=PC->bReducedMotion?2.5f:Now-Key.Started;
  const float Fall=FMath::Clamp(Age/.8f,0.f,1.f);
  const float Sink=Age>2.f?(Age-2.f)*.6f:0.f;
  const FVector2D Origin(Key.Tile.X*DeathBatchTileMeters,Key.Tile.Y*DeathBatchTileMeters);
  Mesh->SetWorldLocation(FVector(Origin,-Sink)*100.);
  if(!Input.bVisible&&!bRebuild) {
   Mesh->SetVisibility(false);
   continue;
  }

  if(bRebuild||Batch.Fall!=Fall) {
   FDeathBatchTimer PoseTimer(Profile.PoseMs,bMeasure);
   Buffers.Reset(Input.Vertices,Input.Indices,bRebuild);
   if(bRebuild)Batch.Members.Reset(Input.Effects.Num());
   for(const auto* Effect:Input.Effects) {
    auto& Entry=Rendered.FindChecked(-Effect->Id);
    const FDeathUnitPoseKey PoseKey{Entry.Model.Get(),Fall};
    auto* Pose=Poses.Find(PoseKey);
    if(!Pose) {
     Pose=&Poses.Add(PoseKey);
     SolveDeathUnitPose(*Entry.Model,Fall,*Pose);
     if(bMeasure) {
      ++Profile.PoseCalls;
      ++Profile.DeathPoseCalls;
      ++Profile.DeathBatchPoseSolves;
      Profile.PosedVertices+=Pose->Vertices.Num();
      Profile.DeathPosedVertices+=Pose->Vertices.Num();
      Profile.DeathBatchSolvedVertices+=Pose->Vertices.Num();
     }
    }
    if(bRebuild) {
     Batch.Members.Add(Effect->Id);
     Entry.Mesh=Mesh;
     Entry.BatchFirstVertex=Buffers.Vertices.Num();
     Entry.BatchFirstIndex=Buffers.Indices.Num();
     Entry.PoseState=TEXT("death");
    }
    AppendDeathBody(*Effect,*Entry.Model,*Pose,Origin,bRebuild,Buffers);
    Entry.DeathFall=Fall;
    ++Entry.PoseRevision;
   }
   {
    FDeathBatchTimer SubmitTimer(Profile.MeshSubmitMs,bMeasure);
    FDeathBatchTimer FirstSubmitTimer(Profile.DeathFirstSubmitMs,bMeasure&&bNewMesh);
    if(bMeasure)Profile.DeathBatchUploadedVertices+=Buffers.Vertices.Num();
    if(bRebuild) {
     if(bMeasure) {
      ++Profile.DeathBatchRebuilds;
      if(bNewMesh)++Profile.DeathFirstSubmits;
     }
     Mesh->CreateMeshSection(0,Buffers.Vertices,Buffers.Indices,Buffers.Normals,{},Buffers.Colors,{},false);
    } else {
     if(bMeasure)++Profile.DeathBatchUpdates;
     Mesh->UpdateMeshSection(0,Buffers.Vertices,Buffers.Normals,{},{},{});
    }
   }
   Batch.Fall=Fall;
  }

  for(const auto* Effect:Input.Effects)Rendered.FindChecked(-Effect->Id).PoseElapsed=Age;
  Mesh->SetVisibility(Input.bVisible);
 }
}
