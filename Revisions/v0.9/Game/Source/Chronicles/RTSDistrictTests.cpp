#include "RTSDistrictTests.h"

#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSHUD.h"
#include "RTSScene3D.h"
#include "RTSWorld.h"
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
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace {
constexpr double DistrictPlacementMargin=.025;
constexpr double DistrictGridCm=10.;
constexpr double DistrictBaseHeightCm=15.;
const FVector2D DistrictOrigin(48,48);

struct FDistrictSpec {
 const TCHAR* Type;
 FVector2D PreferredOffset;
};

const FDistrictSpec LabanDistrict[]={
 {TEXT("seat_of_the_people"),{0,0}},
 {TEXT("temple_of_the_land"),{4,4}},
 {TEXT("tents_and_habitations"),{8,5}},
 {TEXT("fields_of_grain"),{-1,-7}},
 {TEXT("winery"),{4,-3}},
 {TEXT("storehouse_of_provisions"),{-5,1}},
 {TEXT("house_of_curious_workmanship"),{0,4}},
 {TEXT("place_of_trade"),{-5,5}},
 {TEXT("encampment_of_war"),{-3,-4}}
};
const FDistrictSpec GadiantonDistrict[]={
 {TEXT("seat_of_the_people"),{0,0}},
 {TEXT("secret_combination_lodge"),{4,3}},
 {TEXT("encampment_of_war"),{-2,4}},
 {TEXT("tents_and_habitations"),{5,-2}}
};

struct FDistrictBuilding {
 int32 Id=0;
 FString Type,Label,Model;
 FVector2D Position;
 double Radius=0;
 TArray<FVector> Vertices;
 TArray<int32> Indices;
 TSet<FIntPoint> Footprint,BaseFootprint;
};

FVector2D DistrictGrid(const FVector& WorldCm) {return FVector2D(WorldCm.X/DistrictGridCm,WorldCm.Y/DistrictGridCm);}

double DistrictEdge(const FVector2D& A,const FVector2D& B,const FVector2D& P) {
 return FVector2D::CrossProduct(B-A,P-A);
}

// The mask reports projection coverage independently of the production ray picker.
void RasterDistrictTriangle(const FVector2D& A,const FVector2D& B,const FVector2D& C,int32 Stride,TSet<FIntPoint>& Cells) {
 const double Area=DistrictEdge(A,B,C);
 if(FMath::Abs(Area)<1.e-7)return;
 const double Sign=Area>0?1.:-1.;
 const int32 FirstX=FMath::CeilToInt32(FMath::Min3(A.X,B.X,C.X)/Stride)*Stride;
 const int32 LastX=FMath::FloorToInt32(FMath::Max3(A.X,B.X,C.X)/Stride)*Stride;
 const int32 FirstY=FMath::CeilToInt32(FMath::Min3(A.Y,B.Y,C.Y)/Stride)*Stride;
 const int32 LastY=FMath::FloorToInt32(FMath::Max3(A.Y,B.Y,C.Y)/Stride)*Stride;
 for(int32 Y=FirstY;Y<=LastY;Y+=Stride)for(int32 X=FirstX;X<=LastX;X+=Stride) {
  const FVector2D P(X,Y);
  if(DistrictEdge(A,B,P)*Sign>=-1.e-7&&DistrictEdge(B,C,P)*Sign>=-1.e-7&&DistrictEdge(C,A,P)*Sign>=-1.e-7)Cells.Add(FIntPoint(X,Y));
 }
}

void RasterDistrictBase(const FVector& A,const FVector& B,const FVector& C,TSet<FIntPoint>& Cells) {
 const FVector Triangle[]={A,B,C};
 TArray<FVector,TInlineAllocator<4>> Below;
 for(int32 I=0;I<3;++I) {
  const FVector& From=Triangle[I];
  const FVector& To=Triangle[(I+1)%3];
  const bool bFromBelow=From.Z<=DistrictBaseHeightCm,bToBelow=To.Z<=DistrictBaseHeightCm;
  if(bFromBelow)Below.Add(From);
  if(bFromBelow!=bToBelow)Below.Add(FMath::Lerp(From,To,(DistrictBaseHeightCm-From.Z)/(To.Z-From.Z)));
 }
 for(int32 I=1;I+1<Below.Num();++I)RasterDistrictTriangle(DistrictGrid(Below[0]),DistrictGrid(Below[I]),DistrictGrid(Below[I+1]),1,Cells);
}

int32 DistrictCommonCells(const TSet<FIntPoint>& A,const TSet<FIntPoint>& B) {
 const auto& Smaller=A.Num()<=B.Num()?A:B;
 const auto& Larger=A.Num()<=B.Num()?B:A;
 int32 Count=0;
 for(const FIntPoint P:Smaller)if(Larger.Contains(P))++Count;
 return Count;
}

TSharedRef<FJsonObject> DistrictPair(const FDistrictBuilding& A,const FDistrictBuilding& B) {
 auto Row=MakeShared<FJsonObject>();
 Row->SetNumberField(TEXT("a_id"),A.Id);
 Row->SetStringField(TEXT("a"),A.Label);
 Row->SetNumberField(TEXT("b_id"),B.Id);
 Row->SetStringField(TEXT("b"),B.Label);
 Row->SetNumberField(TEXT("center_distance_m"),FVector2D::Distance(A.Position,B.Position));
 return Row;
}
}

