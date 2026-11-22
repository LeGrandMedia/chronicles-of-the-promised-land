#include "RTSScene3D.h"
#include "RTSMapTests.h"
#include "RTSController.h"
#include "RTSVisualAssets.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/DirectionalLightComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTSSceneTests.h"
#include "RTSRenderStressTests.h"
#include "RTSDeathStressTests.h"
#include "RTSWallJunctionTests.h"
#include "RTSDistrictTests.h"
#include "HAL/PlatformTime.h"

namespace {
 constexpr double ContextPickPadding=5.;
 constexpr int32 TerrainGrid=64,TerrainFacesPerCell=4;
 constexpr double TerrainCellCm=250.;
 struct FFacetCorner {
  FIntVector Position,Normal;
  bool operator==(const FFacetCorner& Other)const{return Position==Other.Position&&Normal==Other.Normal;}
  friend uint32 GetTypeHash(const FFacetCorner& K){return HashCombine(GetTypeHash(K.Position),GetTypeHash(K.Normal));}
 };
 bool ProfileScene() {static const bool Enabled=RTSDeathStressEnabled()||FParse::Param(FCommandLine::Get(),TEXT("RTSV09RenderStress"));return Enabled;}
 bool UseGPUSkinning() {static const bool Enabled=!FParse::Param(FCommandLine::Get(),TEXT("RTSProceduralMeshes"));return Enabled;}
 struct FSceneTimer {
  double& Value;double Started=0;
  explicit FSceneTimer(double& In,bool bMeasure=true):Value(In){if(bMeasure&&ProfileScene())Started=FPlatformTime::Seconds();}
  ~FSceneTimer(){if(Started>0)Value+=(FPlatformTime::Seconds()-Started)*1000.;}
 };
 FVector Vec(const TArray<TSharedPtr<FJsonValue>>& A) {return A.Num()>=3?FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber()):FVector::ZeroVector;}
 TSharedPtr<FJsonObject> ReadJson(const FString& Path) {FString Text;TSharedPtr<FJsonObject> Obj;if(FFileHelper::LoadFileToString(Text,*Path))FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Obj);return Obj;}
 // A ray test against the rendered, animated geometry, not the old sprite rectangle.
 bool RayTriangle(const FVector& O,const FVector& D,const FVector& A,const FVector& B,const FVector& C,double& Distance) {
  const FVector E1=B-A,E2=C-A,P=FVector::CrossProduct(D,E2);const double Det=FVector::DotProduct(E1,P);
  if(FMath::Abs(Det)<1.e-8)return false;const double Inv=1/Det;const FVector T=O-A;
  const double U=FVector::DotProduct(T,P)*Inv;if(U<0||U>1)return false;const FVector Q=FVector::CrossProduct(T,E1);
  const double V=FVector::DotProduct(D,Q)*Inv;if(V<0||U+V>1)return false;Distance=FVector::DotProduct(E2,Q)*Inv;return Distance>=0;
 }
 bool TriangleOverlapsBox(FVector2D A,FVector2D B,FVector2D C,const FBox2D& Box) {
  FBox2D Triangle(A,A);Triangle+=B;Triangle+=C;
  if(!Box.Intersect(Triangle))return false;
  const FVector2D Center=Box.GetCenter(),Extent=Box.GetSize()*.5;
  for(const FVector2D Edge:{B-A,C-B,A-C}) {
   const FVector2D Normal(-Edge.Y,Edge.X);
   const double Radius=FMath::Abs(Normal.X)*Extent.X+FMath::Abs(Normal.Y)*Extent.Y;
   const double PA=FVector2D::DotProduct(A-Center,Normal),PB=FVector2D::DotProduct(B-Center,Normal),PC=FVector2D::DotProduct(C-Center,Normal);
   if(FMath::Min3(PA,PB,PC)>Radius||FMath::Max3(PA,PB,PC)<-Radius)return false;
  }
  return true;
 }
}

UMeshComponent* FRTSRendered3D::Component() const {return Skin.IsValid()?static_cast<UMeshComponent*>(Skin.Get()):static_cast<UMeshComponent*>(Mesh.Get());}

ARTSScene3D::ARTSScene3D() {
 PrimaryActorTick.bCanEverTick=false;bReplicates=false;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("IsometricCamera"));Camera->SetupAttachment(RootComponent);
 Camera->ProjectionMode=ECameraProjectionMode::Orthographic;Camera->bConstrainAspectRatio=false;
 Camera->OrthoNearClipPlane=10;Camera->OrthoFarClipPlane=80000;
 Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;Camera->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
 Camera->PostProcessSettings.bOverride_AutoExposureBias=true;Camera->PostProcessSettings.AutoExposureBias=0;
 Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
 Camera->PostProcessSettings.bOverride_BloomIntensity=true;Camera->PostProcessSettings.BloomIntensity=.15f;
 Camera->PostProcessSettings.bOverride_MotionBlurAmount=true;Camera->PostProcessSettings.MotionBlurAmount=0;
 Ground=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Terrain"));Ground->SetupAttachment(RootComponent);Ground->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Ground->SetCastShadow(false);
 Projectiles=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CombatProjectiles"));Projectiles->SetupAttachment(RootComponent);
 Projectiles->SetCollisionEnabled(ECollisionEnabled::NoCollision);Projectiles->SetCastShadow(false);
 WallJoints=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WallJunctions"));WallJoints->SetupAttachment(RootComponent);
 WallJoints->SetCollisionEnabled(ECollisionEnabled::NoCollision);WallJoints->SetCastShadow(true);
 auto* Sun=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("WarmSun"));Sun->SetupAttachment(RootComponent);
 Sun->SetRelativeRotation(FRotator(-52,-65,0));Sun->SetIntensity(4.f);Sun->SetLightColor(FLinearColor(1.f,.88f,.67f));
 // The orthographic camera sits 300 m from the map. Cascades must reach the
 // visible ground (roughly 255-330 m away at default zoom) to ground the models.
 Sun->SetMobility(EComponentMobility::Movable);Sun->DynamicShadowDistanceMovableLight=45000;
 auto* Fill=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("CoolFill"));Fill->SetupAttachment(RootComponent);
 Fill->SetRelativeRotation(FRotator(-42,130,0));Fill->SetIntensity(1.4f);Fill->SetLightColor(FLinearColor(.62f,.76f,1.f));Fill->SetCastShadows(false);
 auto* Rim=CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SoftFill"));Rim->SetupAttachment(RootComponent);
 Rim->SetRelativeRotation(FRotator(-65,15,0));Rim->SetIntensity(.5f);Rim->SetCastShadows(false);
}

void ARTSScene3D::BeginPlay() {
 Super::BeginPlay();
 Surface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_Faceted.M_Faceted"));
 if(UseGPUSkinning())SkinSurface=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Materials/M_SkinnedFaceted.M_SkinnedFaceted"));
 if(!Surface) {UE_LOG(LogTemp,Error,TEXT("RTS_3D material missing; cook Tools/GenerateMaterials.py output."));return;}
 Ground->SetMaterial(0,Surface);
 Projectiles->SetMaterial(0,Surface);
 WallJoints->SetMaterial(0,Surface);
 const auto Manifest=ReadJson(FPaths::ProjectContentDir()/TEXT("Data/Models/models.json"));
 if(!Manifest.IsValid()){UE_LOG(LogTemp,Error,TEXT("RTS_3D model manifest missing"));return;}
 for(const TCHAR* Field:{TEXT("models"),TEXT("variants")}) {
  const TSharedPtr<FJsonObject>* Map=nullptr;
  if(Manifest->TryGetObjectField(Field,Map))for(const auto& KV:(*Map)->Values) {
   FString Path;if(KV.Value->TryGetString(Path))ModelPaths.Add(KV.Key,Path);
  }
 }
 bReady=!ModelPaths.IsEmpty();UE_LOG(LogTemp,Display,TEXT("RTS_3D_READY models=%d"),ModelPaths.Num());
}

