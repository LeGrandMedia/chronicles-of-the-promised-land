#include "RTSHUD.h"
#include "RTSController.h"
#include "RTSNames.h"
#include "RTSControls.h"
#include "RTSVisualAssets.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "Engine/World.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

namespace {
 const FLinearColor Ink(.025f,.042f,.047f,1), PanelColor(.035f,.060f,.063f,.98f);
 const FLinearColor Brass(.76f,.58f,.29f,1), Gold(.94f,.79f,.46f,1), Paper(.92f,.88f,.77f,1), Muted(.54f,.58f,.53f,1);
 const FLinearColor StonePanel(.073f,.067f,.049f,.98f), StoneEdge(.28f,.221f,.145f,1);
 const FLinearColor WarmMuted(.65f,.60f,.50f,1);
 const FLinearColor Teal(.20f,.63f,.59f,1), Red(.94f,.35f,.26f,1);
 const TCHAR* FactionLines[]={
  TEXT("Raise Favor with teachers and temples to strengthen your people."),
  TEXT("Raise an army quickly. Anger speeds up recruitment."),
  TEXT("Raid supplies and conceal your forces in hideouts."),
  TEXT("Use builder beasts and strong cement defenses."),
  TEXT("Build with beasts and press the attack early."),
  TEXT("Train Concubines at Wineries to help nearby workers gather.")
 };
 FString DevotionArt(const FString& F){return F==TEXT("lamanites")?TEXT("stats/anger"):F==TEXT("laban")?TEXT("stats/drunkenness"):F==TEXT("gadianton")?TEXT("stats/lasciviousness"):RTS::IsJaredite(F)?TEXT("stats/dominion"):TEXT("stats/favor");}
 FLinearColor Shade(FLinearColor C,float M) {return FLinearColor(C.R*M,C.G*M,C.B*M,C.A);}
 FString Short(const FString& S,int32 Max) {return S.Len()>Max?S.Left(Max-1)+TEXT("…"):S;}
 float ReadableSize(float Size,float Scale) {return FMath::Max(Size,(Size<=12?8.5f:Size<=17?10.f:11.5f)/Scale);}
 UFont* TextFont(float Size) {UFont* Font=Size>=24?GEngine->GetLargeFont():GEngine->GetMediumFont();return Font?Font:GEngine->GetMediumFont();}
 bool UsesStone(const ARTSController* PC) {return PC->bMenu||PC->Phase==0||PC->bHelp||PC->bPauseMenu||PC->bHotkeySettings;}

}

void ARTSHUD::Rect(float X,float Y,float Width,float Height,FLinearColor C) {
 C.A*=EntityOpacity;
 FCanvasTileItem Item(FVector2D(X*U,Y*U),FVector2D(Width*U,Height*U),C);
 Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
}
void ARTSHUD::Line(FVector2D A,FVector2D B,FLinearColor C,float Thickness) {
 C.A*=EntityOpacity;
 FCanvasLineItem Item(A*U,B*U);Item.SetColor(C);Item.LineThickness=Thickness*U;Canvas->DrawItem(Item);
}
void ARTSHUD::Poly(const TArray<FVector2D>& Points,FLinearColor C) {
 if(Points.Num()<3)return;C.A*=EntityOpacity;
 FCanvasTileItem White(FVector2D::ZeroVector,FVector2D(1,1),C);
 for(int32 I=1;I<Points.Num()-1;I++) {
  FCanvasTriangleItem Item(Points[0]*U,Points[I]*U,Points[I+1]*U,White.Texture);
  Item.SetColor(C);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
 }
}
void ARTSHUD::Circle(FVector2D P,float Radius,FLinearColor C,bool Filled,float Flatten) {
 const int32 Sides=24;TArray<FVector2D> Points;
 for(int32 I=0;I<=Sides;I++) {const float A=I*2*PI/Sides;Points.Add(P+FVector2D(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius*Flatten));}
 if(Filled) Poly(Points,C);else for(int32 I=0;I<Sides;I++) Line(Points[I],Points[I+1],C,1.2f);
}
void ARTSHUD::Text(const FString& Value,float X,float Y,float Size,FLinearColor C,bool Center) {
 C.A*=EntityOpacity;UFont* Font=TextFont(Size);
 const float S=ReadableSize(Size,U)/Font->GetMaxCharHeight();
 FCanvasTextItem Item(FVector2D(X*U,Y*U),FText::FromString(Value),Font,C);
 Item.Scale=FVector2D(S*U,S*U);Item.bCentreX=Center;
 Item.EnableShadow(FLinearColor(0,0,0,.8f),FVector2D(1,1));Canvas->DrawItem(Item);
}
float ARTSHUD::HUDTextSize(float Size,float MinPixels) const {
 return FMath::Max(Size,MinPixels/U);
}
float ARTSHUD::TextWidth(const FString& Value,float Size) const {
 UFont* Font=TextFont(Size);
 float Width,Height;
 Canvas->StrLen(Font,Value,Width,Height);
 return Width*ReadableSize(Size,U)/Font->GetMaxCharHeight();
}
void ARTSHUD::FitText(const FString& Value,float X,float Y,float Width,float Size,FLinearColor Color,bool Center) {
 if(Value.IsEmpty()||Width<=0)return;
 const float Anchor=Center?X+Width*.5f:X;
 if(TextWidth(Value,Size)<=Width){Text(Value,Anchor,Y,Size,Color,Center);return;}
 const FString Ellipsis=TEXT("…");
 if(TextWidth(Ellipsis,Size)>Width)return;
 int32 Min=0,Max=Value.Len()-1;
 while(Min<Max){
  const int32 Mid=(Min+Max+1)/2;
  if(TextWidth(Value.Left(Mid)+Ellipsis,Size)<=Width)Min=Mid;else Max=Mid-1;
 }
 Text(Value.Left(Min).TrimEnd()+Ellipsis,Anchor,Y,Size,Color,Center);
}
void ARTSHUD::Wrap(const FString& Value,float X,float Y,float Width,float Size,FLinearColor C,int32 MaxLines) {
 TArray<FString> Lines;Value.ParseIntoArrayLines(Lines,false);int32 N=0;
 UFont* Font=TextFont(Size);const float RenderSize=ReadableSize(Size,U),Scale=RenderSize/Font->GetMaxCharHeight();
 for(const FString& Source:Lines) {
  TArray<FString> Words;Source.ParseIntoArray(Words,TEXT(" "),true);FString Current;
  for(const auto& Word:Words) {
   const FString Next=Current.IsEmpty()?Word:Current+TEXT(" ")+Word;
   float TW,TH;Canvas->StrLen(Font,Next,TW,TH);
   if(TW*Scale>Width && !Current.IsEmpty()) {Text(Current,X,Y+N*(RenderSize+3),Size,C);Current=Word;if(++N>=MaxLines)return;}
   else Current=Next;
  }
  if(!Current.IsEmpty()) {Text(Current,X,Y+N*(RenderSize+3),Size,C);if(++N>=MaxLines)return;}
 }
}
void ARTSHUD::Panel(float X,float Y,float Width,float Height,bool Accent) {
 const bool bStone=UsesStone(PC);
 Rect(X+4,Y+6,Width,Height,FLinearColor(0,0,0,.34f));Rect(X,Y,Width,Height,bStone?StonePanel:PanelColor);
 const FLinearColor Edge=Accent?Brass:bStone?StoneEdge:FLinearColor(.16f,.25f,.24f,1);
 Line(FVector2D(X,Y),FVector2D(X+Width,Y),Edge);Line(FVector2D(X,Y),FVector2D(X,Y+Height),Edge);
 Line(FVector2D(X+Width,Y),FVector2D(X+Width,Y+Height),Shade(Edge,.6f));Line(FVector2D(X,Y+Height),FVector2D(X+Width,Y+Height),Shade(Edge,.6f));
 if(Accent) {
  Rect(X,Y,26,2,Gold);Rect(X+Width-26,Y+Height-2,26,2,Gold);
  if(bStone) {
   Line(FVector2D(X+7,Y+7),FVector2D(X+Width-7,Y+7),StoneEdge);
   Line(FVector2D(X+7,Y+Height-7),FVector2D(X+Width-7,Y+Height-7),StoneEdge);
  }
 }
}
void ARTSHUD::Button(float X,float Y,float Width,float Height,const FString& Label,const FString& Action,const FString& Value,bool Enabled,const FString& Tip,bool Active) {
 const FBox2D Bounds(FVector2D(X*U,Y*U),FVector2D((X+Width)*U,(Y+Height)*U));
 const bool Hover=!PC->bTouchControls&&Bounds.IsInsideOrOn(PC->Mouse);
 const bool bStone=UsesStone(PC);
 FLinearColor Fill;
 if(bStone) {
  Fill=Active?FLinearColor(.24f,.17f,.087f,1):FLinearColor(.119f,.099f,.067f,1);
  if(Hover&&Enabled)Fill=FLinearColor(.21f,.158f,.09f,1);
  if(!Enabled)Fill=FLinearColor(.086f,.075f,.054f,1);
 } else {
  Fill=Active?FLinearColor(.17f,.24f,.20f,1):FLinearColor(.065f,.105f,.105f,1);
  if(Hover&&Enabled)Fill=FLinearColor(.14f,.22f,.20f,1);
  if(!Enabled)Fill=FLinearColor(.045f,.065f,.067f,1);
 }
 Rect(X,Y,Width,Height,Fill);
 Line(FVector2D(X,Y),FVector2D(X+Width,Y),Enabled&&(Active||Hover)?Brass:bStone?StoneEdge:FLinearColor(.18f,.28f,.26f,1));
 Line(FVector2D(X,Y+Height),FVector2D(X+Width,Y+Height),bStone?Shade(StoneEdge,.62f):FLinearColor(.12f,.18f,.17f,1));
 if(Active) Rect(X,Y,3,Height,Gold);
 if(!Label.IsEmpty()) Text(Label,X+Width*.5f,Y+(Height-16)*.5f,16,Enabled?(Active?Gold:Paper):Muted,true);
 FRTSButton B;B.Bounds=Bounds;B.Action=Action;B.Value=Value;B.Enabled=Enabled;B.Tooltip=Tip;Buttons.Add(B);
 if(Hover && !Tip.IsEmpty()) HoverTip=Tip;
}
void ARTSHUD::DrawEmblem(FVector2D P,float R,FLinearColor C,float Rotation) {
 Circle(P,R,C,false);Circle(P,R*.86f,Shade(C,.55f),false);
 for(int I=0;I<8;I++) {
  const float A=I*PI/4+Rotation;const FVector2D D(FMath::Sin(A),-FMath::Cos(A));
  const FVector2D N(-D.Y,D.X);
  Poly({P+D*R*.72f,P+N*R*.10f,P-D*R*.22f,P-N*R*.10f},I%2?Shade(C,.55f):C);
 }
 Circle(P,R*.14f,Gold);
}

