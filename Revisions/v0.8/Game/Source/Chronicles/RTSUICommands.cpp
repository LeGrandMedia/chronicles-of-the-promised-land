#include "RTSController.h"
#include "RTSNames.h"

FVector2D ARTSController::RotateMap(FVector2D Delta,bool Inverse) const {
 const double Angle=Inverse?-CameraRotation:CameraRotation,C=FMath::Cos(Angle),S=FMath::Sin(Angle);
 return FVector2D(Delta.X*C-Delta.Y*S,Delta.X*S+Delta.Y*C);
}
bool ARTSController::SupportsOrder(const FString& Action) const {
 const auto* P=Me();if(!P)return false;
 for(int32 Id:Selection)if(const auto* E=FindEntity(Id))if(E->Owner==LocalSlot&&!E->bRemembered&&E->HP>0) {
  const auto& D=FRTSCatalog::Get().Def(E->Def);if(!ActiveSubgroup.IsEmpty()&&ActiveSubgroup!=D.Id)continue;
  if(Action==TEXT("delete"))return true;
  if(D.bBuilding){if(Action==TEXT("storehouse_raid")&&D.Id==TEXT("storehouse_of_provisions")&&P->Faction==TEXT("gadianton")&&E->Progress>=1)return true;if(Action==TEXT("ability")&&D.Id==TEXT("tents_and_habitations")&&P->Faction==TEXT("gadianton")&&E->Progress>=1)return true;if(Action==TEXT("market_galavant")&&D.Id==TEXT("place_of_trade"))return true;if(Action==TEXT("rally_economic")&&D.Id==TEXT("seat_of_the_people"))return true;if(Action==TEXT("rally")&&FRTSCatalog::Get().IsUnitProducer(D))return true;if(Action==TEXT("cancel")&&(E->Queue.Num()||E->Progress<1))return true;continue;}
  if(Action==TEXT("stop")||Action==TEXT("hold")||Action==TEXT("move")||Action==TEXT("patrol"))return true;
  if(Action==TEXT("attack_move")||Action==TEXT("attack")){if(D.Attack>0)return true;}
  if(Action==TEXT("deliver")&&E->Cargo.SizeSquared()>.01)return true;
  if(Action==TEXT("convert")&&RTS::IsReligious(D))return true;
  if(Action==TEXT("raid_move")&&(D.Id==TEXT("gadianton_robber")||D.Id==TEXT("secret_murderer")||D.Id==TEXT("corrupt_judge")))return true;
  if(Action==TEXT("galavant")&&D.Kind==TEXT("trader"))return true;
  if(Action==TEXT("raid")&&(D.Id==TEXT("gadianton_robber")||D.Id==TEXT("secret_murderer")||D.Id==TEXT("corrupt_judge")))return true;
  if(Action==TEXT("ability")&&!D.Ability.IsEmpty()&&(D.Id==TEXT("amalickiah")||D.Id==TEXT("ammon")||D.Id==TEXT("captain_moroni")||D.Id==TEXT("gadianton")||D.Id==TEXT("giddianhi")||D.Id==TEXT("helaman")||D.Id==TEXT("kishkumen")||D.Id==TEXT("lamoni")||D.Id==TEXT("samuel_the_lamanite")||D.Id==TEXT("teancum")))return true;
  if(Action==TEXT("ability")&&D.Id==TEXT("chief_captain")&&P->Faction==TEXT("lamanites")&&P->Upgrades.Contains(TEXT("fight_like_dragons")))return true;
  if(Action==TEXT("ability")&&(RTS::IsCarrier(D)||(D.Kind==TEXT("ammonite")&&P->Upgrades.Contains(TEXT("mighty_change_of_heart")))))return true;
 }
 return false;
}
TArray<int32> ARTSController::IdleMilitary() const {
 TArray<int32> Result;if(Phase!=1||!Me()||Me()->bDefeated)return Result;
 for(const auto& E:Entities){const auto& D=FRTSCatalog::Get().Def(E.Def);
  if(!E.bRemembered&&E.Owner==LocalSlot&&E.HP>0&&!D.bBuilding&&D.Attack>0&&!RTS::IsWorker(D)&&D.Kind!=TEXT("beast")&&!E.bAttacking&&(E.Status.IsEmpty()||E.Status==TEXT("idle")||E.Status==TEXT("hold")))Result.Add(E.Id);
 }Result.Sort();return Result;
}
void ARTSController::SelectIdleMilitary(bool All) {
 const auto Idle=IdleMilitary();if(Idle.IsEmpty()){ShowNotice(TEXT("All soldiers are busy."));return;}
 PendingAction.Empty();PendingType.Empty();ActiveSubgroup.Empty();bBuildMenu=false;ActionPage=0;bDragging=false;
 if(All)Selection=Idle;
 else {int32 Next=Idle[0];for(int32 Id:Idle)if(Selection.Num()==1&&Id>Selection[0]){Next=Id;break;}Selection={Next};if(const auto* E=FindEntity(Next))CameraCenter=E->Pos;}
 PlayCue(TEXT("select"));
}
FVector ARTSController::Workforce() const {
 return FVector(ResourceWorkers(0).Num(),ResourceWorkers(1).Num(),ResourceWorkers(2).Num());
}
TArray<int32> ARTSController::ResourceWorkers(int32 Resource) const {
 TArray<int32> Result;
 for(const auto& E:Entities)if(E.Owner==LocalSlot&&!E.bRemembered&&E.HP>0) {
  const auto& D=FRTSCatalog::Get().Def(E.Def);int32 Assigned=-1;
  if(D.Kind==TEXT("beast")){Assigned=D.Id==TEXT("curelom")?1:D.Id==TEXT("cumelom")?2:D.Id==TEXT("elephant")?3:-1;}
  else if(D.Kind==TEXT("trader")&&(E.Status.Contains(TEXT("galavant"))||E.Status.Contains(TEXT("trade"))||E.Status.Contains(TEXT("show"))))Assigned=2;
  else if(RTS::IsWorker(D)&&!E.Status.Contains(TEXT("build"))&&!E.Status.Contains(TEXT("repair"))&&!E.Status.IsEmpty()&&E.Status!=TEXT("idle")&&E.Status!=TEXT("hold")) {
   const auto* T=FindEntity(E.Target);const FString Site=T?FRTSCatalog::Get().Def(T->Def).Id:FString();
   if(Site==TEXT("tree")||E.Status.Contains(TEXT("timber")))Assigned=1;
   else if(Site==TEXT("ore")||Site==TEXT("hill_of_ore")||D.Id==TEXT("worker_in_ore")||E.Status.Contains(TEXT("ore")))Assigned=2;
   else if(Site==TEXT("berries")||Site==TEXT("fields_of_grain")||Site==TEXT("gathering_of_flocks")||Site==TEXT("vineyard")||D.Kind==TEXT("job")||E.Status.Contains(TEXT("gather")))Assigned=0;
  }
  if(Assigned==Resource)Result.Add(E.Id);
 }Result.Sort();return Result;
}
void ARTSController::SelectResourceWorker(int32 Resource) {
 const auto Workers=ResourceWorkers(Resource);if(Workers.IsEmpty()){ShowNotice(TEXT("No workers are assigned to that resource."));return;}
 const int32 Previous=LastResourceWorker.FindRef(Resource);int32 Next=Workers[0];for(int32 Id:Workers)if(Id>Previous){Next=Id;break;}
 LastResourceWorker.Add(Resource,Next);Selection={Next};ActiveSubgroup.Empty();PendingAction.Empty();PendingType.Empty();bWallAnchor=false;bBuildMenu=false;ActionPage=0;
 if(const auto* E=FindEntity(Next))CameraCenter=E->Pos;PlayCue(TEXT("select"));
}
bool ARTSController::CanConstruct(const FRTSDef& Building) const {
 const auto* P=Me();if(!P||!Building.bBuilding||Building.Id==TEXT("judgment_seat"))return false;if(P->Faction==TEXT("gadianton")&&(Building.Id==TEXT("fields_of_grain")||Building.Id==TEXT("gathering_of_flocks")))return false;
 for(int32 Id:Selection)if(const auto* E=FindEntity(Id))if(E->Owner==LocalSlot&&!E->bRemembered) {
  const auto& D=FRTSCatalog::Get().Def(E->Def);if(!ActiveSubgroup.IsEmpty()&&D.Id!=ActiveSubgroup)continue;
  if(RTS::IsJaredite(P->Faction)) {
   if(D.Id==TEXT("elephant"))return true;
   if(Building.CementCost>0||Building.Id.StartsWith(TEXT("cement_")))continue;
   if(D.Id==TEXT("curelom")&&FRTSCatalog::Get().RequiredAge(Building,P->Faction)<=2)return true;
   if(D.Id==TEXT("cumelom")&&FRTSCatalog::Get().RequiredAge(Building,P->Faction)<=3)return true;
  } else if(D.Kind==TEXT("worker")||D.Kind==TEXT("job"))return true;
  else if(D.Kind==TEXT("priest")&&P->Faction==TEXT("lamanites")&&Building.Id==TEXT("temple_of_the_land")&&P->Upgrades.Contains(TEXT("by_the_pride_of_their_hearts")))return true;
 }
 return false;
}
FString ARTSController::FactionFor(int32 EntityOwner) const {
 for(const auto& P:Players)if(P.Slot==EntityOwner)return P.Faction;
 return Me()?Me()->Faction:FString();
}
FString ARTSController::DevotionLabel() const {
 const FString F=Me()?Me()->Faction:FString();
 const FString Fallback=F==TEXT("lamanites")?TEXT("Anger"):F==TEXT("laban")?TEXT("Drunkenness"):F==TEXT("gadianton")?TEXT("Lasciviousness"):RTS::IsJaredite(F)?TEXT("Glowing Stones"):TEXT("Favor");
 return RTSNames::Get(TEXT("devotion.")+F,Fallback);
}
float ARTSController::VisualOpacity(const FRTSEntityView& E) const {
 if(!E.bStealthed)return 1.f;const auto* P=Me();if(E.Owner==LocalSlot)return .62f;
 for(const auto& Other:Players)if(P&&Other.Slot==E.Owner&&Other.Team==P->Team)return .62f;
 return 1.f;
}