TSharedPtr<FRTSModel3D> ARTSScene3D::Model(const FRTSEntityView& E,FString& Key) {
 const auto& Def=FRTSCatalog::Get().Def(E.Def);
 const FString& Id=Def.Id;
 const FString Faction=PC.IsValid()?PC->FactionFor(E.Owner):FString();
 Key=Id;
 if(ModelPaths.Contains(Id+TEXT(":")+Faction))Key=Id+TEXT(":")+Faction;
 if(E.bGate){if(ModelPaths.Contains(Id+TEXT(":gate:" )+Faction))Key=Id+TEXT(":gate:")+Faction;else if(ModelPaths.Contains(Id+TEXT(":gate")))Key=Id+TEXT(":gate");}
 if(auto* Existing=Models.Find(Key))return *Existing;
 const FString* File=ModelPaths.Find(Key);if(!File)return nullptr;
 // Manifest paths stay under Models; assets cannot reach outside project content.
 if(File->Contains(TEXT(".."))||FPaths::IsRelative(*File)==false)return nullptr;
 auto Obj=ReadJson(FPaths::ProjectContentDir()/TEXT("Data/Models") / *File);if(!Obj.IsValid())return nullptr;
 auto M=MakeShared<FRTSModel3D>();M->Id=Key;
 const TSharedPtr<FJsonObject>* Animation=nullptr;
 if(Obj->TryGetObjectField(TEXT("animation"),Animation)) {
  (*Animation)->TryGetStringField(TEXT("style"),M->AnimationStyle);
  (*Animation)->TryGetStringField(TEXT("equipment"),M->Equipment);
  const TSharedPtr<FJsonObject>* Sockets=nullptr;
  if((*Animation)->TryGetObjectField(TEXT("sockets"),Sockets))for(const auto& KV:(*Sockets)->Values) {
   const auto S=KV.Value->AsObject();if(!S)continue;FRTSModelSocket Socket;const TArray<TSharedPtr<FJsonValue>>* P=nullptr;
   if(S->TryGetArrayField(TEXT("position"),P))Socket.Position=Vec(*P);S->TryGetStringField(TEXT("bone"),Socket.Bone);M->Sockets.Add(KV.Key,Socket);
  }
 }
 const TSharedPtr<FJsonObject>* Rig=nullptr;
 if(Obj->TryGetObjectField(TEXT("rig"),Rig))for(const auto& KV:(*Rig)->Values) {
  const auto R=KV.Value->AsObject();if(!R)continue;FRTSModelRig Bone;const TArray<TSharedPtr<FJsonValue>>* P=nullptr;
  if(R->TryGetArrayField(TEXT("pivot"),P))Bone.Pivot=Vec(*P);R->TryGetStringField(TEXT("parent"),Bone.Parent);M->Rig.Add(KV.Key,Bone);
 }
 const TArray<TSharedPtr<FJsonValue>>* Parts=nullptr;if(!Obj->TryGetArrayField(TEXT("parts"),Parts))return nullptr;
 for(const auto& Value:*Parts) {
  auto Part=Value->AsObject();if(!Part)continue;FRTSModelPart P;Part->TryGetStringField(TEXT("name"),P.Name);Part->TryGetStringField(TEXT("bone"),P.Bone);Part->TryGetBoolField(TEXT("team"),P.Team);
  const TArray<TSharedPtr<FJsonValue>> *Vertices=nullptr,*Indices=nullptr,*Color=nullptr;
  if(!Part->TryGetArrayField(TEXT("vertices"),Vertices)||!Part->TryGetArrayField(TEXT("triangles"),Indices))continue;
  for(const auto& V:*Vertices){const FVector XYZ=Vec(V->AsArray());if(XYZ.ContainsNaN()||XYZ.GetAbsMax()>1000)return nullptr;P.Vertices.Add(XYZ);P.Bounds+=XYZ;}
  M->Bounds+=P.Bounds;
  for(const auto& I:*Indices){const int32 Index=I->AsNumber();if(!P.Vertices.IsValidIndex(Index))return nullptr;P.Triangles.Add(Index);}
  if(P.Triangles.Num()%3!=0)return nullptr;
  // Share corners only within the same flat surface. This preserves hard edges
  // while avoiding three separate copies of every triangle's common vertices.
  TArray<FVector> Corners,Normals;TArray<int32> CornerIndices;TMap<FFacetCorner,int32> Shared;
  Corners.Reserve(P.Vertices.Num()*2);Normals.Reserve(P.Vertices.Num()*2);CornerIndices.Reserve(P.Triangles.Num());
  for(int I=0;I<P.Triangles.Num();I+=3) {
   const FVector A=P.Vertices[P.Triangles[I]],B=P.Vertices[P.Triangles[I+1]],C=P.Vertices[P.Triangles[I+2]];
   const FVector N=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
   for(int J=0;J<3;++J) {
    const FVector V=P.Vertices[P.Triangles[I+J]];
    const FFacetCorner K{FIntVector(FMath::RoundToInt(V.X*1000000),FMath::RoundToInt(V.Y*1000000),FMath::RoundToInt(V.Z*1000000)),FIntVector(FMath::RoundToInt(N.X*100000),FMath::RoundToInt(N.Y*100000),FMath::RoundToInt(N.Z*100000))};
    if(const int32* Found=Shared.Find(K))CornerIndices.Add(*Found);
    else {const int32 Index=Corners.Add(V);Normals.Add(N);Shared.Add(K,Index);CornerIndices.Add(Index);}
   }
  }
  P.Vertices=MoveTemp(Corners);P.Normals=MoveTemp(Normals);P.Triangles=MoveTemp(CornerIndices);
  if(Part->TryGetArrayField(TEXT("color"),Color)&&Color->Num()>=3)P.Color=FLinearColor((*Color)[0]->AsNumber(),(*Color)[1]->AsNumber(),(*Color)[2]->AsNumber(),Color->Num()>3?(*Color)[3]->AsNumber():1);
  M->Triangles+=P.Triangles.Num()/3;M->VertexCount+=P.Vertices.Num();M->Parts.Add(MoveTemp(P));
 }
 if(M->Parts.IsEmpty())return nullptr;
 if(UseGPUSkinning()&&SkinSurface&&!M->AnimationStyle.IsEmpty()) {
  const FString AssetName=TEXT("SK_")+FPaths::GetBaseFilename(*File).Replace(TEXT("@"),TEXT("__"));
  const FString AssetPath=TEXT("/Game/Art/Skeletal/")+AssetName+TEXT(".")+AssetName;
  if(auto* Asset=LoadObject<USkeletalMesh>(nullptr,*AssetPath)) {
   M->SkeletalMesh=Asset;SkinAssets.Add(Asset);const auto& RigBones=Asset->GetRefSkeleton();
   for(int I=0;I<RigBones.GetNum();++I) {
    const int Parent=RigBones.GetParentIndex(I);M->BoneNames.Add(RigBones.GetBoneName(I).ToString());M->BoneParents.Add(Parent);
    const FTransform Local=RigBones.GetRefBonePose()[I];M->ReferenceBones.Add(Parent>=0?Local*M->ReferenceBones[Parent]:Local);
   }
  } else UE_LOG(LogTemp,Warning,TEXT("RTS_GPU_SKIN_FALLBACK %s"),*AssetPath);
 }
 Models.Add(Key,M);return M;
}

UMaterialInterface* ARTSScene3D::SkinMaterial(const FRTSEntityView& E) {
 const int Key=(E.Owner+2)*4+(E.bRemembered?2:0)+(E.bStealthed?1:0);
 if(auto* Existing=SkinMaterials.Find(Key))return *Existing;
 auto* Material=UMaterialInstanceDynamic::Create(SkinSurface,this);
 Material->SetVectorParameterValue(TEXT("TeamColor"),E.Owner>=0?RTS::PlayerColor(E.Owner):FLinearColor::White);
 Material->SetScalarParameterValue(TEXT("TeamWeight"),E.Owner>=0?.68f:0.f);
 Material->SetScalarParameterValue(TEXT("StealthAmount"),!E.bRemembered&&E.bStealthed?.4f:0.f);
 Material->SetScalarParameterValue(TEXT("VisibilityTint"),E.bRemembered?.38f:1.f);
 SkinMaterials.Add(Key,Material);return Material;
}