const FRTSButton* ARTSHUD::ButtonAt(FVector2D Pixel) const {
 for(int32 I=Buttons.Num()-1;I>=0;I--)if(Buttons[I].Bounds.IsInsideOrOn(Pixel))return &Buttons[I];
 return nullptr;
}
bool ARTSHUD::HitButton(FVector2D Pixel,FString& Action,FString& Value) const {
 const FRTSButton* Hit=ButtonAt(Pixel);
 if(!Hit)return false;
 Action=Hit->Action;
 Value=Hit->Value;
 return true;
}
bool ARTSHUD::Click(FVector2D Pixel) {
 const FRTSButton* Hit=ButtonAt(Pixel);
 if(!Hit||!PC)return false;
 const FRTSButton B=*Hit;
 if(!B.Enabled) {
  if(!B.Tooltip.IsEmpty())PC->ShowNotice(B.Tooltip);
 } else if(B.Action==TEXT("touch_home")) {
  PC->FocusHeadquarters();
  PC->PlayCue(TEXT("ui"));
 } else if(!HandleRelicAction(B.Action,B.Value)) {
  PC->UIAction(B.Action,B.Value);
 }
 return true;
}
bool ARTSHUD::FindButtonBounds(const FString& Action,const FString& Value,FBox2D& Out) const {
 for(int32 I=Buttons.Num()-1;I>=0;--I) if(Buttons[I].Action==Action && Buttons[I].Value==Value && Buttons[I].Enabled) {Out=Buttons[I].Bounds;return true;}
 return false;
}
int32 ARTSHUD::CountButtons(const FString& Action,const FString& Value) const {
 int32 Count=0;for(const auto& B:Buttons)if(B.Action==Action&&(Value.IsEmpty()||B.Value==Value))Count++;return Count;
}

bool ARTSHUD::BlocksWorld(FVector2D P) const {
 return !PC||!PC->InWorld(P)||ButtonAt(P)||
  (MapBounds.bIsValid&&MapBounds.IsInsideOrOn(P))||
  (TouchControlsBounds.bIsValid&&TouchControlsBounds.IsInsideOrOn(P));
}

void ARTSHUD::DrawHUD() {
 Super::DrawHUD();PC=Cast<ARTSController>(PlayerOwner);if(!Canvas||!PC)return;
 PC->ViewSize=FVector2D(Canvas->SizeX,Canvas->SizeY);
 U=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f);U=FMath::Max(U,.25f);
 W=Canvas->SizeX/U;H=Canvas->SizeY/U;Buttons.Empty();HoverTip.Empty();MapBounds=FBox2D(EForceInit::ForceInit);TouchControlsBounds=FBox2D(EForceInit::ForceInit);QueueHits.Empty();PC->CardHotkeys.Empty();
 DrawBackdrop(PC->bMenu||PC->Phase==0);
 if(PC->bMenu) DrawMenu();
 else if(PC->Phase==0) DrawLobby();
 else {DrawWorld();DrawTopBar();DrawSelection();DrawMinimap();DrawProductionQueue();}
 if(!PC->bMenu) DrawChat();
 DrawOverlays();DrawTooltip();
#if !UE_BUILD_SHIPPING
 RunLoreTest();
#endif
}

void ARTSHUD::DrawBackdrop(bool Menu) {
 if(!Menu&&PC->Is3DReady())return;
 Rect(0,0,W,H,Ink);
 if(Menu)DrawMenuLandscape();
}

void ARTSHUD::DrawMenu() {
 const float X=(W-1440)*.5f+17,Y=(H-666)*.5f;
 const float Inset=X+36,Width=508;
 Panel(X,Y,580,666,true);
 DrawEmblem(FVector2D(Inset+23,Y+58),23,Brass);
 Line(FVector2D(Inset+60,Y+59),FVector2D(Inset+Width,Y+59),StoneEdge);
 if(!bMenuTitleFontResolved) {
  bMenuTitleFontResolved=true;
  MenuTitleFont=LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField"));
 }
 if(MenuTitleFont&&MenuTitleFont->GetMaxCharHeight()>0) {
  FCanvasTextItem Title(FVector2D(Inset*U,(Y+101)*U),FText::FromString(TEXT("CHRONICLES")),MenuTitleFont.Get(),Paper);
  const float Scale=66.f*U/MenuTitleFont->GetMaxCharHeight();
  Title.Scale=FVector2D(Scale,Scale);
  Title.EnableShadow(FLinearColor(0,0,0,.55f),FVector2D(1,2)*U);
  Canvas->DrawItem(Title);
 } else {
  Text(TEXT("CHRONICLES"),Inset,Y+101,64,Paper);
 }
 Text(TEXT("OF THE PROMISED LAND"),Inset+2,Y+171,25,Gold);
 Text(TEXT("Strategy inspired by the Book of Mormon"),Inset+2,Y+218,18,WarmMuted);
 const float B=Y+275;
 if(PC->MenuScreen==0) {
  const auto Mode=[&](float Top,const TCHAR* Name,const TCHAR* Detail,const TCHAR* Action,bool Primary) {
   Button(Inset,Top,Width,75,FString(),Action,FString(),true,FString(),Primary);
   Text(Name,Inset+20,Top+9,23,Primary?Gold:Paper);
   Text(Detail,Inset+21,Top+42,15,WarmMuted);
  };
  Mode(B,TEXT("Practice"),TEXT("Start here. Gather, build, and train at your own pace."),TEXT("solo"),true);
  Mode(B+86,TEXT("Skirmish"),TEXT("Play against a computer opponent"),TEXT("skirmish"),false);
  Text(TEXT("Local multiplayer"),Inset,B+178,14,Brass);
  Button(Inset,B+206,247,62,TEXT("Host LAN"),TEXT("host"),FString(),true,TEXT("Host a match for people on the same network. You can also add computer opponents."));
  Button(Inset+261,B+206,247,62,TEXT("Join LAN"),TEXT("join_screen"));
  const float Footer=B+290;
  Line(FVector2D(Inset,Footer),FVector2D(Inset+Width,Footer),StoneEdge);
  const float GuideWidth=PC->bTouchControls?341:210;
  Button(Inset,Footer+16,GuideWidth,53,PC->bTouchControls?TEXT("How to play"):TEXT("How to play  ·  F1"),TEXT("help"));
  if(!PC->bTouchControls)Button(Inset+222,Footer+16,145,53,TEXT("Hotkeys"),TEXT("hotkeys"));
  Button(Inset+379,Footer+16,129,53,TEXT("Quit"),TEXT("exit"));
  DrawRelics();
 } else {
  Text(TEXT("Join a LAN match"),Inset,B,27,Gold);
  Wrap(TEXT("Ask the host for the address shown in their lobby. Both devices need to be on the same network."),Inset,B+48,Width,18,Paper,3);
  Text(TEXT("Host address"),Inset,B+128,15,Brass);
  const bool bEditing=PC->TextFocus==TEXT("join");
  const bool bCaret=bEditing&&!PC->bTouchControls&&FMath::Fmod(GetWorld()->GetRealTimeSeconds(),1.f)<.5f;
  const bool bCanJoin=PC->HasJoinAddress();
  const FString Address=bCanJoin?PC->JoinAddress.Right(50)+(bCaret?TEXT("|"):TEXT("")):TEXT("Enter the address from the host's lobby");
  Button(Inset,B+156,Width,59,Address,TEXT("focus"),TEXT("join"),true,FString(),bEditing);
  Text(TEXT("For example, 192.168.1.15"),Inset,B+228,14,WarmMuted);
  Button(Inset,B+274,247,64,TEXT("Join match"),TEXT("join"),FString(),bCanJoin,bCanJoin?FString():TEXT("Enter the address shown in the host's lobby."),bCanJoin);
  Button(Inset+261,B+274,247,64,TEXT("Back"),TEXT("back"));
 }
}

