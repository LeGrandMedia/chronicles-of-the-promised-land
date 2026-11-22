#include "RTSController.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

bool ARTSController::PlaySelectionVoice(){
 if(!GetWorld())return false;
 const FRTSDef* Speaker=nullptr;int SpeakerId=MAX_int32;
 for(int Id:Selection)if(const auto* E=FindEntity(Id))if(E->Owner==LocalSlot&&!E->bRemembered){const auto& D=FRTSCatalog::Get().Def(E->Def);if(!D.SelectLine.IsEmpty()&&Id<SpeakerId){Speaker=&D;SpeakerId=Id;}}
 if(!Speaker)return false;
 const float Now=GetWorld()->GetRealTimeSeconds();const FName Key(TEXT("voice.select"));
 if(const float* Last=LastCueTimes.Find(Key))if(Now-*Last<2)return true;
 LastCueTimes.Add(Key,Now);ShowNotice(Speaker->SelectLine);
 if(!bSoundEnabled||GetNetMode()==NM_DedicatedServer||Speaker->SelectAudio.IsEmpty())return true;
 // Optional local PCM voice clips; originals may replace these files without code changes.
 static TMap<FString,TArray<uint8>> VoicePCM;
 if(!VoicePCM.Contains(Speaker->SelectAudio)){
  TArray<uint8> PCM,File;const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("Data"));FString Path=FPaths::ConvertRelativePathToFull(Root/Speaker->SelectAudio);FPaths::CollapseRelativeDirectories(Path);
  if(FPaths::IsRelative(Speaker->SelectAudio)&&FPaths::IsUnderDirectory(Path,Root)&&FFileHelper::LoadFileToArray(File,*Path)&&File.Num()>44&&FMemory::Memcmp(File.GetData(),"RIFF",4)==0){
   bool Valid=false;for(int32 Offset=12;Offset+8<=File.Num();){uint32 Length=0;FMemory::Memcpy(&Length,File.GetData()+Offset+4,4);if(Length>uint32(File.Num()-Offset-8))break;
    if(FMemory::Memcmp(File.GetData()+Offset,"fmt ",4)==0&&Length>=16){uint16 Format=0,Channels=0,Bits=0;uint32 Rate=0;FMemory::Memcpy(&Format,File.GetData()+Offset+8,2);FMemory::Memcpy(&Channels,File.GetData()+Offset+10,2);FMemory::Memcpy(&Rate,File.GetData()+Offset+12,4);FMemory::Memcpy(&Bits,File.GetData()+Offset+22,2);Valid=Format==1&&Channels==1&&Rate==24000&&Bits==16;}
    if(Valid&&FMemory::Memcmp(File.GetData()+Offset,"data",4)==0){PCM.Append(File.GetData()+Offset+8,Length);break;}Offset+=8+Length+(Length&1);
   }
  }VoicePCM.Add(Speaker->SelectAudio,MoveTemp(PCM));
 }
 const auto& PCM=VoicePCM.FindChecked(Speaker->SelectAudio);if(PCM.IsEmpty())return true;
 auto Wave=NewObject<USoundWaveProcedural>(this);Wave->SetSampleRate(24000);Wave->NumChannels=1;Wave->Duration=PCM.Num()/48000.f;Wave->bLooping=false;Wave->SoundGroup=SOUNDGROUP_Voice;Wave->QueueAudio(PCM.GetData(),PCM.Num());
 if(auto Audio=UGameplayStatics::SpawnSound2D(this,Wave,.75f)){PlayingCues.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& A){return !A.IsValid();});PlayingCues.Add(Audio);const TWeakObjectPtr<UAudioComponent> Weak=Audio;FTimerHandle Handle;GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([Weak](){if(Weak.IsValid())Weak->Stop();}),Wave->Duration+.1f,false);}
 return true;
}
