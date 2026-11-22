#include "RTSHUD.h"
#include "RTSController.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"

namespace {
const FLinearColor Carving(.47f,.34f,.18f,1);
const FLinearColor CarvingLight(.87f,.69f,.39f,1);
const FLinearColor Inscription(.91f,.84f,.70f,1);
const FLinearColor InsetText(.67f,.61f,.49f,1);

struct FPlateLeaf {
 const TCHAR* Name;
 const TCHAR* Passage;
 const TCHAR* Reference;
};

const FPlateLeaf PlateLeaves[]={
 {TEXT("Nephi"),TEXT("Nephi kept two records of his people. The smaller plates carried their sacred account, alongside the fuller record of their history."),TEXT("1 Nephi 9")},
 {TEXT("Mormon"),TEXT("While gathering the records, Mormon found the small plates and placed them with the account he was preparing to leave behind."),TEXT("Words of Mormon 1")}
};
constexpr int32 VesselCount=8;
constexpr uint8 AllVessels=(1<<VesselCount)-1;
}

void ARTSHUD::DrawMenuLandscape() {
 const auto Point=[this](float X,float Y) {return FVector2D(X*W,Y*H);};
 const auto Gradient=[this](FVector2D Size,FLinearColor Start,FLinearColor End,bool bVertical) {
  const FVector2D TopRight(Size.X*U,0),BottomLeft(0,Size.Y*U),BottomRight=Size*U;
  FCanvasTileItem White(FVector2D::ZeroVector,FVector2D(1,1),FLinearColor::White);
  FCanvasTriangleItem Quad(FVector2D::ZeroVector,TopRight,BottomRight,White.Texture);
  Quad.BlendMode=SE_BLEND_Translucent;
  FCanvasUVTri& Upper=Quad.TriangleList[0];
  Upper.V0_Color=Start;
  Upper.V1_Color=bVertical?Start:End;
  Upper.V2_Color=End;
  FCanvasUVTri Lower=Upper;
  Lower.V1_Pos=BottomRight;
  Lower.V1_Color=End;
  Lower.V2_Pos=BottomLeft;
  Lower.V2_Color=bVertical?End:Start;
  Quad.TriangleList.Add(Lower);
  Canvas->DrawItem(Quad);
 };
 const FLinearColor Sky(.065f,.087f,.095f,1), Haze(.36f,.24f,.16f,1);
 Gradient(FVector2D(W,H*.62f),Sky,Haze,true);

 const FVector2D Sun=Point(.767f,.302f);
 Circle(Sun,112,FLinearColor(.78f,.49f,.23f,.08f));
 Circle(Sun,91,FLinearColor(.83f,.52f,.26f,.12f));
 Circle(Sun,55,FLinearColor(.91f,.67f,.38f,1));
 for(int32 I=0;I<4;I++) {
  const float X=W*(.53f+.041f*(I%3)),Y=H*(.356f+I*.022f);
  Rect(X,Y,W*.963f-X,1,FLinearColor(.62f,.43f,.29f,.26f));
 }

 const FVector2D Peaks[]={Point(0,.56f),Point(.19f,.46f),Point(.30f,.53f),Point(.49f,.36f),Point(.60f,.50f),Point(.69f,.438f),Point(.79f,.53f),Point(.91f,.408f),Point(1,.50f)};
 for(int32 I=0;I<UE_ARRAY_COUNT(Peaks)-1;I++) {
  const FVector2D Left=Peaks[I],Right=Peaks[I+1];
  const FLinearColor Face=I%2?FLinearColor(.24f,.19f,.15f,1):FLinearColor(.29f,.215f,.16f,1);
  Poly({Left,Right,FVector2D(Right.X,H*.69f),FVector2D(Left.X,H*.69f)},Face);
  const FVector2D Peak=Left.Y<Right.Y?Left:Right;
  const FVector2D Valley=Left.Y<Right.Y?Right:Left;
  Poly({Peak,Valley,FVector2D(FMath::Lerp(Peak.X,Valley.X,.45),H*.64f)},FLinearColor(.38f,.275f,.19f,.35f));
 }

 Rect(0,H*.61f,W,H*.39f,FLinearColor(.071f,.105f,.104f,1));
 Poly({Point(0,.57f),Point(.26f,.52f),Point(.66f,.58f),Point(.65f,.70f),Point(0,.87f)},FLinearColor(.113f,.145f,.129f,1));
 Poly({Point(.72f,.58f),Point(1,.50f),Point(1,.94f),Point(.65f,.72f)},FLinearColor(.14f,.162f,.127f,1));
 Poly({Point(.67f,.585f),Point(.72f,.58f),Point(.77f,.64f),Point(.71f,.66f)},FLinearColor(.41f,.34f,.23f,1));
 Poly({Point(.71f,.66f),Point(.77f,.64f),Point(.69f,.75f),Point(.58f,.76f)},FLinearColor(.24f,.34f,.29f,1));
 Poly({Point(.58f,.76f),Point(.69f,.75f),Point(.76f,.843f),Point(.63f,.88f)},FLinearColor(.15f,.26f,.24f,1));
 Poly({Point(.63f,.88f),Point(.76f,.843f),Point(.92f,1),Point(.70f,1)},FLinearColor(.14f,.225f,.226f,1));
 Poly({Point(.73f,.655f),Point(.753f,.648f),Point(.69f,.714f),Point(.667f,.717f)},FLinearColor(.62f,.50f,.30f,.55f));
 Poly({Point(.56f,.72f),Point(.58f,.76f),Point(.63f,.88f),Point(.50f,1),Point(.39f,1)},FLinearColor(.09f,.115f,.104f,1));

 const FVector2D Terrace=Point(.845f,.734f);
 const auto Stone=[this,&Terrace](float HalfWidth,float Top,float Height) {
  const FVector2D L=Terrace+FVector2D(-HalfWidth,Top),R=Terrace+FVector2D(HalfWidth,Top);
  Poly({L,R,R+FVector2D(-18,Height),L+FVector2D(18,Height)},FLinearColor(.31f,.245f,.169f,1));
  Poly({R,R+FVector2D(43,-18),R+FVector2D(26,Height-17),R+FVector2D(-18,Height)},FLinearColor(.137f,.141f,.112f,1));
  Line(L,R,FLinearColor(.59f,.451f,.282f,.72f),1.6f);
  for(int32 Block=1;Block<5;Block++) {
   const FVector2D Joint=FMath::Lerp(L,R,Block/5.f);
   Line(Joint,Joint+FVector2D(Block<3?7:-7,Height),FLinearColor(.15f,.13f,.093f,.7f));
  }
 };
 for(int32 Tier=0;Tier<5;Tier++) Stone(156-Tier*24,-Tier*25,26);
 Stone(49,-146,46);
 Rect(Terrace.X-20,Terrace.Y-127,31,28,FLinearColor(.064f,.068f,.055f,1));
 Line(Terrace+FVector2D(-45,-147),Terrace+FVector2D(49,-147),CarvingLight,1.5f);

 for(int32 I=0;I<19;I++) {
  const float T=float(I)/18;
  const float Direction=I%2?1.f:-1.f;
  const FVector2D Base=Point(.83f+Direction*(.026f+.16f*T),.79f+.16f*T);
  const float Height=16+I%6*4;
  const FLinearColor Leaves=I%3?FLinearColor(.105f,.155f,.12f,1):FLinearColor(.17f,.19f,.116f,1);
  Poly({Base+FVector2D(-10,0),Base+FVector2D(-4,-Height*.7f),Base+FVector2D(3,-Height),Base+FVector2D(13,-Height*.30f),Base+FVector2D(6,2)},Leaves);
 }

 Gradient(FVector2D(W*.74f,H),FLinearColor(.043f,.044f,.034f,.92f),FLinearColor(.043f,.044f,.034f,0),false);
 const FLinearColor Trim(.46f,.34f,.21f,.55f);
 Line(FVector2D(34,35),FVector2D(W-34,35),Trim);
 Line(FVector2D(34,H-35),FVector2D(W-34,H-35),Trim);
}

