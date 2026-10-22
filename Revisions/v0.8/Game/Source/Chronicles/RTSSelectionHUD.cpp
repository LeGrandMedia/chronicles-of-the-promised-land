#include "RTSHUD.h"
#include "RTSController.h"
#include "RTSControls.h"
#include "RTSNames.h"
#include "RTSVisualAssets.h"
#include "Engine/World.h"

namespace {
 const FLinearColor UIInk(.025f,.042f,.047f,1),UIPanelColor(.035f,.060f,.063f,.98f),UIBrass(.76f,.58f,.29f,1),UIGold(.94f,.79f,.46f,1),UIPaper(.89f,.89f,.79f,1),UIMuted(.48f,.59f,.57f,1),UITeal(.20f,.63f,.59f,1);
 FString Truncate(const FString& S,int32 N){return S.Len()>N?S.Left(N-1)+TEXT("…"):S;}
 const TCHAR* AgeNumeral(int32 Age){return Age==1?TEXT("I"):Age==2?TEXT("II"):Age==3?TEXT("III"):Age==4?TEXT("IV"):TEXT("V");}
}
FString ARTSHUD::Name(const FRTSDef& D) const {const auto* P=PC?PC->Me():nullptr;return FRTSCatalog::Get().DisplayName(D,P?P->Faction:FString());}
FString ARTSHUD::Description(const FRTSDef& D) const {const auto* P=PC?PC->Me():nullptr;return FRTSCatalog::Get().Description(D,P?P->Faction:FString());}
FString ARTSHUD::NameFor(const FRTSDef& D,int32 EntityOwner) const {return FRTSCatalog::Get().DisplayName(D,PC->FactionFor(EntityOwner));}
FString ARTSHUD::DescriptionFor(const FRTSDef& D,int32 EntityOwner) const {return FRTSCatalog::Get().Description(D,PC->FactionFor(EntityOwner));}
FString ARTSHUD::CounterText(const FRTSDef& D) const {
 if(D.bBuilding)return FString();
 if(RTS::IsReligious(D))return TEXT("Takes 50% extra damage from ranged units.");
 if(D.bHero||RTS::IsCarrier(D)||D.Kind==TEXT("beast"))return TEXT("Immune to conversion.");
 if(D.Attack>0&&D.Range>2)return TEXT("Deals 50% extra damage to religious units.");
 if(D.Attack>0&&!RTS::IsWorker(D))return TEXT("Resists 25% of ranged damage.");
 return FString();
}
void ARTSHUD::Icon(float X,float Y,float Size,const FRTSDef& D,FLinearColor Tint,const FString& Faction) {
 Rect(X,Y,Size,Size,UIInk);
 if(!FRTSVisualAssets::Get().TryDrawIcon(Canvas,D,FVector2D(X*U,Y*U),Size*U,Tint,Faction.IsEmpty()?PC->FactionFor(PC->LocalSlot):Faction)) {
  const FLinearColor Frame(Tint.R*.45f,Tint.G*.45f,Tint.B*.45f,.8f);
  Line(FVector2D(X+3,Y+3),FVector2D(X+Size-3,Y+3),Frame);Line(FVector2D(X+3,Y+Size-3),FVector2D(X+Size-3,Y+Size-3),Frame);
  Line(FVector2D(X+3,Y+3),FVector2D(X+3,Y+Size-3),Frame);Line(FVector2D(X+Size-3,Y+3),FVector2D(X+Size-3,Y+Size-3),Frame);
  Text(Name(D).Left(1).ToUpper(),X+Size*.5f,Y+Size*.5f-8,14,Tint,true);
 }
}
void ARTSHUD::DrawSelection() {
 const float Y=H-222,LX=248,CX=600,AW=W-CX-18;
 Rect(0,Y-6,W,228,UIPanelColor);Line(FVector2D(0,Y-6),FVector2D(W,Y-6),UIBrass);Panel(LX,Y+8,335,199);
 const auto& Cat=FRTSCatalog::Get();const auto* P=PC->Me();
 const FRTSEntityView* E=nullptr;
 for(int32 Id:PC->Selection)if(const auto* Candidate=PC->FindEntity(Id))if(PC->ActiveSubgroup.IsEmpty()||Cat.Def(Candidate->Def).Id==PC->ActiveSubgroup){E=Candidate;break;}
 if(!E){PC->ActiveSubgroup.Empty();Text(TEXT("A PEOPLE AWAIT YOUR WORD"),LX+18,Y+30,17,UIGold);Wrap(TEXT("Select your people, then right-click to command. Shift-click to queue actions. Shift + wheel rotates the map."),LX+18,Y+79,295,15,UIMuted,5);return;}
 const auto& D=Cat.Def(E->Def);const bool Own=E->Owner==PC->LocalSlot;
 Icon(LX+13,Y+18,48,D,Own?RTS::PlayerColor(E->Owner):UIMuted,PC->FactionFor(E->Owner));
 Wrap(NameFor(D,E->Owner),LX+72,Y+19,245,18,UIPaper,2);
 Rect(LX+17,Y+75,299,5,UIInk);Rect(LX+17,Y+75,299*FMath::Clamp(E->HP/FMath::Max(1.f,E->MaxHP),0.f,1.f),5,Own?UITeal:UIBrass);
 Text(FString::Printf(TEXT("%d / %d HP  ·  POP %d"),FMath::CeilToInt(E->HP),FMath::CeilToInt(E->MaxHP),D.Pop),LX+17,Y+88,12,UIMuted);
 const float Attack=E->EffectiveAttack>0?E->EffectiveAttack:D.Attack,Range=E->EffectiveRange>0?E->EffectiveRange:D.Range;
 Text(FString::Printf(TEXT("ATK %.1f   %.2fs/hit   ARM %.0f"),Attack,E->AttackRate,D.Armor),LX+17,Y+106,12,UIPaper);
 Text(FString::Printf(TEXT("RNG %.1fm   AOE %.1fm%s"),Range,E->SplashRadius,E->MinimumRange>0?*FString::Printf(TEXT("   MIN %.1fm"),E->MinimumRange):TEXT("")),LX+17,Y+122,11,UIPaper);
 const FString Modifiers=E->DamageModifiers;
 Text(Modifiers,LX+17,Y+138,10,UIMuted);
 const FString Status=E->Queue.Num()?FString::Printf(TEXT("%s %d%%"),*Name(Cat.Def(E->Queue[0])),FMath::RoundToInt(E->QueueProgress*100)):E->Progress<1?FString::Printf(TEXT("Building %d%%"),FMath::RoundToInt(E->Progress*100)):E->Status.IsEmpty()?TEXT("idle"):D.Id==TEXT("lawyer")&&E->Status==TEXT("convert")?TEXT("bribing"):E->Status.Replace(TEXT("_"),TEXT(" "));
 Text(Truncate(Status,49),LX+17,Y+59,10,UIMuted);
 TArray<FString> Income;
 if(E->ResourceRate.X>0)Income.Add(FString::Printf(TEXT("Food %.2f"),E->ResourceRate.X));
 if(E->ResourceRate.Y>0)Income.Add(FString::Printf(TEXT("Timber %.2f"),E->ResourceRate.Y));
 if(E->ResourceRate.Z>0)Income.Add(FString::Printf(TEXT("PT %.2f"),E->ResourceRate.Z));
 if(E->CementRate>0)Income.Add(FString::Printf(TEXT("Cement %.2f"),E->CementRate));
 if(E->SpiritRate>0)Income.Add(FString::Printf(TEXT("%s %.2f"),*PC->DevotionLabel(),E->SpiritRate));
 const FString IncomeText=Income.IsEmpty()?FString():TEXT("/s: ")+FString::Join(Income,TEXT("  "));
 if(!IncomeText.IsEmpty())Text(Truncate(IncomeText,58),LX+17,Y+154,10,UIGold);
 else if(E->Cargo.SizeSquared()>.01||D.Id==TEXT("gadianton_robber"))Text(FString::Printf(TEXT("Cargo  %.0f Food  %.0f Timber  %.0f PT"),E->Cargo.X,E->Cargo.Y,E->Cargo.Z),LX+17,Y+154,10,UIGold);
 else if(E->Cooldown>0)Text(FString::Printf(TEXT("Ability cooldown %.0fs"),E->Cooldown),LX+17,Y+154,10,UIBrass);
 // Individual portraits preserve mixed selections and support precise changes.
 TArray<const FRTSEntityView*> Selected;for(int32 Id:PC->Selection)if(const auto* Unit=PC->FindEntity(Id))Selected.Add(Unit);
 const int32 PortraitsPerPage=8,MaxSelectionPage=FMath::Max(0,(Selected.Num()-1)/PortraitsPerPage);PC->SelectionPage=FMath::Clamp(PC->SelectionPage,0,MaxSelectionPage);
 for(int32 I=PC->SelectionPage*PortraitsPerPage;I<FMath::Min(Selected.Num(),(PC->SelectionPage+1)*PortraitsPerPage);I++){
  const auto& Unit=*Selected[I];const auto& Type=Cat.Def(Unit.Def);const float X=LX+16+(I%PortraitsPerPage)*33;
  FString Tip=NameFor(Type,Unit.Owner)+FString::Printf(TEXT(" #%d - %.0f/%.0f HP"),Unit.Id,Unit.HP,Unit.MaxHP)+TEXT("\nShift+click: remove this unit. Ctrl+click: select only this unit.");
  if(Unit.Id==E->Id)Tip+=TEXT("\n")+Modifiers+TEXT("\n")+IncomeText;
  Button(X,Y+174,29,29,FString(),TEXT("selection_unit"),FString::FromInt(Unit.Id),true,Tip,Unit.Id==E->Id);Icon(X+3,Y+177,23,Type,RTS::PlayerColor(Unit.Owner),PC->FactionFor(Unit.Owner));
  Rect(X+2,Y+200,25,2,UIInk);Rect(X+2,Y+200,25*FMath::Clamp(Unit.HP/FMath::Max(1.f,Unit.MaxHP),0.f,1.f),2,UITeal);
 }
 if(MaxSelectionPage>0){Button(LX+282,Y+175,19,27,TEXT("<"),TEXT("selection_page"),FString::FromInt(PC->SelectionPage-1),PC->SelectionPage>0,TEXT("Previous selected units"));Button(LX+305,Y+175,19,27,TEXT(">"),TEXT("selection_page"),FString::FromInt(PC->SelectionPage+1),PC->SelectionPage<MaxSelectionPage,TEXT("More selected units"));}
 if(!Own||!P){Wrap(DescriptionFor(D,E->Owner)+TEXT("\n")+CounterText(D),CX,Y+22,AW,16,UIMuted,6);return;}
 struct FOrder {const TCHAR* Label;const TCHAR* Action;const TCHAR* Key;const TCHAR* Tip;};
 const FOrder Orders[]={
 {TEXT("ATTACK"),TEXT("attack_move"),TEXT("A"),TEXT("Attack enemies along the selected route.")},
 {TEXT("STOP"),TEXT("stop"),TEXT("S"),TEXT("Stop the current order and clear queued actions.")},
 {TEXT("HOLD"),TEXT("hold"),TEXT("H"),TEXT("Hold this position.")},
 {TEXT("PATROL"),TEXT("patrol"),TEXT("P"),TEXT("Patrol to the chosen destination.")},
 {TEXT("ABILITY"),TEXT("ability"),TEXT("E"),TEXT("Use this unit's active ability.")},
 {TEXT("CONVERT"),TEXT("convert"),TEXT("C"),TEXT("Convert an eligible enemy military unit.")},
 {TEXT("RAID MOVE"),TEXT("raid_move"),TEXT("V"),TEXT("Raid an economic site along this route, then return the cargo.")},
 {TEXT("GALAVANT"),TEXT("galavant"),TEXT("G"),TEXT("Earn Precious Things while passing laborers. Shift+click adds waypoints.")},
 {TEXT("RAID"),TEXT("raid"),TEXT("R"),TEXT("Plunder an enemy economic site.")},
 {TEXT("RALLY"),TEXT("rally"),TEXT("Y"),TEXT("Set the destination for newly trained military units.")},
 {TEXT("ECON RALLY"),TEXT("rally_economic"),TEXT("Shift+Y"),TEXT("Set the destination for new economic units. Target a resource to gather.")},
 {TEXT("RAID RALLY"),TEXT("storehouse_raid"),TEXT("Y"),TEXT("Robbers depositing here resume raiding this enemy site or location.")},
 {TEXT("SET ROUTE"),TEXT("market_galavant"),TEXT("G"),TEXT("Set the route for new traders. Shift+click adds waypoints; Esc finishes.")},
 {TEXT("DELIVER"),TEXT("deliver"),TEXT(""),TEXT("Return carried resources to a drop-off building.")},
 {TEXT("CANCEL"),TEXT("cancel"),TEXT(""),TEXT("Cancel the first queued item or unfinished construction.")}};
 int32 OrderColumn=0;
 for(const auto& Order:Orders)if(PC->SupportsOrder(Order.Action)) {
  const float X=CX+OrderColumn++*52;const FString Action=Order.Action;
  FString Tip=Order.Tip;
  if(Action==TEXT("ability"))Tip=Description(D);
  if(Action==TEXT("convert")){
   const FString Ability=D.Id==TEXT("lawyer")?TEXT("Bribe"):D.Id==TEXT("drunken_concubine")?TEXT("Seduce"):P->Faction==TEXT("lamanites")?TEXT("Stiffen the Neck"):P->Faction==TEXT("nephites")?TEXT("Soften the Heart"):TEXT("Convert");
   Tip=Ability+TEXT(": gain an eligible enemy in 5 seconds. Converted units are immune to further conversion.");
   if(D.Id==TEXT("drunken_concubine"))Tip=TEXT("Seduce: turn an eligible enemy into a Henpecked Gentile.");
  }
  if(Action==TEXT("ability")&&D.Kind==TEXT("ammonite"))Tip=TEXT("Prostrate: for 5 seconds, its killer becomes an Ammonite. Cooldown: 12 seconds.");
  Button(X,Y+8,45,45,FString(),TEXT("order"),Order.Action,true,Tip);
  FRTSVisualAssets::Get().TryDrawIconId(Canvas,TEXT("actions/")+(Action==TEXT("storehouse_raid")?FString(TEXT("raid")):Action),FVector2D((X+5)*U,(Y+13)*U),18*U,FLinearColor::White,PC->FactionFor(PC->LocalSlot));
  Text(Order.Key,X+38,Y+10,FString(Order.Key).Len()>3?7:10,UIGold,true);
  const FString Label=Action==TEXT("convert")&&D.Id==TEXT("lawyer")?TEXT("BRIBE"):Action==TEXT("convert")&&P->Faction==TEXT("lamanites")?TEXT("STIFFEN NECK"):Action==TEXT("convert")&&P->Faction==TEXT("nephites")?TEXT("SOFTEN HEART"):Action==TEXT("ability")&&D.Kind==TEXT("ammonite")?TEXT("PROSTRATE"):Action==TEXT("rally")&&D.Id==TEXT("seat_of_the_people")?TEXT("MIL RALLY"):Action==TEXT("convert")&&D.Id==TEXT("drunken_concubine")?TEXT("SEDUCE"):Action==TEXT("ability")&&RTS::IsCarrier(D)?TEXT("UNLOAD"):FString(Order.Label);
  Text(Label,X+22,Y+31,Label.Len()>9?7:Label.Len()>6?8:9,UIPaper,true);
 }
 const bool Builder=PC->HasSelectedBuilder();
 if(!D.bBuilding&&Builder){const float X=CX+OrderColumn++*52;Button(X,Y+8,45,45,FString(),TEXT("build_toggle"),FString(),true,FRTSControls::Get().LabelFor(TEXT("menu.build"))+TEXT(": open construction; Esc closes"),PC->bBuildMenu);Text(FRTSControls::Get().LabelFor(TEXT("menu.build")),X+38,Y+10,10,UIGold,true);FRTSVisualAssets::Get().TryDrawIconId(Canvas,TEXT("actions/build_toggle"),FVector2D((X+5)*U,(Y+13)*U),18*U,FLinearColor::White,PC->FactionFor(PC->LocalSlot));Text(TEXT("BUILD"),X+22,Y+31,9,UIPaper,true);}
 struct FOption {const FRTSDef* Def;FString Action;};TArray<FOption> Options;
 const auto& F=Cat.Faction(P->Faction);
 if(PC->bBuildMenu&&Builder) {
  for(const FString& Id:F.Buildings)if(PC->CanConstruct(Cat.Def(Id)))Options.Add({&Cat.Def(Id),TEXT("build")});
 } else if(D.bBuilding) {
  for(const FString& Id:F.Units) {
   const auto& UD=Cat.Def(Id);const bool Emergency=Id==TEXT("curelom")&&D.Id==TEXT("seat_of_the_people")&&RTS::IsJaredite(P->Faction);
   const bool FlockCurelom=(Id==TEXT("curelom")||Id==TEXT("cumelom")||Id==TEXT("elephant"))&&D.Id==TEXT("gathering_of_flocks")&&RTS::IsJaredite(P->Faction);
   if(UD.Producer!=D.Id&&!Emergency&&!FlockCurelom)continue;
   if(UD.Kind==TEXT("job")||UD.Kind==TEXT("ammonite"))continue;
   if(UD.bHero){if(!P->HeroChoices.Contains(Id))continue;bool Exists=false;for(const auto& Unit:PC->Entities)if(Unit.Owner==PC->LocalSlot&&(Cat.Def(Unit.Def).Id==Id||Unit.Queue.Contains(Id)))Exists=true;if(Exists)continue;}
   Options.Add({&UD,TEXT("train")});
  }
  const bool Wall=D.Id.Contains(TEXT("wall"))||D.Id==TEXT("works_of_defense")||D.Id.Contains(TEXT("bulwark"));if(Wall&&!E->bGate&&F.Upgrades.Contains(TEXT("gate_segment_upgrade")))Options.Add({&Cat.Def(TEXT("gate_segment_upgrade")),TEXT("research")});
  for(const FString& Id:F.Upgrades){const auto& UD=Cat.Def(Id);if(Id==TEXT("gate_segment_upgrade")||UD.Kind==TEXT("age_option")||UD.Producer!=D.Id||P->Upgrades.Contains(Id))continue;const bool LamaniteTemple= P->Faction==TEXT("lamanites")&&D.Id==TEXT("temple_of_the_land")&&(Id==TEXT("order_of_the_nehors")||Id==TEXT("by_the_pride_of_their_hearts"));bool Ready=LamaniteTemple; if(!LamaniteTemple)Ready=true;for(const FString& Req:UD.Requires)if(!LamaniteTemple&&Cat.Def(Req).bUpgrade&&!P->Upgrades.Contains(Req))Ready=false;if(Ready)Options.Add({&UD,TEXT("research")});}
  if(D.Id==TEXT("seat_of_the_people")&&P->Age<4)for(const auto& Option:Cat.Defs)if(Option.Kind==TEXT("age_option")&&Option.Age==P->Age) {
   const auto& Hero=Cat.Def(Option.HeroChoice);if(F.Upgrades.Contains(Option.Id)&&F.Units.Contains(Hero.Id))Options.Add({&Option,TEXT("age")});
  }
 }
 const float Side=74,Gap=7,GridY=Y+61;const int32 Cols=FMath::Max(3,FMath::FloorToInt(AW/(Side+Gap))),PerPage=Cols*((D.bBuilding&&E->Queue.Num()>0)?1:2);
 const int32 MaxPage=Options.IsEmpty()?0:(Options.Num()-1)/PerPage;PC->ActionPage=FMath::Clamp(PC->ActionPage,0,MaxPage);
 const FKey ProductionKeys[]={EKeys::Q,EKeys::W,EKeys::E,EKeys::R,EKeys::T,EKeys::U,EKeys::I,EKeys::O,EKeys::P,EKeys::A,EKeys::S,EKeys::D,EKeys::F,EKeys::G,EKeys::H,EKeys::J,EKeys::K,EKeys::L,EKeys::Z,EKeys::X,EKeys::C,EKeys::V,EKeys::B,EKeys::N,EKeys::M};
 TSet<FKey> UsedCardKeys;
 for(int32 I=PC->ActionPage*PerPage;I<FMath::Min(Options.Num(),(PC->ActionPage+1)*PerPage);I++) {
  const int32 J=I-PC->ActionPage*PerPage;const float X=CX+(J%Cols)*(Side+Gap),BY=GridY+(J/Cols)*(Side+Gap);const auto& O=*Options[I].Def;const FString Action=Options[I].Action;
  FString Reason;bool Enabled=PC->CanUse(O,Reason);
  if(D.bBuilding&&(E->Progress<1||E->Queue.Num()>=8)){Enabled=false;Reason=E->Progress<1?TEXT("Finish construction first"):TEXT("Production queue full");}

  FString Badge;
  if(Action==TEXT("build"))Badge=FRTSControls::Get().LabelFor(TEXT("build.")+O.Id);
  else if(D.bBuilding&&J<UE_ARRAY_COUNT(ProductionKeys)) {
   FKey Key=O.Hotkey.IsEmpty()?ProductionKeys[J]:FKey(FName(*O.Hotkey));if(!Key.IsValid())Key=ProductionKeys[J];
   // Catalog overrides and visible cards agree; avoid assigning two visible cards to one key.
   if(Key==EKeys::Y||UsedCardKeys.Contains(Key)){for(FKey Candidate:ProductionKeys)if(!UsedCardKeys.Contains(Candidate)){Key=Candidate;break;}}
   UsedCardKeys.Add(Key);
   Badge=Key.GetDisplayName().ToString();if(Enabled)PC->CardHotkeys.Add(Key,TPair<FString,FString>(Action,O.Id));
  }
  const int32 RequiredAge=Cat.RequiredAge(O,P->Faction);const bool AgeLocked=P->Age<RequiredAge&&(O.bBuilding||(!O.bUpgrade&&O.Kind!=TEXT("age_option")));
  FString Tip=Name(O)+TEXT("\n")+Description(O)+TEXT("\n")+CostText(O)+FString::Printf(TEXT("  ·  %.0fs"),Cat.Duration(O,P->Faction));
  if(!O.bBuilding&&!O.bUpgrade)Tip+=FString::Printf(TEXT("  ·  %d population"),O.Pop);
  if(!Enabled)Tip+=TEXT("\n")+Reason;
  Button(X,BY,Side,Side,FString(),Action,O.Id,Enabled,Tip);Icon(X+5,BY+5,42,O,Enabled?UIPaper:UIMuted);
  if(AgeLocked){Rect(X+5,BY+5,42,42,FLinearColor(.025f,.042f,.047f,.76f));Text(AgeNumeral(RequiredAge),X+26,BY+10,26,UIGold,true);}
  Wrap(Name(O),X+5,BY+50,Side-10,10,Enabled?UIPaper:UIMuted,2);
  if(!Badge.IsEmpty()){Rect(X+48,BY+4,22,18,UIInk);Text(Truncate(Badge,5),X+59,BY+7,9,UIGold,true);}
 }
 if(D.bBuilding&&E->Queue.Num())DrawSelectedQueue(*E,CX,Y+140,AW);
 if(RTS::IsCarrier(D)&&(!D.bBuilding||(D.Id==TEXT("tents_and_habitations")&&P->Faction==TEXT("gadianton")))&&!PC->bBuildMenu)DrawGarrison(*E,CX,Y+66,AW);
 else if(Options.IsEmpty())Wrap(Description(D)+TEXT("\n")+CounterText(D),CX,Y+74,AW-30,15,UIMuted,5);
 if(MaxPage>0){const float X=CX+AW-124;Button(X,Y+11,56,30,TEXT("<"),TEXT("page"),FString::FromInt(PC->ActionPage-1),PC->ActionPage>0);Button(X+64,Y+11,56,30,TEXT(">"),TEXT("page"),FString::FromInt(PC->ActionPage+1),PC->ActionPage<MaxPage);}
 if(D.Id==TEXT("temple_of_the_land")&&P->Faction==TEXT("lamanites"))DrawGarrison(*E,CX+AW-(MaxPage>0?314:182),Y+9,182);
 if(PC->DeleteHeldSince>=0){const float Progress=FMath::Clamp((GetWorld()->GetRealTimeSeconds()-PC->DeleteHeldSince)/2.f,0.f,1.f);Panel(W*.5f-180,H-354,360,55,true);Text(TEXT("HOLD DELETE TO REMOVE OWN SELECTION"),W*.5f,H-342,12,UIGold,true);Rect(W*.5f-165,H-317,330,5,UIInk);Rect(W*.5f-165,H-317,330*Progress,5,FLinearColor(.94f,.35f,.26f,1));}
}
void ARTSHUD::DrawProductionQueue() {
 if(PC->bMenu||PC->Phase!=1)return;
 const auto& Cat=FRTSCatalog::Get();int32 Count=0;for(const auto& E:PC->Entities)if(E.Owner==PC->LocalSlot&&!E.bRemembered)Count+=E.Queue.Num();
 if(!Count)return;const int32 Visible=FMath::Min(Count,12);const float Side=46,Gap=5,X=W-20-Visible*(Side+Gap),Y=H-285;
 Text(TEXT("PRODUCTION  ·  RIGHT-CLICK TO CANCEL"),X,Y-19,10,UIBrass,false);
 int32 Index=0;
 for(const auto& E:PC->Entities)if(E.Owner==PC->LocalSlot&&!E.bRemembered)for(int32 I=0;I<E.Queue.Num()&&Index<12;I++,Index++) {
  const auto& D=Cat.Def(E.Queue[I]);const float BX=X+Index*(Side+Gap);
  Button(BX,Y,Side,Side,FString(),TEXT("queue_info"),FString(),true,Name(D)+FString::Printf(TEXT("\nPopulation cost: %d"),D.Pop)+TEXT("\nRight-click to cancel this item.\nQueued units and research refund fully."));Icon(BX+4,Y+4,Side-8,D,UIPaper);
  if(I==0){Rect(BX,Y+Side-4,Side,4,UIInk);Rect(BX,Y+Side-4,Side*E.QueueProgress,4,UITeal);}
  FQueueHit Hit;Hit.Bounds=FBox2D(FVector2D(BX*U,Y*U),FVector2D((BX+Side)*U,(Y+Side)*U));Hit.Building=E.Id;Hit.Index=I;QueueHits.Add(Hit);
 }
}
bool ARTSHUD::RightClick(FVector2D Pixel) {
 if(!PC||PC->bPauseMenu||PC->bHelp||PC->bHotkeySettings)return false;
 for(const auto& Hit:QueueHits)if(Hit.Bounds.IsInsideOrOn(Pixel)){PC->ServerOrder(TEXT("cancel"),TArray<int32>{Hit.Building},FVector2D::ZeroVector,0,FString::Printf(TEXT("queue:%d"),Hit.Index));PC->PlayCue(TEXT("ui"));return true;}
 return false;
}

