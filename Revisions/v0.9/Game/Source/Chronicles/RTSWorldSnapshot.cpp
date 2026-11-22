#include "RTSWorld.h"
#include "RTSController.h"

namespace {
constexpr int32 SightGridSize=16;
constexpr int32 TerrainCellCount=RTS::FogSize*RTS::FogSize;
constexpr double TerrainCellSize=RTS::MapSize/RTS::FogSize;

int32 FogIndex(FVector2D Point) {
 return FMath::Clamp(int32(Point.Y/TerrainCellSize),0,RTS::FogSize-1)*RTS::FogSize
  +FMath::Clamp(int32(Point.X/TerrainCellSize),0,RTS::FogSize-1);
}

int32 SightCell(double Coordinate) {
 constexpr double CellSize=RTS::MapSize/SightGridSize;
 return FMath::Clamp(FMath::FloorToInt(Coordinate/CellSize),0,SightGridSize-1);
}
}

bool FRTSFog::InEntitySightRange(FVector2D Point) const {
 return EntitySightRange.Num()>0&&EntitySightRange[SightCell(Point.Y)*SightGridSize+SightCell(Point.X)];
}

FRTSFog ARTSWorld::ComputeFog(FRTSPlayer& P) const {
 FRTSFog Fog;
 Fog.Cells.Init(bFogOfWar?0:2,TerrainCellCount);
 if(!bFogOfWar){P.Explored.Init(1,TerrainCellCount);return Fog;}
 if(P.Explored.Num()!=TerrainCellCount)P.Explored.Init(0,TerrainCellCount);
 if(Phase>0){
  Fog.EntitySightRange.Init(false,SightGridSize*SightGridSize);
  const auto& Catalog=FRTSCatalog::Get();
  float MaxTargetRadius=Catalog.Def(INDEX_NONE).Radius;
  for(const auto& Def:Catalog.Defs)MaxTargetRadius=FMath::Max(MaxTargetRadius,Def.Radius);
  const bool Shared=SharesEnemyVision(P.Slot);
  for(const auto& KV:Entities){
   const auto& E=*KV.Value;
   if((!Allied(P.Slot,E.Owner)&&!Allied(P.Slot,E.DisguisedOwner)&&!(Shared&&E.Owner>=0))||E.Carrier||E.HP<=0)continue;
   const float Sight=EffectiveSight(E);

   // Mark every cell touched by the source's expanded range bounds, including
   // the boundary cells. Exact LOS and detection happen in CanSee.
   const double Reach=Sight+MaxTargetRadius+KINDA_SMALL_NUMBER;
   const int32 MinX=SightCell(E.Pos.X-Reach),MaxX=SightCell(E.Pos.X+Reach);
   const int32 MinY=SightCell(E.Pos.Y-Reach),MaxY=SightCell(E.Pos.Y+Reach);
   for(int32 Y=MinY;Y<=MaxY;++Y)Fog.EntitySightRange.SetRange(Y*SightGridSize+MinX,MaxX-MinX+1,true);

   const int32 CX=int32(E.Pos.X/TerrainCellSize),CY=int32(E.Pos.Y/TerrainCellSize);
   const int32 R=FMath::CeilToInt(Sight/TerrainCellSize);
   for(int32 Y=FMath::Max(0,CY-R);Y<=FMath::Min(RTS::FogSize-1,CY+R);++Y){
    for(int32 X=FMath::Max(0,CX-R);X<=FMath::Min(RTS::FogSize-1,CX+R);++X){
     const int32 I=Y*RTS::FogSize+X;
     const FVector2D At((X+.5)*TerrainCellSize,(Y+.5)*TerrainCellSize);
     if(Fog.Cells[I]==2||FVector2D::DistSquared(At,E.Pos)>Sight*Sight)continue;
     if(LOS(E.Pos,At,E.Id)){Fog.Cells[I]=2;P.Explored[I]=1;}
    }
   }
  }
 }
 for(int32 I=0;I<TerrainCellCount;++I)if(Fog.Cells[I]==0)Fog.Cells[I]=P.Explored[I];
 return Fog;
}