void ARTSHUD::DrawLobby() {
 const float X=(W-1440)*.5f,Y=(H-760)*.5f;
 const bool bLAN=GetNetMode()!=NM_Standalone;
 const bool bHasBot=PC->Players.ContainsByPredicate([](const FRTSPlayerView& Player){return Player.bConnected&&Player.bBot;});
 Panel(X,Y,1440,760,true);
 Text(bLAN?TEXT("LAN council"):bHasBot?TEXT("Skirmish setup"):TEXT("Practice setup"),X+30,Y+25,36,Paper);
 Text(bLAN?TEXT("Up to four commanders on the same network"):bHasBot?TEXT("Choose your people and set up your opponents"):TEXT("Learn at your own pace, or add an opponent"),X+31,Y+73,16,WarmMuted);
 Button(X+1210,Y+27,198,54,TEXT("Main menu"),TEXT("menu"));
 Text(TEXT("Your name"),X+30,Y+115,15,Brass);
 Text(TEXT("Other players in this lobby can see it."),X+128,Y+117,14,WarmMuted);
 const bool bEditName=PC->TextFocus==TEXT("name");
 Button(X+30,Y+140,390,50,PC->PlayerName+(bEditName&&!PC->bTouchControls?TEXT("|"):TEXT("")),TEXT("focus"),TEXT("name"),true,PC->bTouchControls?TEXT("Tap to change your name."):TEXT("Click to edit. Enter confirms. Ctrl+A clears the field."),bEditName);
 Button(X+440,Y+140,200,50,FString::Printf(TEXT("Team %d   ›"),PC->LobbyTeam+1),TEXT("team"),FString(),true,TEXT("Commanders on the same team share victory."));
 Text(TEXT("Choose your civilization"),X+30,Y+203,16,Brass);
 const auto& Fs=FRTSCatalog::Get().Factions;
 for(int32 I=0;I<Fs.Num();I++) {
  const float CX=X+30+(I%3)*294,CY=Y+234+(I/3)*121;
  Button(CX,CY,278,106,FString(),TEXT("faction"),FString::FromInt(I),true,FString(),PC->LobbyFaction==I);
  if(!FRTSVisualAssets::Get().TryDrawIconId(Canvas,TEXT("factions/")+Fs[I].Id,FVector2D((CX+15)*U,(CY+10)*U),33*U,FLinearColor::White))DrawEmblem(FVector2D(CX+31,CY+26),14,Brass);
  Text(Fs[I].Name,CX+58,CY+15,18,PC->LobbyFaction==I?Gold:Paper);
  Wrap(RTSNames::ResolveText(FactionLines[FMath::Min(I,5)]),CX+18,CY+51,244,14,WarmMuted,3);
 }
 Text(FString(TEXT("Battlefield: "))+RTS::MapInfo(PC->LobbyMap).Name,X+30,Y+477,16,Brass);
 const int32 Pages=(RTS::MapCount()+2)/3;
 PC->LobbyMapPage=FMath::Clamp(PC->LobbyMapPage,0,Pages-1);
 Text(FString::Printf(TEXT("%d / %d"),PC->LobbyMapPage+1,Pages),X+722,Y+477,14,WarmMuted);
 Button(X+656,Y+468,50,30,TEXT("<"),TEXT("map_page"),TEXT("-1"),true,TEXT("Previous battlefields"));
 Button(X+810,Y+468,50,30,TEXT(">"),TEXT("map_page"),TEXT("1"),true,TEXT("More battlefields"));
 for(int32 Card=0;Card<3;Card++) {
  const int32 I=PC->LobbyMapPage*3+Card;if(I>=RTS::MapCount())break;
  const auto& Info=RTS::MapInfo(I);const float CX=X+30+Card*294,CY=Y+504;
  Button(CX,CY,278,82,FString(),TEXT("map"),FString::FromInt(I),PC->IsHost(),FString(Info.Name)+TEXT("\n")+Info.Description+(PC->IsHost()?TEXT(""):TEXT("\nThe host chooses the battlefield.")),PC->LobbyMap==I);
  for(int32 PY=0;PY<24;++PY)for(int32 PX=0;PX<24;++PX)Rect(CX+7+PX*2.5f,CY+8+PY*2.5f,2.6f,2.6f,RTS::TerrainColor(FVector2D((PX+.5)*160/24.,(PY+.5)*160/24.),I));
  for(int SX:{0,1})for(int SY:{0,1})Rect(CX+7+(SX?51:9)-2,CY+8+(SY?51:9)-2,4,4,Paper);
  FString CardName(Info.Name);CardName.RemoveFromStart(TEXT("The "));
  Wrap(CardName,CX+77,CY+9,190,17,PC->LobbyMap==I?Gold:Paper,2);
  if(U>=.7f)Wrap(Info.Description,CX+77,CY+49,190,12,WarmMuted,2);
 }
 const TCHAR* ResourceLabels[]={TEXT("Lean"),TEXT("Standard"),TEXT("Abundant"),TEXT("Sandbox")};const TCHAR* FertilityLabels[]={TEXT("Sparse"),TEXT("Normal"),TEXT("Rich")};
 const FString Labels[]={FString(TEXT("Resources: "))+ResourceLabels[FMath::Clamp(PC->LobbyResources,0,3)],FString::Printf(TEXT("Speed: %.2gx"),PC->LobbySpeed),TEXT("Wild supplies: ")+FString(FertilityLabels[FMath::Clamp(PC->LobbyFruitfulness,0,2)]),PC->bLobbyFog?TEXT("Fog: On"):TEXT("Fog: Off")};
 const TCHAR* Settings[]={TEXT("resources"),TEXT("speed"),TEXT("fruitfulness"),TEXT("fog")};
 const TCHAR* Tips[]={TEXT("Starting reserves: Lean 150/150/100; Standard 300/300/200; Abundant 1000/1000/750 +200 cement; Sandbox 5000 each +1000 cement."),TEXT("Simulation speed: 0.75x, 1x, 1.25x, 1.5x or 2x. Applies equally to every commander."),TEXT("Natural reserves in trees, ore, berries and hunted animals: Sparse 50%, Normal 100%, Rich 200%."),TEXT("Fog On: scout to reveal enemies; landmarks remain as last seen. Fog Off: the whole map is visible.")};
 for(int I=0;I<4;I++)Button(X+30+I*220,Y+594,212,48,Labels[I],TEXT("lobby_setting"),Settings[I],PC->IsHost(),FString(Tips[I])+TEXT(" Host changes reset readiness."));
 Text(TEXT("Wild supplies set how much each tree, ore deposit, berry patch, and wild animal provides."),X+30,Y+645,14,WarmMuted);
 const float RX=X+958;
 Line(FVector2D(X+924,Y+113),FVector2D(X+924,Y+640),StoneEdge);
 Text(TEXT("Commanders"),RX,Y+114,16,Brass);
 for(int32 I=0;I<4;I++) {
  const float RY=Y+151+I*110;
  const FRTSPlayerView* P=PC->Players.FindByPredicate([I](const FRTSPlayerView& V){return V.Slot==I && V.bConnected;});
  Panel(RX,RY,447,101);
  if(P) {
   Rect(RX,RY,4,101,RTS::PlayerColor(I));
   Text(Short(P->Name,25)+(I==PC->LocalSlot?TEXT("  (you)"):TEXT("")),RX+18,RY+10,18,Paper);
   Text(FRTSCatalog::Get().Faction(P->Faction).Name+FString::Printf(TEXT("  ·  Team %d"),P->Team+1),RX+18,RY+36,14,WarmMuted);
   if(P->bBot&&PC->IsHost()) {
    const TCHAR* Levels[]={TEXT("Easy"),TEXT("Normal"),TEXT("Hard")};
    Button(RX+18,RY+57,132,39,FString(Levels[FMath::Clamp(P->BotDifficulty,0,2)])+TEXT("   ›"),TEXT("bot_difficulty"),FString::FromInt(I),true,TEXT("Change the computer opponent's difficulty."));
    Button(RX+157,RY+57,135,39,TEXT("Civilization  ›"),TEXT("bot_faction"),FString::FromInt(I));
    Button(RX+299,RY+57,84,39,TEXT("Team  ›"),TEXT("bot_team"),FString::FromInt(I));
    Button(RX+390,RY+57,43,39,TEXT("×"),TEXT("bot_remove"),FString::FromInt(I),true,TEXT("Remove this computer opponent."));
   } else Text(P->bBot?TEXT("Computer opponent"):P->bReady?TEXT("Ready"):TEXT("Choosing settings"),RX+18,RY+64,14,P->bReady?Teal:Brass);
  } else if(PC->IsHost()) {
   Button(RX+18,RY+23,255,55,TEXT("Add computer opponent"),TEXT("bot_add"),FString::FromInt(I));
  } else {
   Text(TEXT("Open seat"),RX+18,RY+36,16,WarmMuted);
  }
 }
 if(PC->IsHost() && GetNetMode()==NM_ListenServer) {
  if(!bHostAddressesRead) {
   bHostAddressesRead=true;
   if(ISocketSubsystem* Sockets=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)) {
    TArray<TSharedPtr<FInternetAddr>> Adapters;Sockets->GetLocalAdapterAddresses(Adapters);
    for(const auto& Adapter:Adapters) {
     if(!Adapter.IsValid()||!Adapter->IsValid())continue;
     const FString Address=Adapter->ToString(false);TArray<FString> Parts;Address.ParseIntoArray(Parts,TEXT("."),false);
     if(Address.Contains(TEXT(":"))||Parts.Num()!=4)continue;
     const int32 First=FCString::Atoi(*Parts[0]),Second=FCString::Atoi(*Parts[1]);
     if(First==0||First==127||First>=224)continue;
     const bool Private=First==10||(First==172&&Second>=16&&Second<=31)||(First==192&&Second==168)||(First==169&&Second==254);
     if(Private)HostAddresses.AddUnique(Address);
    }
    HostAddresses.Sort([](const FString& A,const FString& B){
     auto Rank=[](const FString& IP){return IP.StartsWith(TEXT("192.168."))?0:IP.StartsWith(TEXT("10."))?1:IP.StartsWith(TEXT("172."))?2:3;};
     const int32 RA=Rank(A),RB=Rank(B);return RA==RB?A<B:RA<RB;
    });
    if(HostAddresses.Num()>2)HostAddresses.SetNum(2);
   }
  }
  Text(HostAddresses.Num()?TEXT("Share this address with players on your network"):TEXT("Host address"),RX,Y+594,14,Brass);
  if(HostAddresses.Num()) {
   Wrap(FString::Join(HostAddresses,TEXT("   /   ")),RX,Y+617,447,17,Gold,2);
  } else Wrap(TEXT("Find the IPv4 address in this device's network settings."),RX,Y+617,447,14,WarmMuted,2);
 }
 Line(FVector2D(X+30,Y+661),FVector2D(X+1408,Y+661),StoneEdge);
 Button(X+30,Y+679,200,54,TEXT("How to play"),TEXT("help"));
 const bool bLocalReady=PC->Me()&&PC->Me()->bReady;
 Button(X+958,Y+679,207,54,bLocalReady?TEXT("Unready"):TEXT("Ready"),TEXT("ready"),FString(),true,TEXT("Mark yourself ready to begin."),!bLocalReady);
 bool Ready=true;int32 Count=0;TSet<int32> Teams;
 for(const auto& P:PC->Players) if(P.bConnected) {Count++;Teams.Add(P.Team);if(!P.bReady)Ready=false;}
 const bool CanStart=PC->IsHost()&&Ready&&Count>0&&(!bLAN||(Count>=2&&Teams.Num()>=2));
 const TCHAR* StartHint;
 if(!bLocalReady)StartHint=TEXT("Choose your settings, then select Ready.");
 else if(!PC->IsHost())StartHint=TEXT("Waiting for the host to start the match.");
 else if(bLAN&&(Count<2||Teams.Num()<2))StartHint=TEXT("LAN matches need at least two opposing teams.");
 else if(!Ready)StartHint=TEXT("Waiting for the other commanders to ready up.");
 else StartHint=TEXT("Everyone is ready.");
 Button(X+1180,Y+679,228,54,TEXT("Start match"),TEXT("start"),FString(),CanStart,StartHint,CanStart);
 Text(StartHint,X+258,Y+696,16,WarmMuted);
}

void ARTSHUD::DrawWorld() {
 const float Bottom=H-228;
 const auto Screen=[this](FVector2D P){return PC->WorldToScreen(P)/U;};
 // A fixed cell mesh gives terrain and fog the same exact bounds as the simulation.
 if(!PC->Is3DReady())for(int32 Y=0;Y<64;Y++) for(int32 X=0;X<64;X++) {
  const FVector2D P(X*2.5f,Y*2.5f),Center=P+FVector2D(1.25f,1.25f),S=Screen(Center);
  if(S.X<-55 || S.X>W+55 || S.Y<20 || S.Y>Bottom+55) continue;
  const uint8 Fog=PC->Fog.IsValidIndex(Y*64+X)?PC->Fog[Y*64+X]:0;
  const bool Water=RTS::WaterAt(Center,PC->Map),Rock=RTS::RockAt(Center,PC->Map);
  const float Noise=((X*73+Y*147+X*Y*3)%19)/19.f;
  FLinearColor Ground=Shade(RTS::TerrainColor(Center,PC->Map),.72f)+FLinearColor(Noise*.012f,Noise*.015f,Noise*.008f,0);
  if(Fog==0) Ground=FLinearColor(.027f,.047f,.050f,1);else if(Fog==1) Ground=Shade(Ground,.39f);
  Poly({Screen(P),Screen(P+FVector2D(2.53f,0)),Screen(P+FVector2D(2.53f,2.53f)),Screen(P+FVector2D(0,2.53f))},Ground);
  if(Fog==2) {
   if(Water && (X+Y)%3==0) Line(S-FVector2D(5,0),S+FVector2D(6,0),FLinearColor(.20f,.43f,.43f,.5f),.8f);
   else if(!Rock && (X*7+Y*3)%9==0) {Line(S,S+FVector2D(-2,-3)*PC->Zoom,FLinearColor(.36f,.40f,.23f,.6f));Line(S,S+FVector2D(2,-2)*PC->Zoom,FLinearColor(.36f,.40f,.23f,.6f));}
   if(Rock && (X+Y)%2==0) {
    Poly({S+FVector2D(-9,3)*PC->Zoom,S+FVector2D(-3,-12)*PC->Zoom,S+FVector2D(8,-2)*PC->Zoom,S+FVector2D(9,5)*PC->Zoom},FLinearColor(.38f,.39f,.32f,1));
    Poly({S+FVector2D(-3,-12)*PC->Zoom,S+FVector2D(8,-2)*PC->Zoom,S+FVector2D(0,2)*PC->Zoom},FLinearColor(.47f,.47f,.37f,1));
   }
  }
 }
 const FLinearColor Edge(.35f,.43f,.30f,.65f);
 Line(Screen(FVector2D(0,0)),Screen(FVector2D(160,0)),Edge);Line(Screen(FVector2D(160,0)),Screen(FVector2D(160,160)),Edge);Line(Screen(FVector2D(160,160)),Screen(FVector2D(0,160)),Edge);Line(Screen(FVector2D(0,160)),Screen(FVector2D(0,0)),Edge);
 TArray<const FRTSEntityView*> Sorted;for(const auto& E:PC->Entities)Sorted.Add(&E);
 Sorted.Sort([this](const FRTSEntityView& A,const FRTSEntityView& B){return PC->WorldToScreen(A.Pos).Y<PC->WorldToScreen(B.Pos).Y;});
 // Draw true-radius support zones on the ground, including unselected Concubines.
 for(const auto* E:Sorted)if(!E->bRemembered&&E->AuraRadius>0&&E->HP>0){TArray<FVector2D> Ring;for(int I=0;I<=48;I++){const float A=I*2*PI/48;Ring.Add(Screen(E->Pos+FVector2D(FMath::Cos(A),FMath::Sin(A))*E->AuraRadius));}Poly(Ring,FLinearColor(.72f,.36f,.92f,.045f));for(int I=0;I<48;I++)Line(Ring[I],Ring[I+1],FLinearColor(.83f,.52f,.97f,.52f),1.1f);}
 if(!PC->Is3DReady())DrawDeaths();
 for(const auto* E:Sorted) DrawEntity(*E);
 DrawRallies();
 if(PC->bDragging) {
  const FVector2D A=PC->DragStart/U,B=PC->Mouse/U,Min(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),Max(FMath::Max(A.X,B.X),FMath::Max(A.Y,B.Y));
  Rect(Min.X,Min.Y,Max.X-Min.X,Max.Y-Min.Y,FLinearColor(.35f,.85f,.68f,.08f));
  Line(Min,FVector2D(Max.X,Min.Y),Teal);Line(FVector2D(Max.X,Min.Y),Max,Teal);Line(Max,FVector2D(Min.X,Max.Y),Teal);Line(FVector2D(Min.X,Max.Y),Min,Teal);
 }
 const float Flash=GetWorld()->GetRealTimeSeconds()-PC->CommandFlashTime;
 if(Flash>=0 && Flash<.9f) {FLinearColor C=PC->CommandFlashColor;C.A=1-Flash/.9f;Circle(Screen(PC->CommandFlash),(10+Flash*19)*PC->Zoom,C,false,.5f);}
 DrawWallPreview();
 if(!PC->PendingAction.IsEmpty() && PC->InWorld(PC->Mouse)) {
  FVector2D P=PC->ScreenToWorld(PC->Mouse);const FVector2D S=PC->Mouse/U;
  if(PC->PendingAction==TEXT("build")&&(PC->PendingType==TEXT("works_of_defense")||PC->PendingType==TEXT("cement_wall"))) {}
  else if(PC->PendingAction==TEXT("build")) {
   const auto& D=FRTSCatalog::Get().Def(PC->PendingType);const float R=FMath::Max(2.f,D.Radius);
   int32 Deposit=0;
   if(D.Id==TEXT("hill_of_ore")) for(const auto& Other:PC->Entities) if(FRTSCatalog::Get().Def(Other.Def).Id==TEXT("ore")&&FVector2D::Distance(P,Other.Pos)<3) {P=Other.Pos;Deposit=Other.Id;break;}
   bool Valid=P.X>R&&P.Y>R&&P.X<160-R&&P.Y<160-R&&!RTS::RockAt(P,PC->Map)&&!RTS::WaterAt(P,PC->Map);
   if(D.Id==TEXT("hill_of_ore") && !Deposit) Valid=false;
   if(D.Id==TEXT("place_of_many_waters")) Valid=Valid&&RTS::DockSite(P,R,PC->Map);
   for(const auto& Other:PC->Entities) {
    const auto& OD=FRTSCatalog::Get().Def(Other.Def);
    if(Other.Id!=Deposit&&(OD.bBuilding||OD.Id==TEXT("tree")||OD.Id==TEXT("ore"))&&FVector2D::Distance(P,Other.Pos)<R+OD.Radius*.88f)Valid=false;
   }
   FLinearColor C=Valid?Teal:Red;C.A=.45f;
   Poly({Screen(P+FVector2D(-R,-R)),Screen(P+FVector2D(R,-R)),Screen(P+FVector2D(R,R)),Screen(P+FVector2D(-R,R))},C);
   Text(Name(D),S.X+18,S.Y-29,15,Paper);Text(TEXT("Click to build  ·  Esc cancels"),S.X+18,S.Y-9,12,Muted);
  } else {Circle(S,13,Gold,false);Line(S-FVector2D(19,0),S+FVector2D(19,0),Gold);Line(S-FVector2D(0,19),S+FVector2D(0,19),Gold);Text((PC->PendingAction==TEXT("convert")&&PC->Me()&&PC->Me()->Faction==TEXT("gadianton")?FString(TEXT("BRIBE")):PC->PendingAction.Replace(TEXT("_"),TEXT(" ")).ToUpper()),S.X+24,S.Y-9,13,Gold);}
 }
}

