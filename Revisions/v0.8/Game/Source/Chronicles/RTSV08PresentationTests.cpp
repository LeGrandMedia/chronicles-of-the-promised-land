#include "RTSWorld.h"
#include "RTSController.h"
#include "RTSVisualAssets.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/BitWriter.h"
#include "Serialization/BitReader.h"

bool RTSRunV08PresentationTests(ARTSWorld& W){
 int Passed=0,Failed=0;TArray<TSharedPtr<FJsonValue>> Results;
 auto Check=[&](FString Name,bool OK){auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("name"),Name);R->SetBoolField(TEXT("passed"),OK);Results.Add(MakeShared<FJsonValueObject>(R));if(OK)Passed++;else{Failed++;UE_LOG(LogTemp,Error,TEXT("RTS_V08_PRESENTATION_FAIL %s"),*Name);}};
 auto& Cat=FRTSCatalog::Get();auto& Art=FRTSVisualAssets::Get();
 for(const auto& D:Cat.Defs)if(!D.bUpgrade&&D.Kind!=TEXT("resource")){
  const FString State=D.bBuilding?TEXT("destroy"):TEXT("death");
  const auto A=Art.MeasurePose(D,State,0.f,7),B=Art.MeasurePose(D,State,.65f,7),End=Art.MeasurePose(D,State,2.f,7),Still=Art.MeasurePose(D,State,3.f,7);
  bool Finite=true;for(int Frame=0;Frame<=32;Frame++)for(int Y=0;Y<=6;Y++)for(int X=0;X<=4;X++){
   const FVector2D V=Art.PoseVertex(D,State,FVector2D(X/4.,Y/6.),Frame*.1f,7);
   Finite&=FMath::IsFinite(V.X)&&FMath::IsFinite(V.Y)&&FMath::Abs(V.X)<3&&FMath::Abs(V.Y)<3;
  }
  Check(TEXT("animation.non_looping_finite_pose.")+D.Id,A.PoseHash!=B.PoseHash&&End.PoseHash==Still.PoseHash&&Finite&&B.Triangles==48);
 }
 for(const auto& F:Cat.Factions){
  FRTSVisualMetadata HQ,Home;bool Correct=Art.GetMetadata(TEXT("seat_of_the_people"),HQ,F.Id)&&Art.GetMetadata(TEXT("tents_and_habitations"),Home,F.Id)&&Home.Size.X<HQ.Size.X*.5f;
  for(const FString& Id:F.Buildings){FRTSVisualMetadata Other;if(Id!=TEXT("seat_of_the_people")&&!(F.Id==TEXT("laban")&&Id==TEXT("temple_of_the_land"))&&Art.GetMetadata(Id,Other,F.Id))Correct&=Other.Size.X<HQ.Size.X&&Other.Size.Y<HQ.Size.Y;}
  Check(TEXT("art.headquarters_and_housing_hierarchy.")+F.Id,Correct);
 }
 FRTSVisualMetadata Palace,HQ;Check(TEXT("art.great_spacious_building_larger_than_headquarters"),Art.GetMetadata(TEXT("temple_of_the_land"),Palace,TEXT("laban"))&&Art.GetMetadata(TEXT("seat_of_the_people"),HQ,TEXT("laban"))&&Palace.Size.X>HQ.Size.X&&Palace.Size.Y>HQ.Size.Y);
 Check(TEXT("ages.chief_chariot_fifty"),Cat.RequiredAge(Cat.Def(TEXT("chief_captain")),TEXT("laban"))==3&&Cat.RequiredAge(Cat.Def(TEXT("kings_chariot")),TEXT("laban"))==3&&Cat.RequiredAge(Cat.Def(TEXT("one_of_the_fifty")),TEXT("laban"))==4);
 Check(TEXT("ages.great_spacious_override_preserves_other_temples"),Cat.RequiredAge(Cat.Def(TEXT("temple_of_the_land")),TEXT("laban"))==4&&Cat.RequiredAge(Cat.Def(TEXT("temple_of_the_land")),TEXT("nephites"))==2&&Cat.RequiredAge(Cat.Def(TEXT("temple_of_the_land")),TEXT("lamanites"))==2);
 Check(TEXT("laban.winery_is_concubine_producer"),Cat.Def(TEXT("drunken_concubine")).Producer==TEXT("winery"));
 Check(TEXT("teancum.active_ability_has_catalog_binding"),!Cat.Def(TEXT("teancum")).Ability.IsEmpty());
 Check(TEXT("laban.gates_are_available"),Cat.Faction(TEXT("laban")).Upgrades.Contains(TEXT("gate_segment_upgrade")));

 auto Own=W.GetWorld()->SpawnActor<ARTSController>();auto Enemy=W.GetWorld()->SpawnActor<ARTSController>();
 W.Entities.Empty();W.Players.Empty();W.NextId=1;W.Map=0;W.Time=100;W.Phase=1;W.bTesting=true;W.bFogOfWar=true;W.ObstacleCount=-1;W.Obstacles.Empty();
 for(int I=0;I<2;I++){FRTSPlayer P;P.Slot=I;P.Team=I;P.Faction=TEXT("lamanites");P.Favor=50;P.Age=4;P.Explored.Init(0,4096);P.Controller=I?Enemy:Own;W.Players.Add(P);}
 Own->ServerSlot=Own->LocalSlot=0;Enemy->ServerSlot=Enemy->LocalSlot=1;
 W.Snapshot(); // Establish the match before testing deaths; match entry clears prior-match effects.
 auto Unit=W.Spawn(TEXT("cimeter_bearer"),0,{30,30});const int DeadId=Unit->Id;W.Kill(DeadId);
 Check(TEXT("death.authority_removes_collision_and_delivers_only_entitled_event"),!W.Find(DeadId)&&Own->DeathAnimations.Num()==1&&Enemy->DeathAnimations.IsEmpty());
 Own->ClientDeath_Implementation(Own->DeathAnimations[0].Entity);Check(TEXT("death.duplicate_event_is_idempotent"),Own->DeathAnimations.Num()==1);
 auto Scout=W.Spawn(TEXT("wilderness_spy"),1,{40,40});auto Building=W.Spawn(TEXT("tents_and_habitations"),0,{43,40});const int BuildingId=Building->Id;W.Kill(BuildingId);
 Check(TEXT("destruction.visible_enemy_receives_building_event"),Enemy->DeathAnimations.Num()==1&&Enemy->DeathAnimations[0].Entity.Id==BuildingId&&Enemy->DeathAnimations[0].Entity.Status==TEXT("destroy")&&!W.Find(BuildingId));
 const int Before=Own->DeathAnimations.Num();Unit=W.Spawn(TEXT("cimeter_bearer"),0,{42,42});Unit->Carrier=Scout->Id;W.Snapshot();
 Check(TEXT("death.boarding_or_fog_snapshot_does_not_create_death"),Own->DeathAnimations.Num()==Before);

 // Measure the actual custom wire serializer, including a full batch. Death
 // payloads contain no strings, cargo, routes, jobs, or garrison arrays.
 FRTSDeathEvent Compact;Compact.Id=9001;Compact.Owner=1;Compact.Def=Cat.Index(TEXT("cimeter_bearer"));Compact.Pos={19.25,33.5};Compact.Heading=2.25f;Compact.Progress=.7f;Compact.bGate=true;
 FBitWriter Wire(512,true);bool WireOK=false;Compact.NetSerialize(Wire,nullptr,WireOK);
 FBitReader Read(Wire.GetData(),Wire.GetNumBits());FRTSDeathEvent RoundTrip;bool ReadOK=false;RoundTrip.NetSerialize(Read,nullptr,ReadOK);
 Check(TEXT("death.compact_wire_round_trip_and_225_bit_event"),WireOK&&ReadOK&&Wire.GetNumBits()==225&&RoundTrip.Id==Compact.Id&&RoundTrip.Owner==Compact.Owner&&RoundTrip.Def==Compact.Def&&RoundTrip.Pos.Equals(Compact.Pos,.0001)&&FMath::IsNearlyEqual(RoundTrip.Heading,Compact.Heading)&&FMath::IsNearlyEqual(RoundTrip.Progress,Compact.Progress)&&RoundTrip.bGate);
 FBitWriter BatchWire(8192,true);int32 BatchCount=RTSDeath::EventsPerBatch;BatchWire<<BatchCount;bool BatchOK=true;
 for(int I=0;I<BatchCount;I++){FRTSDeathEvent Item=Compact;Item.Id+=I;bool OK=false;Item.NetSerialize(BatchWire,nullptr,OK);BatchOK&=OK;}
 Check(TEXT("death.sixteen_event_batch_payload_under_one_kib"),BatchOK&&BatchWire.GetNumBytes()<1024&&RTSDeath::EventsPerBatch==16);

 Own->DeathAnimations.Empty();FRTSEntityView Private;Private.Id=9002;Private.Owner=0;Private.Def=Compact.Def;Private.Cargo=FVector(9876);Private.Target=33;Private.Queue.Add(TEXT("private_research"));Private.GalavantRoute.Add({100,100});Private.PassengerIds.Add(42);Private.ResourceRate=FVector(9);Private.bRaidRallySet=true;Private.RaidRallyTarget=44;Private.Status=TEXT("gather");
 Own->ClientDeath_Implementation(Private);const auto& Clean=Own->DeathAnimations[0].Entity;
 Check(TEXT("death.presentation_projection_discards_private_state"),Clean.Status==TEXT("death")&&Clean.HP==0&&Clean.Cargo.IsNearlyZero()&&Clean.Target==0&&Clean.Queue.IsEmpty()&&Clean.GalavantRoute.IsEmpty()&&Clean.PassengerIds.IsEmpty()&&Clean.ResourceRate.IsNearlyZero()&&!Clean.bRaidRallySet&&Clean.RaidRallyTarget==0);
 const int CleanCount=Own->DeathAnimations.Num();TArray<FRTSDeathEvent> Oversized;for(int I=0;I<=RTSDeath::EventsPerBatch;I++){auto Item=Compact;Item.Id+=100+I;Oversized.Add(Item);}Own->ClientDeaths_Implementation(Oversized);
 Check(TEXT("death.oversized_incoming_batch_rejected"),Own->DeathAnimations.Num()==CleanCount);

 // A mass resignation may create hundreds of visible deaths in one command.
 // Exercise the same queue/drain methods as a remote connection, with a local
 // receiver so batch limits, ordering and eventual phase-two drain are observable.
 Own->DeathAnimations.Empty();Own->PendingDeaths.Empty();Own->DeathQueueMatchTime=-1;Own->DroppedDeathEvents=0;
 for(int I=0;I<600;I++){auto Item=Compact;Item.Id=1000+I;Own->EnqueueDeathEvent(Item,100);if(I==255)Check(TEXT("death.queue_keeps_all_events_until_presentation_capacity"),Own->PendingDeaths.Num()==256&&Own->DroppedDeathEvents==0);}
 Check(TEXT("death.mass_casualty_queue_bounded_and_drops_oldest_only"),Own->PendingDeaths.Num()==256&&Own->DroppedDeathEvents==344&&Own->PendingDeaths[0].Id==1344&&Own->PendingDeaths.Last().Id==1599);
 auto Duplicate=Compact;Duplicate.Id=1599;Own->EnqueueDeathEvent(Duplicate,100);Check(TEXT("death.duplicate_pending_event_does_not_drop_another_casualty"),Own->PendingDeaths.Num()==256&&Own->DroppedDeathEvents==344);
 const int Flushed=Own->FlushDeathEvents(100,2);
 Check(TEXT("death.phase_two_flush_bounded_to_four_small_batches"),Flushed==64&&Own->LastDeathBatchesFlushed==4&&Own->LastDeathEventsFlushed==64&&Own->PendingDeaths.Num()==192&&Own->DeathAnimations.Num()==64);
 for(int I=0;I<3;I++)Own->FlushDeathEvents(100,2);
 Check(TEXT("death.phase_two_eventually_drains_fifo_without_exceeding_cap"),Own->PendingDeaths.IsEmpty()&&Own->DeathAnimations.Num()==256&&Own->DeathAnimations[0].Entity.Id==1344&&Own->DeathAnimations.Last().Entity.Id==1599);
 Own->DeathAnimations.Empty();auto Stale=Compact;Stale.Id=2000;Own->EnqueueDeathEvent(Stale,100);Own->FlushDeathEvents(0,1);
 Check(TEXT("death.match_clock_reset_discards_old_pending_events"),Own->PendingDeaths.IsEmpty()&&Own->DeathAnimations.IsEmpty()&&Own->LastDeathEventsFlushed==0);
 Own->EnqueueDeathEvent(Stale,1);Own->FlushDeathEvents(1,0);Check(TEXT("death.lobby_return_discards_old_pending_events"),Own->PendingDeaths.IsEmpty()&&Own->DeathAnimations.IsEmpty());
 Stale.Id=2001;Own->EnqueueDeathEvent(Stale,W.Time);W.Phase=2;W.Snapshot();
 Check(TEXT("death.completed_match_snapshot_still_flushes_pending_events"),Own->PendingDeaths.IsEmpty()&&Own->DeathAnimations.Num()==1&&Own->DeathAnimations[0].Entity.Id==2001);
 Own->Destroy();Enemy->Destroy();W.Entities.Empty();W.Players.Empty();W.Phase=0;
 auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("passed"),Passed);Report->SetNumberField(TEXT("failed"),Failed);Report->SetArrayField(TEXT("assertions"),Results);
 FString JSON;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&JSON));const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("v08-presentation-tests.json")));
 return Failed==0;
}
