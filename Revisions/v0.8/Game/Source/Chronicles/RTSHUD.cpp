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
 const FLinearColor Brass(.76f,.58f,.29f,1), Gold(.94f,.79f,.46f,1), Paper(.89f,.89f,.79f,1), Muted(.48f,.59f,.57f,1);
 const FLinearColor Teal(.20f,.63f,.59f,1), Red(.94f,.35f,.26f,1);
 const TCHAR* Maps[]={TEXT("Zarahemla Valley"),TEXT("Waters of Sidon"),TEXT("Land of Many Waters")};
 const TCHAR* FactionLines[]={TEXT("Elite covenant armies\nStrength through faith and industry"),TEXT("Vast armies and Zoramite elites\nSwift recruitment, relentless numbers"),TEXT("An economy of plunder\nStealth, raids and secret combinations"),TEXT("Jaredite industrial fortresses\nMighty beasts and enduring defenses"),TEXT("Relentless Jaredite offense\nOverwhelm the land with power"),TEXT("A wealthy, compact guard state\nPrecious things and elite warriors")};
 FString DevotionArt(const FString& F){return F==TEXT("lamanites")?TEXT("stats/anger"):F==TEXT("laban")?TEXT("stats/drunkenness"):F==TEXT("gadianton")?TEXT("stats/lasciviousness"):RTS::IsJaredite(F)?TEXT("stats/dominion"):TEXT("stats/favor");}
 FLinearColor Shade(FLinearColor C,float M) {return FLinearColor(C.R*M,C.G*M,C.B*M,C.A);}
 FString Short(const FString& S,int32 Max) {return S.Len()>Max?S.Left(Max-1)+TEXT("…"):S;}
 float ReadableSize(float Size,float Scale) {return FMath::Max(Size,(Size<=12?8.5f:Size<=17?10.f:11.5f)/Scale);}
 UFont* TextFont(float Size) {UFont* Font=Size>=24?GEngine->GetLargeFont():GEngine->GetMediumFont();return Font?Font:GEngine->GetMediumFont();}

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
 Rect(X+4,Y+5,Width,Height,FLinearColor(0,0,0,.28f));Rect(X,Y,Width,Height,PanelColor);
 const FLinearColor Edge=Accent?Brass:FLinearColor(.16f,.25f,.24f,1);
 Line(FVector2D(X,Y),FVector2D(X+Width,Y),Edge);Line(FVector2D(X,Y),FVector2D(X,Y+Height),Edge);
 Line(FVector2D(X+Width,Y),FVector2D(X+Width,Y+Height),Shade(Edge,.6f));Line(FVector2D(X,Y+Height),FVector2D(X+Width,Y+Height),Shade(Edge,.6f));
 if(Accent) {Rect(X,Y,26,2,Gold);Rect(X+Width-26,Y+Height-2,26,2,Gold);}
}
void ARTSHUD::Button(float X,float Y,float Width,float Height,const FString& Label,const FString& Action,const FString& Value,bool Enabled,const FString& Tip,bool Active) {
 const FBox2D Bounds(FVector2D(X*U,Y*U),FVector2D((X+Width)*U,(Y+Height)*U));
 const bool Hover=Bounds.IsInside(PC->Mouse);
 FLinearColor Fill=Active?FLinearColor(.17f,.24f,.20f,1):FLinearColor(.065f,.105f,.105f,1);
 if(Hover && Enabled) Fill=FLinearColor(.14f,.22f,.20f,1);if(!Enabled)Fill=FLinearColor(.045f,.065f,.067f,1);
 Rect(X,Y,Width,Height,Fill);
 Line(FVector2D(X,Y),FVector2D(X+Width,Y),Active||Hover?Brass:FLinearColor(.18f,.28f,.26f,1));
 Line(FVector2D(X,Y+Height),FVector2D(X+Width,Y+Height),FLinearColor(.12f,.18f,.17f,1));
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

bool ARTSHUD::Click(FVector2D Pixel) {
 for(int32 I=Buttons.Num()-1;I>=0;I--) if(Buttons[I].Bounds.IsInside(Pixel)) {
  const FRTSButton B=Buttons[I];
  if(B.Enabled) PC->UIAction(B.Action,B.Value);else if(!B.Tooltip.IsEmpty()) PC->ShowNotice(B.Tooltip);
  return true;
 }
 return false;
}
bool ARTSHUD::FindButtonBounds(const FString& Action,const FString& Value,FBox2D& Out) const {
 for(int32 I=Buttons.Num()-1;I>=0;--I) if(Buttons[I].Action==Action && Buttons[I].Value==Value && Buttons[I].Enabled) {Out=Buttons[I].Bounds;return true;}
 return false;
}
int32 ARTSHUD::CountButtons(const FString& Action,const FString& Value) const {
 int32 Count=0;for(const auto& B:Buttons)if(B.Action==Action&&(Value.IsEmpty()||B.Value==Value))Count++;return Count;
}

bool ARTSHUD::BlocksWorld(FVector2D P) const {return PC?!PC->InWorld(P):true;}

void ARTSHUD::DrawHUD() {
 Super::DrawHUD();PC=Cast<ARTSController>(PlayerOwner);if(!Canvas||!PC)return;
 PC->ViewSize=FVector2D(Canvas->SizeX,Canvas->SizeY);
 U=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f);U=FMath::Max(U,.25f);
 W=Canvas->SizeX/U;H=Canvas->SizeY/U;Buttons.Empty();HoverTip.Empty();MapBounds=FBox2D(EForceInit::ForceInit);QueueHits.Empty();PC->CardHotkeys.Empty();
 DrawBackdrop(PC->bMenu||PC->Phase==0);
 if(PC->bMenu) DrawMenu();
 else if(PC->Phase==0) DrawLobby();
 else {DrawWorld();DrawTopBar();DrawSelection();DrawMinimap();DrawProductionQueue();}
 if(!PC->bMenu) DrawChat();
 DrawOverlays();DrawTooltip();
}

