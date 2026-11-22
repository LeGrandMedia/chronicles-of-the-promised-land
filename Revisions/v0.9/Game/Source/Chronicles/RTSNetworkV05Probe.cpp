#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSWorld.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool RunRTSNetworkV05Probe(ARTSController* C) {
 if(!FParse::Param(FCommandLine::Get(),TEXT("RTSNetV05")))return true;
 struct FState {bool Seeded=false,Sent=false,Route=false,Rally=false,Counts=false,Busy=false,Privacy=false;};
 static TMap<ARTSController*,FState> States;auto& S=States.FindOrAdd(C);
 if(C->Phase==1&&C->LocalSlot==0&&C->MatchTime>=10&&!S.Seeded){S.Seeded=true;
  if(auto GM=Cast<ARTSGameMode>(C->GetWorld()->GetAuthGameMode()))if(auto W=GM->Simulation){
   TArray<int32> Seats;for(const auto& KV:W->Entities)if(W->D(*KV.Value).Id==TEXT("seat_of_the_people"))Seats.Add(KV.Key);
   for(int Id:Seats)if(auto Seat=W->Find(Id)){const int Slot=Seat->Owner;W->Players[Slot].Age=2;W->Players[Slot].Resources+=FVector(500);W->Spawn(TEXT("place_of_trade"),Slot,Seat->Pos+FVector2D(-7,0));W->Spawn(Slot==0?TEXT("secret_combination_lodge"):TEXT("temple_of_the_land"),Slot,Seat->Pos+FVector2D(0,-8));W->Spawn(Slot==0?TEXT("lawyer"):Slot==2?TEXT("drunken_concubine"):TEXT("priestcrafter"),Slot,Seat->Pos+FVector2D(3,-5));}
  }
 }
 if(C->Phase==1&&C->MatchTime>10.5f&&C->MatchTime<29){
  const FRTSEntityView* Market=nullptr;const FRTSEntityView* Seat=nullptr;bool Private=true;
  for(const auto& E:C->Entities){const auto& Def=FRTSCatalog::Get().Def(E.Def);if(E.Owner==C->LocalSlot){if(Def.Id==TEXT("place_of_trade"))Market=&E;if(Def.Id==TEXT("seat_of_the_people"))Seat=&E;}else Private&=!E.bEconomicRallySet&&E.EconomicRallyTarget==0&&E.GalavantRoute.IsEmpty();}
  for(const auto& P:C->Players)if(P.Slot!=C->LocalSlot)Private&=P.ReligiousUnits==0&&P.ReligiousStructures==0&&P.ReligionTooltip.IsEmpty();
  if(Market&&Seat&&!S.Sent){S.Sent=true;C->ServerOrder(TEXT("market_galavant"),{Market->Id},Market->Pos+FVector2D(0,5),0,TEXT(""));C->ServerOrder(TEXT("market_galavant"),{Market->Id},Market->Pos+FVector2D(5,5),0,TEXT(""),true);C->ServerOrder(TEXT("rally_economic"),{Seat->Id},Seat->Pos+FVector2D(-5,5),0,TEXT(""));C->ServerOrder(TEXT("rally"),{Seat->Id},Seat->Pos+FVector2D(5,-5),0,TEXT(""));C->ServerOrder(TEXT("train"),{Seat->Id},{0,0},0,TEXT("industrious_laborer"));}
  S.Route|=Market&&Market->GalavantRoute.Num()==2;
  S.Rally|=Seat&&Seat->bEconomicRallySet&&Seat->bRallySet&&!Seat->EconomicRallyPoint.Equals(Seat->RallyPoint,.1f);
  S.Busy|=Seat&&!Seat->Queue.IsEmpty()&&Seat->Status==TEXT("training");
  const auto Me=C->Me();S.Counts|=Me&&Me->ReligiousUnits>0&&Me->ReligiousStructures>0&&!Me->ReligionTooltip.IsEmpty();S.Privacy|=S.Route&&S.Rally&&Private;
 }
 const bool Passed=S.Route&&S.Rally&&S.Counts&&S.Busy&&S.Privacy;
 if(C->Phase==2)UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_V05 slot=%d route=%d split_rally=%d religion=%d busy=%d privacy=%d passed=%d"),C->LocalSlot,S.Route,S.Rally,S.Counts,S.Busy,S.Privacy,Passed);
 return Passed;
}