bool ARTSHUD::HandleRelicAction(const FString& Action,const FString& Value) {
 if(!PC->bMenu||PC->MenuScreen!=0||!Action.StartsWith(TEXT("relic_")))return false;
 if(Action==TEXT("relic_plates"))OpenRelic=OpenRelic==EMenuRelic::Plates?EMenuRelic::None:EMenuRelic::Plates;
 else if(Action==TEXT("relic_vessels"))OpenRelic=OpenRelic==EMenuRelic::Vessels?EMenuRelic::None:EMenuRelic::Vessels;
 else if(Action==TEXT("relic_close"))OpenRelic=EMenuRelic::None;
 else if(Action==TEXT("relic_read")) {
  const int32 LastLeaf=UE_ARRAY_COUNT(PlateLeaves)-1;
  PlateLeaf=FMath::Clamp(FCString::Atoi(*Value),0,LastLeaf);
 }
 else if(Action==TEXT("relic_light")) {
  const int32 Vessel=FCString::Atoi(*Value);
  if(Vessel>=0&&Vessel<VesselCount) {
   const uint8 Before=LitVessels;
   LitVessels|=uint8(1<<Vessel);
   if(Before!=AllVessels&&LitVessels==AllVessels) {
    PC->PlayCue(TEXT("complete"));
    return true;
   }
  }
 }
 PC->PlayCue(TEXT("ui"));
 return true;
}

