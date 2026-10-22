#include "RTSVisualAssets.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "CanvasItem.h"
#include "ImageUtils.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

namespace {
 FString StringField(const TSharedPtr<FJsonObject>& Object,const TCHAR* Field,const FString& Default=FString()) {
  FString Value;return Object->TryGetStringField(Field,Value)?Value:Default;
 }
 FVector2D PairField(const TSharedPtr<FJsonObject>& Object,const TCHAR* Field,FVector2D Default) {
  const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
  if(Object->TryGetArrayField(Field,Values)&&Values->Num()==2) {
   double X,Y;if((*Values)[0]->TryGetNumber(X)&&(*Values)[1]->TryGetNumber(Y)&&FMath::IsFinite(X)&&FMath::IsFinite(Y))return FVector2D(X,Y);
  }
  return Default;
 }
 FVector4 UVField(const TSharedPtr<FJsonObject>& Object) {
  const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
  if(Object->TryGetArrayField(TEXT("uv"),Values)&&Values->Num()==4) {
   double V[4];for(int32 I=0;I<4;I++)if(!(*Values)[I]->TryGetNumber(V[I])||!FMath::IsFinite(V[I]))return FVector4(0,0,1,1);
   V[0]=FMath::Clamp(V[0],0.,.999);V[1]=FMath::Clamp(V[1],0.,.999);
   V[2]=FMath::Clamp(V[2],.001,1-V[0]);V[3]=FMath::Clamp(V[3],.001,1-V[1]);return FVector4(V[0],V[1],V[2],V[3]);
  }
  return FVector4(0,0,1,1);
 }
}


