#include "RTSWorld.h"
#include "RTSController.h"

void ARTSWorld::Snapshot() {
 RefreshReligiousStats();
 const int32 Sequence=++SnapshotSequence;
 for(auto& P:Players){auto C=P.Controller.Get();if(!C)continue;
  // Snapshot ticks continue after victory, allowing bounded reliable batches
  // to drain without delaying the match-complete snapshot behind a death burst.
  C->FlushDeathEvents(Time,Phase);
  TArray<uint8> Fog;Fog.Init(bFogOfWar?0:2,4096);if(P.Explored.Num()!=4096)P.Explored.Init(0,4096);if(!bFogOfWar)P.Explored.Init(1,4096);
  const bool Shared=SharesEnemyVision(P.Slot);
  if(Phase>0&&bFogOfWar)for(auto& KV:Entities){auto& E=*KV.Value;if((!Allied(P.Slot,E.Owner)&&!Allied(P.Slot,E.DisguisedOwner)&&!(Shared&&E.Owner>=0))||E.Carrier||E.HP<=0)continue;float Sight=EffectiveSight(E);int CX=E.Pos.X/2.5,CY=E.Pos.Y/2.5,R=FMath::CeilToInt(Sight/2.5f);for(int Y=FMath::Max(0,CY-R);Y<=FMath::Min(63,CY+R);Y++)for(int X=FMath::Max(0,CX-R);X<=FMath::Min(63,CX+R);X++){int I=Y*64+X;FVector2D At((X+.5)*2.5,(Y+.5)*2.5);if(Fog[I]==2||FVector2D::DistSquared(At,E.Pos)>Sight*Sight)continue;if(LOS(E.Pos,At,E.Id)){Fog[I]=2;P.Explored[I]=1;}}}
  for(int I=0;I<4096;I++)if(Fog[I]==0)Fog[I]=P.Explored[I];
  TArray<FRTSEntityView> View;TArray<int32> Ids;Entities.GetKeys(Ids);Ids.Sort();TSet<int32> VisibleBuildings;
  for(int Id:Ids){auto& E=*Entities[Id];if(E.Carrier||E.HP<=0)continue;int X=FMath::Clamp(int(E.Pos.X/2.5),0,63),Y=FMath::Clamp(int(E.Pos.Y/2.5),0,63);bool Visible=Allied(P.Slot,E.Owner)||Allied(P.Slot,E.DisguisedOwner)||(Shared&&E.Owner>=0)||(Fog[Y*64+X]==2&&CanSee(P.Slot,E));if(!Visible)continue;
   FRTSEntityView V=E;V.bRemembered=false;V.Status=E.Order;V.AuraRadius=LabanAuraRadius(E);V.Queue.Empty();V.WorkersAssigned=0;V.bStealthed=Allied(P.Slot,E.Owner)&&IsStealthed(E);V.PassengerDefs.Empty();V.PassengerIds.Empty();V.EffectiveRange=EffectiveAttackRange(E);V.EffectiveSight=EffectiveSight(E);V.MinimumRange=EffectiveMinimumAttackRange(E);V.SplashRadius=AttackSplashRadius(E);V.AttackRate=D(E).Interval/FMath::Max(.01f,AttackSpeedMultiplier(E));V.EffectiveAttack=AttackStrength(E);V.DamageModifiers=DamageModifierText(E);V.SpiritRate=ReligiousGeneration(E);V.ConversionProgress=E.Order==TEXT("convert")?FMath::Clamp(E.Channel/5.f,0.f,1.f):0;V.ProstrateRemaining=D(E).Kind==TEXT("ammonite")?FMath::Max(0.f,E.BuffUntil-Time):0;
   if(D(E).bBuilding){V.Status=E.Progress<1?TEXT("foundation"):E.Jobs.IsEmpty()?TEXT("idle"):E.PauseUntil>Time?TEXT("paused"):FRTSCatalog::Get().Def(E.Jobs[0].Type).Kind==TEXT("age_option")?TEXT("advancing"):FRTSCatalog::Get().Def(E.Jobs[0].Type).bUpgrade?TEXT("researching"):TEXT("training");}
   for(auto& WK:Entities)if(RTS::IsWorker(D(*WK.Value))&&WK.Value->WorkTarget==E.Id&&(Allied(P.Slot,WK.Value->Owner)||CanSee(P.Slot,*WK.Value)))V.WorkersAssigned++;
   if(E.Owner==P.Slot){if(E.WorkTarget>0)V.Target=E.WorkTarget;for(auto& J:E.Jobs)V.Queue.Add(J.Type);V.QueueProgress=E.Jobs.Num()?1-E.Jobs[0].Remaining/FMath::Max(.001f,E.Jobs[0].Total):0;for(int32 Passenger:E.Passengers)if(auto Unit=Find(Passenger)){V.PassengerIds.Add(Passenger);V.PassengerDefs.Add(Unit->Def);}}
   else{V.Cargo=FVector::ZeroVector;V.Target=0;V.bRaidRallySet=false;V.RaidRallyPoint=FVector2D::ZeroVector;V.RaidRallyTarget=0;V.bRallySet=false;V.RallyTarget=0;V.RallyPoint=FVector2D::ZeroVector;V.bEconomicRallySet=false;V.EconomicRallyPoint=FVector2D::ZeroVector;V.EconomicRallyTarget=0;V.GalavantRoute.Empty();V.QueueProgress=0;V.ResourceRate=FVector::ZeroVector;V.SpiritRate=V.CementRate=0;if(!Allied(P.Slot,E.Owner)){V.ResourceEvent=FVector::ZeroVector;V.ResourceEventTime=-100;V.SpiritEvent=V.CementEvent=0;}}
   if(Allied(P.Slot,E.DisguisedOwner)){V.Owner=E.DisguisedOwner;V.Status=TEXT("idle");V.Target=0;V.bStealthed=false;V.ResourceEvent=FVector::ZeroVector;V.ResourceEventTime=-100;V.SpiritEvent=V.CementEvent=0;if(E.DisguisedMaxHP>0){V.MaxHP=E.DisguisedMaxHP;V.HP=E.HP/FMath::Max(1.f,E.MaxHP)*V.MaxHP;}}
   if(D(E).bBuilding||D(E).Id==TEXT("tree")||D(E).Id==TEXT("ore")||D(E).Kind==TEXT("sheep")){VisibleBuildings.Add(Id);P.KnownBuildings.Add(Id,V);}View.Add(V);
  }
  TArray<int32> Forgotten;for(auto& Known:P.KnownBuildings){if(VisibleBuildings.Contains(Known.Key))continue;auto& V=Known.Value;int X=FMath::Clamp(int(V.Pos.X/2.5),0,63),Y=FMath::Clamp(int(V.Pos.Y/2.5),0,63);
   // Only a newly visible cell may disprove a remembered building. No unseen
   // health, ownership, construction, deletion, or resource updates are copied.
   if(Fog[Y*64+X]==2||(Allied(P.Slot,V.Owner)&&!Find(Known.Key))){Forgotten.Add(Known.Key);continue;}
   FRTSEntityView Memory=V;Memory.bRemembered=true;Memory.AuraRadius=0;Memory.bRaidRallySet=false;Memory.RaidRallyPoint=FVector2D::ZeroVector;Memory.RaidRallyTarget=0;Memory.Status=TEXT("remembered");Memory.bAttacking=false;Memory.AttackEventTime=-100;Memory.ConversionProgress=Memory.ProstrateRemaining=0;Memory.ResourceRate=FVector::ZeroVector;Memory.SpiritRate=Memory.CementRate=Memory.CementEvent=0;Memory.bRallySet=false;Memory.Queue.Empty();Memory.QueueProgress=0;Memory.Cargo=FVector::ZeroVector;Memory.Target=0;Memory.WorkersAssigned=0;Memory.ResourceEvent=FVector::ZeroVector;Memory.ResourceEventTime=-100;Memory.SpiritEvent=0;Memory.bEconomicRallySet=false;Memory.EconomicRallyPoint=FVector2D::ZeroVector;Memory.EconomicRallyTarget=0;Memory.GalavantRoute.Empty();View.Add(Memory);
  }for(int Id:Forgotten)P.KnownBuildings.Remove(Id);
  TArray<FRTSPlayerView> PV;for(auto& Other:Players){FRTSPlayerView V=Other;V.LobbyResources=StartingResources;V.LobbyFruitfulness=Fruitfulness;V.LobbySpeed=GameSpeed;V.bLobbyFog=bFogOfWar;if(!Allied(P.Slot,Other.Slot)){V.Resources=FVector::ZeroVector;V.Cement=0;V.Upgrades.Empty();V.HeroChoices.Empty();V.Population=0;V.Capacity=0;}else{for(const auto& KV:Entities)if(KV.Value->DisguisedOwner==Other.Slot)V.Population+=D(*KV.Value).Pop;}if(Other.Slot!=P.Slot){V.ReligiousUnits=0;V.ReligiousStructures=0;V.ReligionTooltip.Empty();}PV.Add(V);}
  C->SendSnapshot(View,PV,Fog,Time,Phase,Map,Winner,P.Slot,Sequence);
 }
}
