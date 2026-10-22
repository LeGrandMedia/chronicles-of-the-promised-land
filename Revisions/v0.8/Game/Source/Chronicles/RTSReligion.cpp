#include "RTSWorld.h"
#include "RTSController.h"

static int32 HeroPerks(const FRTSPlayer& P,const TCHAR* Kind) {
 int32 N=0;for(const FString& Id:P.Upgrades){const auto& D=FRTSCatalog::Get().Def(Id);if(D.Kind==TEXT("age_option")&&D.Perk==Kind)++N;}return N;
}
float ARTSWorld::GatherMultiplier(int32 OwnerSlot) const {
 if(!Players.IsValidIndex(OwnerSlot))return 1;const auto& P=Players[OwnerSlot];
 return (1+.1f*HeroPerks(P,TEXT("economy")))*(P.Faction==TEXT("lamanites")?1+P.Favor*.003f:1);
}
float ARTSWorld::PlunderMultiplier(int32 OwnerSlot) const {
 if(!Players.IsValidIndex(OwnerSlot)||Players[OwnerSlot].Faction!=TEXT("gadianton")||Players[OwnerSlot].Favor<=30)return 1;
 return Has(OwnerSlot,TEXT("plunder_hoard"))?3:Has(OwnerSlot,TEXT("plunder_share"))?2:1;
}
float ARTSWorld::MilitaryMultiplier(int32 OwnerSlot) const {
 if(!Players.IsValidIndex(OwnerSlot))return 1;const auto& P=Players[OwnerSlot];
 return (1+.1f*HeroPerks(P,TEXT("military")));
}
bool ARTSWorld::CanTrainHero(int32 OwnerSlot,const FString& Type) const {
 if(!Players.IsValidIndex(OwnerSlot)||!Players[OwnerSlot].HeroChoices.Contains(Type)||!Players[OwnerSlot].HeroDeaths.Contains(Type))return false;
 for(const auto& KV:Entities){const auto& E=*KV.Value;if(E.Owner!=OwnerSlot)continue;if(D(E).Id==Type)return false;for(const auto& Job:E.Jobs)if(Job.Type==Type)return false;}return true;
}
bool ARTSWorld::QueueAgeChoice(FRTSEntity& Building,const FString& Type,ARTSController* C) {
 if(!Type.StartsWith(TEXT("age2"))&&!Type.StartsWith(TEXT("age3"))&&!Type.StartsWith(TEXT("age4")))return false;
 const auto& Cat=FRTSCatalog::Get();const auto& Choice=Cat.Def(Type);
 if(!Players.IsValidIndex(Building.Owner))return true;auto& P=Players[Building.Owner];
 if(Choice.Kind!=TEXT("age_option")||D(Building).Id!=TEXT("seat_of_the_people")||Building.Progress<1||P.Age!=Choice.Age||!Cat.Faction(P.Faction).Upgrades.Contains(Type)||P.HeroChoices.Contains(Choice.HeroChoice)) {Error(C,TEXT("Choose one of your people's heroes to advance to the next age."));return true;}
 for(const FString& Req:Choice.Requires)if(!Has(P.Slot,Req)&&!OwnedStructure(P.Slot,Req)){Error(C,TEXT("Complete the required building or research first."));return true;}
 for(const auto& KV:Entities)if(KV.Value->Owner==P.Slot)for(const auto& Job:KV.Value->Jobs)if(Cat.Def(Job.Type).Kind==TEXT("age_option")){Error(C,TEXT("An age advancement is already in progress."));return true;}
 const auto& Hero=Cat.Def(Choice.HeroChoice);Population();if(!Hero.bHero||P.Population+Hero.Pop>P.Capacity){Error(C,TEXT("Build more habitations to make room for your chosen hero."));return true;}
 if(Building.Jobs.Num()>=8){Error(C,TEXT("That production queue is full."));return true;}
 const FVector Cost=Cat.Price(Choice,P.Faction);if(!Pay(P.Slot,Cost)){Error(C,TEXT("You need more resources for this age advancement."));return true;}
 FRTSJob Job;Job.Type=Type;Job.Paid=Cost;Job.Remaining=Job.Total=Cat.Duration(Choice,P.Faction);Job.Pop=Hero.Pop;Building.Jobs.Add(Job);Population();return true;
}
void ARTSWorld::CompleteAgeChoice(int32 OwnerSlot,const FRTSDef& Choice,FVector2D SpawnAt,int32 ProducerId) {
 if(!Players.IsValidIndex(OwnerSlot))return;auto& P=Players[OwnerSlot];P.Age=FMath::Max(P.Age,Choice.Age+1);P.Upgrades.AddUnique(Choice.Id);P.HeroChoices.AddUnique(Choice.HeroChoice);
 const auto& Hero=FRTSCatalog::Get().Def(Choice.HeroChoice);FVector2D Spot=SpawnAt+FVector2D(6,0);bool Found=false;
 for(int Ring=5;Ring<=16&&!Found;++Ring)for(int I=0;I<16;++I){const FVector2D Test=SpawnAt+FVector2D(FMath::Cos(I*PI/8),FMath::Sin(I*PI/8))*Ring;if(Walkable(Test,Hero.Radius)){Spot=Test;Found=true;break;}}
 if(auto Unit=Spawn(Hero.Id,OwnerSlot,Spot))if(auto Producer=Find(ProducerId))ApplyRally(*Unit,*Producer);Rescale(OwnerSlot);if(P.Controller.IsValid())P.Controller->ClientNotice(FString::Printf(TEXT("Age %d: %s has joined your people."),P.Age,*Hero.Name));
}
bool ARTSWorld::IsConvertible(const FRTSEntity& T,int32 OwnerSlot) const {
 // Teams are fixed for a match, and Start resets Time to zero. Hostility therefore
 // begins at match start; any future diplomacy must track this per owner pair.
 const auto& Def=D(T);return Time>=60.f&&Players.IsValidIndex(OwnerSlot)&&Players.IsValidIndex(T.Owner)&&!T.bConverted&&Def.Kind!=TEXT("ammonite")&&T.HP>0&&!Allied(OwnerSlot,T.Owner)&&!Allied(OwnerSlot,T.DisguisedOwner)&&!T.Carrier&&!Def.bBuilding&&(RTS::IsMilitary(Def)||RTS::IsWorker(Def)||RTS::IsReligious(Def))&&!Def.bHero&&!RTS::IsCarrier(Def)&&Def.Kind!=TEXT("beast");
}
bool ARTSWorld::CanReceiveConversion(const FRTSEntity& Target,int32 OwnerSlot,const FString& NewType) const {
 if(!Players.IsValidIndex(OwnerSlot))return false;
 const auto& New=NewType.IsEmpty()?D(Target):FRTSCatalog::Get().Def(NewType);if(New.Id.IsEmpty())return false;
 // Count live state, including production reservations, so several channels in
 // the same simulation step cannot overbook population or the Ammonite cap.
 int32 Population=0,Capacity=0,Ammonites=0;
 for(const auto& KV:Entities){const auto& E=*KV.Value;if(E.Owner!=OwnerSlot)continue;const auto& Def=D(E);
  if(!Def.bBuilding)Population+=Def.Pop;
  else if(E.Progress>=1)Capacity+=Def.Id==TEXT("seat_of_the_people")?15:Def.Id==TEXT("tents_and_habitations")?10:0;
  for(const auto& Job:E.Jobs)Population+=Job.Pop;
  if(Def.Kind==TEXT("ammonite"))++Ammonites;
 }
 return Population+New.Pop<=FMath::Min(Capacity,200)&&(New.Kind!=TEXT("ammonite")||Ammonites<12);
}
bool ARTSWorld::ConvertUnit(FRTSEntity& T,int32 OwnerSlot,const FString& NewType,bool bByMartyrdom) {
 const bool Eligible=bByMartyrdom?(Time>=60.f&&!T.bConverted&&D(T).Kind!=TEXT("ammonite")&&T.HP>0&&Players.IsValidIndex(T.Owner)&&!Allied(OwnerSlot,T.Owner)&&!D(T).bBuilding&&D(T).Attack>0):IsConvertible(T,OwnerSlot);
 if(!Players.IsValidIndex(OwnerSlot)||!Eligible)return false;const auto& New=NewType.IsEmpty()?D(T):FRTSCatalog::Get().Def(NewType);if(New.Id.IsEmpty())return false;
 if(!CanReceiveConversion(T,OwnerSlot,NewType))return false;
 const int32 OldOwner=T.Owner;const float Ratio=T.HP/FMath::Max(1.f,T.MaxHP);T.DisguisedMaxHP=T.MaxHP;DropCargo(T);T.Owner=OwnerSlot;T.DisguisedOwner=(!bByMartyrdom&&Players[OwnerSlot].Faction==TEXT("gadianton"))?OldOwner:-1;T.Def=FRTSCatalog::Get().Index(New.Id);ResetOrder(T,TEXT("idle"));T.PendingOrders.Empty();T.Jobs.Empty();T.BuffUntil=0;T.BonusAttack=0;T.Cooldown=0;T.bConverted=true;T.MaxHP=MaxHP(T);T.HP=Ratio*T.MaxHP;Favor(OldOwner,Players[OldOwner].Faction==TEXT("gadianton")?-10:-5);Favor(OwnerSlot,10);if(T.DisguisedOwner<0)EmitResources(T,FVector::ZeroVector,10);Population();return true;
}
void ARTSWorld::Religion(float DT) {
 TArray<float> Change;Change.Init(0,Players.Num());for(const auto& P:Players){const float Excess=FMath::Max(0.f,P.Favor-20);Change[P.Slot]=-(.025f+.0004f*Excess*Excess)*DT;}
 TArray<int32> Ids;Entities.GetKeys(Ids);Ids.Sort();for(int32 Id:Ids){auto E=Find(Id);if(!E||!Players.IsValidIndex(E->Owner)||E->Carrier||E->HP<=0)continue;const auto& Def=D(*E);float Rate=ReligiousGeneration(*E);
  if(Def.Id==TEXT("secret_combination_lodge")&&E->Progress>=1){if(!E->bDiscovered)for(const auto& P:Players)if(!Allied(E->Owner,P.Slot)&&VisiblePoint(P.Slot,E->Pos)){E->bDiscovered=true;break;}Rate=ReligiousGeneration(*E);}
  Change[E->Owner]+=Rate*DT;
  FVector Gain=FVector::ZeroVector;
  if(Def.Id==TEXT("priestcrafter")&&Has(E->Owner,TEXT("order_of_the_nehors"))){Gain.Z=.25f*DT;E->ResourceRate.Z=.25f;Players[E->Owner].Resources+=Gain;}
  if(Rate>0||Gain.Z>0)EmitTrickleResources(*E,Gain,Rate*DT);
  if(Def.Kind==TEXT("lawyer")&&E->Order==TEXT("compromise")){
   auto T=Find(E->Target);if(!T||T->Owner<0||Allied(E->Owner,T->Owner)||T->Progress<1||(D(*T).Id!=TEXT("seat_of_the_people")&&D(*T).Id!=TEXT("storehouse_of_provisions"))){ResetOrder(*E,TEXT("idle"));continue;}
   const float Range=D(*T).Radius+3;if(FVector2D::Distance(E->Pos,T->Pos)>Range)Move(*E,T->Pos,Range-.2f,DT);
  }
  if(Def.Id==TEXT("temple_of_the_land")&&E->Progress>=1&&Players[E->Owner].Faction==TEXT("lamanites")&&Has(E->Owner,TEXT("by_the_pride_of_their_hearts"))){
   auto T=Find(E->Target);if(!T||!IsConvertible(*T,E->Owner)||!CanReceiveConversion(*T,E->Owner,TEXT("zoramite_captain"))||FVector2D::Distance(E->Pos,T->Pos)>ConversionRange(*E)||!CanSee(E->Owner,*T)){T=nullptr;E->Target=0;E->Channel=0;float Best=ConversionRange(*E);for(int CandidateId:Ids){auto V=Find(CandidateId);if(!V||!IsConvertible(*V,E->Owner)||!CanReceiveConversion(*V,E->Owner,TEXT("zoramite_captain"))||!CanSee(E->Owner,*V))continue;const float Dist=FVector2D::Distance(E->Pos,V->Pos);if(Dist<Best){T=V;Best=Dist;}}if(T)E->Target=T->Id;}
   bool Claimed=false;if(T)for(int OtherId:Ids){auto Other=Find(OtherId);if(Other&&OtherId!=Id&&Other->HP>0&&!Other->Carrier&&Other->Target==T->Id&&Other->Channel>0&&(E->Channel<=0||OtherId<Id)&&(Other->Order==TEXT("convert")||D(*Other).Id==TEXT("temple_of_the_land"))){Claimed=true;break;}}
   if(T&&!Claimed&&LOS(E->Pos,T->Pos,E->Id,T->Id)){E->Channel+=DT;if(E->Channel+KINDA_SMALL_NUMBER>=5.f&&ConvertUnit(*T,E->Owner,TEXT("zoramite_captain"))){E->Target=0;E->Channel=0;}}else E->Channel=0;
  }
 }
 TArray<int32> AuraSources;for(int32 Id:Ids){auto Source=Find(Id);if(Source&&Players.IsValidIndex(Source->Owner)&&Players[Source->Owner].Faction==TEXT("laban")&&!Source->Carrier&&Source->Progress>=1&&(D(*Source).Id==TEXT("temple_of_the_land")||D(*Source).Id==TEXT("drunken_concubine")))AuraSources.Add(Id);}
 for(int32 Id:Ids){auto T=Find(Id);if(!T||T->Owner<0||D(*T).bBuilding||T->Carrier)continue;bool Affected=false;
  for(int32 SourceId:AuraSources){auto Source=Find(SourceId);if(Allied(Source->Owner,T->Owner))continue;
   const float Radius=LabanAuraRadius(*Source);
   if(Radius>0&&FVector2D::DistSquared(Source->Pos,T->Pos)<=Radius*Radius&&LOS(Source->Pos,T->Pos,SourceId,Id)){Affected=true;break;}
  }
  if(T->bDrunkenAura!=Affected){const float Ratio=T->HP/FMath::Max(1.f,T->MaxHP);T->bDrunkenAura=Affected;T->MaxHP=MaxHP(*T);T->HP=T->MaxHP*Ratio;}
 }
 for(int32 I=0;I<Players.Num();++I)if(!Players[I].bDefeated)Favor(I,Change[I]);
}