class FRTSDistrictProbe {
 enum class EStage {Initialize,CollectMeshes,Measure,Capture};
 EStage Stage=EStage::Initialize;
 TArray<TSharedPtr<FJsonValue>> Reports;
 TArray<TSharedPtr<FJsonValue>> Placements,Views;
 TArray<FDistrictBuilding> Buildings;
 FVector2D CameraCenter=DistrictOrigin;
 int32 DistrictIndex=0,ViewIndex=0,Frame=0,ChangedAt=0,Failures=0;
 float CameraZoom=1.65f,CameraAngle=0,DrawAfter=0;
 double Started=FPlatformTime::Seconds(),ChangedReal=Started;
 bool bDone=false;

 const TCHAR* Faction() const {return DistrictIndex==0?TEXT("laban"):TEXT("gadianton");}

 void StageChanged(EStage Next) {
  Stage=Next;
  ChangedAt=Frame;
  ChangedReal=FPlatformTime::Seconds();
 }

 void Fail(const FString& Reason) {
  ++Failures;
  UE_LOG(LogTemp,Error,TEXT("RTS_DISTRICT_FIXTURE_FAIL %s %s"),Faction(),*Reason);
 }

 void Finish() {
  bDone=true;
  auto Report=MakeShared<FJsonObject>();
  Report->SetNumberField(TEXT("fixture_failures"),Failures);
  Report->SetStringField(TEXT("scope"),TEXT("Compact production build placements on clear, pre-explored land. Age and resources are preloaded; accepted foundations are completed for the art fixture. Projection and occlusion counts are observations."));
  Report->SetStringField(TEXT("ground_overlap_method"),TEXT("Horizontal projection of rendered triangles sampled on a 10 cm grid; a separate mask clips triangles to z <= 15 cm. Projected overlap may include vertical separation."));
  Report->SetStringField(TEXT("picking_method"),TEXT("Projected triangles produce a per-building integer-sample mask. The actual HUD world blocker and controller exact EntityUnder result classify each sample."));
  Report->SetArrayField(TEXT("districts"),Reports);
  FString Json;
  FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("Tests");
  IFileManager::Get().MakeDirectory(*Folder,true);
  const bool bSaved=FFileHelper::SaveStringToFile(Json,*(Folder/TEXT("dense-district-tests.json")));
  const bool bPassed=Failures==0&&bSaved;
  UE_LOG(LogTemp,Display,TEXT("RTS_DISTRICT_TEST districts=%d fixture_failures=%d saved=%d"),Reports.Num(),Failures,bSaved);
  FPlatformMisc::RequestExitWithStatus(!bPassed,bPassed?0:1);
 }