void ARTSHUD::DrawEntity(const FRTSEntityView& E) {
 const auto& D=FRTSCatalog::Get().Def(E.Def);const FVector2D S=PC->WorldToScreen(PC->VisualPosition(E))/U;
 if(S.X<-100 || S.X>W+100 || S.Y<20 || S.Y>H-150) return;
 if(D.Id==TEXT("wild_game")&&PC->InWorld(PC->Mouse)&&PC->EntityBounds(E,true).IsInsideOrOn(PC->Mouse))HoverTip=NameFor(D,E.Owner)+TEXT("\nFood source - right-click with a laborer to hunt.");
 const bool Selected=!E.bRemembered&&PC->Selection.Contains(E.Id);const float Z=PC->Zoom;
 const FLinearColor Team=E.Owner<0?Muted:RTS::PlayerColor(E.Owner);
 const float T=GetWorld()->GetRealTimeSeconds();
 float R=D.bBuilding?FMath::Max(18.f,D.Radius*10):D.Kind==TEXT("beast")?14:(D.Kind==TEXT("ship")||D.Kind==TEXT("chariot"))?24:9;
 R*=Z*(D.bBuilding||D.Kind==TEXT("resource")?1.f:FRTSVisualAssets::WorldScale(D));
 if(Selected&&E.EffectiveRange>2&&E.EffectiveAttack>0){auto Ring=[&](float Radius,float Alpha){FLinearColor C=Team;C.A=Alpha;for(int I=0;I<80;I++){float A=I*2*PI/80,B=(I+1)*2*PI/80;Line(PC->WorldToScreen(E.Pos+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius)/U,PC->WorldToScreen(E.Pos+FVector2D(FMath::Cos(B),FMath::Sin(B))*Radius)/U,C,1.2f);}};Ring(E.EffectiveRange,.32f);if(E.MinimumRange>0)Ring(E.MinimumRange,.16f);}
 const bool Three=PC->Is3DReady();
 if(!Three)Circle(S+FVector2D(5*Z,4*Z),R*1.05f,FLinearColor(0,0,0,.23f),true,.46f);
 if(Selected) {Circle(S,R+5*Z,Team,false,.5f);Circle(S,R+8*Z,Shade(Team,.55f),false,.5f);}
 TGuardValue<float> OpacityScope(EntityOpacity,E.bRemembered?.36f:PC->VisualOpacity(E));
 const auto* WorkTarget=PC->FindEntity(E.Target);const auto* WorkDef=WorkTarget?&FRTSCatalog::Get().Def(WorkTarget->Def):nullptr;
 const FString Work=FRTSVisualAssets::WorkState(E,WorkDef);
 FRTSEntityView Animated=E;if(!Work.IsEmpty())Animated.Status=Work;
 if(!Selected&&E.Owner>=0&&FRTSVisualAssets::Get().HasRenderable(D.Id,PC->FactionFor(E.Owner)))Circle(S,FMath::Max(8.f*Z,R+Z),Team,false,.45f);
 const bool Foundation=!Three&&D.bBuilding&&E.Progress<1,Gate=!Three&&D.bBuilding&&E.bGate;
 const bool CustomArt=Three||(!Foundation&&!Gate&&FRTSVisualAssets::Get().TryDraw(Canvas,Animated,D,S,Z,U,Team,T,EntityOpacity,PC->FactionFor(E.Owner)));
 if(Foundation){
  const float Foot=FMath::Max(1.f,D.Radius);auto At=[&](float X,float Y){return PC->WorldToScreen(E.Pos+FVector2D(X,Y)*Foot)/U;};
  TArray<FVector2D> Corners={At(-1,-1),At(1,-1),At(1,1),At(-1,1)};Poly(Corners,FLinearColor(.25f,.22f,.16f,.90f));
  const float Height=(12+26*E.Progress)*Z;for(int I=0;I<4;I++){const FVector2D A=Corners[I],B=Corners[(I+1)%4],Rise(0,Height);Line(A,B,Team,2*Z);Line(A,A-Rise,Brass,3*Z);Line(A-Rise,B-Rise,Brass,2*Z);Line(A,B-Rise,FLinearColor(.55f,.39f,.22f,1),1.5f*Z);}
  Rect(S.X-24*Z,S.Y+13*Z,48*Z,4*Z,Ink);Rect(S.X-24*Z,S.Y+13*Z,48*Z*E.Progress,4*Z,Gold);
 }
 FBox2D ArtBounds;bool HasArtBounds=false;
 if(Three){ArtBounds=PC->EntityBounds(E);if(ArtBounds.bIsValid){ArtBounds.Min/=U;ArtBounds.Max/=U;HasArtBounds=true;}}
 else HasArtBounds=CustomArt&&FRTSVisualAssets::Get().GetHitBounds(D.Id,S,Z,1.f,ArtBounds,true,PC->FactionFor(E.Owner));
 if(!CustomArt&&!Foundation&&!Gate) DrawProceduralEntity(E);
 if(Gate){const float Span=FMath::Max(16.f,D.Radius*9)*Z,Height=38*Z,Width=8*Z;const FLinearColor Stone=D.Id==TEXT("cement_wall")?FLinearColor(.70f,.71f,.65f,1):FLinearColor(.48f,.34f,.20f,1);
  for(int Side:{-1,1}){const FVector2D Base=S+FVector2D(Side*Span,0);Poly({Base+FVector2D(-Width,0),Base+FVector2D(Width,0),Base+FVector2D(Width,-Height),Base+FVector2D(-Width,-Height)},Stone);Poly({Base+FVector2D(Width,0),Base+FVector2D(Width+5*Z,-4*Z),Base+FVector2D(Width+5*Z,-Height-4*Z),Base+FVector2D(Width,-Height)},Shade(Stone,.65f));}
  Rect(S.X-Span-Width,S.Y-Height-5*Z,2*(Span+Width),7*Z,Stone);Line(S+FVector2D(-Span,-Height-5*Z),S+FVector2D(Span,-Height-5*Z),Team,2*Z);
 }

 if(E.bRemembered)return;
 if(!Three&&!Work.IsEmpty()&&!D.bBuilding&&!E.bAttacking&&FVector2D::DistSquared(PC->VisualPosition(E),E.Pos)<.01) {
  const float Swing=FMath::Sin(T*8+E.Id*.7f);const FLinearColor WorkColor=Work==TEXT("gather_timber")?FLinearColor(.67f,.51f,.31f,1):Work==TEXT("gather_ore")?Gold:Work==TEXT("build")?Paper:FLinearColor(.69f,.82f,.38f,1);
  const FVector2D Hand=S+FVector2D(7,-17)*Z,Tool=Hand+FVector2D(7+Swing*5,-6+Swing*7)*Z;
  Line(Hand,Tool,WorkColor,2*Z);Line(Tool-FVector2D(3,1)*Z,Tool+FVector2D(3,1)*Z,WorkColor,2*Z);
  if(Swing>.65f)Circle(S+FVector2D(16,-5)*Z,2*Z,WorkColor,false);
 }
 if((D.Id==TEXT("fields_of_grain")||D.Id==TEXT("gathering_of_flocks")||D.Id==TEXT("hill_of_ore"))&&E.Owner==PC->LocalSlot) {
  const int32 Capacity=D.Id==TEXT("hill_of_ore")?6:10;
  const float FY=HasArtBounds?ArtBounds.Min.Y-17*Z:S.Y-48*Z;Rect(S.X-33*Z,FY-2*Z,66*Z,16*Z,FLinearColor(.025f,.042f,.047f,.88f));
  Text(FString::Printf(TEXT("%d / %d workers"),E.WorkersAssigned,Capacity),S.X,FY,10*FMath::Clamp(Z,.9f,1.3f),Paper,true);
 }
 const float ResourceAge=PC->MatchTime-E.ResourceEventTime;
 if(E.ResourceEventTime>0&&ResourceAge>=0&&ResourceAge<1.6f&&E.ResourceEvent.SizeSquared()>.0001) {
  const FVector Gain=E.ResourceEvent;const int32 K=Gain.X>0?0:Gain.Y>0?1:2;const float Amount=K==0?Gain.X:K==1?Gain.Y:Gain.Z;
  const FLinearColor Colors[]={FLinearColor(.76f,.88f,.45f,1),FLinearColor(.77f,.59f,.38f,1),Gold};FLinearColor C=Colors[K];C.A=FMath::Clamp(1-ResourceAge/1.6f,0.f,1.f);
  const FVector2D Q=S+FVector2D(0,-45-ResourceAge*19)*Z;
  Poly({Q+FVector2D(-6,-3),Q+FVector2D(-2,-8),Q+FVector2D(2,-3),Q+FVector2D(-2,2)},C);
  Text(FString::Printf(TEXT("+%.1f %s"),Amount,K==0?TEXT("F"):K==1?TEXT("T"):TEXT("PT")),Q.X+8,Q.Y-8,12,C);
 }
 if(ResourceAge>=0&&ResourceAge<1.6f&&(E.SpiritEvent>0||E.CementEvent>0)) {FLinearColor C=Gold;C.A=1-ResourceAge/1.6f;const FVector2D Q=S+FVector2D(0,-60-ResourceAge*19)*Z;FString Gain;if(E.SpiritEvent>0)Gain+=FString::Printf(TEXT("+%.0f %s"),E.SpiritEvent,*PC->DevotionLabel());if(E.CementEvent>0)Gain+=FString::Printf(TEXT(" +%.0f cement"),E.CementEvent);Text(Gain,Q.X,Q.Y,12,C,true);}
 if(!Three&&E.bAttacking) {
  if(const auto* Target=PC->FindEntity(E.Target)) {
   const FVector2D TS=PC->WorldToScreen(PC->VisualPosition(*Target))/U;
   const float A=FMath::Fmod(T*3+E.Id*.13f,1.f);
   if(D.Range>3) {const FVector2D Arrow=FMath::Lerp(S-FVector2D(0,16*Z),TS-FVector2D(0,8*Z),A);Line(Arrow,Arrow+FVector2D(7,-3)*Z,Gold,1.5f*Z);}
   else if(A<.5f) {Line(S+FVector2D(7,-18)*Z,S+FVector2D(23,-7)*Z,Gold,2*Z);}
  }
 }
 if(Selected || (E.HP<E.MaxHP && D.Kind!=TEXT("resource"))) {
  const float Bar=FMath::Max(28.f,R*2),BY=HasArtBounds?ArtBounds.Min.Y-6*Z:S.Y-(D.bBuilding?74.f:D.Kind==TEXT("resource")?47.f:40.f)*Z;
  Rect(S.X-Bar*.5f,BY,Bar,4*Z,Ink);Rect(S.X-Bar*.5f,BY,Bar*FMath::Clamp(E.HP/FMath::Max(1.f,E.MaxHP),0.f,1.f),4*Z,E.Owner==PC->LocalSlot?Teal:Team);
  if(Selected && PC->Selection.Num()<5) Text(Short(NameFor(D,E.Owner),29),S.X,BY-17*Z,12*FMath::Clamp(Z,.85f,1.3f),Paper,true);
 }
}