// Laban supports concubines only. The permanent age choice and local aura multiply.
bool ARTSWorld::HasLabanAura(const FRTSEntity& Unit) const {
 if(D(Unit).Id!=TEXT("drunken_concubine")||Unit.Carrier)return false;
 for(const auto& KV:Entities){const auto& Hero=*KV.Value;if(Hero.HP>0&&!Hero.Carrier&&D(Hero).Id==TEXT("laban")&&Allied(Unit.Owner,Hero.Owner)&&FVector2D::DistSquared(Unit.Pos,Hero.Pos)<=64&&LOS(Unit.Pos,Hero.Pos,Unit.Id,Hero.Id))return true;}return false;
}
float ARTSWorld::ConcubinePower(const FRTSEntity& Unit) const {
 if(D(Unit).Id!=TEXT("drunken_concubine")||!Players.IsValidIndex(Unit.Owner))return 1;
 return (1+.1f*HeroPerks(Players[Unit.Owner],TEXT("concubines")))*(HasLabanAura(Unit)?1.25f:1.f);
}
void ARTSWorld::RefreshReligiousStats() {
 for(auto& P:Players){P.ReligiousUnits=0;P.ReligiousStructures=0;float Generation=0;
  for(const auto& KV:Entities){const auto& E=*KV.Value;if(E.Owner!=P.Slot||E.HP<=0)continue;const auto& Def=D(E);Generation+=ReligiousGeneration(E);
   if(RTS::IsReligious(Def)||Def.Id==TEXT("zoramite_captain")){++P.ReligiousUnits;}
   if(E.Progress>=1&&(Def.Id==TEXT("temple_of_the_land")||Def.Id==TEXT("secret_combination_lodge")||(P.Faction==TEXT("laban")&&Def.Id==TEXT("winery")))){++P.ReligiousStructures;}
  }
  const float Excess=FMath::Max(0.f,P.Favor-20),Decay=.025f+.0004f*Excess*Excess;
  P.ReligionTooltip=FString::Printf(TEXT("Religious units: %d | Structures: %d\nGeneration +%.2f/s | Decay -%.2f/s\n"),P.ReligiousUnits,P.ReligiousStructures,Generation,Decay);
  if(P.Faction==TEXT("nephites"))P.ReligionTooltip+=FString::Printf(TEXT("Unit maximum HP: 100 base becomes %.0f HP\nFavor 0: 1 HP | 50: base HP | 75: 10x | 100: 100x\nLosing Favor reduces all unit HP; buildings excluded."),RTS::FavorMaxHP(100.f,P.Favor));
  else if(P.Faction==TEXT("lamanites"))P.ReligionTooltip+=FString::Printf(TEXT("Gathering speed: +%.1f%%\nAttack speed: +%.1f%%\nTraining speed: +%.1f%%\n(0.3%% gather / attack; 0.5%% training per point)"),P.Favor*.3f,P.Favor*.3f,P.Favor*.5f);
  else if(RTS::IsJaredite(P.Faction))P.ReligionTooltip+=FString::Printf(TEXT("Building maximum HP: +%.1f%%\n(0.3%% per glowing stone)"),P.Favor*.3f);
  else if(P.Faction==TEXT("laban"))P.ReligionTooltip+=FString::Printf(TEXT("One of the Fifty splash: %.1f%% damage\nSplash radius: %.2fm\nWinery: +%.2f Drunkenness/s | Great and Spacious Building: +%.2f/s\nConcubine / building aura: +20%% laborer gathering\nEnemy aura: -20%% move, -25%% attack speed, -15%% HP"),P.Favor*.35f,1.4f+P.Favor*.015f,Has(P.Slot,TEXT("winery_drunkness"))?.6f:.5f,Has(P.Slot,TEXT("winery_drunkness"))?3.f:2.5f);
  else if(P.Faction==TEXT("gadianton")){
   P.ReligionTooltip+=FString::Printf(TEXT("Raid stat bonus: x%.0f (above 30)\nEnemy vision: %s (above 50)\nCompromised deposits: %d%% (above 70)\nThreshold abilities require their research."),PlunderMultiplier(P.Slot),SharesEnemyVision(P.Slot)?TEXT("active"):TEXT("inactive"),Has(P.Slot,TEXT("compromised_storehouses"))&&P.Favor>70?(P.Favor>=100?100:50):0);
  }
 }
}

