#include "RTSMaps.h"
#include "RTSTypes.h"

namespace {
const FLinearColor Green(.38f,.49f,.22f),Earth(.54f,.50f,.31f),Blue(.16f,.35f,.43f),Stone(.51f,.47f,.36f);
const FRTSMapInfo Maps[]={
 {TEXT("zarahemla"),TEXT("Zarahemla Valley"),TEXT("Open ground with two rocky landmarks."),Green,Earth,Blue,Stone,12},
 {TEXT("sidon"),TEXT("Waters of Sidon"),TEXT("Three rocky divides and wide flanking routes."),Green,Earth,Blue,Stone,12},
 {TEXT("many_waters"),TEXT("Land of Many Waters"),TEXT("A river divides the banks. Cross at two fords."),Green,Earth,Blue,Stone,12},
 {TEXT("bountiful"),TEXT("Bountiful"),TEXT("Green coastal plains, sheltered bays and broad approaches."),{.32f,.52f,.24f},{.58f,.57f,.35f},{.12f,.43f,.49f},Stone,12},
 {TEXT("moronihah"),TEXT("Moronihah"),TEXT("A broken mountain ring surrounds the central crossing."),{.40f,.43f,.25f},{.58f,.49f,.34f},Blue,{.49f,.42f,.35f},14},
 {TEXT("ammonihah"),TEXT("Ammonihah"),TEXT("Ruined stone quarters divide an open central avenue."),{.43f,.43f,.25f},{.62f,.53f,.36f},Blue,{.57f,.49f,.40f},14},
 {TEXT("northward"),TEXT("The Land Northward"),TEXT("Sparse woodland and rocky ridges reward open-field armies."),{.44f,.46f,.29f},{.64f,.57f,.39f},Blue,{.59f,.56f,.48f},16},
 {TEXT("southward"),TEXT("The Land Southward"),TEXT("Wooded riverbanks with three wide crossings."),{.28f,.45f,.22f},{.49f,.48f,.28f},{.14f,.35f,.39f},Stone,12},
 {TEXT("narrow_neck"),TEXT("The Narrow Neck of Land"),TEXT("Two seas leave one broad passage between the opposing shores."),{.38f,.50f,.26f},{.65f,.61f,.40f},{.12f,.38f,.48f},Stone,14},
 {TEXT("middoni"),TEXT("Middoni"),TEXT("Rocky enclosures hide approach routes around a central valley."),{.38f,.44f,.24f},{.57f,.48f,.31f},Blue,{.51f,.43f,.34f},14},
 {TEXT("cumorah"),TEXT("The Hill Cumorah"),TEXT("Four approaches meet around a great rocky hill."),{.38f,.47f,.26f},{.55f,.54f,.36f},Blue,{.56f,.54f,.44f},14},
 {TEXT("mormon"),TEXT("The Waters of Mormon"),TEXT("A sheltered lake with wooded shores and room to flank."),{.28f,.48f,.28f},{.49f,.53f,.33f},{.11f,.40f,.44f},Stone,12},
 {TEXT("ripliancum"),TEXT("Waters of Ripliancum"),TEXT("A wide lake divides armies; fleets can cross its open waters."),{.39f,.47f,.29f},{.57f,.55f,.37f},{.12f,.32f,.44f},Stone,14},
 {TEXT("lemuel"),TEXT("The Valley of Lemuel"),TEXT("A long dry valley. Plenty of room to murmur and maneuver."),{.45f,.44f,.25f},{.68f,.56f,.35f},Blue,{.57f,.45f,.32f},16},
 {TEXT("spacious_courtyard"),TEXT("The Great and Spacious Courtyard"),TEXT("Wide lanes, four stone terraces, and absolutely no humility."),{.46f,.48f,.30f},{.65f,.59f,.43f},Blue,{.62f,.56f,.44f},16}
};
bool Ellipse(FVector2D P,double X,double Y,double RX,double RY) {
 return FMath::Square((P.X-X)/RX)+FMath::Square((P.Y-Y)/RY)<1.;
}
bool StartClearing(FVector2D P) {
 const double X=FMath::Min(P.X,160.-P.X),Y=FMath::Min(P.Y,160.-P.Y);
 return FMath::Square(X-24)+FMath::Square(Y-24)<21.*21.;
}
bool RawWater(FVector2D P,int32 Map) {
 switch(Map) {
 case 2:return P.X>72&&P.X<88;
 case 3:return Ellipse(P,80,0,24,28)||Ellipse(P,80,160,24,28);
 case 7:return FMath::Abs(P.X-80)<6.+2.*FMath::Cos((P.Y-80)*PI/40.);
 case 8:return P.Y>56&&P.Y<104&&FMath::Abs(P.X-80)>13.+.12*FMath::Abs(P.Y-80);
 case 11:return Ellipse(P,80,80,24,17);
 case 12:return Ellipse(P,80,80,29,49);
 default:return false;
 }
}
}
int32 RTS::MapCount(){return UE_ARRAY_COUNT(Maps);}
const FRTSMapInfo& RTS::MapInfo(int32 Map){return Maps[FMath::Clamp(Map,0,MapCount()-1)];}
bool RTS::NavigableWaterAt(FVector2D P,int32 Map){return RawWater(P,Map);}
bool RTS::WaterAt(FVector2D P,int32 Map) {
 if(Map==2&&((P.Y>34&&P.Y<46)||(P.Y>114&&P.Y<126)))return false;
 if(Map==7&&(FMath::Abs(P.Y-40)<6||FMath::Abs(P.Y-80)<6||FMath::Abs(P.Y-120)<6))return false;
 return RawWater(P,Map);
}
bool RTS::RockAt(FVector2D P,int32 Map) {
 if(Map>=3&&(StartClearing(P)||RawWater(P,Map)))return false;
 const double X=FMath::Abs(P.X-80),Y=FMath::Abs(P.Y-80),R=X*X+Y*Y;
 switch(Map) {
 case 0:return Ellipse(P,80,55,5,5)||Ellipse(P,80,105,5,5);
 case 1:return R<64||Ellipse(P,48,80,4,4)||Ellipse(P,112,80,4,4);
 case 3:return X>43&&X<50&&Y<9;
 case 4:return R>20*20&&R<30*30&&X>8&&Y>8;
 case 5:return X>11&&X<24&&Y>11&&Y<24;
 case 6:return X>25&&X<33&&Y>12&&Y<45;
 case 7:return X>36&&X<43&&Y<10;
 case 8:return false;
 case 9:return (X>17&&X<34&&Y>18&&Y<23)||(X>29&&X<34&&Y>10&&Y<29);
 case 10:return R<13*13||(R>24*24&&R<29*29&&X>10&&Y>10);
 case 11:return X>34&&X<40&&Y<9;
 case 12:return X>39&&X<44&&Y>12&&Y<25;
 case 13:return Y>22&&Y<30&&X>9&&X<47;
 case 14:return X>18&&X<28&&Y>18&&Y<28;
 default:return false;
 }
}
bool RTS::TerrainClear(FVector2D P,float R,int32 Map,bool Ship) {
 if(!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)||P.X<R||P.Y<R||P.X>MapSize-R||P.Y>MapSize-R)return false;
 if(Map<3)return Ship?(Map==2&&P.X>=72+R&&P.X<=88-R):!WaterAt(P,Map)&&!RockAt(P,Map);
 auto Clear=[&](FVector2D At){return Ship?NavigableWaterAt(At,Map):!WaterAt(At,Map)&&!RockAt(At,Map);};
 if(!Clear(P))return false;
 const double D=R*.70710678118;
 for(const FVector2D Offset:{FVector2D(R,0),FVector2D(-R,0),FVector2D(0,R),FVector2D(0,-R),FVector2D(D,D),FVector2D(-D,D),FVector2D(D,-D),FVector2D(-D,-D)})if(!Clear(P+Offset))return false;
 return true;
}
bool RTS::DockSite(FVector2D P,float Radius,int32 Map) {
 if(!TerrainClear(P,Radius,Map))return false;
 // Same launch rings as production: a preview cannot promise an inland dock
 // whose queued boats have nowhere to appear.
 for(int32 Ring=0;Ring<5;++Ring)for(int32 I=0;I<24;++I){const float A=I*PI/12;
  if(TerrainClear(P+FVector2D(FMath::Cos(A),FMath::Sin(A))*(Radius+1.05f+.5f+Ring*.75f),1.05f,Map,true))return true;
 }
 return false;
}
FLinearColor RTS::TerrainColor(FVector2D P,int32 Map) {
 const auto& Info=MapInfo(Map);
 if(WaterAt(P,Map))return Info.Water;
 if(RockAt(P,Map))return Info.Rock;
 const float Dry=.5f+.5f*FMath::Sin(P.X*.034f+P.Y*.012f)*FMath::Cos(P.Y*.044f);
 return FMath::Lerp(Info.Grass,Info.Dry,Dry);
}
bool RTS::MapGroveAt(FVector2D P,int32 Map) {
 if(StartClearing(P))return false;
 if(Map>=3) {
  // Keep the four main approach lanes broad enough for elephants and chariots.
  if(FMath::Abs(P.X-80)<6||FMath::Abs(P.Y-80)<6)return false;
  if(Map==14&&FMath::Abs(P.X-80)<38&&FMath::Abs(P.Y-80)<38)return false;
 }
 return TerrainClear(P,2.f,Map);
}
float RTS::TerrainHeight(FVector2D P,int32 Map) {
 if(Map<3||!RockAt(P,Map))return 0;
 const double X=FMath::Abs(P.X-80),Y=FMath::Abs(P.Y-80),R=FMath::Sqrt(X*X+Y*Y);
 if(Map==10&&R<13)return 7.f*FMath::Pow(FMath::Max(0.,1.-R/13.),.65);
 if(Map==4)return 5.5f*FMath::Clamp(1.-FMath::Abs(R-25.)/5.,0.,1.)*FMath::Clamp((FMath::Min(X,Y)-8.)/4.,0.,1.);
 if(Map==10)return 3.5f*FMath::Clamp(1.-FMath::Abs(R-26.5)/2.5,0.,1.)*FMath::Clamp((FMath::Min(X,Y)-10.)/3.,0.,1.);
 // Low, solid terraces distinguish the city maps from natural ridges.
 if(Map==5)return 1.4f*FMath::Clamp(FMath::Min(FMath::Min(X-11.,24.-X),FMath::Min(Y-11.,24.-Y))/2.,0.,1.);
 if(Map==14)return 1.6f*FMath::Clamp(FMath::Min(FMath::Min(X-18.,28.-X),FMath::Min(Y-18.,28.-Y))/2.,0.,1.);
 // Smooth shared vertices replace the old field of independent pyramids.
 float Edge=3.f;for(const FVector2D Offset:{FVector2D(1,0),FVector2D(-1,0),FVector2D(0,1),FVector2D(0,-1)})
  for(int I=1;I<=3;++I)if(!RockAt(P+Offset*I,Map)){Edge=FMath::Min(Edge,float(I-1)*.9f);break;}
 return Edge*(.85f+.15f*FMath::Cos(P.X*.3f)*FMath::Cos(P.Y*.3f));
}