void ARTSHUD::DrawProceduralEntity(const FRTSEntityView& E) {
 const auto& D=FRTSCatalog::Get().Def(E.Def);const FVector2D S=PC->WorldToScreen(PC->VisualPosition(E))/U;
 const float Z=PC->Zoom*(D.bBuilding||D.Kind==TEXT("resource")?1.f:FRTSVisualAssets::WorldScale(D))*((D.Id==TEXT("fields_of_grain")||D.Id==TEXT("gathering_of_flocks"))?2.f:1.f),T=GetWorld()->GetRealTimeSeconds();
 if(D.Kind==TEXT("sheep")) {Circle(S+FVector2D(0,-10)*Z,10*Z,FLinearColor(.87f,.86f,.74f,1),true,.65f);Circle(S+FVector2D(9,-10)*Z,4*Z,Muted);Line(S+FVector2D(-5,-5)*Z,S+FVector2D(-5,1)*Z,Muted,2*Z);Line(S+FVector2D(5,-5)*Z,S+FVector2D(5,1)*Z,Muted,2*Z);return;}
 const FLinearColor Team=E.Owner<0?Muted:RTS::PlayerColor(E.Owner);
 if(D.Kind==TEXT("resource")) {
  if(D.Id==TEXT("tree")) {
   Rect(S.X-2*Z,S.Y-21*Z,4*Z,22*Z,FLinearColor(.26f,.20f,.12f,1));
   Circle(S+FVector2D(-7,-23)*Z,11*Z,FLinearColor(.10f,.23f,.14f,1));Circle(S+FVector2D(7,-24)*Z,12*Z,FLinearColor(.14f,.28f,.15f,1));Circle(S+FVector2D(0,-33)*Z,13*Z,FLinearColor(.19f,.34f,.17f,1));
   Circle(S+FVector2D(-3,-38)*Z,6*Z,FLinearColor(.27f,.40f,.20f,.65f));
  } else if(D.Id==TEXT("ore")) {
   Poly({S+FVector2D(-15,2)*Z,S+FVector2D(-12,-11)*Z,S+FVector2D(2,-21)*Z,S+FVector2D(16,-6)*Z,S+FVector2D(14,4)*Z},FLinearColor(.39f,.43f,.42f,1));
   Poly({S+FVector2D(2,-21)*Z,S+FVector2D(16,-6)*Z,S+FVector2D(4,-4)*Z,S+FVector2D(-12,-11)*Z},FLinearColor(.57f,.60f,.53f,1));
   Line(S+FVector2D(1,-17)*Z,S+FVector2D(7,-8)*Z,Gold,2*Z);Line(S+FVector2D(-9,-5)*Z,S+FVector2D(-2,-3)*Z,Brass,2*Z);
  } else if(D.Id==TEXT("wild_game")) {
   const FLinearColor Hide(.48f,.31f,.18f,1),Light(.62f,.43f,.24f,1);
   Circle(S+FVector2D(0,-10)*Z,11*Z,Hide,true,.58f);Circle(S+FVector2D(12,-16)*Z,4.5f*Z,Light);
   Line(S+FVector2D(7,-10)*Z,S+FVector2D(12,-17)*Z,Hide,4*Z);Line(S+FVector2D(-7,-5)*Z,S+FVector2D(-9,2)*Z,Hide,2*Z);Line(S+FVector2D(5,-5)*Z,S+FVector2D(8,2)*Z,Hide,2*Z);
   Line(S+FVector2D(11,-19)*Z,S+FVector2D(10,-25)*Z,Light,Z);Line(S+FVector2D(14,-19)*Z,S+FVector2D(16,-24)*Z,Light,Z);Circle(S+FVector2D(14,-17)*Z,.8f*Z,Ink);
  } else if(D.Id==TEXT("berries")) {
   Circle(S+FVector2D(-5,-5)*Z,8*Z,FLinearColor(.16f,.32f,.18f,1));Circle(S+FVector2D(5,-8)*Z,9*Z,FLinearColor(.21f,.38f,.20f,1));
   for(int I=0;I<5;I++) Circle(S+FVector2D((I*7%15)-7,-3-(I*3%11))*Z,2*Z,FLinearColor(.73f,.27f,.22f,1));
  } else {Rect(S.X-7*Z,S.Y-10*Z,14*Z,11*Z,Brass);Line(S+FVector2D(-7,-10)*Z,S+FVector2D(7,1)*Z,Gold);}
 } else if(D.bBuilding) {
  const FLinearColor Stone(.63f,.58f,.43f,1),LightStone(.75f,.70f,.52f,1),DarkStone(.40f,.40f,.32f,1);
  auto Block=[&](FVector2D P,float Width,float Height,FLinearColor Roof) {
   const FVector2D A=P+FVector2D(0,Width*.5f),B=P+FVector2D(Width,0),C=P+FVector2D(0,-Width*.5f),DD=P+FVector2D(-Width,0),Up(0,-Height);
   Poly({A,DD,DD+Up,A+Up},DarkStone);Poly({A,B,B+Up,A+Up},Stone);Poly({A+Up,B+Up,C+Up,DD+Up},Roof);
  };
  if(D.Id==TEXT("fields_of_grain")) {
   Poly({S+FVector2D(-25,0)*Z,S+FVector2D(0,-14)*Z,S+FVector2D(25,0)*Z,S+FVector2D(0,14)*Z},FLinearColor(.34f,.25f,.13f,1));
   for(int I=-3;I<=3;I++) Line(S+FVector2D(-17+I*3,-1+I*2)*Z,S+FVector2D(1+I*3,-10+I*2)*Z,FLinearColor(.70f,.60f,.24f,1),2*Z);
   Block(S+FVector2D(14,-8)*Z,9*Z,12*Z,Team);
  } else if(D.Id==TEXT("gathering_of_flocks")) {
   Block(S,22*Z,8*Z,FLinearColor(.34f,.37f,.21f,1));
   for(int I=0;I<4;I++) {const FVector2D P=S+FVector2D((I%2)*15-7,(I/2)*8-14)*Z;Circle(P,4*Z,Paper,true,.7f);}
   Line(S+FVector2D(-21,0)*Z,S+FVector2D(0,10)*Z,Brass,2*Z);Line(S+FVector2D(0,10)*Z,S+FVector2D(21,0)*Z,Brass,2*Z);
  } else if(D.Id==TEXT("seat_of_the_people") || D.Id==TEXT("temple_of_the_land") || D.Id==TEXT("rameumptom")) {
   const bool HQ=D.Id==TEXT("seat_of_the_people");float BW=HQ?39:30;
   for(int I=0;I<3;I++) Block(S+FVector2D(0,-I*8)*Z,(BW-I*8)*Z,9*Z,I==2?LightStone:Stone);
   Block(S+FVector2D(0,-25)*Z,12*Z,20*Z,Team);
   Rect(S.X-3*Z,S.Y-43*Z,6*Z,17*Z,Ink);
   Line(S+FVector2D(0,-45)*Z,S+FVector2D(0,-67)*Z,Brass,1.3f*Z);
   Poly({S+FVector2D(1,-66)*Z,S+FVector2D(17,-61)*Z,S+FVector2D(1,-56)*Z},Team);
   for(int I=0;I<4;I++) Line(S+FVector2D(-6,12-I*6)*Z,S+FVector2D(6,12-I*6)*Z,LightStone,Z);
  } else if(D.Id==TEXT("works_of_defense")||D.Id==TEXT("cement_wall")) {
   Block(S,27*Z,19*Z,Stone);
   for(int I=-2;I<=2;I++) Block(S+FVector2D(I*9,-17+FMath::Abs(I)*2)*Z,4*Z,8*Z,LightStone);
  } else if(D.Id==TEXT("tower_of_watchmen")||D.Id==TEXT("cement_tower")) {
   Block(S,13*Z,43*Z,LightStone);Block(S+FVector2D(0,-44)*Z,18*Z,12*Z,Team);
   Rect(S.X-3*Z,S.Y-43*Z,6*Z,11*Z,Ink);
  } else if(D.Id==TEXT("place_of_many_waters")) {
   Block(S,30*Z,4*Z,FLinearColor(.40f,.29f,.16f,1));Block(S+FVector2D(0,-6)*Z,16*Z,17*Z,Team);
   for(int I=-2;I<=2;I++) Line(S+FVector2D(I*9,-13)*Z,S+FVector2D(I*9+20,-3)*Z,Brass,Z);
  } else if(D.Id==TEXT("tents_and_habitations")) {
   Block(S,22*Z,10*Z,Stone);Poly({S+FVector2D(-25,-10)*Z,S+FVector2D(0,-37)*Z,S+FVector2D(25,-10)*Z,S+FVector2D(0,3)*Z},Shade(Team,.83f));
   Poly({S+FVector2D(0,-37)*Z,S+FVector2D(25,-10)*Z,S+FVector2D(0,3)*Z},Shade(Team,.55f));Line(S+FVector2D(0,-37)*Z,S+FVector2D(0,2)*Z,Brass,Z);
  } else {
   Block(S,27*Z,23*Z,LightStone);
   Poly({S+FVector2D(-30,-23)*Z,S+FVector2D(-2,-43)*Z,S+FVector2D(30,-23)*Z,S+FVector2D(0,-7)*Z},Shade(Team,.80f));
   Poly({S+FVector2D(-2,-43)*Z,S+FVector2D(30,-23)*Z,S+FVector2D(0,-7)*Z},Shade(Team,.55f));
   Rect(S.X-5*Z,S.Y-17*Z,10*Z,16*Z,Ink);
   if(D.Id==TEXT("encampment_of_war")) {Line(S+FVector2D(-19,-34)*Z,S+FVector2D(19,-8)*Z,Gold,2*Z);Line(S+FVector2D(19,-34)*Z,S+FVector2D(-19,-8)*Z,Gold,2*Z);}
   else if(D.Id==TEXT("house_of_curious_workmanship")) {Block(S+FVector2D(14,-25)*Z,5*Z,27*Z,DarkStone);Circle(S+FVector2D(14,-57)*Z,5*Z,FLinearColor(.5f,.5f,.42f,.25f));}
   else if(D.Id==TEXT("place_of_trade")) {Poly({S+FVector2D(-25,-9)*Z,S+FVector2D(0,-22)*Z,S+FVector2D(25,-9)*Z,S+FVector2D(0,4)*Z},Gold);}
  }
  if(E.Progress<1) {
   for(int I=-1;I<=1;I++) {Line(S+FVector2D(I*18,9)*Z,S+FVector2D(I*18,-49)*Z,Brass,2*Z);Line(S+FVector2D(-28,-I*15-10)*Z,S+FVector2D(28,-I*15-10)*Z,Brass,1.3f*Z);}
   Rect(S.X-24*Z,S.Y+13*Z,48*Z,4*Z,Ink);Rect(S.X-24*Z,S.Y+13*Z,48*Z*E.Progress,4*Z,Gold);
  }
 } else if(D.Kind==TEXT("chariot")) {
  // Original horse-and-cart placeholder; sprite overrides continue to use kings_chariot.
  const FLinearColor Horse(.54f,.35f,.20f,1);const float Step=FVector2D::Distance(PC->VisualPosition(E),E.Pos)>.03f?FMath::Sin(T*12+E.Id)*4:0;
  Circle(S+FVector2D(10,-13)*Z,12*Z,Horse,true,.6f);Circle(S+FVector2D(22,-23)*Z,5*Z,Horse);Line(S+FVector2D(16,-14)*Z,S+FVector2D(22,-25)*Z,Horse,6*Z);
  Line(S+FVector2D(4,-8)*Z,S+FVector2D(1+Step,1)*Z,Horse,2.5f*Z);Line(S+FVector2D(16,-8)*Z,S+FVector2D(18-Step,1)*Z,Horse,2.5f*Z);
  Line(S+FVector2D(24,-27)*Z,S+FVector2D(23,-31)*Z,Horse,2*Z);Line(S+FVector2D(-2,-13)*Z,S+FVector2D(-10,-10)*Z,Brass,2*Z);
  Poly({S+FVector2D(-29,-11)*Z,S+FVector2D(-9,-14)*Z,S+FVector2D(-7,-3)*Z,S+FVector2D(-27,1)*Z},Team);
  Circle(S+FVector2D(-23,0)*Z,6*Z,Brass,false);Circle(S+FVector2D(-10,-2)*Z,6*Z,Brass,false);
 } else if(D.Kind==TEXT("ship")) {
  Poly({S+FVector2D(-29,0)*Z,S+FVector2D(-15,-10)*Z,S+FVector2D(28,-3)*Z,S+FVector2D(15,9)*Z},FLinearColor(.35f,.21f,.12f,1));
  Line(S+FVector2D(-24,-1)*Z,S+FVector2D(23,-3)*Z,Brass,3*Z);
  if(D.Id!=TEXT("jaredite_barge")) {Line(S+FVector2D(0,-3)*Z,S+FVector2D(0,-47)*Z,Brass,2*Z);Poly({S+FVector2D(2,-45)*Z,S+FVector2D(21,-16)*Z,S+FVector2D(2,-13)*Z},Paper);Poly({S+FVector2D(-2,-42)*Z,S+FVector2D(-16,-17)*Z,S+FVector2D(-2,-15)*Z},Team);}
  else {Poly({S+FVector2D(-20,-6)*Z,S+FVector2D(0,-22)*Z,S+FVector2D(20,-7)*Z,S+FVector2D(0,2)*Z},Shade(Team,.7f));}
 } else if(D.Kind==TEXT("beast")) {
  const float B=D.Id==TEXT("elephant")?1.45f:1.f;
  const FLinearColor Skin=D.Id==TEXT("elephant")?FLinearColor(.43f,.47f,.42f,1):FLinearColor(.53f,.42f,.27f,1);
  for(int I=0;I<4;I++) Line(S+FVector2D((I%2)*17-8,-5)*Z*B,S+FVector2D((I%2)*17-8+(I/2)*4,4)*Z*B,Skin,3*Z*B);
  Circle(S+FVector2D(0,-11)*Z*B,13*Z*B,Skin,true,.65f);Circle(S+FVector2D(13,-14)*Z*B,8*Z*B,Shade(Skin,1.1f));
  Line(S+FVector2D(-7,-17)*Z*B,S+FVector2D(8,-17)*Z*B,Team,5*Z*B);
  if(D.Id==TEXT("elephant")) {Line(S+FVector2D(18,-12)*Z*B,S+FVector2D(22,0)*Z*B,Skin,4*Z*B);Line(S+FVector2D(16,-11)*Z*B,S+FVector2D(23,-6)*Z*B,Paper,1.5f*Z*B);}
 } else {
  const bool Worker=RTS::IsWorker(D),Priest=D.Kind==TEXT("priest")||D.Kind==TEXT("ammonite"),Hero=D.bHero;
  const bool Moving=FVector2D::Distance(PC->VisualPosition(E),E.Pos)>.03f;const float Step=Moving?FMath::Sin(T*13+E.Id)*3.f:0;
  const float B=Hero?1.18f:1;
  Line(S+FVector2D(-3,-6)*Z*B,S+FVector2D(-4-Step,0)*Z*B,Ink,2.6f*Z*B);Line(S+FVector2D(3,-6)*Z*B,S+FVector2D(4+Step,0)*Z*B,Ink,2.6f*Z*B);
  Poly({S+FVector2D(-6,-17)*Z*B,S+FVector2D(4,-17)*Z*B,S+FVector2D(7,-5)*Z*B,S+FVector2D(-7,-5)*Z*B},Priest?Paper:Team);
  Circle(S+FVector2D(0,-21)*Z*B,4.5f*Z*B,FLinearColor(.77f,.57f,.35f,1));
  Line(S+FVector2D(-6,-14)*Z*B,S+FVector2D(-10,-10)*Z*B,FLinearColor(.70f,.48f,.29f,1),2*Z*B);
  if(Worker) {Line(S+FVector2D(7,-7)*Z,S+FVector2D(12,-25)*Z,Brass,1.5f*Z);Line(S+FVector2D(8,-25)*Z,S+FVector2D(16,-23)*Z,Paper,2*Z);}
  else if(Priest) {Line(S+FVector2D(10,0)*Z,S+FVector2D(10,-28)*Z,Brass,1.6f*Z);Circle(S+FVector2D(10,-28)*Z,2*Z,Gold);}
  else if(D.Id==TEXT("gadianton_robber")) {Line(S+FVector2D(7,-10)*Z,S+FVector2D(12,-16)*Z,Paper,1.8f*Z);Line(S+FVector2D(8,-11)*Z,S+FVector2D(11,-9)*Z,Brass,Z);}
  else if(D.Kind==TEXT("ranged")) {Line(S+FVector2D(10,-6)*Z,S+FVector2D(13,-17)*Z,Brass,1.6f*Z);Line(S+FVector2D(13,-17)*Z,S+FVector2D(8,-25)*Z,Brass,1.6f*Z);Line(S+FVector2D(8,-25)*Z,S+FVector2D(10,-6)*Z,Paper,.7f*Z);}
  else {Line(S+FVector2D(8,-10)*Z,S+FVector2D(14,-27)*Z,Paper,1.8f*Z);Circle(S+FVector2D(-7,-12)*Z,4.5f*Z,Brass,true,.85f);}
  if(Hero) {Line(S+FVector2D(-5,-26)*Z,S+FVector2D(5,-26)*Z,Gold,2*Z);Circle(S,16*Z,FLinearColor(.9f,.72f,.3f,.25f),false,.5f);}
  if(D.Kind==TEXT("ammonite")) {Line(S+FVector2D(0,-35)*Z,S+FVector2D(0,-29)*Z,Teal,2*Z);Line(S+FVector2D(-3,-32)*Z,S+FVector2D(3,-32)*Z,Teal,2*Z);}
 }
 }