bool ARTSWorld::VisibleInFog(int32 ViewerSlot,const FRTSFog& Fog,const FRTSEntity& E) const {
 if(E.Carrier||E.HP<=0)return false;
 if(Allied(ViewerSlot,E.Owner)||Allied(ViewerSlot,E.DisguisedOwner)||(E.Owner>=0&&SharesEnemyVision(ViewerSlot)))return true;
 // A building can block the ray to its terrain cell center. Entity sight
 // ignores the target itself and accounts for the actual position and radius.
 return (!bFogOfWar||Fog.InEntitySightRange(E.Pos))&&CanSee(ViewerSlot,E);
}

void ARTSWorld::Snapshot() {
 RefreshReligiousStats();
 const int32 Sequence=++SnapshotSequence;
 for(auto& P:Players){auto C=P.Controller.Get();if(!C)continue;
  // Snapshot ticks continue after victory, allowing bounded reliable batches
  // to drain without delaying the match-complete snapshot behind a death burst.
  C->FlushDeathEvents(Time,Phase);
  const FRTSFog Fog=ComputeFog(P);
  TArray<FRTSEntityView> View;TArray<int32> Ids;Entities.GetKeys(Ids);Ids.Sort();TSet<int32> VisibleBuildings;
  for(int Id:Ids){auto& E=*Entities[Id];if(!VisibleInFog(P.Slot,Fog,E))continue;
   FRTSEntityView V=E;V.bRemembered=false;V.Status=E.Order;V.AuraRadius=LabanAuraRadius(E);V.Queue.Empty();V.WorkersAssigned=0;V.bStealthed=Allied(P.Slot,E.Owner)&&IsStealthed(E);V.PassengerDefs.Empty();V.PassengerIds.Empty();V.EffectiveRange=EffectiveAttackRange(E);V.EffectiveSight=EffectiveSight(E);V.MinimumRange=EffectiveMinimumAttackRange(E);V.SplashRadius=AttackSplashRadius(E);V.AttackRate=D(E).Interval/FMath::Max(.01f,AttackSpeedMultiplier(E));V.EffectiveAttack=AttackStrength(E);V.DamageModifiers=DamageModifierText(E);V.SpiritRate=ReligiousGeneration(E);V.ConversionProgress=E.Order==TEXT("convert")?FMath::Clamp(E.Channel/5.f,0.f,1.f):0;V.ProstrateRemaining=D(E).Kind==TEXT("ammonite")?FMath::Max(0.f,E.BuffUntil-Time):0;
   if(D(E).bBuilding){V.Status=E.Progress<1?TEXT("foundation"):E.Jobs.IsEmpty()?TEXT("idle"):E.PauseUntil>Time?TEXT("paused"):FRTSCatalog::Get().Def(E.Jobs[0].Type).Kind==TEXT("age_option")?TEXT("advancing"):FRTSCatalog::Get().Def(E.Jobs[0].Type).bUpgrade?TEXT("researching"):TEXT("training");}
   for(auto& WK:Entities)if(RTS::IsWorker(D(*WK.Value))&&WK.Value->WorkTarget==E.Id&&(Allied(P.Slot,WK.Value->Owner)||CanSee(P.Slot,*WK.Value)))V.WorkersAssigned++;
   if(E.Owner==P.Slot){if(E.WorkTarget>0)V.Target=E.WorkTarget;for(auto& J:E.Jobs)V.Queue.Add(J.Type);V.QueueProgress=E.Jobs.Num()?1-E.Jobs[0].Remaining/FMath::Max(.001f,E.Jobs[0].Total):0;for(int32 Passenger:E.Passengers)if(auto Unit=Find(Passenger)){V.PassengerIds.Add(Passenger);V.PassengerDefs.Add(Unit->Def);}}
   else{V.Cargo=FVector::ZeroVector;V.Target=0;V.bRaidRallySet=false;V.RaidRallyPoint=FVector2D::ZeroVector;V.RaidRallyTarget=0;V.bRallySet=false;V.RallyTarget=0;V.RallyPoint=FVector2D::ZeroVector;V.bEconomicRallySet=false;V.EconomicRallyPoint=FVector2D::ZeroVector;V.EconomicRallyTarget=0;V.GalavantRoute.Empty();V.QueueProgress=0;V.ResourceRate=FVector::ZeroVector;V.SpiritRate=V.CementRate=0;if(!Allied(P.Slot,E.Owner)){V.ResourceEvent=FVector::ZeroVector;V.ResourceEventTime=-100;V.SpiritEvent=V.CementEvent=0;}}
   if(Allied(P.Slot,E.DisguisedOwner)){V.Owner=E.DisguisedOwner;V.Status=TEXT("idle");V.Target=0;V.bStealthed=false;V.ResourceEvent=FVector::ZeroVector;V.ResourceEventTime=-100;V.SpiritEvent=V.CementEvent=0;if(E.DisguisedMaxHP>0){V.MaxHP=E.DisguisedMaxHP;V.HP=E.HP/FMath::Max(1.f,E.MaxHP)*V.MaxHP;}}
   if(D(E).bBuilding||D(E).Id==TEXT("tree")||D(E).Id==TEXT("ore")||D(E).Kind==TEXT("sheep")){VisibleBuildings.Add(Id);P.KnownBuildings.Add(Id,V);}View.Add(V);
  }
  TArray<int32> Forgotten;for(auto& Known:P.KnownBuildings){if(VisibleBuildings.Contains(Known.Key))continue;auto& V=Known.Value;
   // Only a newly visible cell may disprove a remembered building. No unseen
   // health, ownership, construction, deletion, or resource updates are copied.
   if(Fog.Cells[FogIndex(V.Pos)]==2||(Allied(P.Slot,V.Owner)&&!Find(Known.Key))){Forgotten.Add(Known.Key);continue;}
   FRTSEntityView Memory=V;Memory.bRemembered=true;Memory.AuraRadius=0;Memory.bRaidRallySet=false;Memory.RaidRallyPoint=FVector2D::ZeroVector;Memory.RaidRallyTarget=0;Memory.Status=TEXT("remembered");Memory.bAttacking=false;Memory.AttackEventTime=-100;Memory.ConversionProgress=Memory.ProstrateRemaining=0;Memory.ResourceRate=FVector::ZeroVector;Memory.SpiritRate=Memory.CementRate=Memory.CementEvent=0;Memory.bRallySet=false;Memory.Queue.Empty();Memory.QueueProgress=0;Memory.Cargo=FVector::ZeroVector;Memory.Target=0;Memory.WorkersAssigned=0;Memory.ResourceEvent=FVector::ZeroVector;Memory.ResourceEventTime=-100;Memory.SpiritEvent=0;Memory.bEconomicRallySet=false;Memory.EconomicRallyPoint=FVector2D::ZeroVector;Memory.EconomicRallyTarget=0;Memory.GalavantRoute.Empty();View.Add(Memory);
  }for(int Id:Forgotten)P.KnownBuildings.Remove(Id);
  TArray<FRTSPlayerView> PV;for(auto& Other:Players){FRTSPlayerView V=Other;V.LobbyResources=StartingResources;V.LobbyFruitfulness=Fruitfulness;V.LobbySpeed=GameSpeed;V.bLobbyFog=bFogOfWar;if(!Allied(P.Slot,Other.Slot)){V.Resources=FVector::ZeroVector;V.Cement=0;V.Upgrades.Empty();V.HeroChoices.Empty();V.Population=0;V.Capacity=0;}else{for(const auto& KV:Entities)if(KV.Value->DisguisedOwner==Other.Slot)V.Population+=D(*KV.Value).Pop;}if(Other.Slot!=P.Slot){V.ReligiousUnits=0;V.ReligiousStructures=0;V.ReligionTooltip.Empty();}PV.Add(V);}
  C->SendSnapshot(View,PV,Fog.Cells,Time,Phase,Map,Winner,P.Slot,Sequence);
 }
}