void ARTSHUD::DrawGarrison(const FRTSEntityView& E,float X,float Y,float Width) {
 const auto& Type=FRTSCatalog::Get().Def(E.Def);
 if(Type.Id==TEXT("temple_of_the_land")&&PC->FactionFor(E.Owner)==TEXT("lamanites")){
  const FRTSEntityView* Occupant=nullptr;
  for(const auto& Unit:PC->Entities)if(Unit.Owner==E.Owner&&!Unit.bRemembered&&Unit.HP>0&&Unit.Status==TEXT("occupy")&&Unit.Target==E.Id&&FRTSCatalog::Get().Def(Unit.Def).Id==TEXT("priestcrafter")&&FVector2D::Distance(Unit.Pos,E.Pos)<=Type.Radius+.65f){Occupant=&Unit;break;}
  Text(FString::Printf(TEXT("PRIEST SLOT  %d / 1"),Occupant?1:0),X,Y+5,11,UIBrass);
  const FString Tip=Occupant?TEXT("Priestcrafter occupying: +15% allied military attack speed within 8m."):TEXT("Right-click with a Priestcrafter to occupy this Rameumptom and strengthen its attack-speed aura.");
  Button(X+139,Y,36,36,FString(),TEXT("garrison_info"),Occupant?FString::FromInt(Occupant->Id):FString(),true,Tip);
  if(Occupant)Icon(X+142,Y+3,30,FRTSCatalog::Get().Def(Occupant->Def),UIPaper,PC->FactionFor(E.Owner));else Text(TEXT("+"),X+157,Y+7,19,UIMuted,true);
  return;
 }
 const auto& Cat=FRTSCatalog::Get();const auto& D=Cat.Def(E.Def);int32 Used=0;for(int32 Def:E.PassengerDefs)Used+=FMath::Max(1,Cat.Def(Def).Pop);
 Text(FString::Printf(TEXT("GARRISON  %d / %d population  -  E: unload"),Used,RTS::CarrierCapacity(D)),X,Y,13,UIBrass);
 if(E.PassengerDefs.IsEmpty()){Wrap(D.bBuilding?TEXT("Right-click this hideout with land units to garrison. E unloads nearby; capacity is five population."):TEXT("Right-click this carrier with land units to board. Press E or click Unload to disembark nearby."),X,Y+35,Width-30,14,UIMuted,3);return;}
 const int32 Cols=FMath::Max(1,FMath::FloorToInt(Width/58.f));
 for(int32 I=0;I<E.PassengerDefs.Num()&&I<Cols*2;I++) {
  const auto& Passenger=Cat.Def(E.PassengerDefs[I]);const float BX=X+(I%Cols)*58,BY=Y+26+(I/Cols)*58;
  const int32 Id=E.PassengerIds.IsValidIndex(I)?E.PassengerIds[I]:0;
  Button(BX,BY,52,52,FString(),TEXT("garrison_info"),FString::FromInt(Id),true,NameFor(Passenger,E.Owner)+FString::Printf(TEXT("\nPopulation: %d\nSafely aboard. E unloads nearby."),Passenger.Pop));
  Icon(BX+4,BY+4,44,Passenger,UIPaper,PC->FactionFor(E.Owner));
 }
}