void ARTSHUD::DrawTopBar() {
 Rect(0,0,W,65,PanelColor);Line(FVector2D(0,64),FVector2D(W,64),Brass);
 const auto* P=PC->Me();if(!P)return;
 if(!FRTSVisualAssets::Get().TryDrawIconId(Canvas,TEXT("factions/")+P->Faction,FVector2D(9*U,10*U),44*U,FLinearColor::White))DrawEmblem(FVector2D(31,32),21,Brass);
 FitText(FRTSCatalog::Get().Faction(P->Faction).Name,63,9,207,HUDTextSize(21,13),Paper);
 Text(FString::Printf(TEXT("Age %d  ·  Team %d"),P->Age,P->Team+1),64,36,HUDTextSize(14,10.5f),Gold);
 const int32 ResourceCount=RTS::IsJaredite(P->Faction)?4:3;
 constexpr float Start=278,PopulationWidth=123,DevotionWidth=178,WealthExtra=72,Gap=6;
 const float StatEnd=FMath::Min(W-509,1091.f);
 const float ResourceWidth=(StatEnd-Start-PopulationWidth-DevotionWidth-WealthExtra)/ResourceCount;
 const float ValueSize=HUDTextSize(24,14),LabelSize=HUDTextSize(15,11);
 const FString Labels[]={RTSNames::Get(TEXT("resource.food"),TEXT("Food")),RTSNames::Get(TEXT("resource.timber"),TEXT("Timber")),RTSNames::Get(TEXT("resource.wealth"),TEXT("Precious Things")),TEXT("Cement")};
 const float Amounts[]={float(P->Resources.X),float(P->Resources.Y),float(P->Resources.Z),P->Cement};
 const FLinearColor Colors[]={FLinearColor(.69f,.76f,.38f,1),FLinearColor(.66f,.50f,.30f,1),Gold,FLinearColor(.64f,.68f,.65f,1)};
 const TCHAR* StatKeys[]={TEXT("stats/food"),TEXT("stats/timber"),TEXT("stats/wealth"),TEXT("stats/cement")};
 float X=Start;
 for(int32 I=0;I<ResourceCount;I++) {
  const float Width=ResourceWidth+(I==2?WealthExtra:0);
  const int32 Workers=PC->ResourceWorkers(I).Num();
  const FString Tip=Workers>0?FString::Printf(TEXT("Select to cycle through the %d assigned to %s."),Workers,*Labels[I]):
   FString::Printf(TEXT("No workers are assigned to %s."),*Labels[I]);
  Button(X,6,Width-Gap,52,FString(),TEXT("workforce"),FString::FromInt(I),Workers>0,Tip);
  if(!FRTSVisualAssets::Get().TryDrawIconId(Canvas,StatKeys[I],FVector2D((X+6)*U,9*U),24*U,FLinearColor::White))Poly({FVector2D(X+6,21),FVector2D(X+14,10),FVector2D(X+22,21),FVector2D(X+14,32)},Colors[I]);
  FitText(FString::FromInt(FMath::FloorToInt(Amounts[I])),X+35,8,Width-43,ValueSize,Paper);
  const FString Count=Workers>0?FString::FromInt(Workers):FString();
  const float CountWidth=Count.IsEmpty()?0:TextWidth(Count,LabelSize);
  FitText(Labels[I],X+7,35,Width-Gap-14-(Count.IsEmpty()?0:CountWidth+9),LabelSize,Paper);
  if(!Count.IsEmpty())Text(Count,X+Width-Gap-7-CountWidth,35,LabelSize,Gold);
  X+=Width;
 }
 FitText(FString::Printf(TEXT("%d / %d"),P->Population,P->Capacity),X+7,8,PopulationWidth-15,ValueSize,P->Population>=P->Capacity?Red:Paper);
 FitText(TEXT("Population"),X+7,35,PopulationWidth-15,LabelSize,Paper);
 X+=PopulationWidth;
 const FString Devotion=PC->DevotionLabel();
 Button(X,6,DevotionWidth-Gap,52,FString(),TEXT("religion_info"),FString(),true,Devotion+TEXT("\n")+P->ReligionTooltip);
 FRTSVisualAssets::Get().TryDrawIconId(Canvas,DevotionArt(P->Faction),FVector2D((X+6)*U,9*U),24*U,FLinearColor::White);
 Text(FString::FromInt(FMath::RoundToInt(P->Favor)),X+35,8,ValueSize,Gold);
 FitText(Devotion,X+7,35,DevotionWidth-Gap-14,LabelSize,Paper);
 const int32 Idle=PC->IdleVillagers().Num();
 const bool bTouch=PC->bTouchControls;
 const float ButtonY=bTouch?6:14,ButtonHeight=bTouch?53:36,ButtonSize=HUDTextSize(16,11.5f);
 const auto ToolbarButton=[&](float Left,float Top,float Width,float Height,const FString& Label,const TCHAR* Action,bool bEnabled,const FString& Tip,bool bActive=false){
  Button(Left,Top,Width,Height,FString(),Action,FString(),bEnabled,Tip,bActive);
  FitText(Label,Left+4,Top+(Height-ButtonSize)*.5f,Width-8,ButtonSize,bActive?Gold:bEnabled?Paper:WarmMuted,true);
 };
 ToolbarButton(W-495,ButtonY,185,ButtonHeight,FString::Printf(TEXT("Idle workers  %d"),Idle),TEXT("idle"),Idle>0,
  bTouch?FString(TEXT("Select and look at the next idle worker.")):TEXT("Select next idle worker: ")+FRTSControls::Get().LabelFor(TEXT("idle.next"))+TEXT(". Select all: ")+FRTSControls::Get().LabelFor(TEXT("idle.all"))+TEXT(" or Ctrl+click. Builder beasts are excluded."),Idle>0);
 Text(FString::Printf(TEXT("%02d:%02d"),int32(PC->MatchTime)/60,int32(PC->MatchTime)%60),W-280,19,HUDTextSize(21,13),Paper);
 ToolbarButton(W-210,ButtonY,101,ButtonHeight,bTouch?TEXT("Help"):TEXT("Help · F1"),TEXT("help"),true,TEXT("Controls and the rules of the land."));
 ToolbarButton(W-98,ButtonY,78,ButtonHeight,TEXT("Menu"),TEXT("pause"),true,FString());
 if(bTouch&&PC->Phase==1)DrawTouchControls();
 else {
  const int32 IdleArmy=PC->IdleMilitary().Num();
  ToolbarButton(W-305,70,205,30,FString::Printf(TEXT("Idle military  %d"),IdleArmy),TEXT("idle_military"),IdleArmy>0,TEXT("Click to select the next idle soldier. Ctrl+click selects all idle military."),IdleArmy>0);
 }
 const float StatusY=bTouch?150:77;
 if(P->bDefeated) {Panel(W*.5f-220,StatusY,440,44);Text(TEXT("Your chronicle has ended. You may observe the battle."),W*.5f,StatusY+13,15,Brass,true);}
 else if(P->DefeatRemaining<59.9f) {Panel(W*.5f-240,StatusY,480,44,true);Text(FString::Printf(TEXT("Rebuild your Seat  ·  %.0f seconds remain"),FMath::CeilToFloat(P->DefeatRemaining)),W*.5f,StatusY+13,16,Gold,true);}
}

