#include "RTSSceneTests.h"
#include "RTSScene3D.h"
#include "RTSController.h"
#include "RTSWorld.h"
#include "Components/MeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInterface.h"
#include "RawIndexBuffer.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Serialization/JsonSerializer.h"
#include "SkeletalRenderPublic.h"
#include "UnrealClient.h"

namespace {
bool HasActiveGpuSkin(const UPoseableMeshComponent* Skin) {
 return Skin&&Skin->IsRegistered()&&Skin->IsRenderStateCreated()&&Skin->IsVisible()&&
  !Skin->bRenderStatic&&Skin->MeshObject&&!Skin->MeshObject->IsCPUSkinned()&&Skin->GetSkinnedAsset();
}

struct FSkinProbeVertex {
 FVector Rest;
 FColor Color;
 bool bBowstring;
};

struct FSkinProbeSummary {
 int32 Actors=0,RenderVertices=0,AuthoredVertices=0;
 int32 InvalidMeshes=0,MissingRenderPositions=0,MissingColors=0,MissingAuthoredPositions=0,InvalidMaterials=0;
 int32 FrontFaces=0,FrontFaceMismatches=0,InvalidFaceBuffers=0;
 double MaxRigidError=0,MaxBowstringError=0;
 bool Passed() const {
  return Actors>0&&RenderVertices>0&&AuthoredVertices>0&&InvalidMeshes==0&&MissingRenderPositions==0&&
   MissingColors==0&&MissingAuthoredPositions==0&&InvalidMaterials==0;
 }
 FString Describe() const {
  return FString::Printf(TEXT("%d active GPU poses, %d render/%d authored vertices, max rigid %.5f cm, bowstring %.5f cm; invalid meshes/materials %d/%d, unmatched render/authored points %d/%d, unmatched colors %d"),
   Actors,RenderVertices,AuthoredVertices,MaxRigidError,MaxBowstringError,InvalidMeshes,InvalidMaterials,MissingRenderPositions,MissingAuthoredPositions,MissingColors);
 }
};

FIntVector SkinProbeCell(const FVector& Position) {
 constexpr double CellSize=.05;
 return FIntVector(FMath::FloorToInt(Position.X/CellSize),FMath::FloorToInt(Position.Y/CellSize),FMath::FloorToInt(Position.Z/CellSize));
}

bool SkinProbeColorsMatch(FColor Actual,FColor Expected) {
 constexpr int32 ChannelTolerance=1;
 return FMath::Abs(int32(Actual.R)-Expected.R)<=ChannelTolerance&&FMath::Abs(int32(Actual.G)-Expected.G)<=ChannelTolerance&&
  FMath::Abs(int32(Actual.B)-Expected.B)<=ChannelTolerance&&Actual.A==Expected.A;
}

bool SkinProbeMaterialMatches(UMaterialInterface* Material,UMaterialInterface* Surface,const FRTSEntityView& Entity) {
 if(!Material||!Surface||Material->GetBaseMaterial()!=Surface->GetBaseMaterial())return false;
 float TeamWeight=-1,StealthAmount=-1,VisibilityTint=-1;
 FLinearColor TeamColor;
 if(!Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("TeamWeight")),TeamWeight)||
    !Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("StealthAmount")),StealthAmount)||
    !Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("VisibilityTint")),VisibilityTint))return false;
 if(Entity.Owner>=0&&(!Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("TeamColor")),TeamColor)||
    !TeamColor.Equals(RTS::PlayerColor(Entity.Owner),.001)))return false;
 return FMath::IsNearlyEqual(TeamWeight,Entity.Owner>=0?.68f:0.f)&&
  FMath::IsNearlyEqual(StealthAmount,Entity.bStealthed&&!Entity.bRemembered?.4f:0.f)&&
  FMath::IsNearlyEqual(VisibilityTint,Entity.bRemembered?.38f:1.f);
}

void AuditGpuFrontFaces(const FSkeletalMeshLODRenderData& LOD,FSkinProbeSummary& Summary) {
 const FRawStaticIndexBuffer16or32Interface* Indices=LOD.MultiSizeIndexContainer.GetIndexBuffer();
 if(!Indices||Indices->Num()==0||Indices->Num()%3!=0){++Summary.InvalidFaceBuffers;return;}
 const auto& Points=LOD.StaticVertexBuffers.PositionVertexBuffer;
 const auto& Tangents=LOD.StaticVertexBuffers.StaticMeshVertexBuffer;
 for(int32 Face=0;Face<Indices->Num();Face+=3) {
  const uint32 A=Indices->Get(Face),B=Indices->Get(Face+1),C=Indices->Get(Face+2);
  if(A>=LOD.GetNumVertices()||B>=LOD.GetNumVertices()||C>=LOD.GetNumVertices()) {++Summary.InvalidFaceBuffers;return;}
  const FVector FaceCross=FVector::CrossProduct(FVector(Points.VertexPosition(B)-Points.VertexPosition(A)),FVector(Points.VertexPosition(C)-Points.VertexPosition(A)));
  if(FaceCross.SizeSquared()<=1.e-10)continue;
  const FVector4f TangentZ=Tangents.VertexTangentZ(A)+Tangents.VertexTangentZ(B)+Tangents.VertexTangentZ(C);
  const FVector AverageNormal(TangentZ.X,TangentZ.Y,TangentZ.Z);
  ++Summary.FrontFaces;
  if(FVector::DotProduct(FaceCross,AverageNormal)>=0.)++Summary.FrontFaceMismatches;
 }
}