void ARTSHUD::DrawBackdrop(bool Menu) {
 Rect(0,0,W,H,Ink);
 if(!Menu) return;
 for(int32 I=0;I<24;I++) {float Y=H*I/24;Rect(0,Y,W,H/24+1,FLinearColor(.025f+I*.0018f,.065f+I*.0013f,.075f+I*.0005f,1));}
 const FVector2D Sun(W*.76f,H*.31f);
 for(int32 I=0;I<5;I++) Circle(Sun,91-I*9,FLinearColor(.72f,.51f,.23f,.025f+I*.012f));
 Circle(Sun,44,FLinearColor(.68f,.50f,.28f,.65f));
 Poly({FVector2D(0,H*.67f),FVector2D(W*.20f,H*.48f),FVector2D(W*.38f,H*.60f),FVector2D(W*.62f,H*.36f),FVector2D(W*.88f,H*.59f),FVector2D(W,H*.43f),FVector2D(W,H),FVector2D(0,H)},FLinearColor(.06f,.13f,.14f,1));
 Poly({FVector2D(0,H*.78f),FVector2D(W*.28f,H*.61f),FVector2D(W*.49f,H*.78f),FVector2D(W*.81f,H*.49f),FVector2D(W,H*.70f),FVector2D(W,H),FVector2D(0,H)},FLinearColor(.045f,.10f,.105f,1));
 // The stepped silhouette is drawn from simple geometry and remains sharp at every resolution.
 const float X=W*.77f,Y=H*.79f;
 for(int32 I=0;I<5;I++) {const float BW=400-I*62;Rect(X-BW*.5f,Y-I*33,BW,34,FLinearColor(.07f+I*.005f,.115f+I*.006f,.107f+I*.004f,1));Line(FVector2D(X-BW*.5f,Y-I*33),FVector2D(X+BW*.5f,Y-I*33),FLinearColor(.23f,.27f,.20f,.5f));}
 Rect(X-44,Y-203,88,73,FLinearColor(.09f,.14f,.13f,1));Rect(X-55,Y-212,110,12,Shade(Brass,.32f));
 Rect(X-12,Y-186,24,55,Ink);
 for(int32 I=0;I<8;I++) {float XX=FMath::Fmod(I*239.f+47,W);Line(FVector2D(XX,H*.92f),FVector2D(XX+110,H*.80f),FLinearColor(.15f,.24f,.22f,.15f));}
 Line(FVector2D(70,H-62),FVector2D(W-70,H-62),FLinearColor(.25f,.29f,.22f,.7f));
}