FVector2D ARTSScene3D::Project(FVector V) const {
 if(!PC.IsValid())return FVector2D::ZeroVector;
 const float U=FMath::Max(.25f,FMath::Min(PC->ViewSize.X/1600.f,PC->ViewSize.Y/900.f));
 return PC->WorldToScreen(FVector2D(V.X,V.Y))-FVector2D(0,V.Z*12.727922f*.8660254f*PC->Zoom*U);
}

void ARTSScene3D::Terrain() {
 GroundVertices.Empty();GroundTriangles.Empty();GroundNormals.Empty();GroundColors.Empty();GroundMap=PC->Map;LastFog=PC->Fog;
 GroundTopZ=0.;
 for(int Y=0;Y<TerrainGrid;++Y)for(int X=0;X<TerrainGrid;++X) {
  const FVector2D Center(X*2.5+1.25,Y*2.5+1.25);const bool Water=RTS::WaterAt(Center,GroundMap),Rock=RTS::RockAt(Center,GroundMap);
  const float Noise=((X*73+Y*147+X*Y*3)%19)/19.f;
  const int32 Cell=Y*TerrainGrid+X;
  const uint8 Fog=LastFog.IsValidIndex(Cell)?LastFog[Cell]:0;
  // Broad meadow/earth patches read as terrain at RTS scale; independent strong
  // tile noise made the old ground look like a dark checkerboard.
  FLinearColor C=RTS::TerrainColor(Center,GroundMap)+FLinearColor(Noise*.012f,Noise*.015f,Noise*.008f,0);
  if(Fog==0)C=FLinearColor(.025f,.038f,.043f);else if(Fog==1)C*=.30f;C.A=1;
  const float Z=Water?-10:0;
  const auto Corner=[&](int32 CX,int32 CY){return FVector(CX*TerrainCellCm,CY*TerrainCellCm,Fog>0&&GroundMap>=3?RTS::TerrainHeight(FVector2D(CX*2.5,CY*2.5),GroundMap)*100+Z:Z);};
  const FVector A=Corner(X,Y),B=Corner(X+1,Y),D=Corner(X,Y+1),E=Corner(X+1,Y+1);
  // Raised faceted rock faces are actual geometry matching the blocked map cells.
  const FVector Peak(Center.X*100,Center.Y*100,GroundMap>=3?(Fog>0?RTS::TerrainHeight(Center,GroundMap)*100+Z:Z):(Rock&&Fog>0?70+Noise*125:Z));
  const FVector Corners[]={A,B,E,D};
  GroundTopZ=FMath::Max(GroundTopZ,Peak.Z);
  for(int K=0;K<TerrainFacesPerCell;++K) {
   const FVector From=Corners[K],To=Corners[(K+1)%TerrainFacesPerCell];
   GroundTopZ=FMath::Max(GroundTopZ,From.Z);
   const int Base=GroundVertices.Num();const FVector N=FVector::CrossProduct(To-From,Peak-From).GetSafeNormal();
   // Unreal's front-face winding is clockwise. Keep the authored upward normal
   // while reversing indices so a two-sided material does not light the back.
   GroundVertices.Append({From,To,Peak});GroundTriangles.Append({Base,Base+2,Base+1});
   for(int J=0;J<3;++J){GroundNormals.Add(N);GroundColors.Add(C);}
  }
 }
 Ground->CreateMeshSection_LinearColor(0,GroundVertices,GroundTriangles,GroundNormals,{},GroundColors,{},false,false);
 Ground->SetMaterial(0,Surface);
}