 TOptional<FVector2D> TightSite(ARTSWorld& World,const FDistrictSpec& Spec,double Radius) const {
  if(Buildings.IsEmpty()) {
   return World.Walkable(DistrictOrigin,Radius)?TOptional<FVector2D>(DistrictOrigin):TOptional<FVector2D>();
  }
  TOptional<FVector2D> Best;
  double BestScore=TNumericLimits<double>::Max();
  const FVector2D Preferred=DistrictOrigin+Spec.PreferredOffset;
  for(const auto& Existing:Buildings) {
   const double Near=Radius+Existing.Radius*.88+DistrictPlacementMargin;
   for(int32 Bearing=0;Bearing<360;Bearing+=5) {
    const FVector2D Candidate=Existing.Position+FVector2D(Near,0).GetRotated(Bearing);
    if(!World.Walkable(Candidate,Radius))continue;
    const double Score=FVector2D::DistSquared(Candidate,Preferred)+.15*FVector2D::DistSquared(Candidate,DistrictOrigin);
    if(Score<BestScore){BestScore=Score;Best=Candidate;}
   }
  }
  return Best;
 }

 bool Place(ARTSWorld& World,ARTSController& C,FRTSEntity& Builder,const FDistrictSpec& Spec) {
  const auto& Catalog=FRTSCatalog::Get();
  const auto& Def=Catalog.Def(Spec.Type);
  const auto Site=TightSite(World,Spec,Def.Radius);
  if(!Site.IsSet()) {Fail(FString(Spec.Type)+TEXT(" has no legal compact placement"));return false;}
  const int32 ExpectedId=World.NextId;
  const FVector Before=World.Players[0].Resources;
  World.Command(&C,TEXT("build"),{Builder.Id},Site.GetValue(),0,Def.Id);
  auto* Foundation=World.Find(ExpectedId);
  const bool bAccepted=Foundation&&World.NextId==ExpectedId+1&&Builder.Target==ExpectedId&&World.D(*Foundation).Id==Def.Id&&
     Foundation->Owner==0&&Foundation->Progress>0&&Foundation->Progress<1&&Foundation->Pos.Equals(Site.GetValue(),.001)&&
     World.Players[0].Resources.Equals(Before-Catalog.Price(Def,Faction()),.001);
  if(!bAccepted) {Fail(FString(Spec.Type)+TEXT(" did not create a paid authoritative foundation"));return false;}
  auto Row=MakeShared<FJsonObject>();
  Row->SetNumberField(TEXT("id"),Foundation->Id);
  Row->SetStringField(TEXT("type"),Def.Id);
  Row->SetStringField(TEXT("name"),Catalog.DisplayName(Def,Faction()));
  Row->SetNumberField(TEXT("x_m"),Foundation->Pos.X);
  Row->SetNumberField(TEXT("y_m"),Foundation->Pos.Y);
  Row->SetNumberField(TEXT("placement_radius_m"),Def.Radius);
  Row->SetBoolField(TEXT("accepted_paid_foundation"),true);
  if(!Buildings.IsEmpty()) {
   double Clearance=TNumericLimits<double>::Max();
   for(const auto& Prior:Buildings)Clearance=FMath::Min(Clearance,FVector2D::Distance(Prior.Position,Foundation->Pos)-Def.Radius-Prior.Radius*.88);
   Row->SetNumberField(TEXT("nearest_prior_placement_slack_m"),Clearance);
  }
  Placements.Add(MakeShared<FJsonValueObject>(Row));
  auto& Added=Buildings.AddDefaulted_GetRef();
  Added.Id=Foundation->Id;Added.Type=Def.Id;Added.Label=Catalog.DisplayName(Def,Faction());
  Added.Position=Foundation->Pos;Added.Radius=Def.Radius;
  Foundation->Progress=1.f;
  Foundation->HP=Foundation->MaxHP=World.MaxHP(*Foundation);
  return true;
 }