namespace {
 constexpr int32 PoseColumns=4,PoseRows=6;
 constexpr float AttackPoseSeconds=.55f;
 bool AnimatedUnit(const FRTSDef& Def){return !Def.bBuilding&&!Def.bUpgrade&&Def.Kind!=TEXT("resource");}
 FString RigFor(const FRTSDef& Def){return Def.Kind==TEXT("ship")?TEXT("ship"):Def.Kind==TEXT("chariot")?TEXT("wheeled"):Def.Kind==TEXT("beast")||Def.Kind==TEXT("sheep")||Def.Id==TEXT("wild_game")?TEXT("quadruped"):TEXT("humanoid");}
 float Weight(float Start,float End,float Value){const float T=FMath::Clamp((Value-Start)/(End-Start),0.f,1.f);return T*T*(3-2*T);}
}
float FRTSVisualAssets::WorldScale(const FRTSDef& Def){return AnimatedUnit(Def)?(Def.Kind==TEXT("ship")?.70f:.66f):1.f;}
FVector2D FRTSVisualAssets::PoseVertex(const FRTSDef& Def,const FString& State,FVector2D UV,float Elapsed,float Seed){
 if(!FMath::IsFinite(Elapsed))return UV;
 if(State==TEXT("destroy")){
  const float T=FMath::Clamp(Elapsed/1.3f,0.f,1.f),Fall=T*T*(3-2*T),Top=1-UV.Y;
  return FVector2D(UV.X+(UV.X-.5f)*.18f*Fall+FMath::Sin(Elapsed*23+UV.Y*11+Seed)*.025f*Top*(1-Fall),UV.Y+(.94f-UV.Y)*Fall*.91f);
 }
 if(State==TEXT("death")){
  const float T=FMath::Clamp(Elapsed/1.1f,0.f,1.f),Fall=T*T*(3-2*T),Sign=int32(Seed)%2?1.f:-1.f;
  if(Def.Kind==TEXT("ship"))return FVector2D(UV.X+(1-UV.Y)*.16f*Fall,UV.Y+(.95f-UV.Y)*Fall*.88f);
  if(Def.Kind==TEXT("beast")||Def.Kind==TEXT("sheep")||Def.Kind==TEXT("chariot")||Def.Id==TEXT("wild_game"))return FVector2D(UV.X+Sign*(1-UV.Y)*.19f*Fall,UV.Y+(.91f-UV.Y)*Fall*.65f);
  const FVector2D Offset=UV-FVector2D(.5f,.94f);const float Angle=Sign*Fall*1.30f,C=FMath::Cos(Angle),S=FMath::Sin(Angle);
  return FVector2D(.5f+Offset.X*C-Offset.Y*S,.94f+(Offset.X*S+Offset.Y*C)*(1-.4f*Fall));
 }
 if(!AnimatedUnit(Def))return UV;
 const float X=UV.X,Y=UV.Y,Side=X<.5f?-1.f:1.f,Phase=Elapsed*9.f+Seed*.73f;
 const bool Moving=State==TEXT("move"),Attacking=State==TEXT("attack"),Converting=State==TEXT("convert");
 const bool Working=State==TEXT("build")||State.StartsWith(TEXT("gather"))||State==TEXT("work");
 const float Wave=FMath::Sin(Phase),Counter=FMath::Sin(Phase+PI),Breath=FMath::Sin(Elapsed*2.3f+Seed);
 const float Strike=Attacking?FMath::Sin(FMath::Clamp(Elapsed/AttackPoseSeconds,0.f,1.f)*PI):0;
 FVector2D Delta=FVector2D::ZeroVector;
 if(Def.Kind==TEXT("ship")){
  // The hull stays grounded while the sail flexes and the two banks of oars sweep.
  const float Sail=1-Weight(.28f,.72f,Y),Oars=Weight(.46f,.68f,Y)*(1-Weight(.8f,1.f,Y))*Weight(.12f,.35f,FMath::Abs(X-.5f));
  Delta.X=Sail*(Moving?.025f:.009f)*Wave+(Moving?.04f:0)*Oars*Side*Counter;
  Delta.Y=Sail*.012f*FMath::Sin(Phase+X*3)+(Moving?.018f:0)*Oars*Wave;
 }else if(Def.Kind==TEXT("beast")||Def.Kind==TEXT("sheep")||Def.Kind==TEXT("chariot")){
  const float Legs=Weight(.55f,.95f,Y),Front=Weight(.45f,.9f,X),LegWave=FMath::Sin(Phase+X*PI*3);
  Delta.X=(Moving?.045f:0)*Legs*LegWave+Strike*.045f*Front;
  Delta.Y=(Moving?-.027f*FMath::Max(0.f,LegWave):0)*Legs+(1-Legs)*Breath*.003f;
  // Elephants have a separately weighted trunk and head; the saddle/back is stable.
  Delta.X+=Front*(1-Weight(.7f,1.f,Y))*(Attacking?.045f*Strike:.008f*Wave);
  if(Working)Delta.Y+=Front*.025f*Wave;
 }else{
  const float Legs=Weight(.56f,.95f,Y),Arm=Weight(.12f,.4f,FMath::Abs(X-.5f))*(1-Weight(.58f,.77f,Y))*Weight(.1f,.3f,Y);
  const float Torso=(1-Weight(.58f,.86f,Y))*(1-Weight(.26f,.48f,FMath::Abs(X-.5f)));
  if(Moving){Delta.X+=Legs*Side*.05f*Wave+Arm*Side*.035f*Counter+Torso*.012f*Wave;Delta.Y-=Legs*.024f*FMath::Max(0.f,Side*Wave);}
  if(Attacking){Delta.X+=Strike*(Torso*.035f+Arm*Side*.07f);Delta.Y-=Strike*Arm*.045f;Delta.X-=Legs*Side*.008f*Strike;}
  if(Working){const float Swing=FMath::Sin(Elapsed*(State==TEXT("gather_food")?7.f:10.f)+Seed*.73f);Delta.Y+=Arm*.055f*Swing+Torso*.018f*(1+Swing);Delta.X+=Arm*Side*.035f*Swing;}
  if(Converting){const float Gesture=FMath::Sin(Elapsed*5+Seed*.73f);Delta.Y-=Arm*(.025f+.025f*Gesture);Delta.X+=Arm*Side*.025f*Gesture+Torso*.006f*Gesture;}
  Delta.Y-=Torso*.004f*Breath;
 }
 return UV+Delta;
}
FRTSVisualPoseMetrics FRTSVisualAssets::MeasurePose(const FRTSDef& Def,const FString& State,float Elapsed,float Seed){
 FRTSVisualPoseMetrics Out;Out.State=State;Out.Rig=RigFor(Def);Out.Elapsed=Elapsed;Out.Triangles=(AnimatedUnit(Def)||State==TEXT("destroy"))?PoseColumns*PoseRows*2:2;
 FVector2D Mean=FVector2D::ZeroVector;float Square=0;int32 Count=0;
 for(int32 Y=0;Y<=PoseRows;++Y)for(int32 X=0;X<=PoseColumns;++X){const FVector2D UV(double(X)/PoseColumns,double(Y)/PoseRows),P=PoseVertex(Def,State,UV,Elapsed,Seed),D=P-UV;Mean+=D;Square+=D.SizeSquared();Out.MaximumOffset=FMath::Max(Out.MaximumOffset,float(D.Size()));Out.PoseHash=HashCombine(Out.PoseHash,GetTypeHash(FMath::RoundToInt(P.X*100000)));Out.PoseHash=HashCombine(Out.PoseHash,GetTypeHash(FMath::RoundToInt(P.Y*100000)));++Count;}
 Mean/=Count;Out.NonRigidDisplacement=FMath::Sqrt(FMath::Max(0.f,Square/Count-float(Mean.SizeSquared())));return Out;
}
bool FRTSVisualAssets::GetPoseMetrics(int32 EntityId,FRTSVisualPoseMetrics& Out) const {if(const auto* State=Motion.Find(EntityId)){Out=State->Metrics;return true;}return false;}

FRTSVisualAssets& FRTSVisualAssets::Get() {
 // Process-lifetime cache: FGCObject keeps imported textures alive; the OS reclaims
 // the singleton at shutdown. No disk access or image decoding occurs per frame.
 static FRTSVisualAssets* Instance=new FRTSVisualAssets();return *Instance;
}
FRTSVisualAssets::FRTSVisualAssets() {Reload();}