void ARTSScene3D::Pose(FRTSRendered3D& R,const FRTSEntityView& E,float Now,bool Rebuild,float DeathAge) {
 if(!R.Component()||!R.Model)return;
 FSceneTimer PoseTimer(Profile.PoseMs);
 if(ProfileScene()){
  ++Profile.PoseCalls;
  if(!R.Skin.IsValid())Profile.PosedVertices+=R.Model->VertexCount;
  if(DeathAge>=0){++Profile.DeathPoseCalls;Profile.DeathPosedVertices+=R.Model->VertexCount;}
 }
 const auto& Def=FRTSCatalog::Get().Def(E.Def);const auto* Target=PC->FindEntity(E.Target);const auto* TargetDef=Target?&FRTSCatalog::Get().Def(Target->Def):nullptr;
 const FString Work=FRTSVisualAssets::WorkState(E,TargetDef);
 const FVector2D Pos=PC->VisualPosition(E);const bool Moving=R.MoveWeight>.025f&&DeathAge<0;
 const bool Reduced=PC->bReducedMotion||E.bRemembered||DeathAge>=0;
 const float Cycle=Now*8.f+E.Id*.73f,Walk=Reduced?0:FMath::Sin(R.WalkPhase)*R.MoveWeight;
 const float AttackAge=Now-R.AttackStarted;
 const bool Attacking=AttackAge>=0&&AttackAge<.55f;
 const bool Bow=R.Model->Equipment==TEXT("bow")||R.Model->Equipment==TEXT("longbow");
 // The replicated strike event is authoritative; local time keeps recovery smooth
 // between snapshots, including when the one-step bAttacking pulse was missed.
 const float Attack=Reduced||!Attacking?0:Bow?1-FMath::Clamp(AttackAge/.22f,0.f,1.f):FMath::Sin(AttackAge/.55f*PI);
 const float WorkSwing=!Moving&&!Work.IsEmpty()&&!Reduced?FMath::Sin(Now*7.f+E.Id):0;
 const float Breath=Reduced?0:FMath::Sin(Now*2.3f+E.Id)*.012f;
 const FString State=DeathAge>=0?(Def.bBuilding?TEXT("destroy"):TEXT("death")):Attacking?TEXT("attack"):Moving?TEXT("move"):!Work.IsEmpty()?Work:TEXT("idle");
 if(State!=R.PoseState){R.PoseState=State;R.StateStarted=Now;}
 R.PoseElapsed=DeathAge>=0?DeathAge:State==TEXT("attack")?AttackAge:Now-R.StateStarted;
 TMap<FString,FQuat> Rotations;
 Rotations.Add(TEXT("leg_l"),FQuat(FVector::YAxisVector,Walk*.55f));Rotations.Add(TEXT("leg_r"),FQuat(FVector::YAxisVector,-Walk*.55f));
 Rotations.Add(TEXT("shin_l"),FQuat(FVector::YAxisVector,.55f*FMath::Max(0.f,-Walk)));
 Rotations.Add(TEXT("shin_r"),FQuat(FVector::YAxisVector,.55f*FMath::Max(0.f,Walk)));
 Rotations.Add(TEXT("arm_l"),FQuat(FVector::YAxisVector,-Walk*.38f-WorkSwing*.5f));Rotations.Add(TEXT("arm_r"),FQuat(FVector::YAxisVector,Walk*.38f-Attack*1.3f-WorkSwing*.9f));
 Rotations.Add(TEXT("forearm_l"),FQuat(FVector::YAxisVector,-.12f*FMath::Abs(Walk)-.16f*WorkSwing));
 Rotations.Add(TEXT("forearm_r"),FQuat(FVector::YAxisVector,-.12f*FMath::Abs(Walk)-.35f*Attack-.24f*WorkSwing));
 Rotations.Add(TEXT("head"),FQuat(FVector::ZAxisVector,Moving||Reduced?0:FMath::Sin(Now*.7f+E.Id)*.035f));
 Rotations.Add(TEXT("tail"),FQuat(FVector::ZAxisVector,Reduced?0:FMath::Sin(Cycle*.7f)*.17f));
 // Wheel phase follows distance travelled, so stopping never resets its spokes.
 Rotations.Add(TEXT("wheel_l"),FQuat(FVector::YAxisVector,R.TravelDistance/.54f));Rotations.Add(TEXT("wheel_r"),FQuat(FVector::YAxisVector,R.TravelDistance/.54f));
 Rotations.Add(TEXT("torso"),FQuat(FVector::YAxisVector,Attack*.13f+WorkSwing*.08f));
 const FString& Style=R.Model->AnimationStyle;
 if(Style==TEXT("ranged")) {
  if(R.Model->Equipment==TEXT("bow")||R.Model->Equipment==TEXT("longbow")) {
   Rotations[TEXT("arm_l")]=FQuat(FVector::YAxisVector,-Walk*.12f-Attack*.07f);
   Rotations[TEXT("arm_r")]=FQuat(FVector::YAxisVector,Walk*.12f)*FQuat(FVector::ZAxisVector,-Attack*.09f);
   Rotations[TEXT("forearm_l")]=FQuat(FVector::YAxisVector,-Attack*.04f);
   Rotations[TEXT("forearm_r")]=FQuat(FVector::ZAxisVector,-Attack*.24f);
  } else {
   Rotations[TEXT("arm_r")]=FQuat(FVector::YAxisVector,Walk*.3f-Attack*.85f);
   Rotations[TEXT("forearm_r")]=FQuat(FVector::XAxisVector,Attack*1.35f);
  }
 } else if(Style==TEXT("spear")) {
  Rotations[TEXT("arm_r")]=FQuat(FVector::YAxisVector,Walk*.25f-Attack*.42f);
  Rotations[TEXT("forearm_r")]=FQuat(FVector::YAxisVector,-Attack*.30f);
 } else if(Style==TEXT("staff")) {
  Rotations[TEXT("arm_r")]=FQuat(FVector::YAxisVector,Walk*.20f-Attack*.28f-WorkSwing*.16f);
  Rotations[TEXT("forearm_r")]=FQuat(FVector::YAxisVector,-Attack*.2f-WorkSwing*.12f);
 } else if(Style==TEXT("concubine")) {
  const float Sway=Reduced?0:FMath::Sin(Now*2.2f+E.Id)*(1-R.MoveWeight);
  Rotations[TEXT("torso")]=FQuat(FVector::XAxisVector,Sway*.045f)*FQuat(FVector::ZAxisVector,Sway*.065f);
  Rotations[TEXT("arm_r")]=FQuat(FVector::YAxisVector,Walk*.22f-Sway*.12f);
  Rotations[TEXT("forearm_r")]=FQuat(FVector::YAxisVector,-.12f*FMath::Abs(Sway));
 }
 // Animal lower legs bend at their own knees; front/back diagonal pairs share stance.
 for(const TCHAR* Side:{TEXT("l"),TEXT("r")}) {
  const float Phase=FCString::Strcmp(Side,TEXT("l"))==0?Walk:-Walk;
  Rotations.Add(FString(TEXT("shin_front_"))+Side,FQuat(FVector::YAxisVector,-.45f*FMath::Max(0.f,Phase)));
  Rotations.Add(FString(TEXT("shin_back_"))+Side,FQuat(FVector::YAxisVector,.45f*FMath::Max(0.f,-Phase)));
  Rotations.Add(FString(TEXT("shin_mid_"))+Side,FQuat(FVector::YAxisVector,.4f*FMath::Max(0.f,Phase)));
 }
 if(Style==TEXT("beast")) {
  Rotations[TEXT("head")]=FQuat(FVector::YAxisVector,-Attack*.28f+WorkSwing*.15f);
  Rotations[TEXT("torso")]=FQuat(FVector::YAxisVector,Attack*.06f);
 }
 if(Style==TEXT("ship")) {
  const float Wave=Reduced?0:FMath::Sin(Now*1.7f+E.Id);
  Rotations.Add(TEXT("hull"),FQuat(FVector::XAxisVector,Wave*.025f)*FQuat(FVector::YAxisVector,Breath));
  Rotations.Add(TEXT("sail"),FQuat(FVector::YAxisVector,Wave*.04f));
  Rotations.Add(TEXT("oar_l"),FQuat(FVector::ZAxisVector,Walk*.25f));
  Rotations.Add(TEXT("oar_r"),FQuat(FVector::ZAxisVector,-Walk*.25f));
 }
 for(const auto& KV:R.Model->Rig)if(!Rotations.Contains(KV.Key)) {
  // Match longer suffixes first: forearm_r also ends in arm_r.
  const TCHAR* Names[]={TEXT("shin_front_l"),TEXT("shin_front_r"),TEXT("shin_back_l"),TEXT("shin_back_r"),TEXT("shin_mid_l"),TEXT("shin_mid_r"),TEXT("forearm_l"),TEXT("forearm_r"),TEXT("shin_l"),TEXT("shin_r"),TEXT("leg_l"),TEXT("leg_r"),TEXT("arm_l"),TEXT("arm_r"),TEXT("head"),TEXT("tail"),TEXT("wheel_l"),TEXT("wheel_r"),TEXT("torso")};
  for(const TCHAR* Name:Names)if(KV.Key.EndsWith(Name)){const FQuat Q=Rotations.FindRef(Name);Rotations.Add(KV.Key,Q);break;}
  if(KV.Key.Contains(TEXT("leg_mid")))Rotations.Add(KV.Key,FQuat(FVector::YAxisVector,(KV.Key.EndsWith(TEXT("_l"))?-Walk:Walk)*.4f));
  if(KV.Key.StartsWith(TEXT("driver_")))Rotations.Add(KV.Key,FQuat::Identity);
 }
 // Resolve each hierarchy once per pose; many clothing/weapon parts share a joint.
 TMap<FString,FTransform> Joints;
 const auto JointFor=[&](const FString& Name) {
  if(const auto* Cached=Joints.Find(Name))return *Cached;
  FTransform Transform=FTransform::Identity;FString Bone=Name;
  for(int Depth=0;Depth<8&&!Bone.IsEmpty()&&Bone!=TEXT("root");++Depth) {
   const auto* Joint=R.Model->Rig.Find(Bone);const FVector Pivot=Joint?Joint->Pivot:FVector::ZeroVector;
   if(const auto* Q=Rotations.Find(Bone))Transform=Transform*FTransform(*Q,Pivot-Q->RotateVector(Pivot));
   Bone=Joint?Joint->Parent:FString();
  }
  Joints.Add(Name,Transform);return Transform;
 };
 R.PosedSockets.Reset();
 for(const auto& KV:R.Model->Sockets)R.PosedSockets.Add(KV.Key,JointFor(KV.Value.Bone).TransformPosition(KV.Value.Position));
 FVector StringOffset=FVector::ZeroVector,StringLow=FVector::ZeroVector,StringAxis=FVector::ZeroVector;
 if(Bow&&R.Model->Sockets.Contains(TEXT("bowstring_lower"))&&R.Model->Sockets.Contains(TEXT("bowstring_upper"))&&R.PosedSockets.Contains(TEXT("draw_hand"))) {
  StringLow=R.Model->Sockets.FindChecked(TEXT("bowstring_lower")).Position;
  StringAxis=R.Model->Sockets.FindChecked(TEXT("bowstring_upper")).Position-StringLow;
  const FVector Middle=(R.PosedSockets.FindChecked(TEXT("bowstring_lower"))+R.PosedSockets.FindChecked(TEXT("bowstring_upper")))*.5;
  StringOffset=(R.PosedSockets.FindChecked(TEXT("draw_hand"))-Middle)*Attack;
 }
 const bool ColorDirty=Rebuild||R.Owner!=E.Owner||R.Remembered!=E.bRemembered||R.Stealthed!=E.bStealthed;
 if(R.Skin.IsValid()) {
  // Preserve the authored pose for exact, lazy CPU picking while sending only a
  // small bone array to Unreal's GPU skinning path each animation update.
  R.Bob=Def.Kind==TEXT("resource")?0:Breath+FMath::Abs(Walk)*.035f;R.StringOffset=StringOffset;R.StringLow=StringLow;R.StringAxis=StringAxis;
  R.PartTransforms.Reset(R.Model->Parts.Num());R.BoundsPoints.Reset(R.Model->Parts.Num()*8);
  for(const auto& Part:R.Model->Parts) {
   const FTransform Transform=JointFor(Part.Bone);R.PartTransforms.Add(Transform);
   for(int Corner=0;Corner<8;++Corner) {
    const FVector Rest(Corner&1?Part.Bounds.Max.X:Part.Bounds.Min.X,Corner&2?Part.Bounds.Max.Y:Part.Bounds.Min.Y,Corner&4?Part.Bounds.Max.Z:Part.Bounds.Min.Z);
    const FVector Point=Transform.TransformPosition(Rest)+FVector(0,0,R.Bob);R.BoundsPoints.Add(Point*100);
    if(Part.Name.StartsWith(TEXT("bowstring")))R.BoundsPoints.Add((Point+StringOffset)*100);
   }
  }
  R.BoneTransforms.SetNumUninitialized(R.Model->BoneNames.Num());
  auto* Skin=R.Skin.Get();
  for(int I=0;I<R.Model->BoneNames.Num();++I) {
   const FString& Bone=R.Model->BoneNames[I];
   FTransform Deformation=JointFor(Bone==TEXT("string_draw")?TEXT("weapon"):Bone);
   Deformation.SetTranslation((Deformation.GetTranslation()+FVector(0,0,R.Bob)+(Bone==TEXT("string_draw")?StringOffset:FVector::ZeroVector))*100);
   R.BoneTransforms[I]=R.Model->ReferenceBones[I]*Deformation;
   const int Parent=R.Model->BoneParents[I];
   Skin->BoneSpaceTransforms[I]=Parent>=0?R.BoneTransforms[I].GetRelativeTransform(R.BoneTransforms[Parent]):R.BoneTransforms[I];
  }
  {FSceneTimer SubmitTimer(Profile.MeshSubmitMs);Skin->RefreshBoneTransforms();}
  if(ColorDirty)Skin->SetMaterial(0,SkinMaterial(E));
  R.LastAnimation=Now;R.LastPosition=FVector(Pos,0);R.Remembered=E.bRemembered;R.Stealthed=E.bStealthed;R.Owner=E.Owner;++R.PoseRevision;
  return;
 }
 R.Vertices.Reset(R.Model->VertexCount);R.Normals.Reset(R.Model->VertexCount);if(Rebuild)R.Triangles.Reset(R.Model->Triangles*3);if(ColorDirty)R.Colors.Reset(R.Model->VertexCount);
 const float DeathFall=DeathAge>=0?FMath::Clamp(DeathAge/(Def.bBuilding?1.5f:.8f),0.f,1.f):0.f;
 int PartIndex=0;
 for(const auto& Part:R.Model->Parts) {
  const int Base=R.Vertices.Num();FLinearColor C=Part.Color;
  if(E.Owner>=0&&(Part.Team||Part.Name.Contains(TEXT("team"))))C=FMath::Lerp(C,RTS::PlayerColor(E.Owner),.68f);
  if(E.bRemembered)C*=.38f;else if(E.bStealthed)C=FMath::Lerp(C,FLinearColor(.16f,.27f,.29f),.4f);C.A=1;
  const FColor Color=C.ToFColor(false);
  const FTransform JointTransform=JointFor(Part.Bone);
  const bool Flexible=Part.Name.StartsWith(TEXT("bowstring"))||DeathAge>=0||(Def.bBuilding&&E.Progress<1);
  // Death deformation is identical for every vertex in a part. Resolve the
  // rotation and scatter once instead of repeating trigonometry per corner.
  const FQuat DeathRotation=DeathAge<0?FQuat::Identity:Def.bBuilding?
   FQuat(FVector(PartIndex%2?1.f:0.f,PartIndex%2?0.f:1.f,0),DeathFall*(.08f+(PartIndex%5)*.035f)):
   FQuat(FVector::YAxisVector,DeathFall*PI*.48f);
  const FVector DeathScatter=DeathAge>=0&&Def.bBuilding?
   FVector(FMath::Sin(PartIndex*5.73f)*DeathFall*.18f,FMath::Cos(PartIndex*3.19f)*DeathFall*.18f,0):FVector::ZeroVector;
  for(int VertexIndex=0;VertexIndex<Part.Vertices.Num();++VertexIndex) {
   const FVector& Rest=Part.Vertices[VertexIndex];
   FVector V=JointTransform.TransformPosition(Rest);
   if(Part.Name.StartsWith(TEXT("bowstring"))&&!StringAxis.IsNearlyZero()) {
    const float Along=FMath::Clamp(float(FVector::DotProduct(Rest-StringLow,StringAxis)/StringAxis.SizeSquared()),0.f,1.f);
    V+=StringOffset*(1-FMath::Abs(2*Along-1));
   }
   if(!Def.bBuilding&&Def.Kind!=TEXT("resource"))V.Z+=Breath+FMath::Abs(Walk)*.035f;
   if(Def.bBuilding&&E.Progress<1)V.Z*=.08f+.92f*FMath::Clamp(E.Progress,0.f,1.f);
   if(DeathAge>=0) {
    V=DeathRotation.RotateVector(V);
    if(Def.bBuilding){V.Z*=1-DeathFall*.85f;V+=DeathScatter;}
    else V.Z=FMath::Max(.06,V.Z+.12f);
   }
   R.Vertices.Add(V*100);R.Normals.Add(Flexible?FVector::UpVector:JointTransform.TransformVectorNoScale(Part.Normals[VertexIndex]));if(ColorDirty)R.Colors.Add(Color);
  }
  for(int I=0;I<Part.Triangles.Num();I+=3) {
   const int A=Base+Part.Triangles[I],B=Base+Part.Triangles[I+1],CC=Base+Part.Triangles[I+2];
   if(Rebuild)R.Triangles.Append({A,CC,B});
   if(Flexible){const FVector N=FVector::CrossProduct(R.Vertices[B]-R.Vertices[A],R.Vertices[CC]-R.Vertices[A]).GetSafeNormal();R.Normals[A]=N;R.Normals[B]=N;R.Normals[CC]=N;}
  }
  ++PartIndex;
 }
 {
  FSceneTimer SubmitTimer(Profile.MeshSubmitMs);
  if(Rebuild){
   const bool bDeath=DeathAge>=0&&RTSDeathStressEnabled();
   FSceneTimer FirstSubmitTimer(Profile.DeathFirstSubmitMs,bDeath);
   if(bDeath)++Profile.DeathFirstSubmits;
   R.Mesh->CreateMeshSection(0,R.Vertices,R.Triangles,R.Normals,{},R.Colors,{},false);
  }
  else if(ColorDirty)R.Mesh->UpdateMeshSection(0,R.Vertices,R.Normals,{},R.Colors,{});
  else R.Mesh->UpdateMeshSection(0,R.Vertices,R.Normals,{},{},{});
 }
 R.Mesh->SetMaterial(0,Surface);R.LastAnimation=Now;R.LastPosition=FVector(Pos,0);R.Remembered=E.bRemembered;R.Stealthed=E.bStealthed;R.Owner=E.Owner;++R.PoseRevision;
}

