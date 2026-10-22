#include "RTSWorld.h"
#include "RTSController.h"

void ARTSWorld::Damage(FRTSEntity& T,FRTSEntity& A,float Raw) {
 if(T.HP<=0||Allied(T.Owner,A.Owner)||Allied(T.DisguisedOwner,A.Owner)||Raw<=0)return;Raw*=AttackMultiplierAgainst(A,T);auto& TD=D(T);float Armor=TD.Armor;
 if(RTS::IsInfantry(TD)){if(Has(T.Owner,TEXT("thick_clothing")))Armor+=1;if(Has(T.Owner,TEXT("breastplates_and_arm_shields")))Armor+=2;}
 float Aura=0;for(auto& KV:Entities){auto& H=*KV.Value;if(!Allied(T.Owner,H.Owner)||H.Carrier)continue;float Dist=FVector2D::Distance(T.Pos,H.Pos);if(D(H).Id==TEXT("laban")&&TD.Id==TEXT("drunken_concubine")&&H.HP>0&&Dist<=8&&LOS(H.Pos,T.Pos,H.Id,T.Id))Aura=FMath::Max(Aura,1.f);if(D(H).Id==TEXT("helaman")&&H.BuffUntil>Time&&TD.Id==TEXT("stripling_warrior")&&Dist<=8)Aura=FMath::Max(Aura,2.f);if(D(H).Id==TEXT("samuel_the_lamanite")&&H.Order==TEXT("sermon")&&Dist<=10)Aura=FMath::Max(Aura,2.f);}
 float Hit=FMath::Max(1.f,FMath::FloorToFloat(Raw)-Armor-Aura);T.HP-=Hit;T.LastDamage=Time;T.RevealedUntil=Time+5;A.RevealedUntil=Time+5;A.bAttacking=true;
 if(T.Order==TEXT("convert")||T.Order==TEXT("raid")||T.Order==TEXT("injunction")||TD.Id==TEXT("temple_of_the_land")){T.Channel=0;}
 if(T.HP<=0&&!T.bSurvivalUsed){if(TD.Id==TEXT("shiz")){T.bSurvivalUsed=true;T.HP=1;T.DeathAt=Time+2;T.AttackTimer=0;return;}if(TD.Id==TEXT("stripling_warrior")&&Players[T.Owner].Favor>=75){T.bSurvivalUsed=true;T.HP=1;T.DisarmedUntil=Time+5;return;}}
 if(T.DeathAt>Time&&TD.Id==TEXT("shiz")){T.HP=1;return;}if(T.HP<=0)Kill(T.Id,A.Id);
}
void ARTSWorld::Kill(int32 Id,int32 Killer) {
 auto Ptr=Entities.Find(Id);if(!Ptr)return;TSharedPtr<FRTSEntity> Keep=*Ptr;auto& E=*Keep;auto& Def=D(E);auto A=Find(Killer);RecordDeath(E);
 if(Players.IsValidIndex(E.Owner)){
  if(Def.bHero)Players[E.Owner].HeroDeaths.Add(Def.Id,Time);
  if(Players[E.Owner].Faction==TEXT("lamanites")&&Has(E.Owner,TEXT("wrath_of_the_fallen"))&&A&&A->Owner>=0&&!Allied(E.Owner,A->Owner))Favor(E.Owner,5);
  if(Def.Id==TEXT("temple_of_the_land")&&E.Progress>=1&&A&&A->Owner>=0&&!Allied(E.Owner,A->Owner)&&Players[E.Owner].Faction==TEXT("nephites"))Favor(E.Owner,-10);
  if(A&&Players.IsValidIndex(A->Owner)&&!Allied(E.Owner,A->Owner)){
   const bool EconomicBuilding=Def.bBuilding&&(Def.Id==TEXT("seat_of_the_people")||Def.Id==TEXT("fields_of_grain")||Def.Id==TEXT("gathering_of_flocks")||Def.Id==TEXT("hill_of_ore")||Def.Id==TEXT("storehouse_of_provisions")||Def.Id==TEXT("place_of_trade")||Def.Id==TEXT("tents_and_habitations")||Def.Id==TEXT("winery"));
   if(Players[A->Owner].Faction==TEXT("nephites")&&(RTS::IsWorker(Def)||EconomicBuilding))Favor(A->Owner,-8);
   if(Players[A->Owner].Faction==TEXT("nephites")&&Def.Attack>0&&!Def.bBuilding){auto& P=Players[A->Owner];P.DefenseAwards.RemoveAll([&](float T){return Time-T>=60;});if(P.DefenseAwards.Num()<3)for(auto& KV:Entities)if(KV.Value->Owner==A->Owner&&D(*KV.Value).Id==TEXT("seat_of_the_people")&&KV.Value->Progress>=1&&FVector2D::Distance(E.Pos,KV.Value->Pos)<=15){P.DefenseAwards.Add(Time);Favor(A->Owner,1);break;}}
  }
  if(Def.Attack>0&&!Def.bBuilding)for(auto& KV:Entities){auto& H=*KV.Value;if(H.Id!=E.Id&&Allied(E.Owner,H.Owner)&&D(H).Id==TEXT("coriantumr")&&FVector2D::Distance(E.Pos,H.Pos)<=10){H.CoriantumrStacks.RemoveAll([&](float T){return T<=Time;});if(H.CoriantumrStacks.Num()<10)H.CoriantumrStacks.Add(Time+20);}}
  for(int I=1;I<E.Jobs.Num();I++)Refund(E.Owner,E.Jobs[I].Paid,1,E.Jobs[I].CementPaid);
 }
 const int32 ConvertKiller=(A&&Def.Kind==TEXT("ammonite")&&E.BuffUntil>Time&&A->Owner>=0&&!Allied(E.Owner,A->Owner)&&!D(*A).bBuilding&&D(*A).Attack>0)?A->Id:0;
 if(A&&Def.Id==TEXT("gathering_of_flocks")&&A->Owner>=0&&!Allied(E.Owner,A->Owner))for(auto& KV:Entities){auto& Sheep=*KV.Value;if(D(Sheep).Kind==TEXT("sheep")&&Sheep.HomeBuilding==E.Id){Sheep.Owner=A->Owner;Sheep.HomeBuilding=0;Sheep.ResourcePulseAt=Time;ResetOrder(Sheep,TEXT("idle"));}}
 DropCargo(E);
 auto Passengers=E.Passengers;Entities.Remove(Id);ObstacleCount=-1;
 for(int P:Passengers){auto Passenger=Find(P);if(!Passenger)continue;bool Found=false;FVector2D Landing;for(int R=1;R<=5&&!Found;R++)for(int I=0;I<16;I++){Landing=E.Pos+FVector2D(FMath::Cos(I*PI/8),FMath::Sin(I*PI/8))*R;if(Walkable(Landing,D(*Passenger).Radius,0,false)){Found=true;break;}}if(Found){Passenger->Carrier=0;Passenger->Pos=Landing;ResetOrder(*Passenger,TEXT("idle"));}else Kill(P,Killer);}
 if(auto Converted=Find(ConvertKiller))ConvertUnit(*Converted,E.Owner,TEXT("ammonite_covenant_keeper"),true);
 Population();
}
void ARTSWorld::Combat(FRTSEntity& E,float DT) {
 auto& Def=D(E);if(E.Order==TEXT("dissension")){auto T=Find(E.Target);if(!T||Time-E.LastDamage<.2||FVector2D::Distance(E.Pos,T->Pos)>6||!LOS(E.Pos,T->Pos,E.Id,T->Id)){ResetOrder(E,TEXT("idle"));return;}E.Channel-=DT;if(E.Channel<=0){T->PauseUntil=Time+6;ResetOrder(E,TEXT("idle"));}return;}
 if(RTS::IsWorker(Def)&&!E.bDirectAttack)return;
 if((E.DisguisedOwner>=0&&!E.bDirectAttack)||Def.Attack<=0||E.DisarmedUntil>Time||E.Progress<1||E.Carrier||!E.Evolution.IsEmpty())return;
 if(E.Order==TEXT("gather")||E.Order==TEXT("deliver")||E.Order==TEXT("build")||E.Order==TEXT("repair")||E.Order==TEXT("raid")||E.Order==TEXT("raid_move")||E.Order==TEXT("raid_return")||E.Order==TEXT("galavant")||E.Order==TEXT("move")||E.Order==TEXT("board"))return;
 auto T=Find(E.Target);if(T&&(T->Owner<0||Allied(E.Owner,T->Owner)||Allied(E.Owner,T->DisguisedOwner)||T->Carrier||!CanSee(E.Owner,*T))){T=nullptr;E.Target=0;}if(!T)E.Target=0;
 if(E.bDirectAttack&&!T){ResetOrder(E,TEXT("idle"));return;}
 if(!T){float Best=Def.bBuilding?EffectiveAttackRange(E):EffectiveSight(E);for(auto& KV:Entities){auto& Candidate=*KV.Value;if(Candidate.Owner<0||Allied(E.Owner,Candidate.Owner)||Allied(E.Owner,Candidate.DisguisedOwner)||Candidate.HP<=0||Candidate.Carrier)continue;auto& CD=D(Candidate);if(!CD.bBuilding&&((CD.Attack==0&&!RTS::IsReligious(CD))||RTS::IsWorker(CD)))continue;float Distance=FVector2D::Distance(E.Pos,Candidate.Pos)-CD.Radius;if(FVector2D::Distance(E.Pos,Candidate.Pos)<EffectiveMinimumAttackRange(E))continue;if(Distance<Best&&CanSee(E.Owner,Candidate)){T=&Candidate;Best=Distance;}}if(T)E.Target=T->Id;}
 if(!T)return;
 float Range=EffectiveAttackRange(E)+D(*T).Radius;if(FVector2D::Distance(E.Pos,T->Pos)<EffectiveMinimumAttackRange(E)){E.Target=0;return;}bool Sling=Def.Id==TEXT("ammon")&&FVector2D::Distance(E.Pos,T->Pos)>Range&&FVector2D::Distance(E.Pos,T->Pos)<=6+D(*T).Radius;
 if(Sling)Range=6+D(*T).Radius;
 if(FVector2D::Distance(E.Pos,T->Pos)>Range||!LOS(E.Pos,T->Pos,E.Id,T->Id)){if(!Def.bBuilding&&E.Order!=TEXT("hold"))Move(E,T->Pos,FMath::Max(.8f,Range-.15f),DT);return;}
 if(E.AttackTimer>0)return;float Attack=AttackStrength(E);
 E.CoriantumrStacks.RemoveAll([&](float X){return X<=Time;});
 if(Def.Id==TEXT("secret_murderer")&&Time>=E.RevealedUntil&&E.Cooldown<=0)E.Cooldown=20;
 if(Def.Id==TEXT("teancum")&&E.BuffUntil>Time)E.BuffUntil=0;
 if(Sling&&D(*T).bBuilding)Attack*=.25f;
 const FVector2D Impact=T->Pos;const int32 MainId=T->Id;const int32 AttackOwner=E.Owner;
 E.AttackEventTime=Time;E.AttackTimer=FMath::Max(.1f,Def.Interval/AttackSpeedMultiplier(E));Damage(*T,E,Attack);
 if(E.Owner!=AttackOwner)return; // A prostrate victim may convert its killer.
 if(Def.Id==TEXT("cement_tower")){
  TArray<int32> Splash;for(const auto& KV:Entities)if(KV.Key!=MainId&&KV.Value->Owner>=0&&!Allied(E.Owner,KV.Value->Owner)&&FVector2D::DistSquared(Impact,KV.Value->Pos)<=FMath::Square(AttackSplashRadius(E))&&LOS(E.Pos,KV.Value->Pos,E.Id,KV.Key))Splash.Add(KV.Key);
  Splash.Sort();for(int32 Id:Splash)if(auto V=Find(Id))Damage(*V,E,Attack*.5f);
 }

 if(Def.Id==TEXT("one_of_the_fifty")&&Players[E.Owner].Faction==TEXT("laban")&&Players[E.Owner].Favor>0){TArray<int32> Splash;for(auto& KV:Entities)if(KV.Key!=MainId&&KV.Value->Owner>=0&&!Allied(E.Owner,KV.Value->Owner)&&FVector2D::Distance(Impact,KV.Value->Pos)<=1.4f+Players[E.Owner].Favor*.015f)Splash.Add(KV.Key);for(int32 Id:Splash){if(E.Owner!=AttackOwner)break;if(auto V=Find(Id))Damage(*V,E,Attack*.35f*Players[E.Owner].Favor/100);}}
 if(E.DeathAt>0&&Def.Id==TEXT("shiz"))Kill(E.Id);
}
void ARTSWorld::Conversion(FRTSEntity& E,float DT) {
 if(!RTS::IsReligious(D(E))||!Players.IsValidIndex(E.Owner)||E.HP<=0||E.Carrier||E.Progress<1)return;
 if(E.Order==TEXT("idle")&&E.PendingOrders.IsEmpty()){
  FRTSEntity* BestTarget=nullptr;float Best=ConversionRange(E);
  for(auto& KV:Entities){auto& Candidate=*KV.Value;if(!CanConvertWith(E,Candidate)||!CanSee(E.Owner,Candidate)||!LOS(E.Pos,Candidate.Pos,E.Id,Candidate.Id))continue;
   const float Distance=FVector2D::Distance(E.Pos,Candidate.Pos);
   if(Distance<Best||(Distance==Best&&(!BestTarget||Candidate.Id<BestTarget->Id))){BestTarget=&Candidate;Best=Distance;}
  }
  if(BestTarget){E.Target=BestTarget->Id;E.Channel=0;E.Order=TEXT("convert");}
 }
 if(E.Order!=TEXT("convert"))return;
 // Bribery exposes the Lawyer for its whole approach/channel and one second
 // after completion or interruption; channel resets do not restore stealth.
 if(D(E).Kind==TEXT("lawyer"))E.RevealedUntil=FMath::Max(E.RevealedUntil,Time+1.f);
 auto T=Find(E.Target);if(!T||!CanConvertWith(E,*T)){ResetOrder(E,TEXT("idle"));return;}
 const float Range=ConversionRange(E);
 if(FVector2D::Distance(E.Pos,T->Pos)>Range){E.Channel=0;Move(E,T->Pos,Range-.2f,DT);return;}
 if(!CanSee(E.Owner,*T)||!LOS(E.Pos,T->Pos,E.Id,T->Id)){E.Channel=0;return;}
 // One channel per target. Additional converters may select it after the first is interrupted.
 for(auto& KV:Entities)if(KV.Key!=E.Id&&KV.Value->HP>0&&!KV.Value->Carrier&&(KV.Value->Order==TEXT("convert")||D(*KV.Value).Id==TEXT("temple_of_the_land"))&&KV.Value->Target==T->Id&&KV.Value->Channel>0&&(E.Channel<=0||KV.Key<E.Id)){E.Channel=0;return;}
 E.Channel+=DT;T->Status=D(E).Kind==TEXT("lawyer")?TEXT("Bribing"):TEXT("Converting");if(E.Channel+KINDA_SMALL_NUMBER<5.f)return;
 if(ConvertUnit(*T,E.Owner,ConversionResultType(E)))ResetOrder(E,TEXT("idle"));
}