void CheckActualSkin(const FRTSRendered3D& R,const FRTSEntityView& Entity,UMaterialInterface* Surface,FSkinProbeSummary& Summary) {
 UPoseableMeshComponent* Skin=R.Skin.Get();
 if(!HasActiveGpuSkin(Skin)||!R.Model||R.Vertices.Num()!=R.Model->VertexCount) {++Summary.InvalidMeshes;return;}
 const FSkeletalMeshRenderData* RenderData=Skin->GetSkeletalMeshRenderData();
 if(!RenderData||!RenderData->LODRenderData.IsValidIndex(0)) {++Summary.InvalidMeshes;return;}
 const FSkeletalMeshLODRenderData& LOD=RenderData->LODRenderData[0];
 const FSkinWeightVertexBuffer* Weights=Skin->GetSkinWeightBuffer(0);
 const auto& Colors=LOD.StaticVertexBuffers.ColorVertexBuffer;
 if(!Weights||LOD.GetNumVertices()==0||LOD.RenderSections.IsEmpty()||Colors.GetNumVertices()!=LOD.GetNumVertices()) {
  UE_LOG(LogTemp,Display,TEXT("RTS_3D_SKIN_BUFFER_INVALID model=%s vertices=%u colors=%u sections=%d weights=%d"),
   *R.Model->Id,LOD.GetNumVertices(),Colors.GetNumVertices(),LOD.RenderSections.Num(),Weights!=nullptr);
  ++Summary.InvalidMeshes;
  return;
 }

 TArray<FMatrix44f> RefToLocal;
 TArray<FVector3f> Skinned;
 Skin->GetCurrentRefToLocalMatrices(RefToLocal,0);
 if(RefToLocal.IsEmpty()){++Summary.InvalidMeshes;return;}
 USkinnedMeshComponent::ComputeSkinnedPositions(Skin,Skinned,RefToLocal,LOD,*Weights);
 if(Skinned.Num()!=int32(LOD.GetNumVertices())){++Summary.InvalidMeshes;return;}

 ++Summary.Actors;
 Summary.RenderVertices+=Skinned.Num();
 Summary.AuthoredVertices+=R.Vertices.Num();
 AuditGpuFrontFaces(LOD,Summary);
 for(const FSkelMeshRenderSection& Section:LOD.RenderSections)
  if(!SkinProbeMaterialMatches(Skin->GetMaterial(Section.MaterialIndex),Surface,Entity))++Summary.InvalidMaterials;

 TArray<FSkinProbeVertex> Authored;
 TMap<FIntVector,TArray<int32>> RestCells;
 Authored.Reserve(R.Vertices.Num());
 RestCells.Reserve(R.Vertices.Num());
 for(const FRTSModelPart& Part:R.Model->Parts) {
  FColor Color=Part.Color.ToFColor(false);
  Color.A=Part.Team||Part.Name.Contains(TEXT("team"))?255:0;
  const bool bBowstring=Part.Name.StartsWith(TEXT("bowstring"));
  for(const FVector& Rest:Part.Vertices) {
   const FVector RestCm=Rest*100.;
   RestCells.FindOrAdd(SkinProbeCell(RestCm)).Add(Authored.Add({RestCm,Color,bBowstring}));
  }
 }
 if(Authored.Num()!=R.Vertices.Num()){++Summary.InvalidMeshes;return;}

 constexpr double RestTolerance=.05;
 constexpr double RigidPoseTolerance=.05;
 TBitArray<> MatchedAuthored(false,Authored.Num());
 for(int32 Vertex=0;Vertex<Skinned.Num();++Vertex) {
  const FVector Rest(LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Vertex));
  const FVector Posed(Skinned[Vertex]);
  const FColor ActualColor=Colors.VertexColor(Vertex);
  const FIntVector Cell=SkinProbeCell(Rest);
  double ClosestError=TNumericLimits<double>::Max();
  int32 ClosestIndex=INDEX_NONE;
  bool bMatchedColor=false,bMatchedPosition=false;
  for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y)for(int32 Z=-1;Z<=1;++Z) {
   const TArray<int32>* Neighbors=RestCells.Find(Cell+FIntVector(X,Y,Z));
   if(!Neighbors)continue;
   for(int32 Index:*Neighbors) {
    const FSkinProbeVertex& Expected=Authored[Index];
    if(FVector::DistSquared(Expected.Rest,Rest)>FMath::Square(RestTolerance))continue;
    const double Error=FVector::Distance(R.Vertices[Index],Posed);
    if(Error<ClosestError){ClosestError=Error;ClosestIndex=Index;}
    if(Error>RigidPoseTolerance)continue;
    bMatchedPosition=true;
    if(SkinProbeColorsMatch(ActualColor,Expected.Color)){bMatchedColor=true;MatchedAuthored[Index]=true;}
   }
  }
  if(ClosestIndex!=INDEX_NONE) {
   double& Maximum=Authored[ClosestIndex].bBowstring?Summary.MaxBowstringError:Summary.MaxRigidError;
   Maximum=FMath::Max(Maximum,ClosestError);
  }
  if(!bMatchedPosition)++Summary.MissingRenderPositions;
  else if(!bMatchedColor)++Summary.MissingColors;
  if(Vertex==0&&!bMatchedColor)UE_LOG(LogTemp,Display,TEXT("RTS_3D_SKIN_VERTEX_DETAIL model=%s rest=%s posed=%s color=%s nearest_cm=%.5f"),
   *R.Model->Id,*Rest.ToString(),*Posed.ToString(),*ActualColor.ToString(),ClosestIndex==INDEX_NONE?-1.:ClosestError);
 }
 for(int32 Index=0;Index<MatchedAuthored.Num();++Index)if(!MatchedAuthored[Index])++Summary.MissingAuthoredPositions;
}
}