 bool Initialize(ARTSWorld& World,ARTSController& C) {
  World.SetActorTickEnabled(false);
  World.Entities.Reset();World.Obstacles.Reset();World.ObstacleCount=-1;World.Players.Reset();World.NextId=1;
  World.Map=0;World.Phase=1;World.Time=0;World.bSandbox=true;World.bFogOfWar=true;
  auto& Player=World.Players.AddDefaulted_GetRef();
  Player.Slot=0;Player.Team=0;Player.Faction=Faction();Player.Age=4;
  Player.Controller=&C;Player.Resources=FVector(10000);Player.Cement=1000;
  Player.Explored.Init(0,RTS::FogSize*RTS::FogSize);
  const int32 FogX=FMath::FloorToInt32(DistrictOrigin.X/2.5),FogY=FMath::FloorToInt32(DistrictOrigin.Y/2.5);
  for(int32 Y=FogY-6;Y<=FogY+6;++Y)for(int32 X=FogX-6;X<=FogX+6;++X)Player.Explored[Y*RTS::FogSize+X]=1;
  C.ServerSlot=0;C.bMenu=false;C.bPauseMenu=false;C.bHelp=false;
  C.DisplayPositions.Reset();C.Selection.Reset();C.DeathAnimations.Reset();
  Buildings.Reset();Placements.Reset();Views.Reset();ViewIndex=0;
  CameraCenter=DistrictOrigin;CameraZoom=1.65f;CameraAngle=0;
  auto* Builder=World.Spawn(TEXT("industrious_laborer"),0,DistrictOrigin-FVector2D(14,10));
  if(!Builder){Fail(TEXT("No builder fixture"));return false;}
  const TArrayView<const FDistrictSpec> Layout=DistrictIndex==0?MakeArrayView(LabanDistrict):MakeArrayView(GadiantonDistrict);
  for(const auto& Spec:Layout)if(!Place(World,C,*Builder,Spec))return false;
  World.Entities.Remove(Builder->Id);
  World.Snapshot();
  if(C.Entities.Num()!=Buildings.Num()) {Fail(TEXT("Authoritative district snapshot is incomplete"));return false;}
  StageChanged(EStage::CollectMeshes);
  return true;
 }

 bool CollectMeshes(ARTSScene3D& Scene,const ARTSController& C) {
  for(auto& Building:Buildings) {
   const auto* R=Scene.Rendered.Find(Building.Id);
   const auto* Entity=C.FindEntity(Building.Id);
   if(!R||!R->Component()||!R->Model||!Entity||Entity->bRemembered||!R->Component()->IsRenderStateCreated()) {
    Fail(Building.Label+TEXT(" has no current client mesh"));return false;
   }
   Scene.EnsureGeometry(*R);
   if(R->Vertices.IsEmpty()||R->Triangles.IsEmpty()) {Fail(Building.Label+TEXT(" has empty rendered geometry"));return false;}
   Building.Model=R->Key;
   Building.Vertices.Reserve(R->Vertices.Num());
   Building.Indices=R->Triangles;
   const FTransform Transform=R->Component()->GetComponentTransform();
   for(const FVector& V:R->Vertices)Building.Vertices.Add(Transform.TransformPosition(V));
   for(int32 I=0;I+2<Building.Indices.Num();I+=3) {
    const FVector& A=Building.Vertices[Building.Indices[I]],& B=Building.Vertices[Building.Indices[I+1]],& D=Building.Vertices[Building.Indices[I+2]];
    RasterDistrictTriangle(DistrictGrid(A),DistrictGrid(B),DistrictGrid(D),1,Building.Footprint);
    RasterDistrictBase(A,B,D,Building.BaseFootprint);
   }
  }
  return true;
 }

