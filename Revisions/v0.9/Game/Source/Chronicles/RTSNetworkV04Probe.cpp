#include "RTSController.h"
#include "RTSGameMode.h"
#include "RTSWorld.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Opt-in mixed-faction fixture. No fixture runs in an ordinary match.
bool RunRTSNetworkV04Probe(ARTSController* C) {
 if(!FParse::Param(FCommandLine::Get(),TEXT("RTSNetV04")))return true;
 struct FState {bool Seeded=false,Disguise=false,Passengers=false,Stealth=false,Cement=false,Vision=false;};
 static TMap<ARTSController*,FState> States;auto& S=States.FindOrAdd(C);
 if(C->Phase==1&&C->LocalSlot==0&&C->MatchTime>=60&&!S.Seeded){S.Seeded=true;
  if(auto GM=Cast<ARTSGameMode>(C->GetWorld()->GetAuthGameMode()))if(auto W=GM->Simulation)if(W->Players.Num()==4){
   W->Players[0].Age=2;W->Players[0].Favor=90;W->Players[0].Upgrades.AddUnique(TEXT("network_of_secrets"));
   W->Players[3].Cement=123;auto Victim=W->Spawn(TEXT("industrious_laborer"),1,{120,80});if(Victim)W->ConvertUnit(*Victim,0,TEXT(""));
   auto Carrier=W->Spawn(TEXT("elephant"),3,{124,80});auto Passenger=W->Spawn(TEXT("industrious_laborer"),3,{125,80});
   if(Carrier&&Passenger)W->Command(W->Players[3].Controller.Get(),TEXT("context"),{Passenger->Id},Carrier->Pos,Carrier->Id,TEXT(""));
   W->Spawn(TEXT("industrious_laborer"),2,{120,84});auto Hidden=W->Spawn(TEXT("gadianton_robber"),0,{118,78});if(Hidden){Hidden->LastMove=W->Time-10;Hidden->RevealedUntil=0;Hidden->Order=TEXT("hold");}
  }
 }
 if(C->Phase==1&&C->MatchTime>60.5f&&C->MatchTime<79){
  const bool FormerTeam=C->LocalSlot%2==1;const FRTSEntityView* Victim=nullptr;const FRTSEntityView* Carrier=nullptr;const FRTSEntityView* Hidden=nullptr;
  for(const auto& E:C->Entities){const auto& D=FRTSCatalog::Get().Def(E.Def);if(D.Id==TEXT("industrious_laborer")&&E.Pos.Equals({120,80},.1f))Victim=&E;if(D.Id==TEXT("elephant"))Carrier=&E;if(D.Id==TEXT("gadianton_robber")&&E.Pos.Equals({118,78},.1f))Hidden=&E;}
  S.Disguise|=Victim&&Victim->Owner==(FormerTeam?1:0);
  S.Passengers|=Carrier&&(C->LocalSlot==3?(Carrier->PassengerIds.Num()==1&&Carrier->PassengerDefs.Num()==1):Carrier->PassengerIds.IsEmpty()&&Carrier->PassengerDefs.IsEmpty());
  S.Stealth|=FormerTeam?Hidden==nullptr:Hidden&&Hidden->bStealthed;
  S.Cement|=C->Players.IsValidIndex(3)&&(FormerTeam?C->Players[3].Cement>123:C->Players[3].Cement==0);
  // Only Gadianton directly borrows enemy sight; its teammate must not inherit it.
  const FVector2D EnemyBase(136,136);const int Cell=54*64+54;S.Vision|=C->LocalSlot==0?(C->Fog.IsValidIndex(Cell)&&C->Fog[Cell]==2):true;
 }
 const bool Passed=S.Disguise&&S.Passengers&&S.Stealth&&S.Cement&&S.Vision;
 if(C->Phase==2)UE_LOG(LogTemp,Display,TEXT("RTS_NETWORK_V04 slot=%d disguise=%d passengers=%d stealth=%d cement=%d vision=%d passed=%d"),C->LocalSlot,S.Disguise,S.Passengers,S.Stealth,S.Cement,S.Vision,Passed);
 return Passed;
}