void ARTSHUD::DrawTouchControls() {
 const float X=24,Y=76,Height=58;
 const bool bTargeting=!PC->PendingAction.IsEmpty();
 const bool bBox=PC->bTouchBoxSelect&&!bTargeting;
 const bool bOrders=PC->bTouchOrderMode&&!bTargeting;
 const bool bCanCancel=bTargeting||PC->bBuildMenu||PC->bWallAnchor;
 const float DeckWidth=bCanCancel?592:472;
 Panel(X-5,Y-5,DeckWidth,Height+10);
 TouchControlsBounds=FBox2D(FVector2D((X-5)*U,(Y-5)*U),FVector2D((X-5+DeckWidth)*U,(Y+Height+5)*U));
 Button(X,Y,109,Height,TEXT("Select"),TEXT("touch_select"),FString(),true,FString(),!bBox&&!bOrders&&!bTargeting);
 Button(X+117,Y,112,Height,TEXT("Orders"),TEXT("touch_order"),FString(),true,FString(),bOrders);
 Button(X+237,Y,109,Height,TEXT("Box select"),TEXT("touch_box"),FString(),true,FString(),bBox);
 Button(X+354,Y,108,Height,TEXT("Home"),TEXT("touch_home"));
 if(bCanCancel)Button(X+470,Y,106,Height,TEXT("Cancel"),TEXT("touch_cancel"),FString(),true,FString(),bTargeting);
 const TCHAR* Hint=bTargeting?TEXT("Tap the target to confirm"):bBox?TEXT("Drag a box around your units"):bOrders?TEXT("Tap where your units should go"):TEXT("Tap a unit to select it");
 Text(Hint,X+DeckWidth+15,Y+22,14,Paper);
 const int32 IdleArmy=PC->IdleMilitary().Num();
 Button(W-311,Y,185,Height,FString::Printf(TEXT("Idle military  %d"),IdleArmy),TEXT("idle_military"),FString(),IdleArmy>0,TEXT("Select the next idle soldier."),IdleArmy>0);
 Button(W-118,Y,98,Height,TEXT("Chat"),TEXT("touch_chat"));
}

FString ARTSHUD::CostText(const FRTSDef& Def) const {
 const auto* P=PC->Me();const FVector C=FRTSCatalog::Get().Price(Def,P?P->Faction:TEXT(""));
 TArray<FString> Parts;
 if(C.X>0) Parts.Add(FString::Printf(TEXT("%d %s"),int32(C.X),*RTSNames::Get(TEXT("resource.food_short"),TEXT("F"))));
 if(C.Y>0) Parts.Add(FString::Printf(TEXT("%d %s"),int32(C.Y),*RTSNames::Get(TEXT("resource.timber_short"),TEXT("T"))));
 if(Def.CementCost>0&&P&&RTS::IsJaredite(P->Faction))Parts.Add(FString::Printf(TEXT("%d Cement"),FMath::CeilToInt(Def.CementCost)));
 if(C.Z>0) Parts.Add(FString::Printf(TEXT("%d %s"),int32(C.Z),*RTSNames::Get(TEXT("resource.wealth_short"),TEXT("PT"))));
 return FString::Join(Parts,TEXT("  "));
}


void ARTSHUD::DrawChat() {
 const bool Match=PC->Phase!=0;
 const float X=Match?260:(W-1380)*.5f,Y=Match?H-270:H-45;
 if(Match) {
  const int32 Start=FMath::Max(0,PC->ChatLines.Num()-5);
  for(int32 I=Start;I<PC->ChatLines.Num();I++) {
   const float CY=Y-30-(PC->ChatLines.Num()-1-I)*23;
   Rect(X-7,CY-2,680,23,FLinearColor(.015f,.030f,.033f,.55f));Text(Short(PC->ChatLines[I],100),X,CY,14,Paper);
  }
 } else if(!PC->bChat && PC->ChatLines.Num()>0) {Text(Short(PC->ChatLines.Last(),170),X,Y,13,Muted);}
 if(PC->bChat) {
  Panel(X-10,Y-6,850,38,true);
  Text(TEXT("ALL  >"),X,Y+3,14,Brass);
  Text(PC->ChatDraft.Right(103)+(FMath::Fmod(GetWorld()->GetRealTimeSeconds(),1.f)<.5f?TEXT("|"):TEXT("")),X+65,Y+3,15,Paper);
  Text(TEXT("Enter sends  ·  Esc closes"),X+864,Y+3,12,Muted);
 } else if(Match&&!PC->bTouchControls) {Text(TEXT("Enter to chat"),X,Y+8,11,Muted);}
}