 TArray<TSharedPtr<FJsonValue>> FootprintOverlaps() const {
  TArray<TSharedPtr<FJsonValue>> Pairs;
  for(int32 I=0;I<Buildings.Num();++I)for(int32 J=I+1;J<Buildings.Num();++J) {
   const auto& A=Buildings[I];const auto& B=Buildings[J];
   const int32 Overlap=DistrictCommonCells(A.Footprint,B.Footprint),BaseOverlap=DistrictCommonCells(A.BaseFootprint,B.BaseFootprint);
   if(!Overlap&&!BaseOverlap)continue;
   auto Pair=DistrictPair(A,B);
   Pair->SetNumberField(TEXT("footprint_overlap_m2"),Overlap*.01);
   Pair->SetNumberField(TEXT("near_ground_overlap_m2"),BaseOverlap*.01);
   Pairs.Add(MakeShared<FJsonValueObject>(Pair));
  }
  return Pairs;
 }

 void PinCamera(ARTSController& C) const {C.CameraCenter=CameraCenter;C.Zoom=CameraZoom;C.CameraRotation=CameraAngle;}

 void PrepareView(ARTSScene3D& Scene,ARTSController& C) {
  CameraZoom=ViewIndex<2?.55f:4.f;
  CameraAngle=ViewIndex%2?PI/4:0;
  CameraCenter=DistrictOrigin;
  PinCamera(C);
  FBox2D Envelope(EForceInit::ForceInit);
  for(const auto& Building:Buildings)for(const FVector& V:Building.Vertices)Envelope+=Scene.Project(V/100.);
  const double U=FMath::Min(C.ViewSize.X/1600.,C.ViewSize.Y/900.);
  const FVector2D PlayCenter(C.ViewSize.X*.5,(64.*U+C.ViewSize.Y-228.*U)*.5);
  CameraCenter+=C.ScreenToWorld(Envelope.GetCenter())-C.ScreenToWorld(PlayCenter);
  PinCamera(C);
  DrawAfter=C.GetWorld()->GetTimeSeconds();
  StageChanged(EStage::Measure);
 }

 bool IsDrawReady(ARTSScene3D& Scene) const {
  for(const auto& Building:Buildings) {
   const auto* R=Scene.Rendered.Find(Building.Id);
   if(R&&R->Component()&&R->Component()->GetLastRenderTimeOnScreen()>=DrawAfter)return true;
  }
  return false;
 }