void ARTSHUD::DrawMenu() {
 const float X=FMath::Max(90.f,(W-1420)*.5f),Y=H*.15f;
 DrawEmblem(FVector2D(X+26,Y-24),23,Brass);
 Text(TEXT("A REAL-TIME STRATEGY CHRONICLE"),X+66,Y-33,14,Brass);
 Text(TEXT("CHRONICLES"),X,Y+14,68,Paper);
 Text(TEXT("OF THE PROMISED LAND"),X+4,Y+88,25,Gold);
 Line(FVector2D(X+4,Y+138),FVector2D(X+405,Y+138),Brass);
 Wrap(TEXT("Raise a people. Shape a kingdom.\nWrite your chronicle across the land."),X+4,Y+164,490,20,Muted,3);
 const float B=Y+252;
 if(PC->MenuScreen==0) {
  Button(X,B,480,57,TEXT("HOST A LAN GAME"),TEXT("host"),FString(),true,TEXT("Open a four-player LAN lobby. Other commanders join using this computer's IPv4 address."),true);
  Button(X,B+69,480,57,TEXT("JOIN A LAN GAME"),TEXT("join_screen"));
  Button(X,B+138,480,57,TEXT("SOLO SANDBOX"),TEXT("solo"),FString(),true,TEXT("Explore every civilization and its economy in a local sandbox."));
  Button(X,B+214,230,40,TEXT("HOW TO PLAY  ·  F1"),TEXT("help"));Button(X+250,B+214,230,40,TEXT("QUIT"),TEXT("exit"));
  Button(X,B+265,480,40,TEXT("CUSTOMIZE HOTKEYS"),TEXT("hotkeys"));
 } else {
  Panel(X,B,510,250,true);Text(TEXT("JOIN THE HOST"),X+22,B+20,22,Gold);
  Text(TEXT("Host's LAN address  ·  port 7777"),X+22,B+61,16,Muted);
  Button(X+22,B+91,466,47,PC->JoinAddress+(FMath::Fmod(GetWorld()->GetRealTimeSeconds(),1.f)<.5f?TEXT("|"):TEXT("")),TEXT("focus"),TEXT("join"),true,TEXT("Enter an IPv4 address such as 192.168.1.15. Optional :port is supported."),PC->TextFocus==TEXT("join"));
  Button(X+22,B+163,220,52,TEXT("CONNECT"),TEXT("join"),FString(),true,FString(),true);Button(X+260,B+163,228,52,TEXT("BACK"),TEXT("back"));
 }
 Text(TEXT("SIX CIVILIZATIONS  /  FOUR COMMANDERS"),W-456,H-102,14,Brass,false);
 Text(TEXT("Chronicles  ·  LAN edition"),X,H-42,13,Muted);
}