void ARTSScene3D::Entity(const FRTSEntityView& E,float Now,float DeathAge) {
 FString Key;auto M=Model(E,Key);if(!M)return;
 const bool GPU=DeathAge<0&&M->SkeletalMesh.IsValid();
 const int RenderId=DeathAge>=0?-E.Id:E.Id;auto& R=Rendered.FindOrAdd(RenderId);
 const bool Rebuild=!R.Component()||R.Key!=Key||R.Skin.IsValid()!=GPU;
 if(Rebuild&&R.Component()) {R.Component()->DestroyComponent();R.Mesh.Reset();R.Skin.Reset();}
 if(!R.Component()&&GPU) {
  R.Skin=NewObject<UPoseableMeshComponent>(this);AddInstanceComponent(R.Skin.Get());R.Skin->SetupAttachment(RootComponent);
  R.Skin->SetSkinnedAssetAndUpdate(M->SkeletalMesh.Get());R.Skin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  R.Skin->SetCastShadow(true);R.Skin->bComponentUseFixedSkelBounds=true;R.Skin->SetBoundsScale(1.6f);
  R.Skin->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  R.Skin->RegisterComponent();R.Skin->SetComponentTickEnabled(false);
 } else if(!R.Component()) {
  const bool bDeath=DeathAge>=0&&RTSDeathStressEnabled();
  FSceneTimer CreateTimer(Profile.DeathCreateMs,bDeath);
  if(bDeath)++Profile.DeathCreates;
  R.Mesh=NewObject<UProceduralMeshComponent>(this);AddInstanceComponent(R.Mesh.Get());R.Mesh->RegisterComponent();R.Mesh->AttachToComponent(RootComponent,FAttachmentTransformRules::KeepRelativeTransform);
  R.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);R.Mesh->SetCastShadow(true);
 }
 R.Key=Key;R.Model=M;
 const float EventAge=PC->MatchTime-E.AttackEventTime;
 const bool NewEvent=E.AttackEventTime>R.LastAttackEvent&&EventAge>=0&&EventAge<.55f;
 const bool NewAttack=DeathAge<0&&!E.bRemembered&&(NewEvent||(E.bAttacking&&!R.WasAttacking));
 if(NewAttack)R.AttackStarted=Now-(NewEvent?EventAge:0);
 R.LastAttackEvent=E.AttackEventTime;R.WasAttacking=E.bAttacking;
 const auto& Def=FRTSCatalog::Get().Def(E.Def);
 const float DeathFall=DeathAge>=0?FMath::Clamp(DeathAge/(Def.bBuilding?1.5f:.8f),0.f,1.f):-1.f;
 // Once fallen, the mesh is rigid. Sinking moves the component without
 // rebuilding its vertices, normals or render buffers every frame.
 const float Sink=DeathAge>2.f?(DeathAge-2.f)*.6f:0.f;
 const FVector2D Pos=DeathAge>=0?E.Pos:PC->VisualPosition(E);R.Component()->SetWorldLocation(FVector(Pos,-Sink)*100);
 const float Distance=Rebuild?0:FVector2D::Distance(Pos,R.PreviousPosition);
 R.PreviousPosition=Pos;
 if(Distance<8&&DeathAge<0&&!E.bRemembered) {
  R.TravelDistance=FMath::Fmod(R.TravelDistance+Distance,PI*1.08f);R.WalkPhase=FMath::Fmod(R.WalkPhase+Distance*4.5f,2*PI);
 }
 const float TargetWeight=Distance>.0001f&&Distance<8&&DeathAge<0&&!E.bRemembered?1.f:0.f;
 R.MoveWeight=FMath::FInterpTo(R.MoveWeight,TargetWeight,FrameDelta,12.f);
 R.Component()->SetWorldRotation(FRotator(0,FMath::RadiansToDegrees(E.Heading),0));
 const FVector2D Screen=PC->WorldToScreen(Pos);
 const bool Visible=Screen.X>-300&&Screen.X<PC->ViewSize.X+300&&Screen.Y>-100&&Screen.Y<PC->ViewSize.Y+300;
 R.Component()->SetVisibility(Visible&&!TrimmedWallCorners.Contains(E.Id));if(!Visible&&!Rebuild)return;
 if(DeathAge>=0)R.PoseElapsed=DeathAge;
 const bool Animated=!FRTSCatalog::Get().Def(E.Def).bBuilding&&!R.Model->AnimationStyle.IsEmpty()&&!E.bRemembered;
 const bool AnimationDue=R.Skin.IsValid()?Now>=R.NextAnimation:Now-R.LastAnimation>1.f/24.f;
 const bool DeathChanged=DeathAge>=0&&R.DeathFall!=DeathFall;
 if(Rebuild||NewAttack||R.Owner!=E.Owner||R.Remembered!=E.bRemembered||R.Stealthed!=E.bStealthed||DeathChanged||R.Progress!=E.Progress||(DeathAge<0&&Animated&&AnimationDue)) {
  Pose(R,E,Now,Rebuild,DeathAge);
  R.Progress=E.Progress;R.DeathFall=DeathFall;
  if(R.Skin.IsValid()) {
   // Stable phases spread large armies' 24 Hz bone work across frames instead
   // of updating every unit together. New attacks and state changes stay immediate.
   constexpr float Interval=1.f/24.f;
   const float Phase=((uint32(E.Id)*2654435761u)&1023u)*(Interval/1024.f);
   R.NextAnimation=Now+Interval-FMath::Fmod(Now-Phase+Interval,Interval);
  }
 }
 if(NewAttack)SpawnProjectile(E,R,Now);
}

