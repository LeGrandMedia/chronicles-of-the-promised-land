#include "RTSMapTests.h"
#include "RTSWorld.h"
#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSHUD.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace {
void SaveMapReport(const TSharedPtr<FJsonObject>& Report,const TCHAR* Name){
 const FString Folder=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Folder,true);
 FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Folder/Name));
}
void MapPlayers(ARTSWorld& W,ARTSController* C=nullptr){
 W.Phase=0;W.Players.Empty();W.bFogOfWar=false;W.StartingResources=2;
 for(int32 I=0;I<4;++I){auto& P=W.Players.AddDefaulted_GetRef();P.Slot=I;P.Team=I%2;P.bConnected=true;P.bReady=true;P.Faction=I==2?TEXT("gadianton"):I==3?TEXT("coriantumr"):TEXT("nephites");P.Name=FString::Printf(TEXT("Commander %d"),I+1);if(I==0)P.Controller=C;}
}
}
bool RTSRunMapTests(ARTSWorld& W){
 TArray<TSharedPtr<FJsonValue>> Rows;int32 Failed=0,Checks=0;TSet<uint32> Signatures;
 for(int32 Map=0;Map<RTS::MapCount();++Map){
  MapPlayers(W);W.Start(nullptr,Map);TArray<FString> Errors;
  auto Check=[&](bool Ok,const FString& What){++Checks;if(!Ok){++Failed;Errors.Add(What);UE_LOG(LogTemp,Error,TEXT("RTS_MAP_FAIL %s: %s"),RTS::MapInfo(Map).Id,*What);}};
  uint32 Signature=2166136261u;int32 Water=0,Rock=0;bool Mirrored=true;
  for(int Y=0;Y<80;++Y)for(int X=0;X<80;++X){const FVector2D P(X*2+1,Y*2+1);const int32 Mask=int32(RTS::WaterAt(P,Map))+2*int32(RTS::RockAt(P,Map));Water+=Mask==1;Rock+=(Mask&2)!=0;Signature=(Signature^Mask)*16777619u;
   for(auto Reflected:{FVector2D(160-P.X,P.Y),FVector2D(P.X,160-P.Y)})Mirrored&=(RTS::WaterAt(Reflected,Map)==RTS::WaterAt(P,Map)&&RTS::RockAt(Reflected,Map)==RTS::RockAt(P,Map));
  }
  Check(!Signatures.Contains(Signature),TEXT("terrain must have a unique layout"));Signatures.Add(Signature);Check(Mirrored,TEXT("terrain must give mirrored starts equal routes"));
  // Real obstacle-aware flood fill at elephant clearance. Cardinal steps avoid
  // diagonal corner-cutting and verify a common reachable land component.
  TArray<uint8> Open,Reached;Open.Init(0,6400);Reached.Init(0,6400);
  for(int Y=0;Y<80;++Y)for(int X=0;X<80;++X)Open[Y*80+X]=W.Walkable(FVector2D(X*2+1,Y*2+1),1.05f);
  int32 Seed=INDEX_NONE;for(int I=0;I<6400;++I)if(Open[I]&&FVector2D::Distance(FVector2D((I%80)*2+1,(I/80)*2+1),FVector2D(30,30))<8){Seed=I;break;}
  TArray<int32> Queue;if(Seed!=INDEX_NONE){Queue.Add(Seed);Reached[Seed]=1;}
  for(int Read=0;Read<Queue.Num();++Read){const int I=Queue[Read],X=I%80,Y=I/80;for(auto Step:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)}){const int NX=X+Step.X,NY=Y+Step.Y,J=NY*80+NX;if(NX<0||NY<0||NX>=80||NY>=80||!Open[J]||Reached[J])continue;const FVector2D Mid(X*2+1+Step.X,Y*2+1+Step.Y);if(!W.Walkable(Mid,1.05f))continue;Reached[J]=1;Queue.Add(J);}}
  auto NearReached=[&](FVector2D P,double Radius){const int CX=FMath::Clamp(int(P.X/2),0,79),CY=FMath::Clamp(int(P.Y/2),0,79),R=FMath::CeilToInt(Radius/2)+1;for(int Y=FMath::Max(0,CY-R);Y<=FMath::Min(79,CY+R);++Y)for(int X=FMath::Max(0,CX-R);X<=FMath::Min(79,CX+R);++X)if(Reached[Y*80+X]&&FVector2D::Distance(P,FVector2D(X*2+1,Y*2+1))<Radius)return true;return false;};
  Check(Seed!=INDEX_NONE,TEXT("first start has a clear exit"));
  for(const FVector2D Start:{FVector2D(24,24),FVector2D(136,136),FVector2D(24,136),FVector2D(136,24)})Check(NearReached(Start,9),TEXT("all four starts connect with elephant clearance"));
  int32 Resources=0,Unreachable=0;TArray<int32> LocalFood={0,0,0,0},LocalOre={0,0,0,0},LocalTrees={0,0,0,0};
  const FVector2D Starts[]={{24,24},{136,136},{24,136},{136,24}};
  for(const auto& Pair:W.Entities){const auto& E=*Pair.Value;const auto& D=W.D(E);if(D.Kind!=TEXT("resource"))continue;++Resources;
   if(!NearReached(E.Pos,D.Radius+4))++Unreachable;
   Check(!RTS::WaterAt(E.Pos,Map)&&!RTS::RockAt(E.Pos,Map),TEXT("resource placed on clear land: ")+D.Id);
   for(int32 S=0;S<4;++S)if(FVector2D::Distance(E.Pos,Starts[S])<21){if(D.Id==TEXT("tree"))++LocalTrees[S];if(D.Id==TEXT("ore"))++LocalOre[S];if(D.Id==TEXT("berries")||D.Id==TEXT("wild_game"))++LocalFood[S];}
  }
  Check(Unreachable==0,FString::Printf(TEXT("all resources accessible; %d isolated"),Unreachable));
  for(int32 S=0;S<4;++S)Check(LocalTrees[S]>=9&&LocalOre[S]>=1&&LocalFood[S]>=4,TEXT("each start has guaranteed timber, ore and food"));
  // Check actual spawned formations, not the placement formula: every starting
  // worker should have equal access to the same local supplies in every slot.
  TArray<TSharedPtr<FJsonValue>> OpeningRows;TArray<FVector> ReferenceDistances;
  for(int32 S=0;S<4;++S){
   TArray<FRTSEntity*> Workers;
   for(auto& KV:W.Entities)if(KV.Value->Owner==S&&W.D(*KV.Value).Kind==TEXT("worker"))Workers.Add(KV.Value.Get());
   Workers.Sort([](const FRTSEntity& A,const FRTSEntity& B){return A.Id<B.Id;});
   Check(Workers.Num()==6,TEXT("six initial laborers in each slot"));
   FVector Sum=FVector::ZeroVector;TArray<FVector> Distances;
   for(const auto* Worker:Workers){
    FVector Nearest(10000);const FRTSEntity* Berry=nullptr;
    for(const auto& KV:W.Entities){const auto& R=*KV.Value;if(R.Owner>=0)continue;const auto& Id=W.D(R).Id;const double Dist=FVector2D::Distance(Worker->Pos,R.Pos);
     if(Id==TEXT("berries")&&Dist<Nearest.X){Nearest.X=Dist;Berry=&R;}
     if(Id==TEXT("tree"))Nearest.Y=FMath::Min(Nearest.Y,Dist);
     if(Id==TEXT("ore"))Nearest.Z=FMath::Min(Nearest.Z,Dist);
    }
    Check(W.Walkable(Worker->Pos,W.D(*Worker).Radius,Worker->Id),TEXT("starting laborer clears terrain and structures"));
    bool Clear=Berry!=nullptr;
    if(Berry)for(int I=1;I<=20;++I)Clear&=W.Walkable(FMath::Lerp(Worker->Pos,Berry->Pos,I/20.f),W.D(*Worker).Radius,Worker->Id);
    Check(Clear,TEXT("starting laborer has a clear first-berry approach"));
    Distances.Add(Nearest);Sum+=Nearest;
   }
   if(S==0)ReferenceDistances=Distances;
   bool Equal=Distances.Num()==ReferenceDistances.Num();
   for(int I=0;I<FMath::Min(Distances.Num(),ReferenceDistances.Num());++I)Equal&=Distances[I].Equals(ReferenceDistances[I],.001);
   Check(Equal,TEXT("all slots have equal per-worker berry, timber and ore distances"));
   auto Opening=MakeShared<FJsonObject>();Opening->SetNumberField(TEXT("slot"),S);
   if(!Workers.IsEmpty()){Opening->SetNumberField(TEXT("mean_berry_distance"),Sum.X/Workers.Num());Opening->SetNumberField(TEXT("mean_tree_distance"),Sum.Y/Workers.Num());Opening->SetNumberField(TEXT("mean_ore_distance"),Sum.Z/Workers.Num());}
   OpeningRows.Add(MakeShared<FJsonValueObject>(Opening));
  }
  for(auto& KV:W.Entities)if(W.D(*KV.Value).Id==TEXT("curelom"))Check(W.Walkable(KV.Value->Pos,W.D(*KV.Value).Radius,KV.Key),TEXT("starting Jaredite beast clears terrain and structures"));
  int32 Docks=0;bool Launched=false;
  if(Water>0){const auto& DockDef=FRTSCatalog::Get().Def(TEXT("place_of_many_waters"));
   for(int Y=8;Y<152&&!Launched;Y+=2)for(int X=8;X<152&&!Launched;X+=2){const FVector2D P(X,Y);if(!RTS::DockSite(P,DockDef.Radius,Map)||!W.Walkable(P,DockDef.Radius)||!NearReached(P,DockDef.Radius+4))continue;
    ++Docks;auto& Player=W.Players[0];Player.Age=4;Player.Resources=FVector(10000);auto* Builder=W.Spawn(TEXT("industrious_laborer"),0,FVector2D(24,31));W.Build(*Builder,DockDef.Id,P,nullptr);
    FRTSEntity* Dock=nullptr;for(auto& KV:W.Entities)if(W.D(*KV.Value).Id==DockDef.Id)Dock=KV.Value.Get();
    if(!Dock)continue;Dock->Progress=1;Dock->MaxHP=DockDef.HP;Dock->HP=Dock->MaxHP;W.Queue(*Dock,TEXT("hagoths_vessel"),nullptr);W.Production(*Dock,1000);
    for(const auto& KV:W.Entities)if(W.D(*KV.Value).Kind==TEXT("ship")){Launched=W.Walkable(KV.Value->Pos,W.D(*KV.Value).Radius,KV.Key,true);}
   }
   Check(Docks>0&&Launched,TEXT("legal coastal dock builds and launches a ship into water"));
  }
  auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("map"),RTS::MapInfo(Map).Name);Row->SetNumberField(TEXT("index"),Map);Row->SetArrayField(TEXT("opening_distances"),OpeningRows);Row->SetNumberField(TEXT("resources"),Resources);Row->SetNumberField(TEXT("unreachable_resources"),Unreachable);Row->SetNumberField(TEXT("reachable_elephant_cells"),Queue.Num());Row->SetNumberField(TEXT("water_cells"),Water);Row->SetNumberField(TEXT("rock_cells"),Rock);Row->SetBoolField(TEXT("ship_launched"),Launched);Row->SetBoolField(TEXT("passed"),Errors.IsEmpty());TArray<TSharedPtr<FJsonValue>> Messages;for(auto S:Errors)Messages.Add(MakeShared<FJsonValueString>(S));Row->SetArrayField(TEXT("failures"),Messages);Rows.Add(MakeShared<FJsonValueObject>(Row));
 }
 auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),Failed==0);Report->SetNumberField(TEXT("checks"),Checks);Report->SetNumberField(TEXT("failed"),Failed);Report->SetArrayField(TEXT("maps"),Rows);SaveMapReport(Report,TEXT("v09-maps.json"));return Failed==0;
}

