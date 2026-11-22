#include "RTSScene3D.h"
#include "RTSController.h"
#include "ProceduralMeshComponent.h"

// Visual flight follows a replicated strike. It never deals damage, performs
// collision queries, or reaches beyond the client's filtered snapshot.
void ARTSScene3D::SpawnProjectile(const FRTSEntityView& E,const FRTSRendered3D& R,float Now) {
 const auto& Def=FRTSCatalog::Get().Def(E.Def);
 if(Def.Range<=3||!R.Component()||!R.Component()->IsVisible()||PC->bReducedMotion)return;
 const auto* Target=PC->FindEntity(E.Target);if(!Target||Target->bRemembered||E.bRemembered)return;
 if(Shots.Num()>=128)Shots.RemoveAt(0,1,false);
 FRTSProjectile3D Shot;Shot.Source=E.Id;Shot.Target=Target->Id;Shot.Started=Now;
 const FVector Release=R.PosedSockets.Contains(TEXT("projectile_release"))?R.PosedSockets.FindChecked(TEXT("projectile_release")):FVector(0,0,FMath::Max(.8,R.Model->Bounds.Max.Z*.72));
 Shot.Start=R.Component()->GetComponentTransform().TransformPosition(Release*100);
 float Height=.85f;
 if(const auto* TargetMesh=Rendered.Find(Target->Id))if(TargetMesh->Model)Height=FMath::Clamp(float(TargetMesh->Model->Bounds.Max.Z*.55),.3f,3.f);
 Shot.End=FVector(PC->VisualPosition(*Target),Height)*100;
 Shot.Duration=FMath::Clamp(float(FVector::Distance(Shot.Start,Shot.End)/2200),.18f,.65f);
 Shot.Stone=R.Model->Equipment==TEXT("sling")||R.Model->Equipment==TEXT("stone");
 Shots.Add(Shot);
}

void ARTSScene3D::DrawProjectiles(float Now) {
 Shots.RemoveAll([&](const FRTSProjectile3D& Shot){return Now-Shot.Started>=Shot.Duration||Now<Shot.Started;});
 Projectiles->SetVisibility(!Shots.IsEmpty()&&!PC->bReducedMotion);
 if(Shots.IsEmpty()||PC->bReducedMotion)return;
 TArray<FVector> Vertices,Normals;TArray<int32> Triangles;TArray<FLinearColor> Colors;
 Vertices.Reserve(Shots.Num()*60);Normals.Reserve(Shots.Num()*60);Colors.Reserve(Shots.Num()*60);
 const auto Face=[&](FVector A,FVector B,FVector C,FLinearColor Color) {
  const int Base=Vertices.Num();const FVector N=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
  Vertices.Append({A,B,C});Triangles.Append({Base,Base+2,Base+1});
  Normals.Append({N,N,N});Colors.Append({Color,Color,Color});
 };
 for(const auto& Shot:Shots) {
  const float T=FMath::Clamp((Now-Shot.Started)/Shot.Duration,0.f,1.f);
  const float Arc=Shot.Stone?120.f:45.f;
  const FVector Point=FMath::Lerp(Shot.Start,Shot.End,T)+FVector(0,0,4*Arc*T*(1-T));
  const int FogX=FMath::FloorToInt(Point.X/250),FogY=FMath::FloorToInt(Point.Y/250),FogCell=FogY*64+FogX;
  if(FogX<0||FogX>=64||FogY<0||FogY>=64||!PC->Fog.IsValidIndex(FogCell)||PC->Fog[FogCell]!=2)continue;
  FVector Forward=(Shot.End-Shot.Start+FVector(0,0,4*Arc*(1-2*T))).GetSafeNormal();
  FVector Side=FVector::CrossProduct(Forward,FVector::UpVector).GetSafeNormal();
  if(Side.IsNearlyZero())Side=FVector::YAxisVector;
  const FVector Up=FVector::CrossProduct(Side,Forward).GetSafeNormal();
  if(Shot.Stone) {
   const FVector Ring[]={Point+Forward*12,Point+Side*11,Point-Forward*12,Point-Side*11};
   for(int I=0;I<4;++I){Face(Point+Up*13,Ring[I],Ring[(I+1)%4],FLinearColor(.55f,.50f,.39f));Face(Point-Up*13,Ring[(I+1)%4],Ring[I],FLinearColor(.43f,.38f,.29f));}
  } else {
   const FVector Tail=Point-Forward*72,Neck=Point-Forward*11;
   const FVector Cross[]={Side*1.8,Up*1.8,-Side*1.8,-Up*1.8};
   for(int I=0;I<4;++I) {
    const int J=(I+1)%4;
    Face(Tail+Cross[I],Tail+Cross[J],Neck+Cross[J],FLinearColor(.67f,.47f,.25f));
    Face(Tail+Cross[I],Neck+Cross[J],Neck+Cross[I],FLinearColor(.67f,.47f,.25f));
    Face(Point,Neck+Cross[I]*3,Neck+Cross[J]*3,FLinearColor(.80f,.76f,.58f));
    Face(Tail,Tail+Forward*20,Tail+Forward*5+Cross[I]*4,FLinearColor(.92f,.85f,.61f));
   }
  }
 }
 Projectiles->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,{},Colors,{},false,false);
 Projectiles->SetMaterial(0,Surface);
}
