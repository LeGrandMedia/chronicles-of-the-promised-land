#include "RTSScene3D.h"
#include "RTSController.h"
#include "Components/MeshComponent.h"
#include "ProceduralMeshComponent.h"

namespace {
constexpr double WallCellSize=5.;
constexpr double WallSliceHeight=.45;

FIntPoint WallCell(const FVector2D& P) {
 return {int32(FMath::FloorToInt(P.X/WallCellSize)),int32(FMath::FloorToInt(P.Y/WallCellSize))};
}

// The section is measured from the authored solid, not from decorations at the
// top of a wall or its navigation circle. It is used only to detect a gap.
TArray<FVector2D> WallSection(const FRTSModel3D& Model) {
 TArray<FVector2D> Points;
 for(const auto& Part:Model.Parts)for(int32 I=0;I+2<Part.Triangles.Num();I+=3) {
  for(int32 Edge=0;Edge<3;++Edge) {
   const FVector& A=Part.Vertices[Part.Triangles[I+Edge]];
   const FVector& B=Part.Vertices[Part.Triangles[I+(Edge+1)%3]];
   if(FMath::IsNearlyEqual(A.Z,WallSliceHeight,1.e-8))Points.Emplace(A.X,A.Y);
   if((A.Z<WallSliceHeight&&B.Z>WallSliceHeight)||(B.Z<WallSliceHeight&&A.Z>WallSliceHeight)) {
    const FVector P=FMath::Lerp(A,B,(WallSliceHeight-A.Z)/(B.Z-A.Z));Points.Emplace(P.X,P.Y);
   }
  }
 }
 return Points;
}

double WallReach(const TArray<FVector2D>& Section,float Heading,const FVector2D& Direction) {
 double Reach=0;
 const FVector2D Local=Direction.GetRotated(-FMath::RadiansToDegrees(Heading));
 for(const FVector2D& Point:Section)Reach=FMath::Max(Reach,FVector2D::DotProduct(Point,Local));
 return Reach;
}

// Retain the positive side of a plane through the source model's center.
// At a two-leg bend the two models meet on the same world-space bisector.
TArray<FVector> WallHalfTriangle(const FVector& A,const FVector& B,const FVector& C,const FVector2D& Normal) {
 const FVector Input[]={A,B,C};TArray<FVector> Out;Out.Reserve(4);
 for(int32 Edge=0;Edge<3;++Edge) {
  const FVector& From=Input[Edge];const FVector& To=Input[(Edge+1)%3];
  const double DFrom=FVector2D::DotProduct(FVector2D(From),Normal),DTo=FVector2D::DotProduct(FVector2D(To),Normal);
  if(DFrom>=0)Out.Add(From);
  if((DFrom<0&&DTo>0)||(DFrom>0&&DTo<0))Out.Add(FMath::Lerp(From,To,DFrom/(DFrom-DTo)));
 }
 return Out;
}

bool WallRayTriangle(const FVector& Origin,const FVector& Direction,const FVector& A,const FVector& B,const FVector& C,double& Distance) {
 const FVector Edge1=B-A,Edge2=C-A,P=FVector::CrossProduct(Direction,Edge2);
 const double Determinant=FVector::DotProduct(Edge1,P);if(FMath::Abs(Determinant)<1.e-8)return false;
 const double Inv=1./Determinant;const FVector Offset=Origin-A;
 const double U=FVector::DotProduct(Offset,P)*Inv;if(U<0||U>1)return false;
 const FVector Q=FVector::CrossProduct(Offset,Edge1);const double V=FVector::DotProduct(Direction,Q)*Inv;
 if(V<0||U+V>1)return false;Distance=FVector::DotProduct(Edge2,Q)*Inv;return Distance>=0;
}
}

