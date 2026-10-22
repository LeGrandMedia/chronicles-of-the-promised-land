#include "RTSV08UITests.h"
#include "RTSController.h"
#include "RTSHUD.h"

bool VerifyRTSV08Minimap(ARTSController* C,ARTSHUD* HUD,FString& Failure) {
 if(!C||!HUD){Failure=TEXT("Minimap regression needs a controller and rendered HUD.");return false;}
 const FVector2D Samples[]={FVector2D(0,0),FVector2D(0,160),FVector2D(160,0),FVector2D(160,160),FVector2D(80,80),FVector2D(20,35),FVector2D(140,125),FVector2D(80,0),FVector2D(0,80)};
 for(FVector2D World:Samples){FVector2D Pixel,RoundTrip;
  if(!HUD->MinimapProject(World,Pixel)||!HUD->MinimapPoint(Pixel,RoundTrip)||FVector2D::Distance(World,RoundTrip)>.001){Failure=FString::Printf(TEXT("Rendered minimap inverse failed at %.1f,%.1f; rotation %.3f."),World.X,World.Y,C->CameraRotation);return false;}
 }
 FVector2D Center,North;HUD->MinimapProject(FVector2D(80,80),Center);HUD->MinimapProject(FVector2D(80,0),North);
 const FVector2D Actual=(North-Center).GetSafeNormal(),Expected=C->RotateMap(FVector2D(0,-1));
 if(FVector2D::Distance(Actual,Expected)>.001){Failure=TEXT("Minimap terrain/entity projection does not follow camera rotation.");return false;}
 // A point just beyond the world boundary must never become a camera order,
 // even while that point lies in the square frame around a rotated map.
 FVector2D Outside,Unused;HUD->MinimapProject(FVector2D(-1,80),Outside);
 if(HUD->MinimapPoint(Outside,Unused)){Failure=TEXT("Minimap accepted an out-of-world location in its rotated frame.");return false;}
 return true;
}
