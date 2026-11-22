#include "RTSEntityVisibilityTests.h"
#include "RTSController.h"
#include "RTSWorld.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

bool RTSRunEntityVisibilityTests(ARTSWorld& World) {
 auto* Viewer=World.GetWorld()->SpawnActor<ARTSController>();
 auto* Enemy=World.GetWorld()->SpawnActor<ARTSController>();
 if(!Viewer||!Enemy)return false;
 Viewer->ServerSlot=0;
 Enemy->ServerSlot=1;

 TArray<TSharedPtr<FJsonValue>> Results;
 int32 Failed=0;
 const auto Check=[&](const TCHAR* Name,bool bPassed){
  auto Result=MakeShared<FJsonObject>();
  Result->SetStringField(TEXT("name"),Name);
  Result->SetBoolField(TEXT("passed"),bPassed);
  Results.Add(MakeShared<FJsonValueObject>(Result));
  if(!bPassed){++Failed;UE_LOG(LogTemp,Error,TEXT("RTS_ENTITY_VISIBILITY_FAIL %s"),Name);}
 };
 const auto Reset=[&]{
  World.Entities.Reset();
  World.Obstacles.Reset();
  World.ObstacleCount=-1;
  World.Players.Reset();
  World.Players.SetNum(2);
  World.Map=0;
  World.Phase=1;
  World.Time=100;
  World.bFogOfWar=true;
  World.bSandbox=true;
  for(int32 Slot=0;Slot<2;++Slot){
   auto& Player=World.Players[Slot];
   Player.Slot=Player.Team=Slot;
   Player.Controller=Slot==0?Viewer:Enemy;
   Player.Faction=Slot==0?TEXT("nephites"):TEXT("gadianton");
  }
 };
 const auto TerrainAt=[](const TArray<uint8>& Cells,FVector2D Point){
  constexpr double CellSize=RTS::MapSize/RTS::FogSize;
  const int32 X=int32(Point.X/CellSize),Y=int32(Point.Y/CellSize);
  const int32 Index=Y*RTS::FogSize+X;
  return Cells.IsValidIndex(Index)?Cells[Index]:uint8(255);
 };

 Reset();
 auto* Scout=World.Spawn(TEXT("cimeter_bearer"),0,{40,40});
 auto* Store=World.Spawn(TEXT("storehouse_of_provisions"),1,{48,40});
 Store->Heading=.63f;
 const int32 StoreId=Store->Id;
 const float SeenHP=Store->HP;
 const FVector2D SeenAt=Store->Pos;
 const auto Fog=World.ComputeFog(World.Players[0]);
 Check(TEXT("off_grid_rotated_store_blocks_terrain_sample_only"),
  !World.LOS(Scout->Pos,{48.75,41.25},Scout->Id)&&TerrainAt(Fog.Cells,SeenAt)==0&&
  World.CanSee(0,*Store)&&World.VisibleInFog(0,Fog,*Store));

 Store->Cargo=FVector(11,12,13);
 Store->Target=Scout->Id;
 Store->ResourceRate=FVector(4,5,6);
 Store->bRallySet=true;
 Store->RallyPoint={54,43};
 auto& Job=Store->Jobs.AddDefaulted_GetRef();
 Job.Type=TEXT("industrious_laborer");
 Job.Total=10;
 Job.Remaining=7;
 World.Snapshot();
 const auto* Live=Viewer->FindEntity(StoreId);
 const auto* Owned=Enemy->FindEntity(StoreId);
 Check(TEXT("snapshot_delivers_live_store_in_dark_terrain_cell"),Live&&!Live->bRemembered&&
  Live->HP==SeenHP&&Live->Heading==Store->Heading&&TerrainAt(Viewer->Fog,SeenAt)==0);
 Check(TEXT("snapshot_keeps_enemy_cargo_queue_and_rally_private"),Live&&Owned&&
  Live->Cargo.IsNearlyZero()&&Live->Target==0&&Live->Queue.IsEmpty()&&
  Live->ResourceRate.IsNearlyZero()&&!Live->bRallySet&&Live->RallyPoint.IsNearlyZero()&&
  Owned->Cargo==Store->Cargo&&Owned->Target==Scout->Id&&Owned->Queue.Num()==1&&Owned->bRallySet);

 World.Command(Viewer,TEXT("context"),{Scout->Id},SeenAt,StoreId,TEXT(""));
 Check(TEXT("live_off_grid_store_accepts_attack_order"),Scout->Order==TEXT("attack")&&Scout->Target==StoreId);

 const auto* Wall=World.Spawn(TEXT("works_of_defense"),1,{44,40});
 const int32 WallId=Wall->Id;
 Store->HP*=.5f;
 const auto WallFog=World.ComputeFog(World.Players[0]);
 World.Snapshot();
 const auto* Memory=Viewer->FindEntity(StoreId);
 Check(TEXT("separate_wall_blocks_live_store_and_freezes_memory"),WallFog.InEntitySightRange(SeenAt)&&
  !World.CanSee(0,*Store)&&!World.VisibleInFog(0,WallFog,*Store)&&Memory&&Memory->bRemembered&&
  Memory->HP==SeenHP&&Memory->Pos==SeenAt&&Memory->Cargo.IsNearlyZero()&&Memory->Queue.IsEmpty());
 World.Command(Viewer,TEXT("stop"),{Scout->Id},Scout->Pos,0,TEXT(""));
 World.Command(Viewer,TEXT("context"),{Scout->Id},SeenAt,StoreId,TEXT(""));
 Check(TEXT("hidden_store_rejects_attack_order"),Scout->Order==TEXT("idle")&&Scout->Target==0);

 World.Kill(StoreId);
 World.Snapshot();
 const auto* Unscouted=Viewer->FindEntity(StoreId);
 Check(TEXT("unseen_destruction_does_not_remove_remembered_store"),Unscouted&&Unscouted->bRemembered&&
  Unscouted->HP==SeenHP&&World.Players[0].KnownBuildings.Contains(StoreId));
 World.Kill(WallId);
 World.Snapshot();
 Check(TEXT("rescouted_terrain_clears_destroyed_store_memory"),TerrainAt(Viewer->Fog,SeenAt)==2&&
  !Viewer->FindEntity(StoreId)&&!World.Players[0].KnownBuildings.Contains(StoreId));

 Reset();
 World.Spawn(TEXT("wilderness_spy"),0,{72,55});
 auto* RockTarget=World.Spawn(TEXT("storehouse_of_provisions"),1,{88,55});
 const auto RockFog=World.ComputeFog(World.Players[0]);
 World.Snapshot();
 Check(TEXT("separate_rock_blocks_entity_even_in_sight_range"),RockFog.InEntitySightRange(RockTarget->Pos)&&
  !World.CanSee(0,*RockTarget)&&!World.VisibleInFog(0,RockFog,*RockTarget)&&!Viewer->FindEntity(RockTarget->Id));

 Reset();
 World.Spawn(TEXT("cimeter_bearer"),0,{42.5,40});
 auto* HiddenHQ=World.Spawn(TEXT("seat_of_the_people"),1,{48,40});
 World.Time+=6;
 const auto HiddenFog=World.ComputeFog(World.Players[0]);
 World.Snapshot();
 Check(TEXT("undetected_stealth_structure_stays_private"),World.IsStealthed(*HiddenHQ)&&
  HiddenFog.InEntitySightRange(HiddenHQ->Pos)&&!World.VisibleInFog(0,HiddenFog,*HiddenHQ)&&!Viewer->FindEntity(HiddenHQ->Id));
 World.bFogOfWar=false;
 World.Snapshot();
 Check(TEXT("disabled_terrain_fog_preserves_stealth_detection_rule"),!Viewer->FindEntity(HiddenHQ->Id));
 World.bFogOfWar=true;
 World.Spawn(TEXT("wilderness_spy"),0,{43,37});
 const auto DetectorFog=World.ComputeFog(World.Players[0]);
 World.Snapshot();
 const auto* Detected=Viewer->FindEntity(HiddenHQ->Id);
 Check(TEXT("nearby_spy_detects_concealed_structure"),World.IsStealthed(*HiddenHQ)&&
  World.VisibleInFog(0,DetectorFog,*HiddenHQ)&&Detected&&!Detected->bRemembered);

 Reset();
 World.Spawn(TEXT("cimeter_bearer"),0,{40,40});
 auto* Carrier=World.Spawn(TEXT("tents_and_habitations"),1,{46,40});
 auto* Passenger=World.Spawn(TEXT("gadianton_robber"),1,Carrier->Pos);
 Carrier->Passengers.Add(Passenger->Id);
 Passenger->Carrier=Carrier->Id;
 const auto CarrierFog=World.ComputeFog(World.Players[0]);
 World.Snapshot();
 const auto* ForeignCarrier=Viewer->FindEntity(Carrier->Id);
 const auto* OwnCarrier=Enemy->FindEntity(Carrier->Id);
 Check(TEXT("visible_carrier_keeps_passenger_identity_owner_only"),
  World.VisibleInFog(0,CarrierFog,*Carrier)&&!World.VisibleInFog(0,CarrierFog,*Passenger)&&
  !Viewer->FindEntity(Passenger->Id)&&!Enemy->FindEntity(Passenger->Id)&&
  ForeignCarrier&&ForeignCarrier->PassengerIds.IsEmpty()&&ForeignCarrier->PassengerDefs.IsEmpty()&&
  OwnCarrier&&OwnCarrier->PassengerIds.Contains(Passenger->Id)&&OwnCarrier->PassengerDefs.Contains(Passenger->Def));

 Reset();
 Scout=World.Spawn(TEXT("cimeter_bearer"),0,{46.25,40});
 auto* LargeField=World.Spawn(TEXT("fields_of_grain"),1,{60.01,40});
 const auto EdgeFog=World.ComputeFog(World.Players[0]);
 Check(TEXT("large_footprint_reaches_across_coarse_cell_boundary"),TerrainAt(EdgeFog.Cells,LargeField->Pos)==0&&
  FVector2D::Distance(Scout->Pos,LargeField->Pos)>World.EffectiveSight(*Scout)&&
  World.CanSee(0,*LargeField)&&World.VisibleInFog(0,EdgeFog,*LargeField));
 LargeField->Pos.X=60.25;
 Check(TEXT("coarse_candidates_still_obey_exact_sight_range"),EdgeFog.InEntitySightRange(LargeField->Pos)&&
  !World.VisibleInFog(0,EdgeFog,*LargeField));

 // Compare the mask with an exact reference on a mixed near/far army layout.
 // Timing is diagnostic evidence because the test machine can also be busy.
 Reset();
 for(int32 I=0;I<64;++I)World.Spawn(TEXT("bowman"),0,{30.+I%8,30.+I/8});
 TArray<const FRTSEntity*> Targets;
 for(int32 I=0;I<64;++I)Targets.Add(World.Spawn(TEXT("cimeter_bearer"),1,{40.+I%8,34.+I/8}));
 for(int32 I=0;I<512;++I)Targets.Add(World.Spawn(TEXT("cimeter_bearer"),1,{100.+I%32,100.+I/32}));
 const double StartFogTime=FPlatformTime::Seconds();
 const auto ArmyFog=World.ComputeFog(World.Players[0]);
 const double FogMs=(FPlatformTime::Seconds()-StartFogTime)*1000.;
 int32 RangeRejected=0;
 for(const auto* Target:Targets)RangeRejected+=!ArmyFog.InEntitySightRange(Target->Pos);
 Check(TEXT("range_grid_rejects_remote_army_before_exact_visibility"),RangeRejected==512);
 const double StartExactTime=FPlatformTime::Seconds();
 TBitArray<> ExactVisible(false,Targets.Num());
 for(int32 I=0;I<Targets.Num();++I)ExactVisible[I]=World.CanSee(0,*Targets[I]);
 const double ExactMs=(FPlatformTime::Seconds()-StartExactTime)*1000.;
 const double StartFilteredTime=FPlatformTime::Seconds();
 bool bAgree=true;
 for(int32 I=0;I<Targets.Num();++I)bAgree&=World.VisibleInFog(0,ArmyFog,*Targets[I])==ExactVisible[I];
 const double FilteredMs=(FPlatformTime::Seconds()-StartFilteredTime)*1000.;
 Check(TEXT("range_grid_matches_exact_visibility_on_army_layout"),bAgree);

 auto Report=MakeShared<FJsonObject>();
 Report->SetBoolField(TEXT("passed"),Failed==0);
 Report->SetNumberField(TEXT("checks"),Results.Num());
 Report->SetNumberField(TEXT("failed"),Failed);
 Report->SetNumberField(TEXT("army_targets"),Targets.Num());
 Report->SetNumberField(TEXT("army_range_rejected"),RangeRejected);
 Report->SetNumberField(TEXT("army_terrain_and_range_ms"),FogMs);
 Report->SetNumberField(TEXT("army_exact_filter_ms"),ExactMs);
 Report->SetNumberField(TEXT("army_range_and_exact_filter_ms"),FilteredMs);
 Report->SetArrayField(TEXT("results"),Results);
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("Tests");
 IFileManager::Get().MakeDirectory(*Folder,true);
 FString Json;
 FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
 const bool bSaved=FFileHelper::SaveStringToFile(Json,*(Folder/TEXT("v09-entity-visibility.json")));
 UE_LOG(LogTemp,Display,TEXT("RTS_ENTITY_VISIBILITY checks=%d failed=%d range_rejected=%d/%d saved=%d"),
  Results.Num(),Failed,RangeRejected,Targets.Num(),bSaved);
 return Failed==0&&bSaved;
}