void RTSRunMapCapture(ARTSController& C){
 static const bool Enabled=FParse::Param(FCommandLine::Get(),TEXT("RTSV09MapCapture"));if(!Enabled||!C.Me())return;
 static int32 Index=-1,Stage=0,Frames=0;static TArray<TSharedPtr<FJsonValue>> Captures;static bool Failed=false;
 auto* Mode=C.GetWorld()->GetAuthGameMode<ARTSGameMode>();if(!Mode||!Mode->Simulation)return;auto& W=*Mode->Simulation;
 W.SetActorTickEnabled(false);C.bMenu=false;C.bPauseMenu=false;C.CameraCenter=FVector2D(80,80);C.Zoom=.55f;C.CameraRotation=Index%2?PI/4:0;
 if(++Frames<12)return;Frames=0;
 if(Stage==0){
  if(++Index>=RTS::MapCount()+5){auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("passed"),!Failed);Report->SetArrayField(TEXT("captures"),Captures);SaveMapReport(Report,TEXT("v09-map-captures.json"));FPlatformMisc::RequestExitWithStatus(Failed,Failed?1:0);Stage=3;return;}
  MapPlayers(W,&C);if(Index<RTS::MapCount())W.Start(nullptr,Index);else{W.Phase=0;W.Map=FMath::Min((Index-RTS::MapCount())*3,RTS::MapCount()-1);W.Snapshot();C.LobbyMapPage=Index-RTS::MapCount();}Stage=1;return;
 }
 if(Stage==1){
  const FString Name=FString::Printf(TEXT("v09-map-%02d.png"),Index);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("screenshot"),Name);Row->SetNumberField(TEXT("index"),Index);Row->SetStringField(TEXT("name"),Index<RTS::MapCount()?RTS::MapInfo(Index).Name:TEXT("Lobby map selection"));Captures.Add(MakeShared<FJsonValueObject>(Row));
  if(Index>=RTS::MapCount()){
   auto* HUD=Cast<ARTSHUD>(C.GetHUD());bool Selected=true;
   if(HUD){
    const int32 Page=Index-RTS::MapCount();
    for(int32 Offset:{1,2,0}){const int32 Target=Page*3+Offset;FBox2D Bounds(EForceInit::ForceInit);
     Selected&=HUD->FindButtonBounds(TEXT("map"),FString::FromInt(Target),Bounds)&&HUD->Click(Bounds.GetCenter())&&W.Map==Target&&C.Map==Target;
     for(const auto& P:W.Players)Selected&=P.bBot||!P.bReady;
    }
    FBox2D Arrow(EForceInit::ForceInit);
    Selected&=HUD->FindButtonBounds(TEXT("map_page"),TEXT("1"),Arrow)&&HUD->Click(Arrow.GetCenter())&&C.LobbyMapPage==(Page+1)%5;
    Selected&=HUD->FindButtonBounds(TEXT("map_page"),TEXT("-1"),Arrow)&&HUD->Click(Arrow.GetCenter())&&C.LobbyMapPage==Page;
   }
   Failed|=!HUD||!Selected||HUD->CountButtons(TEXT("map"))!=3||HUD->CountButtons(TEXT("map_page"))!=2;
   Row->SetBoolField(TEXT("map_buttons_paging_and_readiness"),Selected);
  }
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("Screenshots");IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(Folder/Name),false,false);Stage=2;return;
 }
 if(Stage==2){Failed|=!IFileManager::Get().FileExists(*(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Captures.Last()->AsObject()->GetStringField(TEXT("screenshot"))));Stage=0;}
}