 void Measure(ARTSScene3D& Scene,ARTSController& C,const ARTSHUD& HUD) {
  const int32 Stride=CameraZoom<1?1:5;
  const FString Capture=FString::Printf(TEXT("dense-district-%s-%s-%d.png"),Faction(),ViewIndex<2?TEXT("far"):TEXT("near"),ViewIndex%2?45:0);
  TArray<TSet<FIntPoint>> Masks;
  Masks.SetNum(Buildings.Num());
  TArray<TSharedPtr<FJsonValue>> Rows,Pairs;
  TMap<FIntPoint,int32> Hits;
  int32 PickableBuildings=0,Covered=0,Offscreen=0,HudOnly=0;
  for(int32 Index=0;Index<Buildings.Num();++Index) {
   const auto& Building=Buildings[Index];auto& Mask=Masks[Index];
   TArray<FVector2D> Projected;
   Projected.Reserve(Building.Vertices.Num());
   FBox2D Bounds(EForceInit::ForceInit);
   for(const FVector& V:Building.Vertices) {const FVector2D P=Scene.Project(V/100.);Projected.Add(P);Bounds+=P;}
   for(int32 I=0;I+2<Building.Indices.Num();I+=3)RasterDistrictTriangle(Projected[Building.Indices[I]],Projected[Building.Indices[I+1]],Projected[Building.Indices[I+2]],Stride,Mask);
   if(Mask.IsEmpty())Fail(Building.Label+TEXT(" has no integer silhouette samples"));
   int32 Off=0,Blocked=0,Own=0,Other=0,Miss=0;
   FIntPoint Pick(-1,-1);
   double PickDistance=TNumericLimits<double>::Max();
   TMap<int32,int32> Overlying;
   for(const FIntPoint Pixel:Mask) {
    const FVector2D P(Pixel);
    if(P.X<0||P.Y<0||P.X>=C.ViewSize.X||P.Y>=C.ViewSize.Y){++Off;continue;}
    if(HUD.BlocksWorld(P)){++Blocked;continue;}
    const int32* Cached=Hits.Find(Pixel);
    const int32 Id=Cached?*Cached:C.EntityUnder(P);
    if(!Cached)Hits.Add(Pixel,Id);
    if(Id==Building.Id) {
     ++Own;
     const double Distance=FVector2D::DistSquared(P,Bounds.GetCenter());
     if(Distance<PickDistance){PickDistance=Distance;Pick=Pixel;}
    }else if(Id>0) {++Other;++Overlying.FindOrAdd(Id);}
    else ++Miss;
   }
   auto Row=MakeShared<FJsonObject>();
   Row->SetNumberField(TEXT("id"),Building.Id);
   Row->SetStringField(TEXT("name"),Building.Label);
   Row->SetStringField(TEXT("model"),Building.Model);
   Row->SetNumberField(TEXT("bounds_width_px"),Bounds.GetSize().X);
   Row->SetNumberField(TEXT("bounds_height_px"),Bounds.GetSize().Y);
   Row->SetNumberField(TEXT("projected_samples"),Mask.Num());
   Row->SetNumberField(TEXT("offscreen_samples"),Off);
   Row->SetNumberField(TEXT("ui_blocked_samples"),Blocked);
   Row->SetNumberField(TEXT("own_exact_pick_samples"),Own);
   Row->SetNumberField(TEXT("other_building_pick_samples"),Other);
   Row->SetNumberField(TEXT("ray_miss_samples"),Miss);
   if(Own>0) {
    ++PickableBuildings;
    Row->SetNumberField(TEXT("pick_x"),Pick.X);Row->SetNumberField(TEXT("pick_y"),Pick.Y);
   }else if(Other>0)++Covered;
   else if(Off==Mask.Num())++Offscreen;
   else if(Off+Blocked==Mask.Num())++HudOnly;
   if(Own+Other+Miss>0)Row->SetNumberField(TEXT("playable_exact_pick_fraction"),double(Own)/(Own+Other+Miss));
   TArray<TSharedPtr<FJsonValue>> Occluders;
   for(const auto& Site:Buildings)if(const int32* Count=Overlying.Find(Site.Id)) {
    auto Layer=MakeShared<FJsonObject>();Layer->SetStringField(TEXT("name"),Site.Label);Layer->SetNumberField(TEXT("samples"),*Count);
    Occluders.Add(MakeShared<FJsonValueObject>(Layer));
   }
   Row->SetArrayField(TEXT("occluders"),Occluders);
   Rows.Add(MakeShared<FJsonValueObject>(Row));
  }
  for(int32 I=0;I<Masks.Num();++I)for(int32 J=I+1;J<Masks.Num();++J) {
   const int32 Common=DistrictCommonCells(Masks[I],Masks[J]);
   if(Common==0)continue;
   auto Pair=DistrictPair(Buildings[I],Buildings[J]);
   Pair->SetNumberField(TEXT("silhouette_overlap_samples"),Common);
   Pair->SetNumberField(TEXT("fraction_of_smaller_silhouette"),double(Common)/FMath::Min(Masks[I].Num(),Masks[J].Num()));
   Pairs.Add(MakeShared<FJsonValueObject>(Pair));
  }
  auto View=MakeShared<FJsonObject>();
  View->SetNumberField(TEXT("zoom"),CameraZoom);
  View->SetNumberField(TEXT("camera_heading_degrees"),ViewIndex%2?45:0);
  View->SetNumberField(TEXT("viewport_width_px"),C.ViewSize.X);View->SetNumberField(TEXT("viewport_height_px"),C.ViewSize.Y);
  View->SetNumberField(TEXT("sample_stride_px"),Stride);
  View->SetNumberField(TEXT("pickable_buildings"),PickableBuildings);
  View->SetNumberField(TEXT("covered_buildings_without_own_pick"),Covered);
  View->SetNumberField(TEXT("offscreen_buildings"),Offscreen);
  View->SetNumberField(TEXT("ui_only_buildings"),HudOnly);
  View->SetNumberField(TEXT("unique_world_pixels_tested"),Hits.Num());
  View->SetStringField(TEXT("screenshot"),Capture);
  View->SetArrayField(TEXT("buildings"),Rows);
  View->SetArrayField(TEXT("projected_overlap_pairs"),Pairs);
  Views.Add(MakeShared<FJsonValueObject>(View));
  UE_LOG(LogTemp,Display,TEXT("RTS_DISTRICT_VIEW %s zoom=%.2f heading=%d pickable=%d/%d covered=%d offscreen=%d ui_only=%d pixels=%d"),
         Faction(),CameraZoom,ViewIndex%2?45:0,PickableBuildings,Buildings.Num(),Covered,Offscreen,HudOnly,Hits.Num());
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");
  IFileManager::Get().MakeDirectory(*Folder,true);
  FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(Folder/Capture),false,false);
  StageChanged(EStage::Capture);
 }

 void FinishDistrict() {
  auto Result=MakeShared<FJsonObject>();
  Result->SetStringField(TEXT("faction"),Faction());
  Result->SetArrayField(TEXT("authoritative_placements"),Placements);
  Result->SetArrayField(TEXT("ground_projected_overlap_pairs"),FootprintOverlaps());
  Result->SetArrayField(TEXT("views"),Views);
  Reports.Add(MakeShared<FJsonValueObject>(Result));
 }