float ARTSWorld::EffectiveAttackRange(const FRTSEntity& Unit) const {
 return D(Unit).Id==TEXT("cement_tower")?EffectiveSight(Unit):D(Unit).Range;
}
float ARTSWorld::EffectiveMinimumAttackRange(const FRTSEntity& Unit) const {
 return D(Unit).Id==TEXT("cement_tower")?3.f:0.f;
}
float ARTSWorld::AttackSplashRadius(const FRTSEntity& Unit) const {
 if(D(Unit).Id==TEXT("cement_tower"))return 2.f;
 if(D(Unit).Id==TEXT("one_of_the_fifty")&&Players.IsValidIndex(Unit.Owner)&&Players[Unit.Owner].Faction==TEXT("laban")&&Players[Unit.Owner].Favor>0)return 1.4f+Players[Unit.Owner].Favor*.015f;
 return 0;
}
float ARTSWorld::AttackMultiplierAgainst(const FRTSEntity& Unit,const FRTSEntity& Target) const {
 const auto& Def=D(Unit);const auto& TD=D(Target);float Result=1;
 if(RTS::IsRanged(Def)&&RTS::IsReligious(TD))Result*=1.5f;
 if(RTS::IsRanged(Def)&&RTS::IsMelee(TD))Result*=.75f;
 if(TD.bBuilding){if(Def.Range>2)Result*=.25f;else if(Def.Id==TEXT("cimeter_bearer")||Def.Id==TEXT("man_of_the_sword"))Result*=1.5f;else if(Def.Id==TEXT("man_of_moroni")||Def.Id==TEXT("one_of_the_fifty"))Result*=2;}
 return Result;
}
float ARTSWorld::AttackSpeedMultiplier(const FRTSEntity& Unit) const {
 if(!Players.IsValidIndex(Unit.Owner))return 1;
 float Result=Players[Unit.Owner].Faction==TEXT("lamanites")?1+Players[Unit.Owner].Favor*.003f:1;
 if(Unit.bDrunkenAura)Result*=.75f;
 if(RTS::IsMilitary(D(Unit)))for(const auto& KV:Entities){const auto& Source=*KV.Value;
  if(Source.HP>0&&!Source.Carrier&&Allied(Unit.Owner,Source.Owner)&&D(Source).Id==TEXT("zoramite_captain")&&FVector2D::DistSquared(Unit.Pos,Source.Pos)<=36&&LOS(Unit.Pos,Source.Pos,Unit.Id,Source.Id)){Result*=1.15f;break;}
 }
 return Result;
}