class FRTSSceneProbe {
 TArray<TSharedPtr<FJsonValue>> Cases;
 TArray<TSharedPtr<FJsonValue>> Shots;
 int Frame=0,ChangedAt=0,FactionIndex=0,Stage=0,SelectionView=0,TerrainView=0;
 bool Failed=false,Done=false;
 const bool bGpuRequested=!FParse::Param(FCommandLine::Get(),TEXT("RTSProceduralMeshes"));
 double Began=FPlatformTime::Seconds();
 double DrawWaitStarted=0.;
 float DrawAfterWorldSeconds=0.f;
 void Check(const FString& Name,bool Pass,const FString& Detail) {
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),Name);Row->SetBoolField(TEXT("passed"),Pass);Row->SetStringField(TEXT("detail"),Detail);
  Cases.Add(MakeShared<FJsonValueObject>(Row));Failed|=!Pass;
  UE_LOG(LogTemp,Display,TEXT("RTS_3D_TEST_%s %s %s"),Pass?TEXT("PASS"):TEXT("FAIL"),*Name,*Detail);
 }
 void Finish(ARTSScene3D* Scene) {
  Done=true;auto Root=MakeShared<FJsonObject>();Root->SetBoolField(TEXT("passed"),!Failed);Root->SetBoolField(TEXT("gpu_skinning_requested"),bGpuRequested);Root->SetArrayField(TEXT("cases"),Cases);Root->SetArrayField(TEXT("screenshots"),Shots);
  Root->SetStringField(TEXT("scope"),TEXT("Isolated real procedural-mesh presentation, projection, picking, animation and death fixtures. Gameplay/authority are tested separately."));
  Root->SetNumberField(TEXT("models_loaded"),Scene->Models.Num());Root->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Began);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FString JSON;
  FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&JSON));const bool Saved=FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("scene3d-tests.json")));
  FPlatformMisc::RequestExitWithStatus(Failed||!Saved,Failed||!Saved?1:0);
 }
 void Catalogue(ARTSScene3D* Scene,ARTSController* C) {
  const auto& Cat=FRTSCatalog::Get();int Missing=0;const FString Original=C->Players[0].Faction;
  C->Players[0].Faction.Empty();
  // Neutral entries exercise the base fallback; all-faction variants otherwise hide it.
  for(const auto& D:Cat.Defs)if(Scene->ModelPaths.Contains(D.Id)) {
   FRTSEntityView E;E.Def=Cat.Index(D.Id);E.Owner=-1;FString Key;
   if(!Scene->Model(E,Key))++Missing;
   if(D.Id==TEXT("works_of_defense")||D.Id==TEXT("cement_wall")){E.bGate=true;if(!Scene->Model(E,Key))++Missing;}
  }
  for(const auto& F:Cat.Factions) {
   C->Players[0].Faction=F.Id;
   for(const auto& D:Cat.Defs)if(Scene->ModelPaths.Contains(D.Id)) {
    FRTSEntityView E;E.Def=Cat.Index(D.Id);E.Owner=0;FString Key;
    if(!Scene->Model(E,Key))++Missing;
    if(D.Id==TEXT("works_of_defense")||D.Id==TEXT("cement_wall")){E.bGate=true;if(!Scene->Model(E,Key))++Missing;}
   }
  }
  C->Players[0].Faction=Original;
  Check(TEXT("complete_mesh_library"),Missing==0&&Scene->Models.Num()==Scene->ModelPaths.Num(),FString::Printf(TEXT("Loaded %d/%d mapped real meshes, failures=%d"),Scene->Models.Num(),Scene->ModelPaths.Num(),Missing));
 }
 void PinFixtureCamera(ARTSController* C) const {
  C->CameraCenter=FVector2D(62,62);
  C->CameraRotation=FactionIndex%2?PI/4:0;
  C->Zoom=1.65f;
 }
 void Fixture(ARTSController* C) {
  const auto& Cat=FRTSCatalog::Get();const auto& F=Cat.Factions[FactionIndex];C->Players[0].Faction=F.Id;C->Players[0].Age=4;C->Players[0].Favor=50;
  C->Entities.Empty();C->DisplayPositions.Empty();C->DeathAnimations.Empty();C->Map=0;C->Fog.Init(2,64*64);C->Selection={1};
  PinFixtureCamera(C);
  const auto Add=[&](const FString& Id,FVector2D Pos,float Heading=0,bool Gate=false) {
   if(Cat.Index(Id)<0)return;
   FRTSEntityView E;E.Id=C->Entities.Num()+1;E.Def=Cat.Index(Id);E.Owner=0;E.Pos=Pos;E.Heading=Heading;E.HP=E.MaxHP=Cat.Def(Id).HP;E.Status=TEXT("idle");E.bGate=Gate;
   C->Entities.Add(E);C->DisplayPositions.Add(E.Id,Pos);
  };
  Add(TEXT("seat_of_the_people"),{60,59});Add(TEXT("tents_and_habitations"),{47,63});Add(TEXT("storehouse_of_provisions"),{72,51});Add(TEXT("temple_of_the_land"),{79,66});
  if(F.Id==TEXT("laban"))Add(TEXT("winery"),{65,78});
  Add(TEXT("works_of_defense"),{43,48});Add(TEXT("works_of_defense"),{47,48});Add(TEXT("works_of_defense"),{51,48},0,true);Add(TEXT("works_of_defense"),{43,52},PI/2);Add(TEXT("works_of_defense"),{43,56},PI/2);
  Add(TEXT("tree"),{65,43});Add(TEXT("ore"),{75,43});Add(TEXT("berries"),{80,48});
  const TCHAR* Units[]={TEXT("industrious_laborer"),TEXT("bowman"),TEXT("chief_captain"),TEXT("drunken_concubine"),TEXT("curelom"),TEXT("kings_chariot"),TEXT("wild_game"),TEXT("hagoths_vessel")};
  for(int I=0;I<8;++I)Add(Units[I],FVector2D(50+(I%4)*7,72+(I/4)*9),PI/4);
  // These deliberately isolated fixtures show all body styles; faction legality is not under test.
  Stage=1;ChangedAt=Frame;
 }
 enum ESelectionFixtureId { PickSeat=101,PickWorker,PickTree,PickEnemy,PickRemembered,PickDead,PickSolo,PickOffscreen,PickHud,PickRim };
 void PinSelectionCamera(ARTSController* C) const {
  constexpr float Zooms[]={.55f,1.65f,4.f};
  C->Zoom=Zooms[SelectionView/8];
  C->CameraRotation=(SelectionView%8)*PI/4;
  C->CameraCenter=FVector2D(82,79)+FVector2D(1.4,-1.1).GetRotated(SelectionView*45.);
 }
 void SelectionFixture(ARTSController* C) {
  PinSelectionCamera(C);
  C->Players[0].Faction=TEXT("nephites");C->Selection.Empty();C->DeathAnimations.Empty();C->Entities.Empty();C->DisplayPositions.Empty();
  const auto Add=[&](int32 Id,const TCHAR* Type,int32 Owner,FVector2D Position) {
   FRTSEntityView E;E.Id=Id;E.Def=FRTSCatalog::Get().Index(Type);E.Owner=Owner;E.Pos=Position;E.Heading=PI/6;E.Status=TEXT("idle");
   E.HP=E.MaxHP=FRTSCatalog::Get().Def(E.Def).HP;
   C->Entities.Add(E);C->DisplayPositions.Add(Id,Position);
  };
  const auto At=[&](FVector2D Position){return FVector2D(82,79)+C->RotateMap(Position,true);};
  Add(PickSeat,TEXT("seat_of_the_people"),0,At({0,0}));
  Add(PickWorker,TEXT("industrious_laborer"),0,At({1.8,1.8}));
  Add(PickTree,TEXT("tree"),-1,At({5,-2}));
  Add(PickEnemy,TEXT("industrious_laborer"),1,At({3.25,1.0}));
  Add(PickRemembered,TEXT("tents_and_habitations"),1,At({-5.4,5.8}));C->Entities.Last().bRemembered=true;
  Add(PickDead,TEXT("industrious_laborer"),0,At({6,-6}));C->Entities.Last().HP=0;
  Add(PickSolo,TEXT("industrious_laborer"),0,At({-2.7,4.6}));
  const double U=FMath::Min(C->ViewSize.X/1600.,C->ViewSize.Y/900.);
  Add(PickOffscreen,TEXT("industrious_laborer"),0,C->ScreenToWorld({C->ViewSize.X+120*U,C->ViewSize.Y*.52}));
  Add(PickHud,TEXT("industrious_laborer"),0,C->ScreenToWorld({C->ViewSize.X*.17,44*U}));
  Add(PickRim,TEXT("tree"),-1,C->ScreenToWorld({1,C->ViewSize.Y*.56}));
  Stage=4;ChangedAt=Frame;
 }
 struct FAdjacentPick { int32 Id=0;FVector2D Pixel=FVector2D::ZeroVector; };
 bool FindSelectionPixel(ARTSController* C,ARTSScene3D* Scene,int32 Id,FVector2D& Pixel,bool bMissBeside=false,FAdjacentPick* AdjacentPick=nullptr) {
  const auto* Entity=C->FindEntity(Id);if(!Entity)return false;
  const FBox2D Bounds=Scene->Bounds(*Entity);if(!Bounds.bIsValid)return false;
  const FVector2D Center=Bounds.GetCenter().RoundToVector();
  const int32 Radius=FMath::CeilToInt(Bounds.GetExtent().GetAbsMax());
  for(int32 R=0;R<=Radius;++R)for(int32 Y=-R;Y<=R;++Y)for(int32 X=-R;X<=R;++X) {
   if(FMath::Max(FMath::Abs(X),FMath::Abs(Y))!=R)continue;
   const FVector2D Point=Center+FVector2D(X,Y);
   if(!Bounds.IsInsideOrOn(Point)||!C->CanInteractWithWorld(Point)||Scene->Hit(Point)!=Id)continue;
   if(!bMissBeside){Pixel=Point;return true;}
   for(const FVector2D Offset:{FVector2D(1,0),FVector2D(-1,0),FVector2D(0,1),FVector2D(0,-1)}) {
    const FVector2D Neighbor=Point+Offset;
    if(!C->CanInteractWithWorld(Neighbor))continue;
    const int32 NeighborId=Scene->Hit(Neighbor);
    if(NeighborId==0){Pixel=Neighbor;return true;}
    if(AdjacentPick&&AdjacentPick->Id==0&&NeighborId!=Id)*AdjacentPick={NeighborId,Neighbor};
   }
  }
  return false;
 }
 bool RejectEmptyDragCorner(ARTSController* C,ARTSScene3D* Scene) {
  const FBox2D Bounds=Scene->Bounds(*C->FindEntity(PickSolo));
  const auto* R=Scene->Rendered.Find(PickSolo);if(!R||!R->Component()||!Bounds.bIsValid)return false;
  Scene->EnsureGeometry(*R);
  const FTransform Transform=R->Component()->GetComponentTransform();
  TArray<FVector2D> Vertices;Vertices.Reserve(R->Vertices.Num());
  for(const FVector& V:R->Vertices)Vertices.Add(Scene->Project(Transform.TransformPosition(V)/100.));
  for(double X:{-1.,1.})for(double Y:{-1.,1.}) {
   const FVector2D Corner(X<0?Bounds.Min.X:Bounds.Max.X,Y<0?Bounds.Min.Y:Bounds.Max.Y);
   const FVector2D Start=Corner+FVector2D(X*8,Y),End=Corner-FVector2D(X*.25,Y*.25);
   if(!C->CanInteractWithWorld(Start)||!C->CanInteractWithWorld(End))continue;
   FBox2D Box(Start,Start);Box+=End;
   bool bTouchesTriangleBounds=false;
   for(int32 I=0;I+2<R->Triangles.Num();I+=3) {
    const FVector2D A=Vertices[R->Triangles[I]];
    FBox2D Triangle(A,A);Triangle+=Vertices[R->Triangles[I+1]];Triangle+=Vertices[R->Triangles[I+2]];
    if(Box.Intersect(Triangle)){bTouchesTriangleBounds=true;break;}
   }
   if(bTouchesTriangleBounds)continue;
   C->SelectBox(Start,End,false);
   return !C->Selection.Contains(PickSolo);
  }
  return false;
 }
 void CheckSelection(ARTSScene3D* Scene,ARTSController* C) {
  const auto Pick=[&](FVector2D Pixel,int32 Id,bool bTouch=false) {
   C->LastSelectedId=0;C->SelectAt(Pixel,false,bTouch);
   return C->Selection.Num()==1&&C->Selection[0]==Id;
  };
  FVector2D Seat,Worker,Tree,Rim;
  const bool bExactSeat=FindSelectionPixel(C,Scene,PickSeat,Seat)&&Pick(Seat,PickSeat);
  const bool bExactWorker=FindSelectionPixel(C,Scene,PickWorker,Worker)&&Pick(Worker,PickWorker);
  const bool bExactTree=FindSelectionPixel(C,Scene,PickTree,Tree)&&Pick(Tree,PickTree)&&C->EntityUnder(Tree,true)==PickTree;
  const bool bRim=FindSelectionPixel(C,Scene,PickRim,Rim)&&Pick(Rim,PickRim);
  const FBox2D SeatBounds=Scene->Bounds(*C->FindEntity(PickSeat)),WorkerBounds=Scene->Bounds(*C->FindEntity(PickWorker));
  const bool bOverlap=SeatBounds.bIsValid&&WorkerBounds.bIsValid&&SeatBounds.Intersect(WorkerBounds);
  FVector2D Near;
  FAdjacentPick Adjacent;
  const bool bOverlapNearFound=FindSelectionPixel(C,Scene,PickWorker,Near,true,&Adjacent);
  const bool bSoloNearFound=!bOverlapNearFound&&FindSelectionPixel(C,Scene,PickSolo,Near,true);
  const int32 NearWorkerId=bOverlapNearFound?PickWorker:bSoloNearFound?PickSolo:0;
  const bool bNearWorker=NearWorkerId&&Pick(Near,NearWorkerId);
  const int32 NearResolved=NearWorkerId&&C->Selection.Num()==1?C->Selection[0]:0;
  const bool bAdjacentPriority=bOverlapNearFound||(Adjacent.Id&&Pick(Adjacent.Pixel,Adjacent.Id));
  const bool bNearTree=FindSelectionPixel(C,Scene,PickTree,Near,true)&&Pick(Near,PickTree,true);
  const bool bContextNear=bNearTree&&C->EntityUnder(Near,true)==PickTree;
  const FBox2D SoloBounds=Scene->Bounds(*C->FindEntity(PickSolo));
  bool bContextBlank=false;
  if(SoloBounds.bIsValid)for(double X:{-1.,1.})for(double Y:{-1.,1.}) {
   const FVector2D Corner(X<0?SoloBounds.Min.X:SoloBounds.Max.X,Y<0?SoloBounds.Min.Y:SoloBounds.Max.Y);
   const FVector2D Blank=Corner+FVector2D(4.5*X,4.5*Y);
   if(C->CanInteractWithWorld(Blank)&&C->EntityUnder(Blank,true)==0)bContextBlank=true;
  }
  const bool bCorner=RejectEmptyDragCorner(C,Scene);
  bool bDrag=false,bShift=false,bDouble=false;
  if(bExactWorker&&bExactSeat) {
   const auto* Enemy=C->FindEntity(PickEnemy);
   FBox2D Box=SeatBounds+WorkerBounds;Box+=Scene->Bounds(*Enemy);
   C->SelectBox(Box.Max+FVector2D(2,2),Box.Min-FVector2D(2,2),false);
   bDrag=C->Selection.Num()==1&&C->Selection.Contains(PickWorker);
   C->SelectAt(Worker,true);bShift=C->Selection.IsEmpty();
   if(Pick(Worker,PickWorker)) {C->SelectAt(Worker,false);bDouble=C->Selection.Num()==2&&C->Selection.Contains(PickSolo)&&C->Selection.Contains(PickWorker);}
  }
  const auto RejectHidden=[&](int32 Index) {
   auto& Fixture=C->Entities[Index];const float HP=Fixture.HP;const bool bRemembered=Fixture.bRemembered;
   Fixture.HP=Fixture.MaxHP;Fixture.bRemembered=false;
   FVector2D Point;const bool bMeasurable=FindSelectionPixel(C,Scene,Fixture.Id,Point);
   Fixture.HP=HP;Fixture.bRemembered=bRemembered;
   return bMeasurable&&Scene->Hit(Point)==0&&Scene->Hit(Point,true)==0&&Scene->SelectionHit(Point,C->TouchSlop()*.5)==0;
  };
  const bool bRemembered=RejectHidden(PickRemembered-PickSeat),bDead=RejectHidden(PickDead-PickSeat);
  const FVector2D HUDPoint=C->WorldToScreen(C->FindEntity(PickHud)->Pos),Offscreen=C->WorldToScreen(C->FindEntity(PickOffscreen)->Pos);
  const double U=FMath::Min(C->ViewSize.X/1600.,C->ViewSize.Y/900.);
  C->Selection={PickSeat};C->SelectAt(HUDPoint,false);C->SelectAt(Offscreen,false);
  const bool bBlocked=C->Selection.Num()==1&&C->Selection[0]==PickSeat;
  C->SelectBox({HUDPoint.X-12,164*U},{HUDPoint.X+12,20*U},false);
  const bool bClipped=C->Selection.IsEmpty();
  const FVector World(C->FindEntity(PickWorker)->Pos,1.2);FVector2D Actual;
  const double ProjectionError=C->ProjectWorldLocationToScreen(World*100,Actual)?FVector2D::Distance(Actual,Scene->Project(World)):1.e9;
  Check(FString::Printf(TEXT("rotated_selection_%02d"),SelectionView),
   ProjectionError<.8&&bExactSeat&&bExactWorker&&bExactTree&&bRim&&bOverlap&&bNearWorker&&bAdjacentPriority&&bNearTree&&bContextNear&&bContextBlank&&bCorner&&bDrag&&bShift&&bDouble&&bRemembered&&bDead&&bBlocked&&bClipped,
   FString::Printf(TEXT("heading %d zoom %.2f projection %.4f px exact seat/unit/tree/edge=%d/%d/%d/%d overlap=%d mouse near found overlap/solo=%d/%d expected/actual=%d/%d adjacent exact target=%d passed=%d touch near=%d context edge/blank=%d/%d drag corner/units=%d/%d shift/double=%d/%d fog/death=%d/%d HUD clicks/drag=%d/%d"),
    (SelectionView%8)*45,C->Zoom,ProjectionError,bExactSeat,bExactWorker,bExactTree,bRim,bOverlap,bOverlapNearFound,bSoloNearFound,NearWorkerId,NearResolved,Adjacent.Id,bAdjacentPriority,bNearTree,bContextNear,bContextBlank,bCorner,bDrag,bShift,bDouble,bRemembered,bDead,bBlocked,bClipped));
 }
 void PinTerrainCamera(ARTSController* C) const {
  C->CameraCenter=FVector2D(93.45,93.45);C->Zoom=1.65f;C->CameraRotation=TerrainView*PI;
 }
 void TerrainSelectionFixture(ARTSController* C) {
  PinTerrainCamera(C);
  C->Map=4;C->Fog.Init(2,64*64);C->Entities.Empty();C->DisplayPositions.Empty();C->DeathAnimations.Empty();C->Selection.Empty();
  FRTSEntityView Worker;Worker.Id=PickWorker;Worker.Owner=0;Worker.Def=FRTSCatalog::Get().Index(TEXT("industrious_laborer"));
  Worker.Pos=C->CameraCenter;Worker.Status=TEXT("idle");
  C->Entities.Add(Worker);C->DisplayPositions.Add(Worker.Id,Worker.Pos);
  Stage=5;ChangedAt=Frame;
 }
 void CheckTerrainSelection(ARTSScene3D* Scene,ARTSController* C) {
  const auto* R=Scene->Rendered.Find(PickWorker);
  TSet<FIntPoint> Pixels;
  if(R&&R->Component()&&R->Component()->IsVisible()) {
   Scene->EnsureGeometry(*R);
   const FTransform Transform=R->Component()->GetComponentTransform();
   for(int32 I=0;I+2<R->Triangles.Num();I+=9) {
    const FVector P=(R->Vertices[R->Triangles[I]]+R->Vertices[R->Triangles[I+1]]+R->Vertices[R->Triangles[I+2]])/3;
    const FVector2D Screen=Scene->Project(Transform.TransformPosition(P)/100.);
    if(C->CanInteractWithWorld(Screen))Pixels.Add(FIntPoint(FMath::RoundToInt(Screen.X),FMath::RoundToInt(Screen.Y)));
   }
  }
  int32 Exact=0,Mouse=0,Context=0,Touch=0;
  for(const FIntPoint Pixel:Pixels) {
   Exact+=Scene->Hit(FVector2D(Pixel))==PickWorker;
   Mouse+=Scene->SelectionHit(FVector2D(Pixel))==PickWorker;
   Context+=Scene->Hit(FVector2D(Pixel),true)==PickWorker;
   Touch+=Scene->SelectionHit(FVector2D(Pixel),C->TouchSlop()*.5)==PickWorker;
  }
  const bool bCorrect=TerrainView==0?Exact==0&&Mouse==0&&Context==0&&Touch==0:Exact>0&&Mouse>0&&Context>0&&Touch>0;
  Check(TerrainView==0?TEXT("terrain_selection_blocked_at_yaw_0"):TEXT("terrain_selection_exposed_at_yaw_180"),
   Scene->GroundMap==4&&Scene->GroundTopZ>400.&&Pixels.Num()>10&&bCorrect,
   FString::Printf(TEXT("Moronihah ring max %.2f m, worker hits exact/mouse/context/touch %d/%d/%d/%d of %d posed surface samples"),
    Scene->GroundTopZ/100.,Exact,Mouse,Context,Touch,Pixels.Num()));
 }
 void Geometry(ARTSScene3D* Scene,ARTSController* C) {
  const FString Prefix=FRTSCatalog::Get().Factions[FactionIndex].Id;
  double Error=0;for(float X:{-8.f,0.f,9.f})for(float Y:{-5.f,6.f})for(float Z:{0.f,2.f,5.f}) {
   const FVector World(C->CameraCenter.X+X,C->CameraCenter.Y+Y,Z);FVector2D Actual;
   if(!C->ProjectWorldLocationToScreen(World*100,Actual)){Error=1.e9;continue;}
   Error=FMath::Max(Error,FVector2D::Distance(Actual,Scene->Project(World)));
  }
  Check(Prefix+TEXT(".engine_projection"),Error<.8,FString::Printf(TEXT("18 elevated/ground points, max projection error %.4f pixels, camera %.1f degrees"),Error,FMath::RadiansToDegrees(C->CameraRotation)));
  int Visible=0,Pickable=0,Inside=0,ExpectedSkin=0,ActiveSkin=0,IntegerSkinPicks=0,EmptySkinCorners=0;
  FSkinProbeSummary SkinSummary;
  for(const auto& E:C->Entities) {
   const auto* R=Scene->Rendered.Find(E.Id);
   const UMeshComponent* Component=R?R->Component():nullptr;
   if(!R||!R->Model||!Component||!Component->IsVisible())continue;
   ++Visible;
   Scene->EnsureGeometry(*R);
   const bool bExpectedSkin=bGpuRequested&&!R->Model->AnimationStyle.IsEmpty();
   if(bExpectedSkin) {
    ++ExpectedSkin;
    if(HasActiveGpuSkin(R->Skin.Get()))++ActiveSkin;
    CheckActualSkin(*R,E,Scene->SkinSurface,SkinSummary);
   }
   const FTransform Transform=Component->GetComponentTransform();
   const FBox2D Bounds=Scene->Bounds(E),TolerantBounds=Bounds.ExpandBy(FVector2D(.1,.1));
   bool Fits=Bounds.bIsValid&&!R->Vertices.IsEmpty();
   for(const FVector& V:R->Vertices)Fits&=TolerantBounds.IsInsideOrOn(Scene->Project(Transform.TransformPosition(V)/100));
   Inside+=Fits?1:0;
   // Sample actual visible face interiors, not a sprite-box center or floor marker.
   bool Hit=false,bIntegerHit=false;
   const int Stride=FMath::Max(3,(R->Triangles.Num()/48)/3*3);
   for(int I=0;I+2<R->Triangles.Num()&&(!Hit||(bExpectedSkin&&!bIntegerHit));I+=Stride) {
    const FVector P=(R->Vertices[R->Triangles[I]]+R->Vertices[R->Triangles[I+1]]+R->Vertices[R->Triangles[I+2]])/3;
    const FVector2D Projected=Scene->Project(Transform.TransformPosition(P)/100);
    Hit|=Scene->Hit(Projected)==E.Id;
    if(bExpectedSkin) {
     const FVector2D Pixel=Projected.RoundToVector();
     bIntegerHit|=C->InWorld(Pixel)&&Scene->Hit(Pixel)==E.Id;
    }
   }
   Pickable+=Hit?1:0;
   if(bExpectedSkin&&Bounds.bIsValid) {
    IntegerSkinPicks+=bIntegerHit?1:0;
    const FVector2D Corners[]={Bounds.Min,FVector2D(Bounds.Min.X,Bounds.Max.Y),FVector2D(Bounds.Max.X,Bounds.Min.Y),Bounds.Max};
    for(const FVector2D Corner:Corners) {
     const FVector2D Pixel=(Corner*.94+Bounds.GetCenter()*.06).RoundToVector();
     if(C->InWorld(Pixel)&&Bounds.IsInsideOrOn(Pixel)&&Scene->Hit(Pixel)==0){++EmptySkinCorners;break;}
    }
   }
  }
  Check(Prefix+TEXT(".tight_mesh_bounds"),Visible==C->Entities.Num()&&Inside==Visible,FString::Printf(TEXT("All authored vertices enclosed by projected bounds for %d/%d real meshes"),Inside,C->Entities.Num()));
  Check(Prefix+TEXT(".visible_surface_picking"),Pickable>=Visible-2,FString::Printf(TEXT("Visible face clicks reached %d/%d fixtures; up to two may be occluded in rotated gallery"),Pickable,Visible));
  if(bGpuRequested) {
   Check(Prefix+TEXT(".active_gpu_components"),ExpectedSkin>=8&&ActiveSkin==ExpectedSkin,FString::Printf(TEXT("%d/%d live rigs use registered GPU skin render objects"),ActiveSkin,ExpectedSkin));
   Check(Prefix+TEXT(".gpu_render_positions_colors_materials"),SkinSummary.Passed(),SkinSummary.Describe());
   Check(Prefix+TEXT(".gpu_front_face_orientation"),SkinSummary.FrontFaces>0&&SkinSummary.FrontFaceMismatches==0&&SkinSummary.InvalidFaceBuffers==0,
    FString::Printf(TEXT("Checked %d nondegenerate imported LOD0 triangles against their packed tangent normals; mismatches=%d invalid_buffers=%d"),
     SkinSummary.FrontFaces,SkinSummary.FrontFaceMismatches,SkinSummary.InvalidFaceBuffers));
   Check(Prefix+TEXT(".integer_skin_picking"),IntegerSkinPicks>=ExpectedSkin-2&&EmptySkinCorners>0,
    FString::Printf(TEXT("%d/%d live rigs selected at integer pixels; %d conservative bounds had real empty corners"),IntegerSkinPicks,ExpectedSkin,EmptySkinCorners));
  }
 }
 void Animation(ARTSScene3D* Scene,ARTSController* C) {
  int Animated=0,Changed=0,Rigid=0;const float SavedTime=C->MatchTime;C->MatchTime=100.2f;
  FSkinProbeSummary SkinSummary;
  for(auto E:C->Entities) {
   auto* R=Scene->Rendered.Find(E.Id);if(!R||!R->Model||R->Model->AnimationStyle.IsEmpty())continue;
   ++Animated;
   if(bGpuRequested) {
    const TGuardValue<bool> RestPose(C->bReducedMotion,true);
    R->MoveWeight=0;
    Scene->Pose(*R,E,0.f,false);
    Scene->EnsureGeometry(*R);
    CheckActualSkin(*R,E,Scene->SkinSurface,SkinSummary);
   }
   E.AttackEventTime=100;E.Status=TEXT("gathering timber");R->MoveWeight=1;R->WalkPhase=.4f;
   Scene->Pose(*R,E,1.f,false);
   Scene->EnsureGeometry(*R);
   const TArray<FVector> Before=R->Vertices;
   if(bGpuRequested)CheckActualSkin(*R,E,Scene->SkinSurface,SkinSummary);
   R->WalkPhase=1.9f;
   Scene->Pose(*R,E,1.6f,false);
   Scene->EnsureGeometry(*R);
   if(bGpuRequested)CheckActualSkin(*R,E,Scene->SkinSurface,SkinSummary);
   double Motion=0;
   bool IsRigid=Before.Num()==R->Vertices.Num()&&!Before.IsEmpty();
   int Base=0;
   if(IsRigid)for(int I=0;I<Before.Num();++I)Motion=FMath::Max(Motion,FVector::Distance(Before[I],R->Vertices[I]));
   for(const auto& Part:R->Model->Parts) {
    if(Part.Vertices.Num()>=2&&!Part.Name.StartsWith(TEXT("bowstring"))) {
     IsRigid&=R->Vertices.IsValidIndex(Base+1)&&FMath::Abs(FVector::Distance(R->Vertices[Base],R->Vertices[Base+1])-100*FVector::Distance(Part.Vertices[0],Part.Vertices[1]))<.05;
    }
    Base+=Part.Vertices.Num();
   }
   Changed+=Motion>.1?1:0;Rigid+=IsRigid?1:0;
   R->MoveWeight=0;Scene->Pose(*R,E,Scene->GetWorld()->GetRealTimeSeconds(),false);
  }
  C->MatchTime=SavedTime;
  Check(TEXT("articulated_action_motion"),Animated>=8&&Animated==Changed&&Rigid==Animated,FString::Printf(TEXT("%d/%d body styles moved; %d retained rigid part lengths"),Changed,Animated,Rigid));
  if(bGpuRequested)Check(TEXT("gpu_ref_and_animated_pose_parity"),SkinSummary.Actors==Animated*3&&SkinSummary.Passed(),SkinSummary.Describe());
  for(int Id:{1,14}) {
   FRTSRendered3D* R=Scene->Rendered.Find(Id);
   const FRTSEntityView* E=C->FindEntity(Id);
   if(!R||!E) {Check(FString::Printf(TEXT("death_pose_%d"),Id),false,TEXT("Live fixture missing"));continue;}
   double BeforeTop=0,AfterTop=0;
   Scene->EnsureGeometry(*R);
   for(const FVector& V:R->Vertices)BeforeTop=FMath::Max(BeforeTop,V.Z);
   Scene->Entity(*E,2.f,1.6f);
   const FRTSRendered3D* Death=Scene->Rendered.Find(-Id);
   const bool bDeathMesh=Death&&Death->Component()&&!Death->Skin.IsValid();
   if(bDeathMesh) {
    Scene->EnsureGeometry(*Death);
    for(const FVector& V:Death->Vertices)AfterTop=FMath::Max(AfterTop,V.Z);
   }
   Check(FString::Printf(TEXT("death_pose_%d"),Id),bDeathMesh&&AfterTop>0&&AfterTop<BeforeTop*.8,
    FString::Printf(TEXT("Death mesh height %.2f to %.2f cm; procedural loss-state component=%d"),BeforeTop,AfterTop,bDeathMesh));
   if(Death&&Death->Component())Death->Component()->DestroyComponent();
   Scene->Rendered.Remove(-Id);
  }
 }
 void CombatPresentation(ARTSScene3D* Scene,ARTSController* C) {
  auto* R=Scene->Rendered.Find(14);const auto* Original=C->FindEntity(14);if(!R||!Original){Check(TEXT("combat_fixture"),false,TEXT("Bowman fixture missing"));return;}
  FRTSEntityView E=*Original;const float Time=C->MatchTime;C->MatchTime=100.1f;
  E.Target=15;E.AttackEventTime=100.05f;E.bAttacking=false;R->LastAttackEvent=99;R->WasAttacking=false;R->MoveWeight=0;
  Scene->Shots.Empty();Scene->Entity(E,5.f);const bool EventHeld=R->PoseState==TEXT("attack");
  Scene->EnsureGeometry(*R);
  const TArray<FVector> First=R->Vertices;
  Scene->Entity(E,5.1f);
  Scene->EnsureGeometry(*R);
  double Motion=0;
  if(First.Num()==R->Vertices.Num())for(int I=0;I<First.Num();++I)Motion=FMath::Max(Motion,FVector::Distance(First[I],R->Vertices[I]));
  if(bGpuRequested) {
   FSkinProbeSummary AttackSkin;
   CheckActualSkin(*R,E,Scene->SkinSurface,AttackSkin);
   Check(TEXT("gpu_drawn_bow_pose_parity"),AttackSkin.Passed(),AttackSkin.Describe());
  }
  Check(TEXT("attack_event_survives_missing_pulse"),EventHeld&&R->PoseState==TEXT("attack")&&Motion>.1,FString::Printf(TEXT("Replicated event without attack flag: state=%s, vertex motion %.3f cm"),*R->PoseState,Motion));
  Check(TEXT("one_projectile_per_strike"),Scene->Shots.Num()==1,FString::Printf(TEXT("Repeated snapshot spawned %d arrows"),Scene->Shots.Num()));
  Scene->DrawProjectiles(5.1f);const auto* Visible=Scene->Projectiles->GetProcMeshSection(0);
  Check(TEXT("real_projectile_mesh"),Visible&&Visible->ProcIndexBuffer.Num()>0,TEXT("Visible strike generated a lit triangle mesh"));
  C->Fog.Init(0,64*64);Scene->DrawProjectiles(5.1f);const auto* Hidden=Scene->Projectiles->GetProcMeshSection(0);
  Check(TEXT("projectile_respects_fog"),!Hidden||Hidden->ProcIndexBuffer.IsEmpty(),TEXT("Projectile contributes no geometry in unexplored fog"));C->Fog.Init(2,64*64);
  Scene->DrawProjectiles(6.f);Check(TEXT("projectile_expiry"),Scene->Shots.IsEmpty()&&!Scene->Projectiles->IsVisible(),TEXT("Transient mesh expired after bounded flight"));
  R->AttackStarted=7.f;Scene->Pose(*R,E,7.f,false);
  Scene->EnsureGeometry(*R);
  double StringError=1.e9;int Base=0;const FVector Draw=R->PosedSockets.FindRef(TEXT("draw_hand"))*100;
  for(const auto& Part:R->Model->Parts) {
   if(Part.Name.StartsWith(TEXT("bowstring")))for(int I=0;I<Part.Vertices.Num();++I) {
    if(R->Vertices.IsValidIndex(Base+I))StringError=FMath::Min(StringError,FVector::Distance(R->Vertices[Base+I],Draw));
   }
   Base+=Part.Vertices.Num();
  }
  Check(TEXT("bowstring_follows_draw_hand"),R->Model->Sockets.Num()>=4&&StringError<4,FString::Printf(TEXT("Drawn string within %.3f cm of animated hand socket"),StringError));
  C->MatchTime=Time;R->LastAttackEvent=-100;R->AttackStarted=-100;R->WasAttacking=false;
  Scene->Entity(*Original,Scene->GetWorld()->GetRealTimeSeconds());
 }
 bool CheckGpuDrawReadiness(ARTSScene3D* Scene,ARTSController* C) {
  if(!bGpuRequested)return true;
  int32 Expected=0,OnScreen=0;
  FString Missing;
  for(const FRTSEntityView& Entity:C->Entities) {
   const FRTSRendered3D* R=Scene->Rendered.Find(Entity.Id);
   if(!R||!R->Model||R->Model->AnimationStyle.IsEmpty())continue;
   ++Expected;
   const UPoseableMeshComponent* Skin=R->Skin.Get();
   const bool bUpdated=HasActiveGpuSkin(Skin)&&Skin->MeshObject->bHasBeenUpdatedAtLeastOnce;
   const float LastDraw=Skin?Skin->GetLastRenderTimeOnScreen():-1000.f;
   if(bUpdated&&LastDraw>=DrawAfterWorldSeconds)++OnScreen;
   else if(Missing.IsEmpty())Missing=FString::Printf(TEXT("%s updated=%d last_draw=%.4f"),*R->Model->Id,bUpdated,LastDraw);
  }
  const bool bReady=Expected>=8&&OnScreen==Expected;
  if(!bReady&&FPlatformTime::Seconds()-DrawWaitStarted<3.&&Frame-ChangedAt<180)return false;
  const FString Prefix=FRTSCatalog::Get().Factions[FactionIndex].Id;
  Check(Prefix+TEXT(".gpu_draw_ready"),bReady,
   FString::Printf(TEXT("%d/%d GPU rigs submitted dynamic data and recorded on-screen draws since t=%.4f. %s"),OnScreen,Expected,DrawAfterWorldSeconds,*Missing));
  return true;
 }