void ARTSScene3D::Update(ARTSController* Controller,float Delta) {
 PC=Controller;FrameDelta=FMath::Clamp(Delta,.001f,.1f);if(!PC.IsValid()||!bReady)return;
 if(RTSDeathStressEnabled())RunRTSDeathStressProbe(this,Controller);
 else {
  RunRTSSceneProbe(this,Controller);
  RunRTSRenderStressProbe(this,Controller);
  RunRTSWallJunctionProbe(this,Controller);
  RunRTSDistrictProbe(this,Controller);
  RTSRunMapCapture(*Controller);
 }
 Profile=FRTSSceneProfile();FSceneTimer UpdateTimer(Profile.UpdateMs);
 const bool Match=PC->Phase>0&&!PC->bMenu;Ground->SetVisibility(Match);
 const bool bBatchDeaths=RTSBatchUnitDeathsEnabled();
 WallJoints->SetVisibility(Match);
 if(!Match){
  for(auto& KV:Rendered)if(!KV.Value.IsBatchedDeath()&&KV.Value.Component())KV.Value.Component()->SetVisibility(false);
  if(bBatchDeaths)HideUnitDeathBatches(GetWorld()->GetRealTimeSeconds());
  Shots.Empty();Projectiles->SetVisibility(false);return;
 }
 const float U=FMath::Max(.25f,FMath::Min(PC->ViewSize.X/1600.f,PC->ViewSize.Y/900.f)),PixelsPerMeter=12.727922f*PC->Zoom*U;
 const FRotator Look(-30,-135-FMath::RadiansToDegrees(PC->CameraRotation),0);
 const FVector Forward=Look.Vector(),Up=FRotationMatrix(Look).GetUnitAxis(EAxis::Z);
 Camera->SetWorldRotation(Look);Camera->SetWorldLocation(FVector(PC->CameraCenter,0)*100-Forward*30000-Up*(90*U/PixelsPerMeter*100));
 Camera->SetOrthoWidth(PC->ViewSize.X/PixelsPerMeter*100);PC->SetViewTarget(this);
 const float Now=GetWorld()->GetRealTimeSeconds();
 if(GroundMap!=PC->Map||(Now-LastGroundUpdate>.35f&&LastFog!=PC->Fog)){Terrain();LastGroundUpdate=Now;}
 TSet<int32> Live;
 for(const auto& E:PC->Entities){Live.Add(E.Id);Entity(E,Now);}
 PC->DeathAnimations.RemoveAll([&](const ARTSController::FDeathAnimation& D){return Now-D.Started>3.2f;});
 if(bBatchDeaths)UpdateUnitDeathBatches(Now,Live);
 for(const auto& D:PC->DeathAnimations){
  if(bBatchDeaths&&!FRTSCatalog::Get().Def(D.Entity.Def).bBuilding)continue;
  Live.Add(-D.Entity.Id);Entity(D.Entity,Now,PC->bReducedMotion?2.5f:Now-D.Started);
 }
 for(auto It=Rendered.CreateIterator();It;++It)if(!Live.Contains(It.Key())){
  if(auto* Component=It.Value().IsBatchedDeath()?nullptr:It.Value().Component()){
   const bool bDeath=It.Key()<0,bMeasure=RTSDeathStressEnabled();
   FSceneTimer RetireTimer(bDeath?Profile.DeathRetireMs:Profile.LiveRetireMs,bMeasure);
   if(bMeasure)++(bDeath?Profile.DeathRetires:Profile.LiveRetires);
   Component->DestroyComponent();
  }
  It.RemoveCurrent();
 }
 UpdateWallJoints();
 DrawProjectiles(Now);
 if(FParse::Param(FCommandLine::Get(),TEXT("RTSPreview"))) {
  static int LastSecond=-1;const int Second=FMath::FloorToInt(Now);
  if(LastSecond!=Second){LastSecond=Second;FVector2D EngineScreen;PC->ProjectWorldLocationToScreen(FVector(PC->CameraCenter,0)*100,EngineScreen);
   UE_LOG(LogTemp,Display,TEXT("RTS_3D_FRAME models=%d tris=%d ground=%d target=%s camera=%s engine_center=%s expected_center=%s"),Rendered.Num(),RenderedTriangles(),GroundVertices.Num(),*GetNameSafe(PC->GetViewTarget()),*PC->PlayerCameraManager->GetCameraLocation().ToString(),*EngineScreen.ToString(),*PC->WorldToScreen(PC->CameraCenter).ToString());
  }
 }
}