void ARTSHUD::DrawLobby() {
 const float X=(W-1440)*.5f,Y=(H-760)*.5f;
 Panel(X,Y,1440,760,true);
 Text(TEXT("THE COUNCIL OF COMMANDERS"),X+30,Y+24,31,Paper);
 Text(GetNetMode()==NM_Standalone?TEXT("SOLO SANDBOX"):TEXT("LAN LOBBY  ·  UP TO 4 PLAYERS"),X+31,Y+68,14,Brass);
 Button(X+1218,Y+24,190,40,TEXT("MAIN MENU"),TEXT("menu"));
 Text(TEXT("YOUR NAME"),X+30,Y+111,12,Muted);
 Button(X+30,Y+134,390,45,PC->PlayerName+(PC->TextFocus==TEXT("name")?TEXT("|"):TEXT("")),TEXT("focus"),TEXT("name"),true,TEXT("Click to edit. Enter confirms. Ctrl+A clears the field."),PC->TextFocus==TEXT("name"));
 Button(X+440,Y+134,200,45,FString::Printf(TEXT("TEAM %d  >"),PC->LobbyTeam+1),TEXT("team"),FString(),true,TEXT("Commanders on the same team share victory."));
 Text(TEXT("CHOOSE YOUR CIVILIZATION"),X+30,Y+204,14,Brass);
 const auto& Fs=FRTSCatalog::Get().Factions;
 for(int32 I=0;I<Fs.Num();I++) {
  const float CX=X+30+(I%3)*294,CY=Y+234+(I/3)*121;
  Button(CX,CY,278,106,FString(),TEXT("faction"),FString::FromInt(I),true,FString(),PC->LobbyFaction==I);
  Circle(FVector2D(CX+22,CY+26),6,RTS::PlayerColor(I%4));
  FRTSVisualAssets::Get().TryDrawIconId(Canvas,TEXT("factions/")+Fs[I].Id,FVector2D((CX+12)*U,(CY+8)*U),31*U,FLinearColor::White);
  Text(Fs[I].Name,CX+51,CY+14,17,PC->LobbyFaction==I?Gold:Paper);
  Wrap(RTSNames::ResolveText(FactionLines[FMath::Min(I,5)]),CX+18,CY+44,244,13,Muted,3);
 }
 Text(TEXT("THE BATTLEFIELD"),X+30,Y+478,14,Brass);
 for(int32 I=0;I<3;I++) {
  const float CX=X+30+I*294,CY=Y+504;
  Button(CX,CY,278,76,FString(),TEXT("map"),FString::FromInt(I),PC->IsHost(),PC->IsHost()?TEXT("The host chooses the battlefield."):TEXT("The host chooses the battlefield."),PC->LobbyMap==I);
  Text(Maps[I],CX+15,CY+12,17,Paper);
  Wrap(I==0?TEXT("Open valley · rocky central passes"):I==1?TEXT("Team frontiers · contested heartland"):TEXT("Crossings · ships and landings"),CX+15,CY+39,245,13,Muted,2);
 }
 const TCHAR* ResourceLabels[]={TEXT("Lean"),TEXT("Standard"),TEXT("Abundant"),TEXT("Sandbox")};const TCHAR* FertilityLabels[]={TEXT("Sparse"),TEXT("Normal"),TEXT("Rich")};
 const FString Labels[]={FString(TEXT("Resources: "))+ResourceLabels[FMath::Clamp(PC->LobbyResources,0,3)],FString::Printf(TEXT("Speed: %.2gx"),PC->LobbySpeed),TEXT("Land: ")+FString(FertilityLabels[FMath::Clamp(PC->LobbyFruitfulness,0,2)]),PC->bLobbyFog?TEXT("Fog: On"):TEXT("Fog: Off")};
 const TCHAR* Settings[]={TEXT("resources"),TEXT("speed"),TEXT("fruitfulness"),TEXT("fog")};
 const TCHAR* Tips[]={TEXT("Starting reserves: Lean 150/150/100; Standard 300/300/200; Abundant 1000/1000/750 +200 cement; Sandbox 5000 each +1000 cement."),TEXT("Simulation speed: 0.75x, 1x, 1.25x, 1.5x or 2x. Applies equally to every commander."),TEXT("Natural reserves in trees, ore, berries and hunted animals: Sparse 50%, Normal 100%, Rich 200%."),TEXT("Fog On: scout to reveal enemies; landmarks remain as last seen. Fog Off: the whole map is visible.")};
 for(int I=0;I<4;I++)Button(X+30+I*220,Y+597,212,38,Labels[I],TEXT("lobby_setting"),Settings[I],PC->IsHost(),FString(Tips[I])+TEXT(" Host changes reset readiness."));
 const float RX=X+958;Line(FVector2D(X+924,Y+113),FVector2D(X+924,Y+620),FLinearColor(.17f,.27f,.24f,1));
 Text(TEXT("COMMANDERS"),RX,Y+113,14,Brass);
 for(int32 I=0;I<4;I++) {
  const float RY=Y+151+I*110;
  const FRTSPlayerView* P=PC->Players.FindByPredicate([I](const FRTSPlayerView& V){return V.Slot==I && V.bConnected;});
  Rect(RX,RY,447,94,FLinearColor(.04f,.075f,.077f,1));
  if(P) {
   Rect(RX,RY,4,94,RTS::PlayerColor(I));Text(Short(P->Name,25)+(I==PC->LocalSlot?TEXT("  (you)"):TEXT("")),RX+18,RY+13,18,Paper);
   Text(FRTSCatalog::Get().Faction(P->Faction).Name+FString::Printf(TEXT("  ·  Team %d"),P->Team+1),RX+18,RY+42,14,Muted);
   Text(P->bReady?TEXT("READY"):TEXT("PREPARING"),RX+18,RY+65,12,P->bReady?Teal:Brass);
  } else {Text(TEXT("Waiting for a commander..."),RX+18,RY+29,16,Muted);}
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
  Text(HostAddresses.Num()?TEXT("HOST IPv4 ADDRESS — ENTER THIS ON THE OTHER PC"):TEXT("FIND THIS PC'S IPv4 ADDRESS"),RX,Y+591,11,Brass);
  if(HostAddresses.Num()) {
   Wrap(FString::Join(HostAddresses,TEXT("   /   ")),RX,Y+611,447,15,Gold,2);
   Text(TEXT("Port 7777 (automatic)  ·  Enter only the IPv4 address"),RX,Y+635,11,Muted);
  } else Wrap(TEXT("Windows Settings > Network & Internet > Properties."),RX,Y+611,447,13,Muted,2);
 } else Text(GetNetMode()==NM_Standalone?TEXT("Solo sandbox. Host a LAN game from the main menu."):TEXT("Connected to the host's LAN lobby."),RX,Y+608,12,Muted);
 Line(FVector2D(X+30,Y+649),FVector2D(X+1408,Y+649),FLinearColor(.20f,.30f,.26f,1));
 Button(X+30,Y+680,200,49,TEXT("HOW TO PLAY"),TEXT("help"));
 Button(X+958,Y+680,207,49,PC->Me()&&PC->Me()->bReady?TEXT("UNREADY"):TEXT("READY"),TEXT("ready"),FString(),true,TEXT("Your civilization and team are locked in when you ready up."),PC->Me()&&PC->Me()->bReady);
 bool Ready=true;int32 Count=0;TSet<int32> Teams;
 for(const auto& P:PC->Players) if(P.bConnected) {Count++;Teams.Add(P.Team);if(!P.bReady)Ready=false;}
 const bool CanStart=PC->IsHost() && Ready && Count>0 && (GetNetMode()==NM_Standalone || (Count>=2&&Teams.Num()>=2));
 Button(X+1180,Y+680,228,49,TEXT("BEGIN CHRONICLE"),TEXT("start"),FString(),CanStart,PC->IsHost()?TEXT("All commanders must be ready. LAN matches require at least two opposing teams."):TEXT("Waiting for the host to begin."),true);
 Text(GetNetMode()==NM_Standalone?TEXT("One commander can begin a sandbox. No computer opponents are added."):TEXT("Everyone ready. At least two teams. The host begins the chronicle."),X+250,Y+694,13,Muted);
}

