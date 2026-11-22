#include "RTSHUD.h"
#include "RTSController.h"

void ARTSHUD::DrawRallies() {
 const FLinearColor RallyGold(.94f,.79f,.46f,1),EconomicTeal(.20f,.63f,.59f,1),Raid(.94f,.48f,.26f,1);
 for(int32 Id:PC->Selection)if(const auto* E=PC->FindEntity(Id))if(E->Owner==PC->LocalSlot){
  const auto& D=FRTSCatalog::Get().Def(E->Def);const bool Headquarters=D.Id==TEXT("seat_of_the_people");
  const auto DrawRally=[&](bool Set,FVector2D Point,FLinearColor Color,const FString& Label,float Offset){
   if(!Set)return;const FVector2D A=PC->WorldToScreen(E->Pos)/U,B=PC->WorldToScreen(Point)/U;if(B.Y<65||B.Y>H-228)return;
   Line(A,B,FLinearColor(Color.R,Color.G,Color.B,.55f));Circle(B,10*PC->Zoom,Color,false,.5f);Line(B,B-FVector2D(0,31)*PC->Zoom,Color,2);
   Poly({B-FVector2D(0,31)*PC->Zoom,B+FVector2D(18,-26)*PC->Zoom,B-FVector2D(0,20)*PC->Zoom},Color);
   const float Size=HUDTextSize(13,10.5f),Width=TextWidth(Label,Size)+12;
   const float X=FMath::Clamp(float(B.X)+20,6.f,W-Width-6),Y=FMath::Clamp(float(B.Y)-28,69.f,H-228-2*(Size+14));
   Rect(X,Y+Offset,Width,Size+6,FLinearColor(.025f,.042f,.047f,.9f));
   Text(Label,X+6,Y+Offset+2,Size,FLinearColor(.92f,.88f,.77f,1));
  };
  DrawRally(E->bRallySet,E->RallyPoint,RallyGold,Headquarters?TEXT("Military rally"):TEXT("Rally"),0);
  if(Headquarters)DrawRally(E->bEconomicRallySet,E->EconomicRallyPoint,EconomicTeal,TEXT("Economic rally"),HUDTextSize(13,10.5f)+8);
  if(D.Id==TEXT("storehouse_of_provisions")){
   const auto* Target=PC->FindEntity(E->RaidRallyTarget);DrawRally(E->bRaidRallySet,Target?Target->Pos:E->RaidRallyPoint,Raid,TEXT("Raid route"),0);
  }
 }
}

void ARTSHUD::DrawWallPreview() {
 const FLinearColor RouteColor(.22f,.75f,.69f,.8f),WallColor(.91f,.77f,.46f,.6f),Invalid(.94f,.35f,.26f,.6f);
 for(int32 Id:PC->Selection)if(const auto* Entity=PC->FindEntity(Id)) {
  const bool Market=FRTSCatalog::Get().Def(Entity->Def).Id==TEXT("place_of_trade");
  const TArray<FVector2D>* Route=Market?&Entity->GalavantRoute:PC->GalavantRoutes.Find(Id);if(!Route||Route->IsEmpty())continue;
  FVector2D Previous=PC->WorldToScreen(Entity->Pos)/U;
  for(int32 I=0;I<Route->Num();I++){const FVector2D Next=PC->WorldToScreen((*Route)[I])/U;Line(Previous,Next,RouteColor,1.2f);Circle(Next,5*PC->Zoom,RouteColor,false,.6f);Text(FString::FromInt(I+1),Next.X+7,Next.Y-12,10,RouteColor);Previous=Next;}
  if(Route->Num()>1)Line(Previous,PC->WorldToScreen((*Route)[0])/U,RouteColor,.7f);
 }
 if(PC->PendingAction!=TEXT("build")||!PC->InWorld(PC->Mouse)||(PC->PendingType!=TEXT("works_of_defense")&&PC->PendingType!=TEXT("cement_wall")))return;
 const auto& Def=FRTSCatalog::Get().Def(PC->PendingType);const FVector2D End=PC->ScreenToWorld(PC->Mouse),Start=PC->bWallAnchor?PC->WallAnchor:End;
 const float Spacing=2*Def.Radius+.15f,Distance=FVector2D::Distance(Start,End);const int32 Segments=PC->bWallAnchor?FMath::Min(128,FMath::FloorToInt(Distance/Spacing+.0001f)+1):1;
 FVector2D Direction=End-Start;Direction.Normalize();
 for(int32 I=0;I<Segments;I++) {
  const FVector2D Point=Start+Direction*I*Spacing;const int32 Index=FMath::Clamp(int32(Point.Y/2.5f),0,63)*64+FMath::Clamp(int32(Point.X/2.5f),0,63);
  const bool Valid=Point.X>Def.Radius&&Point.Y>Def.Radius&&Point.X<160-Def.Radius&&Point.Y<160-Def.Radius&&!RTS::WaterAt(Point,PC->Map)&&!RTS::RockAt(Point,PC->Map);
  const float R=Def.Radius;const auto Project=[this](FVector2D P){return PC->WorldToScreen(P)/U;};const FLinearColor Color=Valid?WallColor:Invalid;
  Poly({Project(Point+FVector2D(-R,-R)),Project(Point+FVector2D(R,-R)),Project(Point+FVector2D(R,R)),Project(Point+FVector2D(-R,R))},Color);
 }
 if(PC->bWallAnchor)Line(PC->WorldToScreen(Start)/U,PC->WorldToScreen(Start+Direction*(Segments-1)*Spacing)/U,WallColor,2);
 const FString Title=PC->bWallAnchor?FString::Printf(TEXT("%d wall %s"),Segments,Segments==1?TEXT("segment"):TEXT("segments")):TEXT("Set the first wall endpoint");
 const float TitleSize=HUDTextSize(16,12),Width=TextWidth(Title,TitleSize)+20,Height=TitleSize+10;
 const FVector2D Mouse=PC->Mouse/U;
 const float LabelX=FMath::Clamp(float(Mouse.X)+18,8.f,W-Width-8),LabelY=FMath::Clamp(float(Mouse.Y)-Height-10,69.f,H-228-Height-4);
 Rect(LabelX,LabelY,Width,Height,FLinearColor(.025f,.042f,.047f,.94f));
 Text(Title,LabelX+10,LabelY+5,TitleSize,FLinearColor(.94f,.79f,.46f,1));
}
