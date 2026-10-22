#include "RTSPresentation.h"
#include "RTSController.h"
#include "Engine/World.h"

bool URTSViewport::InputChar(FViewport* InViewport,int32 ControllerId,TCHAR Character) {
 if (GetWorld()) {
  if (ARTSController* PC=Cast<ARTSController>(GetWorld()->GetFirstPlayerController())) {
   if (!PC->TextFocus.IsEmpty() || PC->bChat) { PC->TextCharacter(Character); return true; }
  }
 }
 return Super::InputChar(InViewport,ControllerId,Character);
}

void URTSViewport::LostFocus(FViewport* InViewport) {
 Super::LostFocus(InViewport);
 if(GetWorld())if(auto* PC=Cast<ARTSController>(GetWorld()->GetFirstPlayerController()))PC->bDragging=false;
}
