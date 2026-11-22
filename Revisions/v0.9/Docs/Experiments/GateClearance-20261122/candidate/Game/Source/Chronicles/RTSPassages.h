#pragma once

#include "RTSTypes.h"

namespace RTS {
constexpr uint8 GateJoinNegative=1;
constexpr uint8 GateJoinPositive=2;

// A join removes the pier on the specified side of the first gate's local X axis.
inline uint8 AdjacentGateSide(FVector2D GatePos,double GateHeading,FVector2D NeighborPos,double NeighborHeading,double Radius) {
 constexpr double PositionTolerance=.15;
 const double ExpectedDistance=2*Radius+.15;
 const FVector2D Offset=NeighborPos-GatePos;
 const double DistanceSquared=Offset.SizeSquared();
 if(DistanceSquared<FMath::Square(ExpectedDistance-PositionTolerance)||
    DistanceSquared>FMath::Square(ExpectedDistance+PositionTolerance))return 0;

 const FVector2D Axis(FMath::Cos(GateHeading),FMath::Sin(GateHeading));
 const FVector2D NeighborAxis(FMath::Cos(NeighborHeading),FMath::Sin(NeighborHeading));
 if(FMath::Abs(FVector2D::DotProduct(Axis,NeighborAxis))<.999||
    FMath::Abs(FVector2D::CrossProduct(Axis,Offset))>PositionTolerance||
    FMath::Abs(FVector2D::CrossProduct(NeighborAxis,Offset))>PositionTolerance)return 0;
 return FVector2D::DotProduct(Axis,Offset)<0?GateJoinNegative:GateJoinPositive;
}

// Keeping live and remembered gates separate avoids changing a live opening
// based on the last known state of a hidden neighbor.
inline uint8 AdjacentGateSide(const FRTSEntityView& Gate,const FRTSEntityView& Neighbor,double Radius) {
 if(!Gate.bGate||!Neighbor.bGate||Gate.Progress<1||Neighbor.Progress<1||Gate.Id==Neighbor.Id||
    Gate.Owner!=Neighbor.Owner||Gate.Def!=Neighbor.Def||Gate.bRemembered!=Neighbor.bRemembered)return 0;
 return AdjacentGateSide(Gate.Pos,Gate.Heading,Neighbor.Pos,Neighbor.Heading,Radius);
}
}