void ARTSHUD::DrawWorld() {
 const float Bottom=H-228;
 const auto Screen=[this](FVector2D P){return PC->WorldToScreen(P)/U;};
 // A fixed cell mesh gives terrain and fog the same exact bounds as the simulation.
 for(int32 Y=0;Y<64;Y++) for(int32 X=0;X<64;X++) {
  const FVector2D P(X*2.5f,Y*2.5f),Center=P+FVector2D(1.25f,1.25f),S=Screen(Center);
  if(S.X<-55 || S.X>W+55 || S.Y<20 || S.Y>Bottom+55) continue;
  const uint8 Fog=PC->Fog.IsValidIndex(Y*64+X)?PC->Fog[Y*64+X]:0;
  const bool Water=RTS::WaterAt(Center,PC->Map),Rock=RTS::RockAt(Center,PC->Map);
  const float Noise=((X*73+Y*147+X*Y*3)%19)/19.f;
  FLinearColor Ground=Water?FLinearColor(.075f+Noise*.018f,.24f+Noise*.025f,.28f+Noise*.02f,1):Rock?FLinearColor(.30f+Noise*.025f,.32f+Noise*.03f,.26f+Noise*.02f,1):FLinearColor(.25f+Noise*.022f,.31f+Noise*.028f,.18f+Noise*.02f,1);
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
 DrawDeaths();
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
   if(D.Id==TEXT("place_of_many_waters")) Valid=Valid&&PC->Map==2&&(FMath::Abs(P.X-71)<3||FMath::Abs(P.X-89)<3)&&P.Y>10&&P.Y<150;
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
 Circle(S+FVector2D(5*Z,4*Z),R*1.05f,FLinearColor(0,0,0,.23f),true,.46f);
 if(Selected) {Circle(S,R+5*Z,Team,false,.5f);Circle(S,R+8*Z,Shade(Team,.55f),false,.5f);}
 TGuardValue<float> OpacityScope(EntityOpacity,E.bRemembered?.36f:PC->VisualOpacity(E));
 const auto* WorkTarget=PC->FindEntity(E.Target);const auto* WorkDef=WorkTarget?&FRTSCatalog::Get().Def(WorkTarget->Def):nullptr;
 const FString Work=FRTSVisualAssets::WorkState(E,WorkDef);
 FRTSEntityView Animated=E;if(!Work.IsEmpty())Animated.Status=Work;
 if(!Selected&&E.Owner>=0&&FRTSVisualAssets::Get().HasRenderable(D.Id,PC->FactionFor(E.Owner)))Circle(S,FMath::Max(8.f*Z,R+Z),Team,false,.45f);
 const bool Foundation=D.bBuilding&&E.Progress<1,Gate=D.bBuilding&&E.bGate;
 const bool CustomArt=!Foundation&&!Gate&&FRTSVisualAssets::Get().TryDraw(Canvas,Animated,D,S,Z,U,Team,T,EntityOpacity,PC->FactionFor(E.Owner));
 if(Foundation){
  const float Foot=FMath::Max(1.f,D.Radius);auto At=[&](float X,float Y){return PC->WorldToScreen(E.Pos+FVector2D(X,Y)*Foot)/U;};
  TArray<FVector2D> Corners={At(-1,-1),At(1,-1),At(1,1),At(-1,1)};Poly(Corners,FLinearColor(.25f,.22f,.16f,.90f));
  const float Height=(12+26*E.Progress)*Z;for(int I=0;I<4;I++){const FVector2D A=Corners[I],B=Corners[(I+1)%4],Rise(0,Height);Line(A,B,Team,2*Z);Line(A,A-Rise,Brass,3*Z);Line(A-Rise,B-Rise,Brass,2*Z);Line(A,B-Rise,FLinearColor(.55f,.39f,.22f,1),1.5f*Z);}
  Rect(S.X-24*Z,S.Y+13*Z,48*Z,4*Z,Ink);Rect(S.X-24*Z,S.Y+13*Z,48*Z*E.Progress,4*Z,Gold);
 }
 FBox2D ArtBounds;const bool HasArtBounds=CustomArt&&FRTSVisualAssets::Get().GetHitBounds(D.Id,S,Z,1.f,ArtBounds,true,PC->FactionFor(E.Owner));
 if(!CustomArt&&!Foundation&&!Gate) DrawProceduralEntity(E);
 if(Gate){const float Span=FMath::Max(16.f,D.Radius*9)*Z,Height=38*Z,Width=8*Z;const FLinearColor Stone=D.Id==TEXT("cement_wall")?FLinearColor(.70f,.71f,.65f,1):FLinearColor(.48f,.34f,.20f,1);
  for(int Side:{-1,1}){const FVector2D Base=S+FVector2D(Side*Span,0);Poly({Base+FVector2D(-Width,0),Base+FVector2D(Width,0),Base+FVector2D(Width,-Height),Base+FVector2D(-Width,-Height)},Stone);Poly({Base+FVector2D(Width,0),Base+FVector2D(Width+5*Z,-4*Z),Base+FVector2D(Width+5*Z,-Height-4*Z),Base+FVector2D(Width,-Height)},Shade(Stone,.65f));}
  Rect(S.X-Span-Width,S.Y-Height-5*Z,2*(Span+Width),7*Z,Stone);Line(S+FVector2D(-Span,-Height-5*Z),S+FVector2D(Span,-Height-5*Z),Team,2*Z);
 }

 if(E.bRemembered)return;
 if(!Work.IsEmpty()&&!D.bBuilding&&!E.bAttacking&&FVector2D::DistSquared(PC->VisualPosition(E),E.Pos)<.01) {
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
 if(E.bAttacking) {
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
 Text(FRTSCatalog::Get().Faction(P->Faction).Name,63,12,20,Paper);
 Text(FString::Printf(TEXT("AGE %d  ·  TEAM %d"),P->Age,P->Team+1),64,39,12,Brass);
 const bool Jaredite=RTS::IsJaredite(P->Faction);const int32 ResourceCount=Jaredite?4:3;const float Start=Jaredite?310.f:360.f,Stride=Jaredite?132.f:154.f;
 const FString Labels[]={RTSNames::Get(TEXT("resource.food"),TEXT("Food")).ToUpper(),RTSNames::Get(TEXT("resource.timber"),TEXT("Timber")).ToUpper(),RTSNames::Get(TEXT("resource.wealth"),TEXT("Precious Things")).ToUpper(),TEXT("CEMENT")};
 const float Amounts[]={float(P->Resources.X),float(P->Resources.Y),float(P->Resources.Z),P->Cement};
 const FLinearColor Colors[]={FLinearColor(.69f,.76f,.38f,1),FLinearColor(.66f,.50f,.30f,1),Gold,FLinearColor(.64f,.68f,.65f,1)};
 for(int32 I=0;I<ResourceCount;I++) {
  const float X=Start+I*Stride;const int32 Workers=PC->ResourceWorkers(I).Num();
  const TCHAR* StatKeys[]={TEXT("stats/food"),TEXT("stats/timber"),TEXT("stats/wealth"),TEXT("stats/cement")};
  if(!FRTSVisualAssets::Get().TryDrawIconId(Canvas,StatKeys[I],FVector2D((X-3)*U,20*U),22*U,FLinearColor::White))Poly({FVector2D(X,31),FVector2D(X+8,20),FVector2D(X+16,31),FVector2D(X+8,42)},Colors[I]);
  Text(FString::FromInt(FMath::FloorToInt(Amounts[I])),X+27,10,23,Paper);
  Button(X+23,37,Stride-25,25,FString(),TEXT("workforce"),FString::FromInt(I),Workers>0,TEXT("Click to cycle through the workers generating ")+Labels[I].ToLower()+TEXT(". Each click selects and centers on the next worker."));
  Text(Labels[I]+FString::Printf(TEXT(" [%d]"),Workers),X+28,44,9,Muted);
 }
 const float PX=Start+ResourceCount*Stride+8;
 Text(FString::Printf(TEXT("%d / %d"),P->Population,P->Capacity),PX,12,23,P->Population>=P->Capacity?Red:Paper);Text(TEXT("POPULATION"),PX+1,41,10,Muted);
 Button(PX+125,7,130,52,FString(),TEXT("religion_info"),FString(),true,PC->DevotionLabel()+TEXT("\n")+P->ReligionTooltip);
 FRTSVisualAssets::Get().TryDrawIconId(Canvas,DevotionArt(P->Faction),FVector2D((PX+130)*U,13*U),22*U,FLinearColor::White);
 Text(FString::Printf(TEXT("%d"),FMath::RoundToInt(P->Favor)),PX+158,12,23,Gold);Text(PC->DevotionLabel().ToUpper(),PX+136,41,10,Muted);
 const int32 Idle=PC->IdleVillagers().Num();
 Button(W-495,14,185,36,FString::Printf(TEXT("IDLE WORKERS  %d"),Idle),TEXT("idle"),FString(),Idle>0,
  TEXT("Select next idle worker: ")+FRTSControls::Get().LabelFor(TEXT("idle.next"))+TEXT(". Select all: ")+FRTSControls::Get().LabelFor(TEXT("idle.all"))+TEXT(" or Ctrl+click. Builder beasts are excluded."),Idle>0);
 Text(FString::Printf(TEXT("%02d:%02d"),int32(PC->MatchTime)/60,int32(PC->MatchTime)%60),W-284,21,20,Paper);
 Button(W-175,14,66,36,TEXT("F1"),TEXT("help"),FString(),true,TEXT("Controls and the rules of the land."));Button(W-98,14,78,36,TEXT("MENU"),TEXT("pause"));
 // Use the same overlay state as Escape, while keeping the menu button in the hit-test list.
 if(Buttons.Num()) Buttons.Last().Action=TEXT("pause");
 const int32 IdleArmy=PC->IdleMilitary().Num();
 Button(W-305,70,205,30,FString::Printf(TEXT("IDLE MILITARY  %d"),IdleArmy),TEXT("idle_military"),FString(),IdleArmy>0,TEXT("Click to select the next idle soldier. Ctrl+click selects all idle military."),IdleArmy>0);
 if(P->bDefeated) {Panel(W*.5f-220,77,440,44);Text(TEXT("Your chronicle has ended. You may observe the battle."),W*.5f,90,15,Brass,true);}
 else if(P->DefeatRemaining<59.9f) {Panel(W*.5f-240,77,480,44,true);Text(FString::Printf(TEXT("REBUILD YOUR SEAT  ·  %.0f seconds remain"),FMath::CeilToFloat(P->DefeatRemaining)),W*.5f,90,16,Gold,true);}
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
 } else if(Match) {Text(TEXT("Enter to chat"),X,Y+8,11,Muted);}
}

void ARTSHUD::DrawHelp() {
 const float X=(W-1060)*.5f,Y=(H-730)*.5f;
 Panel(X,Y,1060,730,true);DrawEmblem(FVector2D(X+50,Y+45),24,Brass);
 Text(TEXT("THE COMMANDER'S FIELD GUIDE"),X+95,Y+28,28,Paper);
 Text(TEXT("Build a people. Defend their seat. End the enemy's chronicle."),X+95,Y+67,15,Muted);
 Line(FVector2D(X+30,Y+110),FVector2D(X+1030,Y+110),Brass);
 const float A=X+35,B=X+558;
 Text(TEXT("COMMAND THE LAND"),A,Y+137,15,Gold);
 const FString BuildMenuLabel=FRTSControls::Get().LabelFor(TEXT("menu.build"));
 const TCHAR* Controls[]={
  TEXT("Left click / drag"),TEXT("Select a unit / select an army"),
  TEXT("Shift + selection"),TEXT("Add or remove from the selection"),
  TEXT("Double click"),TEXT("Select visible units of the same type"),
  TEXT("Right click"),TEXT("Move, gather, repair, board, trade, attack"),
  TEXT("Arrows / WASD"),TEXT("Pan the camera; A/S pan when unselected"),
  TEXT("Wheel / Shift+wheel"),TEXT("Zoom / rotate the map"),
  TEXT("Space"),TEXT("Return to your headquarters"),
  TEXT("Ctrl + 0–9 / 0–9"),TEXT("Save / select a control group"),
  TEXT("Alt + 0–9"),TEXT("Select and center on a control group"),
  TEXT("A / S / H / P"),TEXT("Attack-move / stop / hold / patrol"),
  *BuildMenuLabel,TEXT("Construction menu (Escape closes)"),
  TEXT("Enter / Esc / F1"),TEXT("Chat / cancel or menu / this guide")
 };
 for(int I=0;I<12;I++) {Text(Controls[I*2],A,Y+176+I*29,13,Brass);Text(Controls[I*2+1],A+179,Y+176+I*29,13,Paper);}
 Text(TEXT("GROW A CIVILIZATION"),B,Y+137,15,Gold);
 Wrap(RTSNames::ResolveText(TEXT("Gather Food from wild provisions and economic sites, Timber from forests, and Wealth from ore. Select workers and right-click a resource or completed site. Work sites have limited slots.")),B,Y+176,460,15,Paper,5);
 const auto* Player=PC->Me();const FString Faction=Player?Player->Faction:FString();const auto& Catalog=FRTSCatalog::Get();
 const FString Builders=RTS::IsJaredite(Faction)?TEXT("Cureloms build Ages I-II, Cumeloms Ages I-III, and Elephants every structure, including Cement defenses."):TEXT("Select a laborer, open construction with ")+FRTSControls::Get().LabelFor(TEXT("menu.build"))+TEXT(", choose a structure, then click a clear site, including beyond your current vision.");
 Wrap(Builders+TEXT(" Walls use two endpoints; hold Shift to chain another endpoint."),B,Y+286,460,15,Paper,5);
 Wrap(TEXT("Select a building to recruit and research. Shift+click queues five units. Advance through four ages at your ")+Catalog.DisplayName(Catalog.Def(TEXT("seat_of_the_people")),Faction)+TEXT(". Hover over any action for costs, population and prerequisites."),B,Y+396,460,15,Paper,5);
 Text(FRTSControls::Get().LabelFor(TEXT("idle.next"))+TEXT(" / ")+FRTSControls::Get().LabelFor(TEXT("idle.all"))+TEXT("   Next / all idle workers"),A,Y+531,13,Gold);
 Line(FVector2D(X+30,Y+557),FVector2D(X+1030,Y+557),FLinearColor(.20f,.30f,.26f,1));
 Text(TEXT("THE LIAHONA"),A,Y+581,15,Gold);
 Wrap(RTSNames::ResolveText(TEXT("The brass compass shows only your explored land and visible enemies. Click to look; right-click to send your selected force. Completed profanity in your own chat draft makes it fail. Submit exactly sorry to restore it. Shiz and roster names are allowed.")),A,Y+612,716,14,Muted,4);
 Button(X+814,Y+642,210,50,TEXT("RETURN TO THE LAND"),TEXT("help"),FString(),true,FString(),true);
}

void ARTSHUD::DrawOverlays() {
 if(PC->bPauseMenu) {
  Buttons.Empty();HoverTip.Empty();
  Rect(0,0,W,H,FLinearColor(0,.015f,.02f,.7f));
  const float X=W*.5f-230,Y=H*.5f-305;
  Panel(X,Y,460,610,true);DrawEmblem(FVector2D(X+230,Y+54),29,Brass);
  Text(TEXT("THE CHRONICLE CONTINUES"),X+230,Y+109,23,Paper,true);
  Text(TEXT("LAN simulation continues while this menu is open."),X+230,Y+146,13,Muted,true);
  Button(X+35,Y+187,390,45,TEXT("RETURN TO THE LAND"),TEXT("resume"),FString(),true,FString(),true);
  Button(X+35,Y+244,390,42,TEXT("FIELD GUIDE"),TEXT("help"));
  Button(X+35,Y+298,390,42,TEXT("CUSTOMIZE HOTKEYS"),TEXT("hotkeys"));
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
  Button(X+165,Y+231,320,47,TEXT("CANCEL CONNECTION  ·  ESC"),TEXT("cancel_join"));
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
  const float NY=(PC->bHotkeySettings||PC->bHelp||PC->bPauseMenu)?H-65:82;
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