FBox2D ARTSScene3D::Bounds(const FRTSEntityView& E,bool Context) const {
 FSceneTimer BoundsTimer(Profile.BoundsMs);
 FBox2D Result(EForceInit::ForceInit);const auto* R=Rendered.Find(E.Id);if(!R||!R->Component())return Result;
 if(TrimmedWallCorners.Contains(E.Id)) {
  IncludeWallJointBounds(E.Id,Result);
  return Context&&Result.bIsValid?Result.ExpandBy(FVector2D(ContextPickPadding)):Result;
 }
 if(!R->Component()->IsVisible())return Result;
 const FTransform Transform=R->Component()->GetComponentTransform();
 const float Angle=FMath::DegreesToRadians(Transform.Rotator().Yaw)+PC->CameraRotation;
 if(R->BoundsRevision!=R->PoseRevision||!FMath::IsNearlyEqual(R->BoundsAngle,Angle,.00001f)) {
  float S,C;FMath::SinCos(&S,&C,Angle);R->ProjectedOffsets=FBox2D(EForceInit::ForceInit);
  for(const FVector& V:R->Skin.IsValid()?R->BoundsPoints:R->Vertices) {
   const float X=V.X*C-V.Y*S,Y=V.X*S+V.Y*C;
   R->ProjectedOffsets+=FVector2D((X-Y)*.09f,(X+Y)*.045f-V.Z*.11022704f);
  }
  R->BoundsRevision=R->PoseRevision;R->BoundsAngle=Angle;
 }
 const float Scale=PC->Zoom*FMath::Max(.25f,FMath::Min(PC->ViewSize.X/1600.f,PC->ViewSize.Y/900.f));
 const FVector2D Origin=Project(Transform.GetTranslation()/100);
 if(R->ProjectedOffsets.bIsValid)Result=FBox2D(Origin+R->ProjectedOffsets.Min*Scale,Origin+R->ProjectedOffsets.Max*Scale);
 IncludeWallJointBounds(E.Id,Result);
 return Context&&Result.bIsValid?Result.ExpandBy(FVector2D(ContextPickPadding)):Result;
}

void ARTSScene3D::GatherPickCandidates(FVector2D Pixel,double Padding,FPickCandidates& Candidates) const {
 for(const auto& E:PC->Entities) {
  if(E.bRemembered||E.HP<=0)continue;
  const FBox2D Tight=Bounds(E);
  if(Tight.bIsValid&&Tight.ExpandBy(FVector2D(Padding)).IsInsideOrOn(Pixel))
   Candidates.Add({&E,TrimmedWallCorners.Contains(E.Id)?nullptr:Rendered.Find(E.Id),Tight});
 }
}

bool ARTSScene3D::TerrainBlocksPick(const FVector& Impact,const FVector& Direction) const {
 if(GroundTriangles.IsEmpty()||!Ground->IsVisible())return false;
 const FTransform Transform=Ground->GetComponentTransform();
 const FVector Start=Transform.InverseTransformPosition(Impact),ToCamera=Transform.InverseTransformVector(-Direction);
 if(Start.Z>=GroundTopZ||ToCamera.Z<=0.)return false;

 // Beyond the highest submitted vertex this ray cannot intersect terrain.
 // Only visit cells spanned by that short segment in the terrain's own grid.
 const double Reach=(GroundTopZ-Start.Z)/ToCamera.Z;
 const FVector End=Start+ToCamera*Reach;
 const auto CellIndex=[](double Position){return FMath::FloorToInt(Position/TerrainCellCm);};
 const int32 MinX=FMath::Max(0,CellIndex(FMath::Min(Start.X,End.X))),MaxX=FMath::Min(TerrainGrid-1,CellIndex(FMath::Max(Start.X,End.X)));
 const int32 MinY=FMath::Max(0,CellIndex(FMath::Min(Start.Y,End.Y))),MaxY=FMath::Min(TerrainGrid-1,CellIndex(FMath::Max(Start.Y,End.Y)));
 for(int32 Y=MinY;Y<=MaxY;++Y)for(int32 X=MinX;X<=MaxX;++X) {
  const int32 First=(Y*TerrainGrid+X)*TerrainFacesPerCell*3;
  for(int32 I=First;I<First+TerrainFacesPerCell*3;I+=3) {
   double Distance;
   if(RayTriangle(Start,ToCamera,GroundVertices[GroundTriangles[I]],GroundVertices[GroundTriangles[I+1]],GroundVertices[GroundTriangles[I+2]],Distance)&&Distance>.05)
    return true;
  }
 }
 return false;
}

int32 ARTSScene3D::TracePick(FVector2D Pixel,const FPickCandidates& Candidates) const {
 const FVector2D Floor=PC->ScreenToWorld(Pixel);
 const FVector Direction=FRotator(-30,-135-FMath::RadiansToDegrees(PC->CameraRotation),0).Vector(),Origin=FVector(Floor,0)*100-Direction*50000;
 int Best=0;double Closest=1.e30;
 for(const FPickCandidate& Candidate:Candidates) {
  const auto& E=*Candidate.Entity;
  const auto* R=Candidate.Rendered;
  if(R&&Candidate.Bounds.IsInsideOrOn(Pixel)) {
   EnsureGeometry(*R);
   const FTransform Transform=R->Component()->GetComponentTransform();
   const FVector O=Transform.InverseTransformPosition(Origin),D=Transform.InverseTransformVector(Direction);
   for(int I=0;I+2<R->Triangles.Num();I+=3) {
    double Distance;
    if(RayTriangle(O,D,R->Vertices[R->Triangles[I]],R->Vertices[R->Triangles[I+1]],R->Vertices[R->Triangles[I+2]],Distance)&&Distance<Closest){Closest=Distance;Best=E.Id;}
   }
  }
 }
 if(const int32 Joint=HitWallJoints(Pixel,Origin,Direction,Closest))Best=Joint;
 return Best&&TerrainBlocksPick(Origin+Direction*Closest,Direction)?0:Best;
}

int32 ARTSScene3D::Hit(FVector2D Pixel,bool Context) const {
 FSceneTimer HitTimer(Profile.HitMs);
 if(!PC.IsValid())return 0;
 return PickWithPadding(Pixel,Context?ContextPickPadding:0.,false);
}

int32 ARTSScene3D::SelectionHit(FVector2D Pixel,double TouchPadding) const {
 FSceneTimer HitTimer(Profile.HitMs);
 if(!PC.IsValid()||!PC->CanInteractWithWorld(Pixel))return 0;
 return PickWithPadding(Pixel,TouchPadding>0?TouchPadding:3.,TouchPadding<=0);
}