public:
 void Run(ARTSScene3D& Scene,ARTSController& C) {
  if(bDone||!C.Me())return;
  ++Frame;
  PinCamera(C);
  if(FPlatformTime::Seconds()-Started>120.) {Fail(TEXT("Timed out waiting for rendered district"));Finish();return;}
  if(Stage==EStage::Initialize) {
   const auto* Mode=C.GetWorld()->GetAuthGameMode<ARTSGameMode>();
   if(!Mode||!Mode->Simulation){Fail(TEXT("No authoritative simulation"));Finish();return;}
   if(!Initialize(*Mode->Simulation,C)) {FinishDistrict();Finish();}
   return;
  }
  if(Frame-ChangedAt<3)return;
  if(Stage==EStage::CollectMeshes) {
   if(CollectMeshes(Scene,C))PrepareView(Scene,C);
   else {FinishDistrict();Finish();}
   return;
  }
  if(Stage==EStage::Measure) {
   if(!IsDrawReady(Scene)) {
    if(FPlatformTime::Seconds()-ChangedReal<3.)return;
    Fail(TEXT("No building recorded an on-screen draw"));
   }
   const auto* HUD=Cast<ARTSHUD>(C.GetHUD());
   if(!HUD){Fail(TEXT("Viewport HUD missing"));FinishDistrict();Finish();return;}
   Measure(Scene,C,*HUD);
   return;
  }
  const FString Capture=Views.Last()->AsObject()->GetStringField(TEXT("screenshot"));
  if(!IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Capture))) {
   if(FPlatformTime::Seconds()-ChangedReal<3.)return;
   Fail(Capture+TEXT(" was not written"));
  }
  if(++ViewIndex<4) {PrepareView(Scene,C);return;}
  FinishDistrict();
  if(DistrictIndex==0){++DistrictIndex;StageChanged(EStage::Initialize);}
  else Finish();
 }
};

void RunRTSDistrictProbe(ARTSScene3D* Scene,ARTSController* Controller) {
 static const bool bEnabled=FParse::Param(FCommandLine::Get(),TEXT("RTSV09DistrictTest"));
 if(!bEnabled)return;
 static FRTSDistrictProbe Probe;
 Probe.Run(*Scene,*Controller);
}