UTexture2D* FRTSVisualAssets::LoadTexture(const FString& RelativePath,int32 MaxDimension) {
 if(RelativePath.IsEmpty()||!FPaths::GetExtension(RelativePath).Equals(TEXT("png"),ESearchCase::IgnoreCase)||!FPaths::IsRelative(RelativePath))return nullptr;
 FString Path=FPaths::ConvertRelativePathToFull(ArtDirectory/RelativePath);FPaths::NormalizeFilename(Path);FPaths::CollapseRelativeDirectories(Path);
 if(!FPaths::IsUnderDirectory(Path,ArtDirectory))return nullptr;
 MaxDimension=FMath::Clamp(MaxDimension,1,512);const FString CacheKey=Path+FString::Printf(TEXT("#%d"),MaxDimension);
 if(UTexture2D** Existing=Textures.Find(CacheKey))return *Existing;
 UTexture2D* Texture=nullptr;TArray64<uint8> File;FImage Source;
 if(FPaths::FileExists(Path)&&FFileHelper::LoadFileToArray(File,*Path)&&FImageUtils::DecompressImage(File.GetData(),File.Num(),Source)&&Source.SizeX>0&&Source.SizeY>0) {
  const double Scale=FMath::Min(1.,double(MaxDimension)/FMath::Max(Source.SizeX,Source.SizeY));
  const int32 Width=FMath::Max(1,FMath::RoundToInt(Source.SizeX*Scale)),Height=FMath::Max(1,FMath::RoundToInt(Source.SizeY*Scale));
  FImage Pixels;Source.ResizeTo(Pixels,Width,Height,ERawImageFormat::BGRA8,EGammaSpace::sRGB);
  Texture=UTexture2D::CreateTransient(Width,Height,PF_B8G8R8A8);
  if(Texture&&Texture->GetPlatformData()&&Texture->GetPlatformData()->Mips.Num()) {
   Texture->NeverStream=true;Texture->SRGB=true;Texture->Filter=TF_Bilinear;Texture->AddressX=TA_Clamp;Texture->AddressY=TA_Clamp;
   auto& Mip=Texture->GetPlatformData()->Mips[0];void* Dest=Mip.BulkData.Lock(LOCK_READ_WRITE);FMemory::Memcpy(Dest,Pixels.RawData.GetData(),int64(Width)*Height*sizeof(FColor));Mip.BulkData.Unlock();Texture->UpdateResource();
   if(MaxDimension==512){FAlphaMask Mask;Mask.Width=Width;Mask.Height=Height;Mask.Pixels.SetNumUninitialized(Width*Height);for(int32 I=0;I<Mask.Pixels.Num();I++)Mask.Pixels[I]=Pixels.RawData[I*4+3];AlphaMasks.Add(Texture,MoveTemp(Mask));}
  } else Texture=nullptr;
 }
 // Cache both successful and absent paths, so neither filesystem work nor PNG decoding happens while drawing.
 Textures.Add(CacheKey,Texture);return Texture;
}

