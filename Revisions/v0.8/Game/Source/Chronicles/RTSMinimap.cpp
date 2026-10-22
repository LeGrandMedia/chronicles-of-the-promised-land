#include "RTSHUD.h"
#include "RTSController.h"
#include "Engine/World.h"

namespace {
 const FLinearColor MiniInk(.025f,.042f,.047f,1),MiniBrass(.76f,.58f,.29f,1),MiniGold(.94f,.79f,.46f,1),MiniPaper(.89f,.89f,.79f,1);
 FLinearColor MiniShade(FLinearColor C,float M){return FLinearColor(C.R*M,C.G*M,C.B*M,C.A);}
 double MapFit(double Angle){return 1.0/(FMath::Abs(FMath::Cos(Angle))+FMath::Abs(FMath::Sin(Angle)));}
 // Clip the actual camera footprint to the world before projecting it. Clamping
 // individual corners distorts the outline when the camera crosses a map edge.
 TArray<FVector2D> ClipToMap(TArray<FVector2D> Points) {
  for(int32 Edge=0;Edge<4&&!Points.IsEmpty();Edge++){
   TArray<FVector2D> Result;const bool X=Edge<2,Upper=(Edge%2)!=0;const double Boundary=Upper?RTS::MapSize:0;
   const auto Coord=[X](FVector2D P){return X?P.X:P.Y;};
   const auto Inside=[&](FVector2D P){return Upper?Coord(P)<=Boundary:Coord(P)>=Boundary;};
   FVector2D Previous=Points.Last();bool PrevInside=Inside(Previous);
   for(FVector2D Current:Points){const bool CurrentInside=Inside(Current);
    if(CurrentInside!=PrevInside){const double T=(Boundary-Coord(Previous))/(Coord(Current)-Coord(Previous));Result.Add(Previous+(Current-Previous)*T);}
    if(CurrentInside)Result.Add(Current);Previous=Current;PrevInside=CurrentInside;
   }Points=MoveTemp(Result);
  }return Points;
 }
}

bool ARTSHUD::MinimapProject(FVector2D World,FVector2D& Pixel) const {
 if(!PC||!MapBounds.bIsValid)return false;
 const FVector2D Center(RTS::MapSize*.5,RTS::MapSize*.5);
 Pixel=MapBounds.GetCenter()+PC->RotateMap(World-Center)*MapBounds.GetSize().X/RTS::MapSize*MapFit(PC->CameraRotation);
 return true;
}

bool ARTSHUD::MinimapPoint(FVector2D Pixel,FVector2D& World) const {
 if(!PC||PC->bMenu||PC->Phase==0||!MapBounds.bIsValid||Pixel.X<MapBounds.Min.X-.001||Pixel.Y<MapBounds.Min.Y-.001||Pixel.X>MapBounds.Max.X+.001||Pixel.Y>MapBounds.Max.Y+.001)return false;
 const FVector2D Center(RTS::MapSize*.5,RTS::MapSize*.5);
 World=Center+PC->RotateMap((Pixel-MapBounds.GetCenter())*RTS::MapSize/MapBounds.GetSize().X/MapFit(PC->CameraRotation),true);
 const double Epsilon=.001;
 if(World.X<-Epsilon||World.Y<-Epsilon||World.X>RTS::MapSize+Epsilon||World.Y>RTS::MapSize+Epsilon)return false;
 World.X=FMath::Clamp(World.X,0.,double(RTS::MapSize));World.Y=FMath::Clamp(World.Y,0.,double(RTS::MapSize));return true;
}

