#include "RTSWorld.h"
#include "RTSController.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Opt-in fixtures. Run only against an isolated test world, never a live match.
bool RunRTSEconomyV03Tests(ARTSWorld* W) {
 if(!W||!W->GetWorld())return false;
 auto C=W->GetWorld()->SpawnActor<ARTSController>();if(!C)return false;C->ServerSlot=0;
 TArray<TSharedPtr<FJsonValue>> Assertions;int Passed=0,Failed=0;
 auto Check=[&](const TCHAR* Name,bool Good){auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("name"),Name);A->SetBoolField(TEXT("passed"),Good);Assertions.Add(MakeShared<FJsonValueObject>(A));if(Good)Passed++;else{Failed++;UE_LOG(LogTemp,Error,TEXT("RTS_ECONOMY_V03_FAIL %s"),Name);}};
 auto Reset=[&](FString Faction=TEXT("nephites")){W->Entities.Empty();W->Players.Empty();W->NextId=1;W->Time=0;W->Phase=1;W->Map=0;W->Winner=-1;W->bSandbox=true;W->bTesting=true;W->NavigationCount=-1;W->NavigationTime=-100;W->ObstacleCount=-1;W->Obstacles.Empty();W->bDispatchingQueuedOrder=false;FRTSPlayer P;P.Slot=0;P.Team=0;P.Age=3;P.Faction=Faction;P.Favor=0;P.Controller=C;P.Resources=FVector(10000);W->Players.Add(P);};
 auto Spawn=[&](const FString& Type,FVector2D Pos,int Owner=0){return W->Spawn(Type,Owner,Pos);};
 auto Assign=[&](FRTSEntity* Worker,FRTSEntity* Site){W->ResetOrder(*Worker,TEXT("gather"));Worker->WorkTarget=Worker->Target=Site->Id;Worker->Pos=W->WorkPosition(*Worker,*Site);};

 Reset();auto Farm=Spawn(TEXT("fields_of_grain"),{35,35});auto Worker=Spawn(TEXT("industrious_laborer"),{30,35});Assign(Worker,Farm);W->Economy(*Worker,1);
 Check(TEXT("food.base_rate_increased_to_1_2"),FMath::IsNearlyEqual(float(Worker->Cargo.X),1.2f,.001f));
 auto Ore=Spawn(TEXT("ore"),{65,35},-1);Worker->Cargo=FVector::ZeroVector;Assign(Worker,Ore);W->Economy(*Worker,1);float NaturalOre=Worker->Cargo.Z;
 auto Hill=Spawn(TEXT("hill_of_ore"),Ore->Pos);Hill->WorkTarget=Ore->Id;Worker->Cargo=FVector::ZeroVector;Assign(Worker,Hill);W->Economy(*Worker,1);
 Check(TEXT("ore.hill_gathers_fifty_percent_faster"),NaturalOre>0&&FMath::IsNearlyEqual(float(Worker->Cargo.Z),NaturalOre*1.5f,.001f));

 Reset();Farm=Spawn(TEXT("fields_of_grain"),{40,40});TArray<FRTSEntity*> Laborers;for(int I=0;I<11;I++)Laborers.Add(Spawn(TEXT("industrious_laborer"),{30.+I,35}));
 bool TenAccepted=true;for(int I=0;I<10;I++){TenAccepted&=W->CanAssignWork(*Laborers[I],*Farm);Assign(Laborers[I],Farm);}Check(TEXT("worksite.accepts_ten_and_rejects_eleventh"),TenAccepted&&!W->CanAssignWork(*Laborers[10],*Farm));
 bool Slots=true;for(int I=0;I<10;I++)for(int J=I+1;J<10;J++)Slots&=FVector2D::Distance(W->WorkPosition(*Laborers[I],*Farm),W->WorkPosition(*Laborers[J],*Farm))>.6f;
 Check(TEXT("pathing.gatherers_have_distinct_walkable_approaches"),Slots&&W->Walkable(W->WorkPosition(*Laborers[0],*Farm),W->D(*Laborers[0]).Radius));
 auto Enemy=Spawn(TEXT("fields_of_grain"),{70,40},-1);Check(TEXT("worksite.rejects_neutral_building"),!W->CanAssignWork(*Laborers[10],*Enemy));

 Reset();auto HQ=Spawn(TEXT("seat_of_the_people"),{20,20});Worker=Spawn(TEXT("industrious_laborer"),{33,30});auto Second=Spawn(TEXT("industrious_laborer"),{34,30});
 W->Command(C,TEXT("build"),{Worker->Id,Second->Id},{36,30},0,TEXT("fields_of_grain"));int Foundations=0;Farm=nullptr;for(auto& KV:W->Entities)if(W->D(*KV.Value).Id==TEXT("fields_of_grain")){Foundations++;Farm=KV.Value.Get();}
 Check(TEXT("build.group_creates_single_foundation_and_assists"),Foundations==1&&Farm&&Worker->Target==Farm->Id&&Second->Target==Farm->Id);
 if(Farm){bool SpeedBound=true;for(int I=0;I<25;I++){FVector2D Before=Worker->Pos;W->Time+=.1f;W->Move(*Worker,W->WorkPosition(*Worker,*Farm),.3f,.1f);SpeedBound&=FVector2D::Distance(Before,Worker->Pos)<=W->D(*Worker).Speed*.1f+.121f;}
  Check(TEXT("build.enclosed_builder_walks_out_without_teleporting"),SpeedBound&&W->Walkable(Worker->Pos,W->D(*Worker).Radius,Worker->Id)&&FVector2D::Distance(Worker->Pos,Farm->Pos)>W->D(*Farm).Radius*.88f+W->D(*Worker).Radius);
 }
 if(Farm){Worker->Pos=Farm->Pos+FVector2D(W->D(*Farm).Radius+.7f,0);Second->Pos=Farm->Pos+FVector2D(0,W->D(*Farm).Radius+.7f);float Before=Farm->Progress;W->Production(*Farm,1);float Two=Farm->Progress-Before;W->ResetOrder(*Second,TEXT("idle"));Before=Farm->Progress;W->Production(*Farm,1);float One=Farm->Progress-Before;Check(TEXT("build.assistants_accelerate_with_diminishing_returns"),Two>One&&Two<2*One);
  Farm->Progress=.9999f;W->Production(*Farm,1);Check(TEXT("build.completed_field_assigns_builder_to_gather"),Worker->Order==TEXT("gather")&&Worker->WorkTarget==Farm->Id);
 }

 Reset();HQ=Spawn(TEXT("seat_of_the_people"),{20,20});Worker=Spawn(TEXT("industrious_laborer"),{30,30});W->Command(C,TEXT("move"),{Worker->Id},{40,30},0,TEXT(""));W->Command(C,TEXT("move"),{Worker->Id},{50,30},0,TEXT(""),true);
 Check(TEXT("orders.shift_preserves_current_order"),Worker->PendingOrders.Num()==1&&Worker->Rally.Equals({40,30},.01f));
 W->ResetOrder(*Worker,TEXT("idle"));W->ProcessQueuedOrder(*Worker);Check(TEXT("orders.queued_move_dispatches_after_completion"),Worker->PendingOrders.IsEmpty()&&Worker->Order==TEXT("move")&&Worker->Rally.Equals({50,30},.01f));
 W->Command(C,TEXT("move"),{Worker->Id},{60,30},0,TEXT(""),true);W->Command(C,TEXT("stop"),{Worker->Id},{0,0},0,TEXT(""));Check(TEXT("orders.fresh_stop_clears_queue"),Worker->PendingOrders.IsEmpty()&&Worker->Order==TEXT("idle"));
 Worker->Cargo=FVector(4,3,2);Worker->Pos=HQ->Pos+FVector2D(W->D(*HQ).Radius+.6f,0);FVector Resources=W->Players[0].Resources;W->Command(C,TEXT("context"),{Worker->Id},HQ->Pos,HQ->Id,TEXT(""));W->Economy(*Worker,.1f);
 Check(TEXT("dropoff.right_click_hq_delivers_selected_cargo"),W->Players[0].Resources.Equals(Resources+FVector(4,3,2),.001f)&&Worker->Cargo.IsNearlyZero());
 for(int I=0;I<3;I++){FRTSJob J;J.Type=FString::FromInt(I);J.Paid=FVector(10);HQ->Jobs.Add(J);}Resources=W->Players[0].Resources;W->Command(C,TEXT("cancel"),{HQ->Id},HQ->Pos,0,TEXT("queue:1"));
 Check(TEXT("queue.cancel_middle_refunds_full_cost"),HQ->Jobs.Num()==2&&HQ->Jobs[1].Type==TEXT("2")&&W->Players[0].Resources.Equals(Resources+FVector(10),.001f));
 W->Command(C,TEXT("cancel"),{HQ->Id},HQ->Pos,0,TEXT("queue:99"));Check(TEXT("queue.invalid_index_is_rejected"),HQ->Jobs.Num()==2);

 Reset(TEXT("coriantumr"));HQ=Spawn(TEXT("seat_of_the_people"),{20,20});auto Flock=Spawn(TEXT("gathering_of_flocks"),{40,40});auto Beast=Spawn(TEXT("cumelom"),{46,40});Beast->HP=Beast->MaxHP-10;auto Sheep=Spawn(TEXT("sheep"),{45,40});Sheep->HomeBuilding=Flock->Id;float Food=W->Players[0].Resources.X;W->EconomyAuras(2);
 Check(TEXT("flocks.sheep_passive_food_and_builder_healing"),FMath::IsNearlyEqual(float(W->Players[0].Resources.X-Food),.6f,.001f)&&FMath::IsNearlyEqual(Beast->HP,Beast->MaxHP-2,.001f));
 W->Players[0].Upgrades.Add(TEXT("curelom_husbandry"));W->Queue(*HQ,TEXT("curelom"),C);Check(TEXT("jaredites.hq_curelom_remains_available_with_flock"),HQ->Jobs.Num()==1&&HQ->Jobs[0].Type==TEXT("curelom"));
 for(int I=1;I<10;I++){auto S=Spawn(TEXT("sheep"),{45.+I*.1,40});S->HomeBuilding=Flock->Id;}W->Queue(*Flock,TEXT("sheep"),C);Check(TEXT("flocks.ten_sheep_cap_includes_alive_animals"),Flock->Jobs.IsEmpty());

 Reset();HQ=Spawn(TEXT("seat_of_the_people"),{20,20});Worker=Spawn(TEXT("industrious_laborer"),{35,35});auto Berries=Spawn(TEXT("berries"),{40,35},-1);Berries->ResourceRemaining=.1f;int BerryId=Berries->Id;Assign(Worker,Berries);W->Economy(*Worker,1);Check(TEXT("resources.depleted_berries_are_removed"),W->Find(BerryId)==nullptr&&Worker->Cargo.X>.09);
 auto Trader=Spawn(TEXT("trader_in_precious_things"),Worker->Pos+FVector2D(1,0));Trader->Order=TEXT("trade");Trader->Target=Worker->Id;Trader->ResourcePulseAt=-10;float Wealth=W->Players[0].Resources.Z;W->Economy(*Trader,.1f);W->Economy(*Trader,.1f);Check(TEXT("trade.visits_laborer_and_generates_precious_things"),W->Players[0].Resources.Z>Wealth);
 W->ResetOrder(*Worker,TEXT("move"));Worker->Rally={60,35};FVector2D Start=Worker->Pos;W->Move(*Worker,Worker->Rally,.35f,.1f);Check(TEXT("pathing.movement_never_teleports"),FVector2D::Distance(Start,Worker->Pos)<=W->D(*Worker).Speed*.1f+.121f);

 // Exercise real group assignment and travel from a compact starting crowd.
 // No gatherer is placed at its work slot or granted a scripted cargo load.
 Reset();HQ=Spawn(TEXT("seat_of_the_people"),{20,20});Farm=Spawn(TEXT("fields_of_grain"),{40,30});Laborers.Empty();TArray<int32> Group;
 for(int I=0;I<10;I++){auto Laborer=Spawn(TEXT("industrious_laborer"),{25.+(I%5)*.35,30.+(I/5)*.35});Laborers.Add(Laborer);Group.Add(Laborer->Id);}
 const float GroupFoodBefore=W->Players[0].Resources.X;W->Command(C,TEXT("gather"),Group,Farm->Pos,Farm->Id,TEXT(""));bool GroupAssigned=true;for(auto Laborer:Laborers)GroupAssigned&=Laborer->WorkTarget==Farm->Id&&Laborer->Order==TEXT("gather");
 for(int I=0;I<900;I++)W->Step(.1f);
 int DeliveredWorkers=0;for(auto Laborer:Laborers){if(Laborer->ResourceEventTime>=0)DeliveredWorkers++;else UE_LOG(LogTemp,Warning,TEXT("RTS_ECONOMY_V03_CROWD worker=%d order=%s pos=%s cargo=%s work=%d drop=%d"),Laborer->Id,*Laborer->Order,*Laborer->Pos.ToString(),*Laborer->Cargo.ToString(),Laborer->WorkTarget,Laborer->DropTarget);}
 Check(TEXT("pathing.ten_clustered_workers_all_gather_and_deliver"),GroupAssigned&&DeliveredWorkers==10&&W->Players[0].Resources.X>=GroupFoodBefore+99.f);

 // Snapshot memory must retain only the last observed structure state. Hidden
 // damage and deletion become observable only when the scout sees its old cell.
 Reset();C->Entities.Empty();C->PackedParts.Empty();C->PackedSequence=-1;C->DeliveredPackedSequence=-1;C->PackedReceived=0;
 FRTSPlayer FogEnemy;FogEnemy.Slot=1;FogEnemy.Team=1;FogEnemy.Faction=TEXT("lamanites");FogEnemy.Favor=0;W->Players.Add(FogEnemy);
 HQ=Spawn(TEXT("seat_of_the_people"),{20,20});auto Scout=Spawn(TEXT("wilderness_spy"),{66,34});auto ScoutedStore=Spawn(TEXT("storehouse_of_provisions"),{60,30},1);const int ScoutedId=ScoutedStore->Id;const float ObservedHP=ScoutedStore->HP;W->Snapshot();
 const bool ClientReceivesSnapshots=!C->Entities.IsEmpty();auto ClientStore=C->FindEntity(ScoutedId);
 Check(TEXT("fog.visible_structure_enters_last_known_snapshot"),W->Players[0].KnownBuildings.Contains(ScoutedId)&&FMath::IsNearlyEqual(W->Players[0].KnownBuildings[ScoutedId].HP,ObservedHP)&&(!ClientReceivesSnapshots||(ClientStore&&!ClientStore->bRemembered)));
 W->Command(C,TEXT("move"),{Scout->Id},{25,35},0,TEXT(""));for(int I=0;I<150;I++)W->Step(.1f);
 ScoutedStore->HP=ObservedHP*.25f;ScoutedStore->ResourceEvent=FVector(999);ScoutedStore->ResourceEventTime=W->Time;W->Snapshot();ClientStore=C->FindEntity(ScoutedId);
 Check(TEXT("fog.hidden_damage_keeps_old_health_and_remembered_icon"),!W->VisiblePoint(0,{61.25,31.25})&&W->Players[0].KnownBuildings.Contains(ScoutedId)&&FMath::IsNearlyEqual(W->Players[0].KnownBuildings[ScoutedId].HP,ObservedHP)&&(!ClientReceivesSnapshots||(ClientStore&&ClientStore->bRemembered&&FMath::IsNearlyEqual(ClientStore->HP,ObservedHP)&&ClientStore->ResourceEvent.IsNearlyZero()&&ClientStore->ResourceEventTime<0)));
 W->Kill(ScoutedId);W->Snapshot();ClientStore=C->FindEntity(ScoutedId);
 Check(TEXT("fog.hidden_deletion_preserves_remembered_structure"),!W->Find(ScoutedId)&&W->Players[0].KnownBuildings.Contains(ScoutedId)&&FMath::IsNearlyEqual(W->Players[0].KnownBuildings[ScoutedId].HP,ObservedHP)&&(!ClientReceivesSnapshots||(ClientStore&&ClientStore->bRemembered&&FMath::IsNearlyEqual(ClientStore->HP,ObservedHP))));
 W->Command(C,TEXT("move"),{Scout->Id},{66,34},0,TEXT(""));for(int I=0;I<150;I++)W->Step(.1f);W->Snapshot();
 Check(TEXT("fog.scout_return_removes_destroyed_structure_memory"),W->VisiblePoint(0,{61.25,31.25})&&!W->Players[0].KnownBuildings.Contains(ScoutedId)&&(!ClientReceivesSnapshots||C->FindEntity(ScoutedId)==nullptr));

 auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("passed"),Passed);Report->SetNumberField(TEXT("failed"),Failed);Report->SetArrayField(TEXT("assertions"),Assertions);FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("economy-v03-tests.json")));C->Destroy();UE_LOG(LogTemp,Display,TEXT("RTS_ECONOMY_V03_TESTS passed=%d failed=%d"),Passed,Failed);return Failed==0;
}