void FRTSVisualAssets::Reload() {
 check(IsInGameThread());Visuals.Empty();Textures.Empty();AlphaMasks.Empty();Motion.Empty();Icons.Empty();FactionIcons.Empty();FactionVisuals.Empty();
 const FString DataDirectory=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("Data"));
 ManifestPath=DataDirectory/TEXT("visuals.json");ArtDirectory=DataDirectory/TEXT("Art");
 FString Source;TSharedPtr<FJsonObject> Root;
 if(!FFileHelper::LoadFileToString(Source,*ManifestPath)) {
  UE_LOG(LogTemp,Display,TEXT("RTS visual manifest absent; using procedural visuals."));return;
 }
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source),Root)||!Root.IsValid()) {
  UE_LOG(LogTemp,Warning,TEXT("RTS visual manifest invalid; using procedural visuals."));return;
 }
 double Version=0;if(!Root->TryGetNumberField(TEXT("schema_version"),Version)||Version!=1) {
  UE_LOG(LogTemp,Warning,TEXT("RTS visual manifest requires schema_version 1; using procedural visuals."));return;
 }
 const FString RequestedRoot=StringField(Root,TEXT("art_root"),TEXT("Art"));
 FString Candidate=FPaths::ConvertRelativePathToFull(DataDirectory/RequestedRoot);FPaths::CollapseRelativeDirectories(Candidate);
 if(FPaths::IsRelative(RequestedRoot)&&FPaths::IsUnderDirectory(Candidate,DataDirectory))ArtDirectory=Candidate;
 FPaths::NormalizeDirectoryName(ArtDirectory);
 const TSharedPtr<FJsonObject>* Sprites=nullptr;
 if(Root->TryGetObjectField(TEXT("sprites"),Sprites)&&Sprites->IsValid())for(const auto& Item:(*Sprites)->Values) {
  if(!Item.Value.IsValid()||Item.Value->Type!=EJson::Object)continue;
  const TSharedPtr<FJsonObject> Object=Item.Value->AsObject();FVisual Visual;
  Visual.Meta.Size=PairField(Object,TEXT("size"),Visual.Meta.Size);
  Visual.Meta.Size.X=FMath::Clamp(Visual.Meta.Size.X,1.,2048.);Visual.Meta.Size.Y=FMath::Clamp(Visual.Meta.Size.Y,1.,2048.);
  Visual.Meta.Pivot=PairField(Object,TEXT("pivot"),Visual.Meta.Pivot);
  Visual.Meta.Pivot.X=FMath::Clamp(Visual.Meta.Pivot.X,0.,1.);Visual.Meta.Pivot.Y=FMath::Clamp(Visual.Meta.Pivot.Y,0.,1.);
  Visual.Meta.bPlayerTint=StringField(Object,TEXT("tint_mode"),TEXT("none"))==TEXT("player");
  const TArray<TSharedPtr<FJsonValue>>* Colors=nullptr;
  if(Object->TryGetArrayField(TEXT("color"),Colors)&&Colors->Num()==4) {
   double C[4];bool Valid=true;for(int I=0;I<4;I++)if(!(*Colors)[I]->TryGetNumber(C[I])||!FMath::IsFinite(C[I]))Valid=false;
   if(Valid)Visual.Meta.Color=FLinearColor(FMath::Clamp(C[0],0.,1.),FMath::Clamp(C[1],0.,1.),FMath::Clamp(C[2],0.,1.),FMath::Clamp(C[3],0.,1.));
  }
  const TSharedPtr<FJsonObject>* Hit=nullptr;
  if(Object->TryGetObjectField(TEXT("hitbox"),Hit)&&Hit->IsValid()) {
   Visual.Meta.HitMin=PairField(*Hit,TEXT("min"),Visual.Meta.HitMin);Visual.Meta.HitMax=PairField(*Hit,TEXT("max"),Visual.Meta.HitMax);
   Visual.Meta.HitMin.X=FMath::Clamp(Visual.Meta.HitMin.X,0.,.99);Visual.Meta.HitMin.Y=FMath::Clamp(Visual.Meta.HitMin.Y,0.,.99);
   Visual.Meta.HitMax.X=FMath::Clamp(Visual.Meta.HitMax.X,Visual.Meta.HitMin.X+.01,1.);Visual.Meta.HitMax.Y=FMath::Clamp(Visual.Meta.HitMax.Y,Visual.Meta.HitMin.Y+.01,1.);
  }
  const TSharedPtr<FJsonObject>* ContextHit=nullptr;
  if(Object->TryGetObjectField(TEXT("context_hitbox"),ContextHit)&&ContextHit->IsValid()){Visual.Meta.ContextHitMin=PairField(*ContextHit,TEXT("min"),Visual.Meta.ContextHitMin);Visual.Meta.ContextHitMax=PairField(*ContextHit,TEXT("max"),Visual.Meta.ContextHitMax);}
  const FString BasePath=StringField(Object,TEXT("texture")),BaseMask=StringField(Object,TEXT("team_mask"));
  Visual.Base.SourcePath=BasePath;Visual.Base.Texture=LoadTexture(BasePath);Visual.Base.TeamMask=LoadTexture(BaseMask);Visual.Base.UV=UVField(Object);
  Visual.bRenderable=Visual.Base.Texture!=nullptr;
  const TSharedPtr<FJsonObject>* Animations=nullptr;
  if(Object->TryGetObjectField(TEXT("animations"),Animations)&&Animations->IsValid())for(const auto& State:(*Animations)->Values) {
   if(State.Key!=TEXT("idle")&&State.Key!=TEXT("move")&&State.Key!=TEXT("attack")&&State.Key!=TEXT("build")&&State.Key!=TEXT("gather_food")&&State.Key!=TEXT("gather_timber")&&State.Key!=TEXT("gather_ore")&&State.Key!=TEXT("work")&&State.Key!=TEXT("convert"))continue;
   if(!State.Value.IsValid()||State.Value->Type!=EJson::Object)continue;
   const TSharedPtr<FJsonObject> AnimationObject=State.Value->AsObject();FAnimation Animation;Animation.bLoop=State.Key!=TEXT("attack");
   double FPS=6;if(AnimationObject->TryGetNumberField(TEXT("fps"),FPS)&&FMath::IsFinite(FPS))Animation.FPS=FMath::Clamp(float(FPS),.1f,60.f);
   AnimationObject->TryGetBoolField(TEXT("loop"),Animation.bLoop);
   const FString Sheet=StringField(AnimationObject,TEXT("texture"),BasePath),Mask=StringField(AnimationObject,TEXT("team_mask"),BaseMask);
   const TArray<TSharedPtr<FJsonValue>>* Frames=nullptr;
   if(AnimationObject->TryGetArrayField(TEXT("frames"),Frames))for(int32 I=0;I<FMath::Min(Frames->Num(),256);I++) {
    const auto& Value=(*Frames)[I];if(!Value.IsValid())continue;FFrame Frame;
    if(Value->Type==EJson::String) {Frame.SourcePath=Value->AsString();Frame.Texture=LoadTexture(Frame.SourcePath);Frame.TeamMask=LoadTexture(Mask);}
    else if(Value->Type==EJson::Object) {
     const auto FrameObject=Value->AsObject();Frame.SourcePath=StringField(FrameObject,TEXT("texture"),Sheet);Frame.Texture=LoadTexture(Frame.SourcePath);Frame.TeamMask=LoadTexture(StringField(FrameObject,TEXT("team_mask"),Mask));Frame.UV=UVField(FrameObject);
    }
    if(Frame.Texture)Animation.Frames.Add(Frame);
   }
   if(Animation.Frames.Num()) {Visual.bRenderable=true;Visual.Animations.Add(State.Key,MoveTemp(Animation));}
  }
  FString BaseId,Variant;if(!Item.Key.Split(TEXT("@"),&BaseId,&Variant))BaseId=Item.Key;
  const auto& Def=FRTSCatalog::Get().Def(BaseId);Visual.Meta.Size*=WorldScale(Def);
  if(AnimatedUnit(Def)){Visual.Meta.HitMin-=FVector2D(.06,.06);Visual.Meta.HitMax+=FVector2D(.06,.06);Visual.Meta.ContextHitMin-=FVector2D(.04,.04);Visual.Meta.ContextHitMax+=FVector2D(.04,.04);}
  Visuals.Add(Item.Key,MoveTemp(Visual));
 }
 const TSharedPtr<FJsonObject>* IconEntries=nullptr;
 if(Root->TryGetObjectField(TEXT("icons"),IconEntries)&&IconEntries->IsValid())for(const auto& Item:(*IconEntries)->Values) {
  if(!Item.Value.IsValid()||Item.Value->Type!=EJson::Object)continue;const auto Object=Item.Value->AsObject();FFrame Frame;Frame.SourcePath=StringField(Object,TEXT("texture"));Frame.Texture=LoadTexture(Frame.SourcePath,256);Frame.UV=UVField(Object);Icons.Add(Item.Key,Frame);
 }
 // Every pointer below is resolved only after the sprite map has finished growing.
 for(const auto& Faction:FRTSCatalog::Get().Factions){auto& Resolved=FactionVisuals.FindOrAdd(Faction.Id);for(const auto& Def:FRTSCatalog::Get().Defs){FVisual* Visual=Visuals.Find(Def.Id+TEXT("@")+Faction.Id);if(!Visual||!Visual->bRenderable)Visual=Visuals.Find(Def.Id);Resolved.Add(Def.Id,Visual);}}
 CacheIcons();
 UE_LOG(LogTemp,Display,TEXT("RTS_VISUAL_ASSETS entries=%d imported_textures=%d resident_bytes=%lld sprite_limit=512 icon_limit=256"),Visuals.Num(),GetLoadedTextureCount(),GetResidentTextureBytes());
}