int32 ARTSScene3D::PickWithPadding(FVector2D Pixel,double Padding,bool bOwnedUnitsOnly) const {
 FPickCandidates Candidates;
 GatherPickCandidates(Pixel,Padding,Candidates);
 if(Candidates.IsEmpty())return 0;
 if(const int32 Exact=TracePick(Pixel,Candidates))return Exact;
 if(Padding<=0||!PC->CanInteractWithWorld(Pixel))return 0;
 const auto Eligible=[this,bOwnedUnitsOnly](const FRTSEntityView& E) {
  return !bOwnedUnitsOnly||(E.Owner==PC->LocalSlot&&!FRTSCatalog::Get().Def(E.Def).bBuilding);
 };
 TArray<const FPickCandidate*,TInlineAllocator<16>> Nearby;
 for(const auto& Candidate:Candidates)if(Eligible(*Candidate.Entity))Nearby.Add(&Candidate);
 if(Nearby.IsEmpty())return 0;

 // Test neighboring pixels in distance order. A coarser outer ring extends
 // touch targets, while the one-pixel grid finds thin silhouettes at any yaw.
 constexpr int32 Steps=5;
 static const TArray<FIntPoint> Offsets=[] {
  TArray<FIntPoint> Result;
  for(int32 Y=-Steps;Y<=Steps;++Y)for(int32 X=-Steps;X<=Steps;++X)
   if(const int32 Distance=X*X+Y*Y;Distance>0&&Distance<=Steps*Steps)Result.Emplace(X,Y);
  Result.StableSort([](FIntPoint A,FIntPoint B){return A.X*A.X+A.Y*A.Y<B.X*B.X+B.Y*B.Y;});
  return Result;
 }();
 const auto TrySample=[&](FVector2D Offset) {
  const FVector2D Sample=Pixel+Offset;
  bool bMayHit=false;
  for(const auto* Candidate:Nearby)if(Candidate->Bounds.IsInsideOrOn(Sample)){bMayHit=true;break;}
  if(!bMayHit||!PC->CanInteractWithWorld(Sample))return 0;
  const int32 Id=TracePick(Sample,Candidates);
  for(const auto* Candidate:Nearby)if(Candidate->Entity->Id==Id)return Id;
  return 0;
 };
 for(const FIntPoint Delta:Offsets) {
  const FVector2D Offset(Delta);
  if(Offset.SizeSquared()>Padding*Padding)break;
  if(const int32 Id=TrySample(Offset))return Id;
 }
 if(Padding>Steps)for(const FIntPoint Delta:Offsets) {
  const FVector2D Offset=FVector2D(Delta)*(Padding/Steps);
  if(Offset.SizeSquared()>Steps*Steps)if(const int32 Id=TrySample(Offset))return Id;
 }
 return 0;
}

bool ARTSScene3D::IntersectsSelection(const FRTSEntityView& E,const FBox2D& Box) const {
 const FBox2D Tight=Bounds(E);
 if(!Tight.bIsValid||!Box.Intersect(Tight))return false;
 const auto& R=Rendered.FindChecked(E.Id);
 if(!TrimmedWallCorners.Contains(E.Id)) {
  if(Box.IsInsideOrOn(Tight.Min)&&Box.IsInsideOrOn(Tight.Max))return true;
  EnsureGeometry(R);
  const FTransform Transform=R.Component()->GetComponentTransform();
  TArray<FVector2D,TInlineAllocator<128>> Points;
  Points.Reserve(R.Vertices.Num());
  for(const FVector& Vertex:R.Vertices)Points.Add(Project(Transform.TransformPosition(Vertex)/100.));
  for(int32 I=0;I+2<R.Triangles.Num();I+=3)
   if(TriangleOverlapsBox(Points[R.Triangles[I]],Points[R.Triangles[I+1]],Points[R.Triangles[I+2]],Box))return true;
 }
 for(const auto& Joint:WallJointEntries)if(Joint.EntityId==E.Id&&!Joint.Remembered) {
  const auto* Neighbor=PC->FindEntity(Joint.NeighborId);
  if(!Neighbor||Neighbor->bRemembered||Neighbor->HP<=0||Neighbor->bGate||E.bGate)continue;
  for(int32 I=Joint.FirstTriangle*3;I<(Joint.FirstTriangle+Joint.NumTriangles)*3;I+=3) {
   const auto Point=[this](int32 Index){return Project(WallJointVertices[WallJointTriangles[Index]]/100.);};
   if(TriangleOverlapsBox(Point(I),Point(I+1),Point(I+2),Box))return true;
  }
 }
 return false;
}

int32 ARTSScene3D::RenderedTriangles() const {int N=WallJoints&&WallJoints->IsVisible()?WallJointTriangles.Num()/3:0;for(const auto& KV:Rendered)if(KV.Value.Component()&&KV.Value.Component()->IsVisible()&&KV.Value.Model)N+=KV.Value.Model->Triangles;return N;}

void ARTSScene3D::EnsureGeometry(const FRTSRendered3D& R) const {
 if(!R.Skin.IsValid()||!R.Model||R.GeometryRevision==R.PoseRevision)return;
 R.Vertices.Reset(R.Model->VertexCount);R.Triangles.Reset(R.Model->Triangles*3);
 for(int I=0;I<R.Model->Parts.Num();++I) {
  const auto& Part=R.Model->Parts[I];const int Base=R.Vertices.Num();
  for(const FVector& Rest:Part.Vertices) {
   FVector Point=R.PartTransforms[I].TransformPosition(Rest)+FVector(0,0,R.Bob);
   if(Part.Name.StartsWith(TEXT("bowstring"))&&!R.StringAxis.IsNearlyZero()) {
    // Match the FBX's rigid auxiliary-bone weights exactly: each string segment
    // has a pinned tip and four center vertices following the draw hand.
    const FVector Middle=R.StringLow+R.StringAxis*.5;
    const FVector Tip=Part.Name==TEXT("bowstring_lower")?R.StringLow:R.StringLow+R.StringAxis;
    if(FVector::DistSquared(Rest,Middle)<FVector::DistSquared(Rest,Tip))Point+=R.StringOffset;
   }
   R.Vertices.Add(Point*100);
  }
  for(int T=0;T+2<Part.Triangles.Num();T+=3)R.Triangles.Append({Base+Part.Triangles[T],Base+Part.Triangles[T+2],Base+Part.Triangles[T+1]});
 }
 R.GeometryRevision=R.PoseRevision;
}

bool ARTSScene3D::GetPoseMetrics(int32 EntityId,FRTSVisualPoseMetrics& Out) const {
 const auto* R=Rendered.Find(EntityId);if(!R&&EntityId>0)R=Rendered.Find(-EntityId);if(!R||!R->Model)return false;
 const FProcMeshSection* BatchSection=nullptr;
 FTransform BatchToBody=FTransform::Identity;
 if(R->IsBatchedDeath()){
  auto* Mesh=R->Mesh.Get();
  BatchSection=Mesh?Mesh->GetProcMeshSection(0):nullptr;
  if(!BatchSection||R->BatchFirstVertex+R->Model->VertexCount>BatchSection->ProcVertexBuffer.Num())return false;
  const FTransform BodyToWorld(FRotator(0,FMath::RadiansToDegrees(R->BatchHeading),0),FVector(R->PreviousPosition,0)*100.);
  BatchToBody=Mesh->GetComponentTransform().GetRelativeTransform(BodyToWorld);
 } else {
  EnsureGeometry(*R);
  if(R->Vertices.IsEmpty())return false;
 }
 Out=FRTSVisualPoseMetrics();Out.State=R->PoseState;Out.Elapsed=R->PoseElapsed;Out.Triangles=BatchSection?R->Model->Triangles:R->Triangles.Num()/3;
 const FString& Style=R->Model->AnimationStyle;Out.Rig=Style==TEXT("ship")?TEXT("ship"):(Style==TEXT("beast")||Style==TEXT("chariot"))?TEXT("quadruped"):TEXT("humanoid");
 const double Sink=!BatchSection&&(R->PoseState==TEXT("death")||R->PoseState==TEXT("destroy"))&&R->Component()?R->Component()->GetComponentLocation().Z/100.:0.;
 FVector Mean=FVector::ZeroVector;double Square=0;int Index=0;
 for(const auto& Part:R->Model->Parts)for(const FVector& Rest:Part.Vertices) {
  const FVector P=BatchSection?
   BatchToBody.TransformPosition(BatchSection->ProcVertexBuffer[R->BatchFirstVertex+Index].Position)/100.:
   R->Vertices[Index]/100.+FVector(0,0,Sink);
  ++Index;
  const FVector Delta=P-Rest;Mean+=Delta;Square+=Delta.SizeSquared();Out.MaximumOffset=FMath::Max(Out.MaximumOffset,float(Delta.Size()));
  Out.PoseHash=HashCombine(Out.PoseHash,GetTypeHash(FMath::RoundToInt(P.X*100000)));Out.PoseHash=HashCombine(Out.PoseHash,GetTypeHash(FMath::RoundToInt(P.Y*100000)));Out.PoseHash=HashCombine(Out.PoseHash,GetTypeHash(FMath::RoundToInt(P.Z*100000)));
 }
 Mean/=Index;Out.NonRigidDisplacement=FMath::Sqrt(FMath::Max(0.,Square/Index-Mean.SizeSquared()));return true;
}