// Conversion channels always require five uninterrupted seconds for every faction.
bool ARTSWorld::CanConvertWith(const FRTSEntity& Unit,const FRTSEntity& Target) const {
 return Unit.HP>0&&!Unit.Carrier&&Unit.Progress>=1&&Players.IsValidIndex(Unit.Owner)&&RTS::IsReligious(D(Unit))&&IsConvertible(Target,Unit.Owner)&&CanReceiveConversion(Target,Unit.Owner,ConversionResultType(Unit));
}
FString ARTSWorld::ConversionResultType(const FRTSEntity& Unit) const {
 if(!Players.IsValidIndex(Unit.Owner))return TEXT("");
 const FString& Faction=Players[Unit.Owner].Faction;
 return Faction==TEXT("nephites")?TEXT("ammonite_covenant_keeper"):Faction==TEXT("lamanites")?TEXT("zoramite_captain"):D(Unit).Id==TEXT("drunken_concubine")?TEXT("industrious_laborer"):TEXT("");
}
float ARTSWorld::ConversionRange(const FRTSEntity& Unit) const {
 const bool Platform=D(Unit).Id==TEXT("temple_of_the_land")||D(Unit).Id==TEXT("rameumptom");
 return (Platform?8.f:5.f)*(Platform&&Has(Unit.Owner,TEXT("lofty_rameumptom"))?3.f:1.f);
}
float ARTSWorld::ReligiousGeneration(const FRTSEntity& Unit) const {
 if(Unit.HP<=0||Unit.Carrier||!Players.IsValidIndex(Unit.Owner))return 0;
 const auto& Def=D(Unit);
 if(Def.Id==TEXT("zoramite_captain"))return .12f*(1+.1f*HeroPerks(Players[Unit.Owner],TEXT("religion_generation")));
 if(RTS::IsReligious(Def))return .06f*(1+.1f*HeroPerks(Players[Unit.Owner],TEXT("religion_generation")));
 if(Unit.Progress>=1&&Def.Id==TEXT("winery")&&Players[Unit.Owner].Faction==TEXT("laban"))return Has(Unit.Owner,TEXT("winery_drunkness"))?.6f:.5f;
 if(Unit.Progress>=1&&Def.Id==TEXT("temple_of_the_land"))return Players[Unit.Owner].Faction==TEXT("laban")?(Has(Unit.Owner,TEXT("winery_drunkness"))?3.f:2.5f):.1f;
 if(Unit.Progress>=1&&Def.Id==TEXT("secret_combination_lodge")&&!Unit.bDiscovered)return .16f;
 return 0;
}
float ARTSWorld::LaborerAuraMultiplier(const FRTSEntity& Unit) const {
 if(Unit.HP<=0||Unit.Carrier||!RTS::IsWorker(D(Unit)))return 1;
 float Bonus=0;
 for(const auto& KV:Entities){const auto& Source=*KV.Value;if(!Allied(Unit.Owner,Source.Owner))continue;
  const float Radius=LabanAuraRadius(Source);if(Radius<=0)continue;
  if(FVector2D::DistSquared(Unit.Pos,Source.Pos)<=Radius*Radius&&LOS(Unit.Pos,Source.Pos,Unit.Id,Source.Id))Bonus=FMath::Max(Bonus,.2f*ConcubinePower(Source));
 }
 return 1+Bonus;
}
float ARTSWorld::LabanAuraRadius(const FRTSEntity& Unit) const {
 if(Unit.HP<=0||Unit.Carrier||Unit.Progress<1||!Players.IsValidIndex(Unit.Owner)||Players[Unit.Owner].Faction!=TEXT("laban"))return 0;
 if(D(Unit).Id==TEXT("temple_of_the_land"))return 8.f;
 return D(Unit).Id==TEXT("drunken_concubine")?(HasLabanAura(Unit)?7.f:5.f):0.f;
}