const FRTSVisualAssets::FFrame* FRTSVisualAssets::ChooseFrame(FVisual& Visual,const FRTSEntityView& Entity,float Time) {
 FMotion& State=Motion.FindOrAdd(Entity.Id);
 if(Time<State.LastTime)State=FMotion();
 State.LastTime=Time;
 if(State.bSeen&&FVector2D::DistSquared(State.Position,Entity.Pos)>.0001)State.LastMove=Time;
 State.Position=Entity.Pos;const bool SeenBefore=State.bSeen;State.bSeen=true;
 const FString Work=WorkState(Entity);
 if(!SeenBefore)State.LastAttackEvent=Entity.AttackEventTime;
 const bool NewAttack=(SeenBefore&&Entity.AttackEventTime>State.LastAttackEvent)||(Entity.bAttacking&&!State.bWasAttacking);
 if(NewAttack){State.AttackStarted=Time;State.LastAttackEvent=Entity.AttackEventTime;State.StateStarted=Time;}
 State.bWasAttacking=Entity.bAttacking;
 const bool Dying=Entity.Status==TEXT("death")||Entity.Status==TEXT("destroy");
 const FString Desired=Dying?Entity.Status:Time-State.AttackStarted<AttackPoseSeconds?TEXT("attack"):Time-State.LastMove<.34f?TEXT("move"):!Work.IsEmpty()?Work:TEXT("idle");
 if(Desired!=State.State) {State.State=Desired;State.StateStarted=Time;}
 State.PoseElapsed=Dying?Entity.Cooldown:Desired==TEXT("attack")?Time-State.AttackStarted:Time-State.StateStarted;
 auto FrameFrom=[&](const FAnimation* Animation)->const FFrame* {
  if(!Animation||Animation->Frames.Num()==0)return nullptr;
  const int32 Raw=FMath::Max(0,FMath::FloorToInt((Dying?Entity.Cooldown:Time-State.StateStarted)*Animation->FPS));
  const int32 Index=Animation->bLoop?Raw%Animation->Frames.Num():FMath::Min(Raw,Animation->Frames.Num()-1);return &Animation->Frames[Index];
 };
 if(const FFrame* Frame=FrameFrom(Visual.Animations.Find(Desired)))return Frame;
 if(!Work.IsEmpty())if(const FFrame* Frame=FrameFrom(Visual.Animations.Find(TEXT("work"))))return Frame;
 if(Desired!=TEXT("idle"))if(const FFrame* Frame=FrameFrom(Visual.Animations.Find(TEXT("idle"))))return Frame;
 if(Visual.Base.Texture)return &Visual.Base;
 for(const TCHAR* Fallback:{TEXT("idle"),TEXT("move"),TEXT("attack")})if(const FAnimation* Animation=Visual.Animations.Find(Fallback))if(Animation->Frames.Num())return &Animation->Frames[0];
 return nullptr;
}

