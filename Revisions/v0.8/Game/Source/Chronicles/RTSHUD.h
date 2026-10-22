#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RTSTypes.h"
#include "RTSHUD.generated.h"

class ARTSController;
struct FRTSButton {
 FBox2D Bounds;
 FString Action, Value, Tooltip;
 bool Enabled=true;
};

UCLASS()
class CHRONICLES_API ARTSHUD : public AHUD {
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
 bool Click(FVector2D Pixel);
 bool RightClick(FVector2D Pixel);
 bool FindButtonBounds(const FString& Action,const FString& Value,FBox2D& Out) const;
 int32 CountButtons(const FString& Action,const FString& Value=FString()) const;
 FBox2D TooltipBounds;
 int32 TooltipLineCount=0;
 const FString& CurrentHoverTip() const {return HoverTip;}
 bool MinimapPoint(FVector2D Pixel,FVector2D& World) const;
 bool MinimapProject(FVector2D World,FVector2D& Pixel) const;
 bool BlocksWorld(FVector2D Pixel) const;
private:
 ARTSController* PC=nullptr;
 TArray<FRTSButton> Buttons;
 struct FQueueHit {FBox2D Bounds;int32 Building=0,Index=0;};
 TArray<FQueueHit> QueueHits;
 FBox2D MapBounds;
 float U=1, W=1600, H=900, EntityOpacity=1;
 FString HoverTip;
 bool bHostAddressesRead=false;
 TArray<FString> HostAddresses;
 void Rect(float X,float Y,float Width,float Height,FLinearColor Color);
 void Line(FVector2D A,FVector2D B,FLinearColor Color,float Thickness=1);
 void Poly(const TArray<FVector2D>& Points,FLinearColor Color);
 void Circle(FVector2D Center,float Radius,FLinearColor Color,bool Filled=true,float Flatten=1);
 void Text(const FString& Value,float X,float Y,float Size=16,FLinearColor Color=FLinearColor::White,bool Center=false);
 void Wrap(const FString& Value,float X,float Y,float Width,float Size,FLinearColor Color,int32 MaxLines=4);
 void Panel(float X,float Y,float Width,float Height,bool Accent=false);
 void Button(float X,float Y,float Width,float Height,const FString& Label,const FString& Action,const FString& Value=FString(),bool Enabled=true,const FString& Tip=FString(),bool Active=false);
 void DrawBackdrop(bool Menu);
 void DrawWorld();
 void DrawDeaths();
 void DrawEntity(const FRTSEntityView& E);
 void DrawMenu();
 void DrawLobby();
 void DrawTopBar();
 void DrawSelection();
 void DrawProductionQueue();
 void DrawSelectedQueue(const FRTSEntityView& Entity,float X,float Y,float Width);
 void Icon(float X,float Y,float Size,const FRTSDef& Def,FLinearColor Tint,const FString& Faction=FString());
 FString Name(const FRTSDef& Def) const;
 FString Description(const FRTSDef& Def) const;
 FString NameFor(const FRTSDef& Def,int32 EntityOwner) const;
 FString DescriptionFor(const FRTSDef& Def,int32 EntityOwner) const;
 FString CounterText(const FRTSDef& Def) const;
 void DrawWallPreview();
 void DrawGarrison(const FRTSEntityView& Entity,float X,float Y,float Width);
 void DrawMinimap();
 void DrawChat();
 void DrawOverlays();
 void DrawHelp();
 void DrawHotkeys();
 void DrawRallies();
 void DrawProceduralEntity(const FRTSEntityView& E);
 void DrawTooltip();
 void DrawEmblem(FVector2D Center,float Radius,FLinearColor Color,float Rotation=0);
 FString CostText(const FRTSDef& Def) const;
};