void ARTSScene3D::UpdateWallJoints() {
 TArray<FRTSWallSnapshot3D> Current;
 // Only the filtered client snapshot is available here. Missing/fogged-out
 // neighbors cannot contribute geometry or pick targets.
 for(const FRTSEntityView& Entity:PC->Entities) {
  const FRTSDef& Def=FRTSCatalog::Get().Def(Entity.Def);
  if(Def.Id!=TEXT("works_of_defense")&&Def.Id!=TEXT("cement_wall"))continue;
  const FRTSRendered3D* Mesh=Rendered.Find(Entity.Id);if(!Mesh||!Mesh->Model)continue;
  FRTSWallSnapshot3D& Wall=Current.AddDefaulted_GetRef();
  Wall.Id=Entity.Id;Wall.Owner=Entity.Owner;Wall.Def=Entity.Def;Wall.Pos=Entity.Pos;
  Wall.Heading=Entity.Heading;Wall.Progress=Entity.Progress;Wall.Remembered=Entity.bRemembered;Wall.Gate=Entity.bGate;Wall.ModelKey=Mesh->Key;
 }
 Current.Sort([](const FRTSWallSnapshot3D& A,const FRTSWallSnapshot3D& B){return A.Id<B.Id;});
 auto HideTrimmedCorners=[&]() {
  for(int32 Id:TrimmedWallCorners)if(const auto* Entry=Rendered.Find(Id))if(Entry->Component())Entry->Component()->SetVisibility(false);
 };
 if(Current==WallSnapshots){HideTrimmedCorners();return;}
 WallSnapshots=MoveTemp(Current);++WallJointRevision;
 const TSet<int32> PreviouslyTrimmed=TrimmedWallCorners;
 WallJointEntries.Reset();WallJointVertices.Reset();WallJointTriangles.Reset();WallJointBounds.Reset();TrimmedWallCorners.Reset();
 TArray<FVector> Normals;TArray<FColor> Colors;
 TMap<FIntPoint,TArray<int32>> Cells;
 TMap<FString,TArray<FVector2D>> Sections;
 for(int32 Index=0;Index<WallSnapshots.Num();++Index)Cells.FindOrAdd(WallCell(WallSnapshots[Index].Pos)).Add(Index);

 auto AddJoint=[&](const FRTSWallSnapshot3D& Corner,const FRTSWallSnapshot3D& Neighbor) {
  const FVector2D Between=(Neighbor.Pos-Corner.Pos).GetSafeNormal();
  const FVector2D Axis(FMath::Cos(Neighbor.Heading),FMath::Sin(Neighbor.Heading));
  const double Sign=FVector2D::DotProduct(Between,Axis)>0?1.:-1.;
  const double Height=.08+.92*FMath::Clamp(FMath::Min(Corner.Progress,Neighbor.Progress),0.f,1.f);
  FRTSWallJoint3D Joint;Joint.EntityId=Corner.Id;Joint.NeighborId=Neighbor.Id;
  Joint.Remembered=Corner.Remembered||Neighbor.Remembered;Joint.FirstTriangle=WallJointTriangles.Num()/3;
  // Trim only a completed, unambiguous two-leg bend. T-junctions and corners
  // whose other neighbor is withheld keep the authored full segment.
  const FRTSWallSnapshot3D* Back=nullptr;int32 Adjacent=0;
  const double Spacing=2.*FRTSCatalog::Get().Def(Corner.Def).Radius+.15;
  const FIntPoint Cell=WallCell(Corner.Pos);
  for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y) {
   const auto* Nearby=Cells.Find(Cell+FIntPoint(X,Y));if(!Nearby)continue;
   for(int32 Index:*Nearby) {
    const auto& Candidate=WallSnapshots[Index];
    if(Candidate.Id==Corner.Id||Candidate.Owner!=Corner.Owner||Candidate.Def!=Corner.Def||(!Candidate.Gate&&Candidate.ModelKey!=Corner.ModelKey))continue;
    const FVector2D Offset=Candidate.Pos-Corner.Pos;
    if(FMath::Abs(Offset.Size()-Spacing)>.035)continue;
    ++Adjacent;
    if(Candidate.Gate)continue;
    const FVector2D Direction=Offset.GetSafeNormal();
    const FVector2D CornerAxis(FMath::Cos(Corner.Heading),FMath::Sin(Corner.Heading));
    const FVector2D CandidateAxis(FMath::Cos(Candidate.Heading),FMath::Sin(Candidate.Heading));
    if(Candidate.Id!=Neighbor.Id&&Candidate.Progress>=1&&Candidate.Remembered==Corner.Remembered&&
       FMath::Abs(FVector2D::DotProduct(Direction,CornerAxis))>.999&&
       FMath::Abs(FVector2D::DotProduct(Direction,CandidateAxis))>.999)Back=&Candidate;
   }
  }
  const bool bTrim=Adjacent==2&&Back&&Corner.Progress>=1&&Neighbor.Progress>=1&&Corner.Remembered==Neighbor.Remembered;
  const FVector2D Bisector=bTrim?((Back->Pos-Corner.Pos).GetSafeNormal()-Between).GetSafeNormal():FVector2D::ZeroVector;
  auto AddHalf=[&](const FRTSWallSnapshot3D& Source,const FVector2D& ClipNormal) {
  const auto& Model=*Rendered.FindChecked(Source.Id).Model;
  const FQuat Rotation(FVector::UpVector,Source.Heading);
  for(const FRTSModelPart& Part:Model.Parts) {
   FLinearColor Tint=Part.Color;
   if(Corner.Owner>=0&&(Part.Team||Part.Name.Contains(TEXT("team"))))Tint=FMath::Lerp(Tint,RTS::PlayerColor(Corner.Owner),.68f);
   if(Joint.Remembered)Tint*=.38f;Tint.A=1;
   const FColor Color=Tint.ToFColor(false);
   for(int32 I=0;I+2<Part.Triangles.Num();I+=3) {
    TArray<FVector> Polygon=WallHalfTriangle(Part.Vertices[Part.Triangles[I]],Part.Vertices[Part.Triangles[I+1]],Part.Vertices[Part.Triangles[I+2]],ClipNormal);
    for(FVector& V:Polygon){V.Z*=Height;V=(Rotation.RotateVector(V)+FVector(Corner.Pos,0))*100.;}
    for(int32 Triangle=1;Triangle+1<Polygon.Num();++Triangle) {
     const FVector A=Polygon[0],B=Polygon[Triangle],C=Polygon[Triangle+1];
     const FVector Normal=FVector::CrossProduct(B-A,C-A).GetSafeNormal();if(Normal.IsNearlyZero())continue;
     const int32 Base=WallJointVertices.Num();WallJointVertices.Append({A,B,C});
     // UE clockwise fronts, authored outward normals, matching the base mesh.
     WallJointTriangles.Append({Base,Base+2,Base+1});
     for(int32 Vertex=0;Vertex<3;++Vertex){Normals.Add(Normal);Colors.Add(Color);}
     Joint.Bounds+=A;Joint.Bounds+=B;Joint.Bounds+=C;
    }
   }
  }
  };
  if(bTrim) {
   AddHalf(Corner,Bisector.GetRotated(-FMath::RadiansToDegrees(Corner.Heading)));
   AddHalf(Neighbor,-Bisector.GetRotated(-FMath::RadiansToDegrees(Neighbor.Heading)));
   TrimmedWallCorners.Add(Corner.Id);
  }else AddHalf(Neighbor,FVector2D(Sign,0));
  Joint.NumTriangles=WallJointTriangles.Num()/3-Joint.FirstTriangle;
  if(Joint.NumTriangles>0) {
   WallJointEntries.Add(Joint);
   if(FBox* Existing=WallJointBounds.Find(Corner.Id))*Existing+=Joint.Bounds;
   else WallJointBounds.Add(Corner.Id,Joint.Bounds);
  }
 };

 for(int32 Index=0;Index<WallSnapshots.Num();++Index) {
  const FRTSWallSnapshot3D& A=WallSnapshots[Index];const FIntPoint Cell=WallCell(A.Pos);
  for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y) {
   const TArray<int32>* Nearby=Cells.Find(Cell+FIntPoint(X,Y));if(!Nearby)continue;
   for(int32 Other:*Nearby) {
    if(Other<=Index)continue;const FRTSWallSnapshot3D& B=WallSnapshots[Other];
    if(A.Gate||B.Gate||A.Owner!=B.Owner||A.Def!=B.Def||A.ModelKey!=B.ModelKey)continue;
    const FVector2D Offset=B.Pos-A.Pos;const double Distance=Offset.Size();
    const double Spacing=2.*FRTSCatalog::Get().Def(A.Def).Radius+.15;
    if(FMath::Abs(Distance-Spacing)>.035)continue;
    const FVector2D Direction=Offset/Distance;
    const double AlongA=FMath::Abs(FVector2D::DotProduct(Direction,{FMath::Cos(A.Heading),FMath::Sin(A.Heading)}));
    const double AlongB=FMath::Abs(FVector2D::DotProduct(Direction,{FMath::Cos(B.Heading),FMath::Sin(B.Heading)}));
    // A queued turn has one segment along the new line and a shared corner
    // retaining the previous heading. Leave straight and unrelated walls alone.
    if((AlongA>.999)==(AlongB>.999))continue;
    if(!Sections.Contains(A.ModelKey))Sections.Add(A.ModelKey,WallSection(*Rendered.FindChecked(A.Id).Model));
    const TArray<FVector2D>& Section=Sections.FindChecked(A.ModelKey);
    if(Section.IsEmpty()||Distance-WallReach(Section,A.Heading,Direction)-WallReach(Section,B.Heading,-Direction)<.025)continue;
    if(AlongB>.999)AddJoint(A,B);else AddJoint(B,A);
   }
  }
 }
 if(WallJointTriangles.IsEmpty())WallJoints->ClearAllMeshSections();
 else WallJoints->CreateMeshSection(0,WallJointVertices,WallJointTriangles,Normals,{},Colors,{},false);
 WallJoints->SetMaterial(0,Surface);
 for(int32 Id:PreviouslyTrimmed)if(!TrimmedWallCorners.Contains(Id))if(const auto* Entry=Rendered.Find(Id))if(Entry->Component()) {
  const FVector2D Screen=PC->WorldToScreen(FVector2D(Entry->Component()->GetComponentLocation()/100.));
  Entry->Component()->SetVisibility(Screen.X>-300&&Screen.X<PC->ViewSize.X+300&&Screen.Y>-100&&Screen.Y<PC->ViewSize.Y+300);
 }
 HideTrimmedCorners();
}