FRTSVisualAssets::FVisual* FRTSVisualAssets::FindVisual(const FString& TypeId,const FString& Faction) const {
 if(const auto* Variants=FactionVisuals.Find(Faction))if(FVisual* const* Found=Variants->Find(TypeId))return *Found;
 return const_cast<FVisual*>(Visuals.Find(TypeId));
}
const FRTSVisualAssets::FFrame* FRTSVisualAssets::ReferenceFrame(const FVisual& Visual) const {
 if(Visual.Base.Texture)return &Visual.Base;
 for(const TCHAR* State:{TEXT("idle"),TEXT("move"),TEXT("attack"),TEXT("work"),TEXT("build"),TEXT("gather_food"),TEXT("gather_timber"),TEXT("gather_ore")})if(const auto* Animation=Visual.Animations.Find(State))if(Animation->Frames.Num())return &Animation->Frames[0];return nullptr;
}
FVector2D FRTSVisualAssets::FittedSize(const FFrame& Frame,FVector2D Box) const {
 if(!Frame.Texture)return Box;
 const FVector2D Native(Frame.Texture->GetSizeX()*Frame.UV.Z,Frame.Texture->GetSizeY()*Frame.UV.W);if(Native.X<=0||Native.Y<=0)return Box;
 return Native*FMath::Min(Box.X/Native.X,Box.Y/Native.Y);
}
bool FRTSVisualAssets::TryDraw(UCanvas* Canvas,const FRTSEntityView& Entity,const FRTSDef& Def,FVector2D LogicalScreenPoint,float Zoom,float UIScale,FLinearColor PlayerColor,float Time,float Opacity,const FString& Faction) {
 if(!Canvas||Zoom<=0||UIScale<=0||!FMath::IsFinite(Time))return false;
 FVisual* Visual=FindVisual(Def.Id,Faction);if(!Visual||!Visual->bRenderable)return false;
 const FFrame* Frame=ChooseFrame(*Visual,Entity,Time);if(!Frame||!Frame->Texture||!Frame->Texture->GetResource())return false;
 const FVector2D Size=FittedSize(*Frame,Visual->Meta.Size)*Zoom*UIScale,TopLeft=LogicalScreenPoint*UIScale-Size*Visual->Meta.Pivot;
 // Painted RGB stays intact; team identification comes from the ground marker or an explicit mask.
 FLinearColor Tint=Visual->Meta.Color;Tint.A*=Opacity;
 const bool Dying=Entity.Status==TEXT("death")||Entity.Status==TEXT("destroy");
 const bool Deform=(Dying&&!Visual->Animations.Contains(Entity.Status))||(AnimatedUnit(Def)&&Visual->Animations.Num()==0);
 const FMotion& Pose=Motion.FindChecked(Entity.Id);
 TArray<FCanvasUVTri> Triangles;
 FRTSVisualPoseMetrics DrawMetrics;DrawMetrics.State=Pose.State;DrawMetrics.Rig=RigFor(Def);DrawMetrics.Elapsed=Pose.PoseElapsed;DrawMetrics.Triangles=Deform?PoseColumns*PoseRows*2:2;
 FVector2D MeanOffset=FVector2D::ZeroVector;float SquareOffset=0;
 if(Deform){
  // Shared vertices keep the painted surface watertight. One batched triangle item
  // per texture avoids per-limb draw calls and performs no per-frame asset I/O.
  FVector2D Points[(PoseColumns+1)*(PoseRows+1)],UVs[(PoseColumns+1)*(PoseRows+1)];
  for(int32 Y=0;Y<=PoseRows;++Y)for(int32 X=0;X<=PoseColumns;++X){const int32 Index=Y*(PoseColumns+1)+X;const FVector2D UV(double(X)/PoseColumns,double(Y)/PoseRows);const FVector2D P=PoseVertex(Def,Pose.State,UV,Pose.PoseElapsed,Entity.Id),Offset=P-UV;
   MeanOffset+=Offset;SquareOffset+=Offset.SizeSquared();DrawMetrics.MaximumOffset=FMath::Max(DrawMetrics.MaximumOffset,float(Offset.Size()));DrawMetrics.PoseHash=HashCombine(DrawMetrics.PoseHash,GetTypeHash(FMath::RoundToInt(P.X*100000)));DrawMetrics.PoseHash=HashCombine(DrawMetrics.PoseHash,GetTypeHash(FMath::RoundToInt(P.Y*100000)));
   Points[Index]=TopLeft+Size*P;UVs[Index]=FVector2D(Frame->UV.X+UV.X*Frame->UV.Z,Frame->UV.Y+UV.Y*Frame->UV.W);}
  Triangles.Reserve(PoseColumns*PoseRows*2);
  const auto Add=[&](int32 A,int32 B,int32 C){FCanvasUVTri Tri;Tri.V0_Pos=Points[A];Tri.V1_Pos=Points[B];Tri.V2_Pos=Points[C];Tri.V0_UV=UVs[A];Tri.V1_UV=UVs[B];Tri.V2_UV=UVs[C];Tri.V0_Color=Tri.V1_Color=Tri.V2_Color=Tint;Triangles.Add(Tri);};
  for(int32 Y=0;Y<PoseRows;++Y)for(int32 X=0;X<PoseColumns;++X){const int32 A=Y*(PoseColumns+1)+X,B=A+1,C=A+PoseColumns+1,D=C+1;Add(A,B,D);Add(A,D,C);}
 }
 if(Deform){const int32 Count=(PoseColumns+1)*(PoseRows+1);MeanOffset/=Count;DrawMetrics.NonRigidDisplacement=FMath::Sqrt(FMath::Max(0.f,SquareOffset/Count-float(MeanOffset.SizeSquared())));}
 Motion.FindChecked(Entity.Id).Metrics=DrawMetrics;
 const auto DrawTexture=[&](UTexture2D* Texture,FLinearColor Color){if(!Texture||!Texture->GetResource())return;
  if(Deform){FCanvasTriangleItem Item(Triangles,Texture->GetResource());Item.SetColor(Color);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);}
  else{FCanvasTileItem Item(TopLeft,Texture->GetResource(),Size,Color);Item.UV0=FVector2D(Frame->UV.X,Frame->UV.Y);Item.UV1=FVector2D(Frame->UV.X+Frame->UV.Z,Frame->UV.Y+Frame->UV.W);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);}
 };
 DrawTexture(Frame->Texture,Tint);if(Frame->TeamMask){FLinearColor MaskTint=PlayerColor*Visual->Meta.Color;MaskTint.A*=Opacity;DrawTexture(Frame->TeamMask,MaskTint);}return true;
}
bool FRTSVisualAssets::HasRenderable(const FString& TypeId,const FString& Faction) const {const auto* Visual=FindVisual(TypeId,Faction);return Visual&&Visual->bRenderable;}
bool FRTSVisualAssets::GetMetadata(const FString& TypeId,FRTSVisualMetadata& OutMetadata,const FString& Faction) const {const auto* Visual=FindVisual(TypeId,Faction);if(!Visual||!Visual->bRenderable)return false;OutMetadata=Visual->Meta;return true;}
bool FRTSVisualAssets::GetHitBounds(const FString& TypeId,FVector2D LogicalScreenPoint,float Zoom,float UIScale,FBox2D& OutPixelBounds,bool bContext,const FString& Faction) const {
 OutPixelBounds=FBox2D(EForceInit::ForceInit);const auto* Visual=FindVisual(TypeId,Faction);if(Zoom<=0||UIScale<=0||!Visual||!Visual->bRenderable)return false;const auto* Frame=ReferenceFrame(*Visual);if(!Frame)return false;
 const auto& Metadata=Visual->Meta;const FVector2D Size=FittedSize(*Frame,Metadata.Size)*Zoom*UIScale,TopLeft=LogicalScreenPoint*UIScale-Size*Metadata.Pivot;
 const FVector2D Center=TopLeft+Size*(Metadata.HitMin+Metadata.HitMax)*.5f;FVector2D Half=Size*(Metadata.HitMax-Metadata.HitMin)*.5f;const auto& Def=FRTSCatalog::Get().Def(TypeId);
 if(Def.bBuilding&&!bContext){Half*=.72f;const FVector2D Lower=Center+FVector2D(0,Size.Y*.12);OutPixelBounds=FBox2D(Lower-Half,Lower+Half);}
 else if(bContext)OutPixelBounds=FBox2D(TopLeft+Size*Metadata.ContextHitMin-FVector2D(8,5)*Zoom*UIScale,TopLeft+Size*Metadata.ContextHitMax+FVector2D(8,5)*Zoom*UIScale);
 else OutPixelBounds=FBox2D(Center-Half,Center+Half);return true;
}
void FRTSVisualAssets::CacheIcons() {
 const TMap<FString,FFrame> Rules=Icons;
 const auto Resolve=[&](const FRTSDef& Def,const FString& Faction){
  FFrame Result;const FFrame* Explicit=Rules.Find(Faction.IsEmpty()?Def.Id:Def.Id+TEXT("@")+Faction);if(!Explicit||!Explicit->Texture)Explicit=Rules.Find(Def.Id);if(Explicit&&Explicit->Texture)return *Explicit;
  Result.SourcePath=Def.Icon.IsEmpty()?TEXT("icons/")+Def.Id+TEXT(".png"):Def.Icon;Result.Texture=LoadTexture(Result.SourcePath,256);if(Result.Texture)return Result;
  const FVisual* Visual=FindVisual(Def.Id,Faction);if((!Visual||!Visual->bRenderable)&&!Def.HeroChoice.IsEmpty())Visual=FindVisual(Def.HeroChoice,Faction);
  if(Visual)if(const FFrame* Source=ReferenceFrame(*Visual)){Result=*Source;Result.Texture=LoadTexture(Result.SourcePath,256);Result.TeamMask=nullptr;}return Result;
 };
 // Non-catalog action icons are also fully imported during Reload, never on first draw.
 for(const auto& Def:FRTSCatalog::Get().Defs){FFrame Base=Resolve(Def,FString());Icons.Add(Def.Id,Base);}
 for(const auto& Faction:FRTSCatalog::Get().Factions){auto& Resolved=FactionIcons.FindOrAdd(Faction.Id);for(const auto& Def:FRTSCatalog::Get().Defs)Resolved.Add(Def.Id,Resolve(Def,Faction.Id));}
 for(const auto& Rule:Rules){FString TypeId,Faction;if(Rule.Key.Split(TEXT("@"),&TypeId,&Faction)&&Rule.Value.Texture)FactionIcons.FindOrAdd(Faction).Add(TypeId,Rule.Value);}
}
const FRTSVisualAssets::FFrame* FRTSVisualAssets::FindIcon(const FString& TypeId,const FString& Faction) const {if(const auto* Variants=FactionIcons.Find(Faction))if(const auto* Frame=Variants->Find(TypeId))return Frame;return Icons.Find(TypeId);}
bool FRTSVisualAssets::TryDrawIcon(UCanvas* Canvas,const FRTSDef& Def,FVector2D Pixel,float Size,FLinearColor Tint,const FString& Faction) {return TryDrawIconId(Canvas,Def.Id,Pixel,Size,Tint,Faction);}
bool FRTSVisualAssets::TryDrawIconId(UCanvas* Canvas,const FString& TypeId,FVector2D Pixel,float Size,FLinearColor Tint,const FString& Faction) const {
 const auto* Frame=FindIcon(TypeId,Faction);if(!Canvas||Size<=0||!Frame||!Frame->Texture||!Frame->Texture->GetResource())return false;
 const FVector2D Fit=FittedSize(*Frame,FVector2D(Size,Size));const FVector2D At=Pixel+(FVector2D(Size,Size)-Fit)*.5f;
 const float Brightness=(Tint.R<.7f&&Tint.G<.7f&&Tint.B<.7f)?.5f:1.f;FCanvasTileItem Item(At,Frame->Texture->GetResource(),Fit,FLinearColor(Brightness,Brightness,Brightness,Tint.A));Item.UV0=FVector2D(Frame->UV.X,Frame->UV.Y);Item.UV1=FVector2D(Frame->UV.X+Frame->UV.Z,Frame->UV.Y+Frame->UV.W);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);return true;
}
bool FRTSVisualAssets::OpaqueAt(const FString& TypeId,FVector2D Pixel,FVector2D LogicalScreenPoint,float Zoom,float UIScale,const FString& Faction,int32 EntityId) const {
 const auto* Visual=FindVisual(TypeId,Faction);if(!Visual||!Visual->bRenderable)return true;
 const auto* Frame=ReferenceFrame(*Visual);if(!Frame||!Frame->Texture)return true;
 const auto* Mask=AlphaMasks.Find(Frame->Texture);if(!Mask)return true;
 const FVector2D Size=FittedSize(*Frame,Visual->Meta.Size)*Zoom*UIScale;
 if(Size.X<=0||Size.Y<=0)return false;
 FVector2D Local=(Pixel-(LogicalScreenPoint*UIScale-Size*Visual->Meta.Pivot))/Size;
 if(const auto* Pose=Motion.Find(EntityId))if(Visual->Animations.Num()==0){
  const auto& Def=FRTSCatalog::Get().Def(TypeId);bool Found=false;
  // Invert the same rendered triangles with barycentric coordinates so alpha
  // picking follows swinging arms and stepping feet rather than the rest pose.
  const auto InTriangle=[&](FVector2D A,FVector2D B,FVector2D C,FVector2D UA,FVector2D UB,FVector2D UC){
   const FVector2D AB=B-A,AC=C-A,AP=Local-A;const double Den=AB.X*AC.Y-AB.Y*AC.X;if(FMath::Abs(Den)<1.e-9)return false;
   const double U=(AP.X*AC.Y-AP.Y*AC.X)/Den,V=(AB.X*AP.Y-AB.Y*AP.X)/Den;
   if(U<-.0001||V<-.0001||U+V>1.0001)return false;Local=UA+(UB-UA)*U+(UC-UA)*V;return true;
  };
  for(int32 Y=0;Y<PoseRows&&!Found;++Y)for(int32 X=0;X<PoseColumns&&!Found;++X){const FVector2D A(double(X)/PoseColumns,double(Y)/PoseRows),B(double(X+1)/PoseColumns,double(Y)/PoseRows),C(double(X)/PoseColumns,double(Y+1)/PoseRows),D(double(X+1)/PoseColumns,double(Y+1)/PoseRows);
   const auto P=[&](FVector2D UV){return PoseVertex(Def,Pose->State,UV,Pose->PoseElapsed,EntityId);};
   Found=InTriangle(P(A),P(B),P(D),A,B,D)||InTriangle(P(A),P(D),P(C),A,D,C);
  }
  if(!Found)return false;
 }
 if(Local.X<0||Local.Y<0||Local.X>=1||Local.Y>=1)return false;
 const int32 X=FMath::Clamp(FMath::FloorToInt((Frame->UV.X+Local.X*Frame->UV.Z)*Mask->Width),0,Mask->Width-1);
 const int32 Y=FMath::Clamp(FMath::FloorToInt((Frame->UV.Y+Local.Y*Frame->UV.W)*Mask->Height),0,Mask->Height-1);
 return Mask->Pixels[Y*Mask->Width+X]>=24;
}
int32 FRTSVisualAssets::GetLoadedTextureCount() const {int32 Count=0;for(const auto& Texture:Textures)if(Texture.Value)Count++;return Count;}
int64 FRTSVisualAssets::GetResidentTextureBytes() const {int64 Bytes=0;for(const auto& Texture:Textures)if(Texture.Value)Bytes+=int64(Texture.Value->GetSizeX())*Texture.Value->GetSizeY()*sizeof(FColor);return Bytes;}
void FRTSVisualAssets::AddReferencedObjects(FReferenceCollector& Collector) {for(auto& Texture:Textures)Collector.AddReferencedObject(Texture.Value);}

FString FRTSVisualAssets::WorkState(const FRTSEntityView& E,const FRTSDef* Target) {
 const FString S=E.Status.ToLower();const auto& D=FRTSCatalog::Get().Def(E.Def);
 if(S.Contains(TEXT("convert")))return TEXT("convert");
 if(S.Contains(TEXT("build"))||S.Contains(TEXT("repair")))return TEXT("build");
 if(S.Contains(TEXT("gather"))||S==TEXT("working")) {
  const FString Id=Target?Target->Id:FString();
  if(Id==TEXT("tree")||S.Contains(TEXT("timber"))||S.Contains(TEXT("wood")))return TEXT("gather_timber");
  if(Id==TEXT("ore")||Id==TEXT("hill_of_ore")||D.Id==TEXT("worker_in_ore")||S.Contains(TEXT("ore")))return TEXT("gather_ore");
  return TEXT("gather_food");
 }
 return FString();
}