void ARTSHUD::DrawSelectedQueue(const FRTSEntityView& E,float X,float Y,float Width) {
 const auto& Cat=FRTSCatalog::Get();const float Side=70,Gap=6;const int32 Visible=FMath::Min(8,E.Queue.Num());
 for(int32 I=0;I<Visible;I++) {
  const auto& D=Cat.Def(E.Queue[I]);const float BX=X+I*(Side+Gap);
  const FString Value=FString::Printf(TEXT("%d|%d"),E.Id,I);
  Button(BX,Y,Side,Side,FString(),TEXT("selected_queue_info"),Value,true,Name(D)+TEXT("\nRight-click to cancel for a full refund."));
  Icon(BX+5,Y+5,25,D,UIPaper);Text(FString::Printf(TEXT("%d"),I+1),BX+37,Y+8,11,UIBrass);
  Wrap(Truncate(Name(D),27),BX+5,Y+35,Side-10,10,UIPaper,2);
  Rect(BX,Y+Side-4,Side,4,UIInk);if(I==0)Rect(BX,Y+Side-4,Side*E.QueueProgress,4,UITeal);
  Button(BX+Side-18,Y+3,15,17,TEXT("x"),TEXT("cancel_queue"),Value,true,TEXT("Cancel this queued item. Full resource refund."));
  FQueueHit Hit;Hit.Bounds=FBox2D(FVector2D(BX*U,Y*U),FVector2D((BX+Side)*U,(Y+Side)*U));Hit.Building=E.Id;Hit.Index=I;QueueHits.Add(Hit);
 }
 const float TX=X+Visible*(Side+Gap)+8;if(TX+150<X+Width){Text(TEXT("SELECTED QUEUE"),TX,Y+7,11,UIBrass);Text(FString::Printf(TEXT("%d items  -  %d%%"),E.Queue.Num(),FMath::RoundToInt(E.QueueProgress*100)),TX,Y+29,11,UIMuted);Text(TEXT("Right-click or x cancels"),TX,Y+49,10,UIMuted);}
}
