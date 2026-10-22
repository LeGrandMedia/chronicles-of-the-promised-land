#include "RTSController.h"

TArray<int32> ARTSController::IdleVillagers() const {
 TArray<int32> Result;if(Phase!=1||!Me()||Me()->bDefeated)return Result;
 for(const auto& E:Entities){const auto& D=FRTSCatalog::Get().Def(E.Def);
  const bool Villager=D.Kind==TEXT("worker")||D.Kind==TEXT("job")||D.Kind==TEXT("trader");
  if(!E.bRemembered&&E.Owner==LocalSlot&&E.HP>0&&Villager&&!E.bAttacking&&(E.Status.IsEmpty()||E.Status==TEXT("idle")||E.Status==TEXT("hold")))Result.Add(E.Id);
 }
 Result.Sort();return Result;
}
void ARTSController::SelectIdleVillagers(bool All) {
 auto Idle=IdleVillagers();if(Idle.IsEmpty()){ShowNotice(TEXT("All villagers are busy."));return;}
 PendingAction.Empty();PendingType.Empty();ActiveSubgroup.Empty();bBuildMenu=false;ActionPage=SelectionPage=0;bDragging=false;
 if(All){Selection=Idle;ShowNotice(FString::Printf(TEXT("Selected all %d idle villagers."),Idle.Num()));}
 else {int32 Next=Idle[0];for(int32 Id:Idle)if(Id>LastIdleId){Next=Id;break;}LastIdleId=Next;Selection={Next};if(const auto* E=FindEntity(Next))CameraCenter=E->Pos;}
 PlayCue(TEXT("select"));
}
bool ARTSController::HasSelectedBuilder() const {
 const auto* P=Me();if(!P)return false;
 for(int32 Id:Selection)if(const auto* E=FindEntity(Id))if(E->Owner==LocalSlot){const auto& D=FRTSCatalog::Get().Def(E->Def);if(!ActiveSubgroup.IsEmpty()&&ActiveSubgroup!=D.Id)continue;if(RTS::IsJaredite(P->Faction)?D.Kind==TEXT("beast"):D.Kind==TEXT("worker")||D.Kind==TEXT("job")||(D.Kind==TEXT("priest")&&P->Faction==TEXT("lamanites")&&P->Upgrades.Contains(TEXT("by_the_pride_of_their_hearts"))))return true;}
 return false;
}