float ARTSWorld::AttackStrength(const FRTSEntity& E) const {
 if(!Players.IsValidIndex(E.Owner))return D(E).Attack+E.BonusAttack;
 const auto& Def=D(E);const auto Target=Entities.Find(E.Target);
 const bool Sling=Def.Id==TEXT("ammon")&&Target&&FVector2D::Distance(E.Pos,(*Target)->Pos)>EffectiveAttackRange(E)+D(**Target).Radius&&FVector2D::Distance(E.Pos,(*Target)->Pos)<=6+D(**Target).Radius;
 float Attack=Sling?12:Def.Attack+E.BonusAttack;
 for(float Until:E.CoriantumrStacks)if(Until>Time)Attack+=2;
 bool Melee=Def.Range<=2;if(Melee&&RTS::IsInfantry(Def)&&Has(E.Owner,TEXT("metallurgy")))Attack+=2;
 if(RTS::IsInfantry(Def)&&Players[E.Owner].Faction==TEXT("shiz"))Attack*=1.15f;
 float Aura=0;for(auto& KV:Entities){auto& H=*KV.Value;if(!Allied(E.Owner,H.Owner)||H.Carrier)continue;auto Hid=D(H).Id;float Dist=FVector2D::Distance(E.Pos,H.Pos);
  if(Hid==TEXT("chief_captain")&&Dist<=8)Aura=FMath::Max(Aura,H.BuffUntil>Time&&Melee?.2f:.1f);
  if(Hid==TEXT("captain_moroni")&&H.BuffUntil>Time&&FVector2D::Distance(E.Pos,H.BuffPoint)<=8)Aura=FMath::Max(Aura,.15f);
  if(Hid==TEXT("priestcrafter")&&H.HP>0&&Players.IsValidIndex(H.Owner)&&Players[H.Owner].Faction==TEXT("lamanites")&&RTS::IsMilitary(Def)){const auto PlatformEntry=Entities.Find(H.Target);auto Platform=PlatformEntry?PlatformEntry->Get():nullptr;bool Occupied=H.Order==TEXT("occupy")&&Platform&&Platform->Owner==H.Owner&&D(*Platform).Id==TEXT("temple_of_the_land")&&Platform->Progress>=1&&FVector2D::Distance(H.Pos,Platform->Pos)<=D(*Platform).Radius+.65f;if(Dist<=(Occupied?8:6)&&LOS(H.Pos,E.Pos,H.Id,E.Id))Aura=FMath::Max(Aura,Occupied?.15f:.05f);}
 }Attack*=(1+Aura)*MilitaryMultiplier(E.Owner);
 if(Def.Id==TEXT("secret_murderer")&&Time>=E.RevealedUntil&&E.Cooldown<=0)Attack+=30;
 if(Def.Id==TEXT("teancum")&&E.BuffUntil>Time)Attack+=40;
 return Attack;
}
FString ARTSWorld::DamageModifierText(const FRTSEntity& Unit) const {
 const auto& Def=D(Unit);FString Text;
 if(RTS::IsRanged(Def))Text=TEXT("Religious x1.5 | Melee x0.75 | Buildings x0.25");
 else if(Def.Range>2)Text=TEXT("Buildings x0.25");
 else if(Def.Id==TEXT("cimeter_bearer")||Def.Id==TEXT("man_of_the_sword"))Text=TEXT("Buildings x1.5");
 else if(Def.Id==TEXT("man_of_moroni")||Def.Id==TEXT("one_of_the_fifty"))Text=TEXT("Buildings x2");
 if(Def.Id==TEXT("cement_tower"))Text+=TEXT(" | Splash 50% | Min range 3m");
 return Text;
}