public:
 void Run(ARTSScene3D* Scene,ARTSController* C) {
  if(Done||!C->Me())return;++Frame;
  if(Stage==5)PinTerrainCamera(C);else if(Stage==4)PinSelectionCamera(C);else PinFixtureCamera(C);
  if(FPlatformTime::Seconds()-Began>90){Check(TEXT("timeout"),false,TEXT("Presentation test did not finish within 90 seconds"));Finish(Scene);return;}
  if(Stage==0) {
   for(TActorIterator<ARTSWorld> It(C->GetWorld());It;++It){It->SetActorTickEnabled(false);}
   C->bMenu=false;C->bPauseMenu=false;C->bHelp=false;C->Phase=1;C->MatchTime=0;C->LocalSlot=0;Catalogue(Scene,C);Fixture(C);return;
  }
  if(Frame-ChangedAt<3)return;
  if(Stage==5) {
   CheckTerrainSelection(Scene,C);
   if(++TerrainView==2)Finish(Scene);else TerrainSelectionFixture(C);
   return;
  }
  if(Stage==4) {
   CheckSelection(Scene,C);
   if(++SelectionView==24)TerrainSelectionFixture(C);else SelectionFixture(C);
   return;
  }
  if(Stage==1) {
   Geometry(Scene,C);if(FactionIndex==0){Animation(Scene,C);CombatPresentation(Scene,C);}
   DrawWaitStarted=FPlatformTime::Seconds();
   DrawAfterWorldSeconds=C->GetWorld()->GetTimeSeconds();
   Stage=2;ChangedAt=Frame;return;
  }
  if(Stage==2) {
   if(!CheckGpuDrawReadiness(Scene,C))return;
   const FString Name=(bGpuRequested?TEXT("scene3d-gpu-"):TEXT("scene3d-"))+FRTSCatalog::Get().Factions[FactionIndex].Id+TEXT(".png");
   FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name),false,false);Shots.Add(MakeShared<FJsonValueString>(Name));Stage=3;ChangedAt=Frame;return;
  }
  if(++FactionIndex>=FRTSCatalog::Get().Factions.Num()){SelectionFixture(C);return;}Fixture(C);
 }
};

void RunRTSSceneProbe(ARTSScene3D* Scene,ARTSController* Controller) {
 if(!FParse::Param(FCommandLine::Get(),TEXT("RTSV09SceneTest")))return;
 static FRTSSceneProbe Probe;Probe.Run(Scene,Controller);
}