void ARTSHUD::DrawHelp() {
 const float X=(W-1180)*.5f,Y=(H-740)*.5f;
 const float Left=X+36,Right=X+619;
 const bool bTouch=PC->bTouchControls;
 Panel(X,Y,1180,740,true);
 DrawEmblem(FVector2D(X+61,Y+58),25,Brass);
 Text(TEXT("Commander's field guide"),X+107,Y+31,34,Paper);
 Text(TEXT("Play a practice match to try these steps without an opponent."),X+108,Y+76,16,WarmMuted);
 Button(X+981,Y+34,160,58,TEXT("Close guide"),TEXT("help"),FString(),true,FString(),true);
 Line(FVector2D(Left,Y+113),FVector2D(X+1143,Y+113),StoneEdge);
 Line(FVector2D(X+587,Y+140),FVector2D(X+587,Y+598),StoneEdge);
 Text(TEXT("Your first settlement"),Left,Y+137,22,Gold);
 Text(bTouch?TEXT("Touch controls"):TEXT("Mouse and keyboard"),Right,Y+137,22,Gold);

 struct FGuideRow {FString Title,Detail;};
 const auto* Player=PC->Me();
 const FString Faction=Player?Player->Faction:FString();
 const auto& Catalog=FRTSCatalog::Get();
 const FString Seat=Catalog.DisplayName(Catalog.Def(TEXT("seat_of_the_people")),Faction);
 const FString Wealth=RTSNames::Get(TEXT("resource.wealth"),TEXT("Precious Things"));
 const bool bJaredite=RTS::IsJaredite(Faction);
 const FGuideRow Settlement[]={
  {TEXT("Gather supplies"),FString(TEXT("Select workers and "))+(bTouch?TEXT("use Orders on"):TEXT("right-click"))+TEXT(" berries, trees, or ore for Food, Timber, and ")+Wealth+TEXT(". Use Idle workers to find someone free.")},
  {TEXT("Build room to grow"),FString(bJaredite?TEXT("Select a builder beast"):TEXT("Select a worker"))+TEXT(", choose Build, then pick a structure and a clear site. Start with housing and a place to train soldiers.")},
  {TEXT("Train and advance"),TEXT("Select a building to recruit or research. Advance through four ages at your ")+Seat+TEXT(" to open new units and buildings.")},
  {TEXT("Protect your Seat"),TEXT("Scout with a few soldiers and keep your workers gathering. If your last Seat falls, start rebuilding it within 60 seconds.")}
 };
 for(int32 I=0;I<UE_ARRAY_COUNT(Settlement);I++) {
  const float RowY=Y+190+I*100;
  Text(Settlement[I].Title,Left,RowY,18,Brass);
  Wrap(Settlement[I].Detail,Left,RowY+29,514,16,Paper,3);
 }
 const auto& Controls=FRTSControls::Get();
 const FGuideRow Desktop[]={
  {TEXT("Left-click / drag"),TEXT("Select one unit or draw a box around a group.")},
  {TEXT("Right-click"),TEXT("Move, gather, repair, board, or attack a target.")},
  {TEXT("Arrows / wheel"),TEXT("Pan and zoom. Shift + wheel rotates the view.")},
  {Controls.LabelFor(TEXT("menu.build"))+TEXT(" / Esc"),TEXT("Open Build. Esc cancels an action or closes Build.")},
  {TEXT("Space / idle keys"),TEXT("Return to your Seat or choose idle workers (")+Controls.LabelFor(TEXT("idle.next"))+TEXT(" / ")+Controls.LabelFor(TEXT("idle.all"))+TEXT(").")},
  {TEXT("Ctrl + 0–9 / 0–9"),TEXT("Save a group and select it again with its number.")},
  {TEXT("Shift + order"),TEXT("Queue an order. Shift + recruit queues five units.")}
 };
 const FGuideRow Touch[]={
  {TEXT("Select"),TEXT("Tap a unit or building. Drag to look around.")},
  {TEXT("Orders"),TEXT("Select units first, then tap a spot or target to give an order.")},
  {TEXT("Box select"),TEXT("Drag a rectangle around units to select them together.")},
  {TEXT("Two fingers"),TEXT("Drag to pan, pinch to zoom, and twist to rotate.")},
  {TEXT("Build / Cancel"),TEXT("Pick a structure and tap its site. Cancel leaves targeting.")},
  {TEXT("Home / Chat"),TEXT("Return to your Seat or open the chat keyboard.")}
 };
 const FGuideRow* InputRows=bTouch?Touch:Desktop;
 const int32 InputCount=bTouch?UE_ARRAY_COUNT(Touch):UE_ARRAY_COUNT(Desktop);
 for(int32 I=0;I<InputCount;I++) {
  const float RowY=Y+192+I*55;
  Text(InputRows[I].Title,Right,RowY,15,Brass);
  Wrap(InputRows[I].Detail,Right,RowY+22,519,15,Paper,2);
 }
 const FString FactionAdvice=Faction==TEXT("nephites")?TEXT("Nephites: build religious support early. Low Favor leaves your people with very little health."):
  Faction==TEXT("lamanites")?TEXT("Lamanites: religious support raises Anger, speeding gathering, attacks and recruitment."):
  Faction==TEXT("gadianton")?TEXT("Gadianton: raid early. Your laborers gather slowly; keep lodges hidden from enemy scouts."):
  Faction==TEXT("laban")?TEXT("Laban: train Concubines at Wineries, then place their visible auras near working laborers."):
  bJaredite?TEXT("Jaredites: builder beasts construct your settlement; Oath Keepers and religious buildings strengthen its defenses."):FString();
 if(!FactionAdvice.IsEmpty())Text(FactionAdvice,Left,Y+587,14,Gold);
 Line(FVector2D(Left,Y+610),FVector2D(X+1143,Y+610),StoneEdge);
 DrawEmblem(FVector2D(Left+21,Y+661),19,Brass);
 Text(TEXT("The Liahona"),Left+57,Y+638,19,Gold);
 Wrap(bTouch?TEXT("Tap the map to look around. In Orders mode, tap it to send selected units. If the Liahona loses guidance, send sorry in chat to restore it."):TEXT("Click the map to look around. Right-click it to send selected units. If the Liahona loses guidance, send sorry in chat to restore it."),Left+57,Y+671,1010,15,Paper,2);
}

void ARTSHUD::DrawOverlays() {
 if(PC->bPauseMenu) {
  Buttons.Empty();HoverTip.Empty();
  Rect(0,0,W,H,FLinearColor(0,.015f,.02f,.7f));
  const float X=W*.5f-230,Y=H*.5f-305;
  Panel(X,Y,460,610,true);DrawEmblem(FVector2D(X+230,Y+54),29,Brass);
  Text(TEXT("Match menu"),X+230,Y+109,26,Paper,true);
  Text(TEXT("The match continues while this menu is open."),X+230,Y+146,14,WarmMuted,true);
  Button(X+35,Y+187,390,48,TEXT("Resume"),TEXT("resume"),FString(),true,FString(),true);
  Button(X+35,Y+244,390,45,TEXT("Field guide"),TEXT("help"));
  Button(X+35,Y+298,390,45,TEXT("Hotkeys"),TEXT("hotkeys"));
  Button(X+35,Y+352,390,42,PC->bReducedMotion?TEXT("REDUCED MOTION: ON"):TEXT("REDUCED MOTION: OFF"),TEXT("motion"));
  Button(X+35,Y+406,390,42,PC->bSoundEnabled?TEXT("SOUND CUES: ON"):TEXT("SOUND CUES: OFF"),TEXT("sound"));
  if(PC->Phase==1) Button(X+35,Y+460,390,42,TEXT("RESIGN"),TEXT("order"),TEXT("resign"),true,TEXT("End your participation in this battle."));
  Button(X+35,Y+519,390,45,TEXT("MAIN MENU"),TEXT("menu"));
 }
 if(PC->Phase==2 && !PC->bMenu && !PC->bPauseMenu) {
  Buttons.Empty();HoverTip.Empty();
  Rect(0,0,W,H,FLinearColor(0,.015f,.02f,.62f));
  const float X=W*.5f-340,Y=H*.5f-211;
  Panel(X,Y,680,422,true);DrawEmblem(FVector2D(W*.5f,Y+65),38,Brass);
  const auto* P=PC->Me();const bool Win=P && P->Team==PC->Winner;
  Text(Win?TEXT("YOUR CHRONICLE ENDURES"):TEXT("THE CHRONICLE HAS ENDED"),W*.5f,Y+133,30,Win?Gold:Paper,true);
  Text(PC->Winner>=0?FString::Printf(TEXT("Team %d claims the land"),PC->Winner+1):TEXT("The commanders depart the land"),W*.5f,Y+183,19,Muted,true);
  Text(FString::Printf(TEXT("Time in the land  %02d:%02d"),int32(PC->MatchTime)/60,int32(PC->MatchTime)%60),W*.5f,Y+225,15,Brass,true);
  if(PC->IsHost()) Button(X+45,Y+287,282,55,TEXT("RETURN TO LOBBY"),TEXT("lobby"),FString(),true,FString(),true);
  else Text(TEXT("Waiting for the host to return to the lobby."),W*.5f,Y+282,14,Muted,true);
  Button(PC->IsHost()?X+350:X+198,Y+287+(PC->IsHost()?0:34),282,55,TEXT("MAIN MENU"),TEXT("menu"));
 }
 if(PC->bHelp) {Buttons.Empty();HoverTip.Empty();Rect(0,0,W,H,FLinearColor(0,.015f,.02f,.8f));DrawHelp();}
 if(PC->bHotkeySettings) {Buttons.Empty();HoverTip.Empty();Rect(0,0,W,H,FLinearColor(0,.015f,.02f,.85f));DrawHotkeys();}
 if(PC->bConnecting) {
  Buttons.Empty();HoverTip.Empty();Rect(0,0,W,H,FLinearColor(0,.015f,.02f,.78f));
  const float X=W*.5f-325,Y=H*.5f-155;
  Panel(X,Y,650,310,true);DrawEmblem(FVector2D(W*.5f,Y+51),25,Brass);
  Text(TEXT("CONNECTING TO THE COUNCIL"),W*.5f,Y+102,26,Paper,true);
  Text(PC->JoinAddress,W*.5f,Y+148,18,Gold,true);
  Text(TEXT("Waiting for the host  ·  connection timeout 30 seconds"),W*.5f,Y+183,14,Muted,true);
  Button(X+165,Y+231,320,50,PC->bTouchControls?TEXT("Cancel connection"):TEXT("Cancel connection  ·  Esc"),TEXT("cancel_join"));
 }
 if(!PC->ConnectionError.IsEmpty()) {
  Buttons.Empty();HoverTip.Empty();Rect(0,0,W,H,FLinearColor(0,.015f,.02f,.82f));
  const float X=W*.5f-350,Y=H*.5f-164;
  Panel(X,Y,700,328,true);Text(TEXT("THE CONNECTION HAS ENDED"),W*.5f,Y+36,28,Paper,true);
  Wrap(PC->ConnectionError,X+37,Y+105,626,18,Muted,4);
  Button(X+170,Y+248,360,49,TEXT("RETURN TO MAIN MENU"),TEXT("dismiss_error"),FString(),true,FString(),true);
 }
 if(PC->ConnectionError.IsEmpty()&&!PC->bConnecting&&!PC->Notice.IsEmpty() && GetWorld()->GetRealTimeSeconds()<PC->NoticeUntil) {
  const float NW=FMath::Min(W-80.f,920.f),NX=(W-NW)*.5f;
  const bool bTouchMatch=PC->bTouchControls&&!PC->bMenu&&PC->Phase==1;
  const float NY=(PC->bHotkeySettings||PC->bHelp||PC->bPauseMenu)?H-65:bTouchMatch?204:82;
  Panel(NX,NY,NW,45,true);Text(Short(PC->Notice,112),W*.5f,NY+14,16,Gold,true);
 }
}

void ARTSHUD::DrawTooltip() {
 TooltipBounds=FBox2D(EForceInit::ForceInit);TooltipLineCount=0;if(HoverTip.IsEmpty())return;
 float Size=14,TW=390;TArray<FString> Lines;
 auto Layout=[&](){Lines.Empty();UFont* Font=TextFont(Size);const float Scale=ReadableSize(Size,U)/Font->GetMaxCharHeight();TArray<FString> Sources;HoverTip.ParseIntoArrayLines(Sources,false);
  for(const FString& Source:Sources){FString Current;int32 LastSpace=-1;for(TCHAR Ch:Source){Current.AppendChar(Ch);if(Ch==TCHAR(' '))LastSpace=Current.Len()-1;float Width,Height;Canvas->StrLen(Font,Current,Width,Height);if(Width*Scale>TW-28&&Current.Len()>1){const int Cut=LastSpace>0?LastSpace:Current.Len()-1;Lines.Add(Current.Left(Cut));Current=Current.Mid(Cut).TrimStart();LastSpace=INDEX_NONE;}}Lines.Add(Current);}
 };
 Layout();while(Lines.Num()*(ReadableSize(Size,U)+3)+26>H-30&&TW<FMath::Min(720.f,W-30)){TW+=55;Layout();}
 while(Lines.Num()*(ReadableSize(Size,U)+3)+26>H-30&&Size>10){Size-=1;Layout();}
 const float TH=Lines.Num()*(ReadableSize(Size,U)+3)+26,TX=FMath::Clamp(float(PC->Mouse.X/U)+19,15.f,W-TW-15),TY=FMath::Clamp(float(PC->Mouse.Y/U)-TH-8,15.f,FMath::Max(15.f,H-TH-15));
 TooltipBounds=FBox2D(FVector2D(TX*U,TY*U),FVector2D((TX+TW)*U,(TY+TH)*U));TooltipLineCount=Lines.Num();Panel(TX,TY,TW,TH,true);
 for(int I=0;I<Lines.Num();I++)Text(Lines[I],TX+14,TY+13+I*(ReadableSize(Size,U)+3),Size,Paper);
}
