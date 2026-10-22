#include "RTSWorld.h"
#include "RTSController.h"

void ARTSWorld::Step(float DT) {
 Time+=DT;TArray<int32> Ids;Entities.GetKeys(Ids);Ids.Sort();
 for(int Id:Ids){auto E=Find(Id);if(!E)continue;if(E->Expiry>0&&Time>=E->Expiry){Entities.Remove(Id);continue;}if(E->DeathAt>0&&Time>=E->DeathAt){Kill(Id);continue;}E->bAttacking=false;E->AttackTimer=FMath::Max(0.f,E->AttackTimer-DT);E->Cooldown=FMath::Max(0.f,E->Cooldown-DT);if(E->Owner<0||E->Carrier)continue;
  if(D(*E).bBuilding){Production(*E,DT);Combat(*E,DT);continue;}
  if(E->Order==TEXT("move")||E->Order==TEXT("attack_move")||E->Order==TEXT("patrol")){
   if(E->Order==TEXT("move")||!Find(E->Target)){if(Move(*E,E->Rally,.35f,DT)){if(E->Order==TEXT("patrol")){auto Old=E->Rally;E->Rally=E->PatrolStart;E->PatrolStart=Old;E->Path.Empty();}else ResetOrder(*E,TEXT("idle"));}}
  }
  if(E->Order==TEXT("build")||E->Order==TEXT("repair")){auto T=Find(E->Target);if(!T||!CanBuildSite(*E,*T)){ResetOrder(*E,TEXT("idle"));continue;}if(Move(*E,WorkPosition(*E,*T),.3f,DT)&&E->Order==TEXT("repair")){float Rate=(BeastTier(*E)==3?2:BeastTier(*E)==2?1.5f:1)*LaborerAuraMultiplier(*E)*(1+EconomicBonus(E->Owner,TEXT("build_speed")));float Heal=FMath::Min(10*Rate*DT,T->MaxHP-T->HP);Heal=FMath::Min(Heal,float(Players[E->Owner].Resources.Y*10));Players[E->Owner].Resources.Y-=Heal*.1;T->HP+=Heal;if(T->HP>=T->MaxHP-.001f)ResetOrder(*E,TEXT("idle"));}}
  if(E->Order==TEXT("board")){
   auto T=Find(E->Target);
   const bool FootUnit=!D(*E).bBuilding&&!RTS::IsCarrier(D(*E))&&D(*E).Kind!=TEXT("beast")&&D(*E).Kind!=TEXT("sheep");
   const bool InvalidHideout=T&&D(*T).bBuilding&&(D(*T).Id!=TEXT("tents_and_habitations")||!Players.IsValidIndex(T->Owner)||Players[T->Owner].Faction!=TEXT("gadianton")||!FootUnit);
   if(!T||T->Owner!=E->Owner||T->Progress<1||!RTS::IsCarrier(D(*T))||RTS::IsCarrier(D(*E))||D(*E).Kind==TEXT("beast")||D(*E).Kind==TEXT("sheep")||InvalidHideout){ResetOrder(*E,TEXT("idle"));continue;}
   int Pop=0;for(int P:T->Passengers)if(auto Passenger=Find(P))Pop+=FMath::Max(1,D(*Passenger).Pop);
   if(Pop+FMath::Max(1,D(*E).Pop)>RTS::CarrierCapacity(D(*T))){ResetOrder(*E,TEXT("idle"));continue;}
   if(Move(*E,T->Pos,D(*T).Kind==TEXT("ship")?5.f:D(*T).Radius+D(*E).Radius+.3f,DT)){E->Carrier=T->Id;E->Pos=T->Pos;T->Passengers.AddUnique(E->Id);ResetOrder(*E,TEXT("idle"));}
  }
  if(E->Order==TEXT("occupy")){auto T=Find(E->Target);if(!T||T->Owner!=E->Owner||D(*E).Id!=TEXT("priestcrafter")||Players[E->Owner].Faction!=TEXT("lamanites")||D(*T).Id!=TEXT("temple_of_the_land")||T->Progress<1){ResetOrder(*E,TEXT("idle"));continue;}bool Taken=false;for(auto& KV:Entities)if(KV.Key<E->Id&&KV.Value->Order==TEXT("occupy")&&KV.Value->Target==T->Id)Taken=true;if(Taken){ResetOrder(*E,TEXT("idle"));continue;}Move(*E,T->Pos,D(*T).Radius+.4f,DT);}
  if(E->Order==TEXT("injunction"))E->Order=TEXT("raid");
  Economy(*E,DT);if(!Find(Id))continue;Conversion(*E,DT);if(!Find(Id))continue;Combat(*E,DT);if(auto Living=Find(Id))ProcessQueuedOrder(*Living);
 }
 EconomyAuras(DT);Religion(DT);
 // Every healer picks one target; only the strongest rate applies to that target.
 TMap<int32,float> Heals;for(auto& KV:Entities){auto& H=*KV.Value;if(H.Owner<0||H.Carrier||H.Order==TEXT("gather")||H.Order==TEXT("deliver")||H.Order==TEXT("convert")||!H.Evolution.IsEmpty())continue;auto Kind=D(H).Kind;float Rate=Kind==TEXT("ammonite")?12:D(H).Id==TEXT("teacher_of_the_word")&&Players[H.Owner].Faction!=TEXT("nephites")?4:0;if(Rate==0)continue;FRTSEntity* Target=nullptr;float Lowest=1;for(auto& TV:Entities){auto& T=*TV.Value;if(!Allied(H.Owner,T.Owner)||D(T).bBuilding||T.Carrier||T.DeathAt>0||T.HP>=T.MaxHP||FVector2D::Distance(H.Pos,T.Pos)>5||!LOS(H.Pos,T.Pos,H.Id,T.Id))continue;if(T.HP/T.MaxHP<Lowest){Target=&T;Lowest=T.HP/T.MaxHP;}}if(Target)Heals.FindOrAdd(Target->Id)=FMath::Max(Heals.FindRef(Target->Id),Rate);}
 for(auto& H:Heals)if(auto T=Find(H.Key))T->HP=FMath::Min(T->MaxHP,T->HP+H.Value*DT);
 for(auto& P:Players){if(P.bDefeated)continue;if(!P.bConnected&&Time-P.DisconnectedAt>60){P.bDefeated=true;TArray<int32> Dead;for(auto& KV:Entities)if(KV.Value->Owner==P.Slot)Dead.Add(KV.Key);for(int Id:Dead)Kill(Id);continue;}
  bool HQ=false,Foundation=false;for(auto& KV:Entities){auto& E=*KV.Value;if(E.Owner==P.Slot&&D(E).Id==TEXT("seat_of_the_people")){if(E.Progress>=1)HQ=true;else if(Time-E.LastProgress<=15)Foundation=true;}}
  if(HQ)P.DefeatRemaining=60;else if(!Foundation)P.DefeatRemaining-=DT;if(P.DefeatRemaining<=0){P.bDefeated=true;TArray<int32> Dead;for(auto& KV:Entities)if(KV.Value->Owner==P.Slot)Dead.Add(KV.Key);for(int Id:Dead)Kill(Id);if(P.Controller.IsValid())P.Controller->ClientNotice(TEXT("Your people have lost their last Seat. Defeat."));}
 }
 if(!bSandbox){TSet<int32> Teams;for(auto& P:Players)if(!P.bDefeated)Teams.Add(P.Team);if(Teams.Num()<=1){Winner=Teams.Num()?*Teams.CreateConstIterator():-1;Phase=2;UE_LOG(LogTemp,Display,TEXT("RTS_MATCH_FINISHED winner=%d time=%.1f"),Winner,Time);Snapshot();}}
}