void ARTSHUD::DrawMinimap() {
 const float X=21,Y=H-212,Size=184,MX=X+14,MY=Y+18;
 Panel(X,Y-3,212,211,true);
 MapBounds=FBox2D(FVector2D(MX*U,MY*U),FVector2D((MX+Size)*U,(MY+Size)*U));
 const auto Project=[this](FVector2D World){FVector2D Pixel;MinimapProject(World,Pixel);return Pixel/U;};
 Rect(MX,MY,Size,Size,MiniInk);
 const float FailureAge=GetWorld()->GetRealTimeSeconds()-PC->LiahonaFailureTime;
 const bool ShowMap=PC->bLocalLiahona||(FailureAge<1.f&&!PC->bReducedMotion);
 if(ShowMap){
  for(int32 YY=0;YY<64;YY++)for(int32 XX=0;XX<64;XX++){
   const uint8 Fog=PC->Fog.IsValidIndex(YY*64+XX)?PC->Fog[YY*64+XX]:0;const FVector2D P((XX+.5)*2.5,(YY+.5)*2.5);
   FLinearColor C=RTS::WaterAt(P,PC->Map)?FLinearColor(.12f,.31f,.35f,1):RTS::RockAt(P,PC->Map)?FLinearColor(.42f,.42f,.32f,1):FLinearColor(.29f,.36f,.21f,1);
   if(Fog==0)C=MiniInk;else if(Fog==1)C=MiniShade(C,.4f);
   const FVector2D Corner(XX*2.5,YY*2.5);
   Poly({Project(Corner),Project(Corner+FVector2D(2.5,0)),Project(Corner+FVector2D(2.5,2.5)),Project(Corner+FVector2D(0,2.5))},C);
  }
  for(const auto& E:PC->Entities){
   const auto& D=FRTSCatalog::Get().Def(E.Def);if(E.HP<=0||(D.Kind==TEXT("resource")&&D.Id!=TEXT("tree")&&D.Id!=TEXT("ore")))continue;
   FVector2D P=Project(E.Pos);
   if(!PC->bLocalLiahona)P+=FVector2D(FMath::Sin(E.Id+FailureAge*20),FMath::Cos(E.Id+FailureAge*17))*FailureAge*13;
   const FLinearColor Mark=E.Owner>=0?RTS::PlayerColor(E.Owner):D.Id==TEXT("tree")?FLinearColor(.25f,.48f,.20f,1):D.Id==TEXT("ore")?MiniBrass:MiniPaper;
   const float S=D.bBuilding?4.5f:2.5f;Rect(FMath::Clamp(float(P.X-S*.5),MX,MX+Size-S),FMath::Clamp(float(P.Y-S*.5),MY,MY+Size-S),S,S,E.bRemembered?MiniShade(Mark,.5f):Mark);
  }
  const FVector2D North=Project(FVector2D(80,0));Text(TEXT("N"),North.X,North.Y+2,10,MiniGold,true);
  const FVector2D Pixels[]={FVector2D(0,65*U),FVector2D(W*U,65*U),FVector2D(W*U,(H-228)*U),FVector2D(0,(H-228)*U)};
  TArray<FVector2D> View;for(FVector2D Pixel:Pixels)View.Add(PC->ScreenToWorld(Pixel));View=ClipToMap(MoveTemp(View));
  for(int32 I=0;I<View.Num();I++)Line(Project(View[I]),Project(View[(I+1)%View.Num()]),FLinearColor(.94f,.89f,.65f,.8f),.8f);
 }else{DrawEmblem(FVector2D(MX+Size*.5f,MY+Size*.5f-8),42,MiniShade(MiniBrass,.45f));Text(TEXT("GUIDANCE LOST"),MX+Size*.5f,MY+Size*.5f+46,12,MiniBrass,true);}
 const float Rot=(!PC->bLocalLiahona&&FailureAge<1&&!PC->bReducedMotion)?FailureAge*25:0;
 DrawEmblem(FVector2D(X+106,Y+7),13,MiniGold,Rot);
 Text(PC->bLocalLiahona?TEXT("L I A H O N A"):TEXT("SUBMIT  sorry  TO RESTORE"),X+106,Y+200,PC->bLocalLiahona?11:9,MiniBrass,true);
 for(FVector2D P:{FVector2D(X+4,Y+4),FVector2D(X+208,Y+4),FVector2D(X+4,Y+204),FVector2D(X+208,Y+204)})Circle(P,2.3f,MiniGold);
}