void ARTSHUD::DrawRelics() {
 const float X=W-690,Y=H-149;
 const auto Gem=[this](FVector2D C,float Radius,FLinearColor Color) {
  Poly({C+FVector2D(0,-Radius),C+FVector2D(.8f*Radius,0),C+FVector2D(0,Radius),C+FVector2D(-.8f*Radius,0)},Color);
  Line(C+FVector2D(0,-Radius),C+FVector2D(-.8f*Radius,0),CarvingLight);
 };
 const auto Tablet=[this](float TX,float TY) {
  Poly({FVector2D(TX+8,TY),FVector2D(TX+57,TY),FVector2D(TX+64,TY+43),FVector2D(TX+6,TY+43),FVector2D(TX,TY+37)},Carving);
  Line(FVector2D(TX+9,TY+2),FVector2D(TX+54,TY+2),CarvingLight,1.4f);
  for(int32 Row=0;Row<3;Row++) {
   const float YY=TY+11+Row*9;
   Line(FVector2D(TX+15,YY),FVector2D(TX+49-Row*4,YY),CarvingLight,1.2f);
  }
  for(int32 Ring=0;Ring<2;Ring++)Circle(FVector2D(TX+8,TY+12+Ring*17),4.5f,CarvingLight,false);
 };
 Panel(X+366,Y,228,76);
 Button(X+380,Y+4,88,68,FString(),TEXT("relic_plates"),FString(),true,TEXT("Examine the etched plates."),OpenRelic==EMenuRelic::Plates);
 Tablet(X+391,Y+14);
 Button(X+492,Y+4,88,68,FString(),TEXT("relic_vessels"),FString(),true,TEXT("Examine the paired stones."),OpenRelic==EMenuRelic::Vessels);
 const FLinearColor Stones=LitVessels==AllVessels?CarvingLight:Carving;
 Gem(FVector2D(X+524,Y+38),14,Stones);
 Gem(FVector2D(X+546,Y+34),11,Stones);

 if(OpenRelic==EMenuRelic::None)return;
 const float TY=Y-308;
 Panel(X,TY,595,297,true);
 Button(X+507,TY+12,72,49,TEXT("Close"),TEXT("relic_close"));
 if(OpenRelic==EMenuRelic::Plates) {
  Tablet(X+26,TY+21);
  Text(TEXT("The small plates"),X+106,TY+26,31,Inscription);
  for(int32 I=0;I<UE_ARRAY_COUNT(PlateLeaves);I++) {
   Button(X+25+I*136,TY+89,124,49,PlateLeaves[I].Name,TEXT("relic_read"),FString::FromInt(I),true,FString(),I==PlateLeaf);
  }
  const FPlateLeaf& Leaf=PlateLeaves[PlateLeaf];
  Wrap(Leaf.Passage,X+27,TY+158,526,19,Inscription,3);
  Text(Leaf.Reference,X+27,TY+254,14,InsetText);
  return;
 }
 Text(TEXT("Lights for the crossing"),X+24,TY+22,29,Inscription);
 for(int32 I=0;I<VesselCount;I++) {
  const bool bLit=(LitVessels&(1<<I))!=0;
  const float VX=X+25+(I%4)*138,VY=TY+79+(I/4)*69;
  Button(VX,VY,129,60,FString(),TEXT("relic_light"),FString::FromInt(I),true,FString(),bLit);
  const FVector2D C(VX+64,VY+32);
  Poly({C+FVector2D(-45,-10),C+FVector2D(45,-10),C+FVector2D(31,12),C+FVector2D(-31,12)},Carving);
  Line(C+FVector2D(-45,-10),C+FVector2D(45,-10),CarvingLight);
  for(float Offset:{-29.f,29.f}) {
   const FVector2D Stone=C+FVector2D(Offset,-7);
   if(bLit)Circle(Stone,16,FLinearColor(.99f,.68f,.27f,.23f));
   Gem(Stone,8,bLit?FLinearColor(1,.89f,.56f,1):FLinearColor(.29f,.26f,.22f,1));
  }
 }
 if(LitVessels==AllVessels) {
  Wrap(TEXT("From Mount Shelem to the far shore, two stones kept each vessel alight through the crossing.  Ether 3 and 6"),X+25,TY+222,540,17,Inscription,3);
 } else {
  Text(TEXT("Light the carvings and look for the pairs."),X+25,TY+243,16,InsetText);
 }
}
