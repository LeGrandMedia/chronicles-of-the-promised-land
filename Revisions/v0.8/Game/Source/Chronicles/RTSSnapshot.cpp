#include "RTSController.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Compression.h"

// Full authoritative snapshots are independently compressed. Fog and player
// metadata are written once, and only complete snapshots replace client state.
// This keeps large armies below Unreal's reliable-bunch buffer without making
// command delivery unreliable or exposing entities outside the player's view.
void ARTSController::SendSnapshot(const TArray<FRTSEntityView>& NewEntities,const TArray<FRTSPlayerView>& NewPlayers,const TArray<uint8>& NewFog,float Time,int32 NewPhase,int32 NewMap,int32 WinningTeam,int32 Slot,int32 Sequence) {
 if(IsLocalController()){ClientState_Implementation(NewEntities,NewPlayers,NewFog,Time,NewPhase,NewMap,WinningTeam,Slot);return;}
 TArray<uint8> Raw;FMemoryWriter Writer(Raw);uint8 Version=3;Writer<<Version<<Time<<NewPhase<<NewMap<<WinningTeam<<Slot;
 int32 Count=NewEntities.Num();Writer<<Count;for(auto Entity:NewEntities)FRTSEntityView::StaticStruct()->SerializeBin(Writer,&Entity);
 Count=NewPlayers.Num();Writer<<Count;for(auto PlayerView:NewPlayers)FRTSPlayerView::StaticStruct()->SerializeBin(Writer,&PlayerView);
 TArray<uint8> Mask=NewFog;Writer<<Mask;Writer.Close();if(Raw.Num()>8*1024*1024)return;
 int32 Size=FCompression::CompressMemoryBound(NAME_Zlib,Raw.Num());TArray<uint8> Packed;Packed.SetNumUninitialized(Size);
 if(!FCompression::CompressMemory(NAME_Zlib,Packed.GetData(),Size,Raw.GetData(),Raw.Num(),COMPRESS_BiasSpeed))return;Packed.SetNum(Size);
 const int32 Chunk=16384,Parts=FMath::DivideAndRoundUp(Size,Chunk);if(Parts>128)return;
 for(int32 Part=0;Part<Parts;++Part){TArray<uint8> Bytes;Bytes.Append(Packed.GetData()+Part*Chunk,FMath::Min(Chunk,Size-Part*Chunk));ClientPackedSnapshot(Bytes,Raw.Num(),Sequence,Part,Parts);}
}
void ARTSController::ClientPackedSnapshot_Implementation(const TArray<uint8>& Bytes,int32 RawBytes,int32 Sequence,int32 PartIndex,int32 PartCount) {
 if(Sequence<=DeliveredPackedSequence||Sequence<PackedSequence||PartCount<1||PartCount>128||PartIndex<0||PartIndex>=PartCount||Bytes.IsEmpty()||Bytes.Num()>16384||RawBytes<=0||RawBytes>8*1024*1024)return;
 if(Sequence!=PackedSequence){PackedSequence=Sequence;PackedExpected=PartCount;PackedRawSize=RawBytes;PackedReceived=0;PackedParts.Empty();PackedParts.SetNum(PartCount);}
 if(PartCount!=PackedExpected||RawBytes!=PackedRawSize||!PackedParts[PartIndex].IsEmpty())return;
 PackedParts[PartIndex]=Bytes;if(++PackedReceived!=PackedExpected)return;
 TArray<uint8> Packed;for(const auto& Part:PackedParts)Packed.Append(Part);PackedParts.Empty();
 TArray<uint8> Raw;Raw.SetNumUninitialized(RawBytes);if(!FCompression::UncompressMemory(NAME_Zlib,Raw.GetData(),RawBytes,Packed.GetData(),Packed.Num()))return;
 FMemoryReader Reader(Raw);uint8 Version=0;float NewTime=0;int32 NewPhase=0,NewMap=0,WinningTeam=-1,Slot=-1,Count=0;Reader<<Version<<NewTime<<NewPhase<<NewMap<<WinningTeam<<Slot;if(Version!=3)return;
 Reader<<Count;if(Count<0||Count>16384)return;TArray<FRTSEntityView> NewEntities;NewEntities.SetNum(Count);for(auto& Entity:NewEntities)FRTSEntityView::StaticStruct()->SerializeBin(Reader,&Entity);
 Reader<<Count;if(Count<0||Count>4)return;TArray<FRTSPlayerView> NewPlayers;NewPlayers.SetNum(Count);for(auto& PlayerView:NewPlayers)FRTSPlayerView::StaticStruct()->SerializeBin(Reader,&PlayerView);
 TArray<uint8> NewFog;Reader<<NewFog;if(Reader.IsError()||NewFog.Num()!=RTS::FogSize*RTS::FogSize||!FMath::IsFinite(NewTime)||Slot<0||Slot>=NewPlayers.Num())return;
 DeliveredPackedSequence=Sequence;LargestSnapshotBytes=FMath::Max(LargestSnapshotBytes,RawBytes);LargestPackedBytes=FMath::Max(LargestPackedBytes,Packed.Num());
 NewEntities.Sort([](const FRTSEntityView& A,const FRTSEntityView& B){return A.Id<B.Id;});ClientState_Implementation(NewEntities,NewPlayers,NewFog,NewTime,NewPhase,NewMap,WinningTeam,Slot);
}