void ARTSScene3D::IncludeWallJointBounds(int32 EntityId,FBox2D& Result) const {
 const FBox* Bounds=WallJointBounds.Find(EntityId);if(!Bounds)return;
 for(int32 Corner=0;Corner<8;++Corner)Result+=Project(FVector(Corner&1?Bounds->Max.X:Bounds->Min.X,
  Corner&2?Bounds->Max.Y:Bounds->Min.Y,Corner&4?Bounds->Max.Z:Bounds->Min.Z)/100.);
}

int32 ARTSScene3D::HitWallJoints(FVector2D Pixel,const FVector& Origin,const FVector& Direction,double& Closest) const {
 int32 Best=0;
 for(const FRTSWallJoint3D& Joint:WallJointEntries) {
  if(Joint.Remembered)continue;
  FBox2D Screen(EForceInit::ForceInit);
  for(int32 Corner=0;Corner<8;++Corner)Screen+=Project(FVector(Corner&1?Joint.Bounds.Max.X:Joint.Bounds.Min.X,
   Corner&2?Joint.Bounds.Max.Y:Joint.Bounds.Min.Y,Corner&4?Joint.Bounds.Max.Z:Joint.Bounds.Min.Z)/100.);
  if(!Screen.IsInsideOrOn(Pixel))continue;
  // Snapshot delivery can precede this frame's renderer update. Never return
  // an ID removed, remembered or changed into a gate since the last rebuild.
  const FRTSEntityView* Corner=PC->FindEntity(Joint.EntityId);
  const FRTSEntityView* Neighbor=PC->FindEntity(Joint.NeighborId);
  if(!Corner||!Neighbor||Corner->bRemembered||Neighbor->bRemembered||Corner->HP<=0||Neighbor->HP<=0||Corner->bGate||Neighbor->bGate)continue;
  for(int32 I=Joint.FirstTriangle*3;I<(Joint.FirstTriangle+Joint.NumTriangles)*3;I+=3) {
   double Distance=0;
   if(WallRayTriangle(Origin,Direction,WallJointVertices[WallJointTriangles[I]],WallJointVertices[WallJointTriangles[I+1]],
     WallJointVertices[WallJointTriangles[I+2]],Distance)&&Distance<Closest) {Closest=Distance;Best=Joint.EntityId;}
  }
 }
 return Best;
}
