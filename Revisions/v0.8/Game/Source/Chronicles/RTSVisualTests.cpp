#include "RTSVisualAssets.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/GarbageCollection.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

bool RunRTSVisualTests() {
 // An opt-in import fixture verifies the real loose-file path and GC ownership.
 // Restore existing artwork byte-for-byte before returning.
 const FString File=FPaths::ProjectContentDir()/TEXT("Data/Art/resources/tree.png");
 TArray<uint8> Original,PNG;
 const bool Existed=FPaths::FileExists(File);
 if(Existed&&!FFileHelper::LoadFileToArray(Original,*File))return false;
 FBase64::Decode(TEXT("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP476DwHwAGAAJfNM1AwAAAAABJRU5ErkJggg=="),PNG);
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);
 const bool Written=FFileHelper::SaveArrayToFile(PNG,*File);
 auto& Art=FRTSVisualAssets::Get();Art.Reload();
 FRTSVisualMetadata Meta;FBox2D Bounds;
 const bool Imported=Written&&Art.HasRenderable(TEXT("tree"))&&Art.GetLoadedTextureCount()>0;
 CollectGarbage(RF_NoFlags);
 const bool Retained=Art.HasRenderable(TEXT("tree"));
 const FVector2D Anchor(400,300);const float Zoom=1.5f,UI=.8f;
 // The fixture is square: a rectangular display box must fit it without stretching.
 const bool Hit=Art.GetMetadata(TEXT("tree"),Meta)&&Art.GetHitBounds(TEXT("tree"),Anchor,Zoom,UI,Bounds)
  && Bounds.GetSize().Equals(FVector2D(FMath::Min(Meta.Size.X,Meta.Size.Y))*(Meta.HitMax-Meta.HitMin)*Zoom*UI,.01);
 const bool Missing=!Art.HasRenderable(TEXT("missing_type"))&&!Art.GetHitBounds(TEXT("missing_type"),Anchor,Zoom,UI,Bounds);
 const bool Restored=Existed?FFileHelper::SaveArrayToFile(Original,*File):IFileManager::Get().Delete(*File,false,true);
 Art.Reload();
 const bool Fallback=Existed||!Art.HasRenderable(TEXT("tree"));

 TArray<TSharedPtr<FJsonValue>> PoseResults;bool AnimationPassed=true,GeometryPassed=true,ScalePassed=true;
 const auto& Cat=FRTSCatalog::Get();
 for(const auto& Def:Cat.Defs)if(!Def.bBuilding&&!Def.bUpgrade&&Def.Kind!=TEXT("resource")){
  const float Scale=FRTSVisualAssets::WorldScale(Def);ScalePassed&=Scale>=.62f&&Scale<=.70f;
 }
 ScalePassed&=FRTSVisualAssets::WorldScale(Cat.Def(TEXT("temple_of_the_land")))==1.f&&FRTSVisualAssets::WorldScale(Cat.Def(TEXT("tree")))==1.f;
 for(const TCHAR* Type:{TEXT("cimeter_bearer"),TEXT("industrious_laborer"),TEXT("teacher_of_the_word"),TEXT("elephant"),TEXT("jaredite_barge")}){
  const auto& Def=Cat.Def(Type);
  for(const TCHAR* State:{TEXT("move"),TEXT("attack"),TEXT("build"),TEXT("gather_food"),TEXT("gather_timber"),TEXT("gather_ore"),TEXT("convert")}){
   if(Def.Kind==TEXT("ship")&&FString(State)!=TEXT("move"))continue;
   if((Def.Kind==TEXT("beast"))&&FString(State)==TEXT("convert"))continue;
   const auto A=FRTSVisualAssets::MeasurePose(Def,State,.05f,7),B=FRTSVisualAssets::MeasurePose(Def,State,.24f,7);
   const bool Changed=A.PoseHash!=B.PoseHash&&FMath::Max(A.NonRigidDisplacement,B.NonRigidDisplacement)>.002f;
   AnimationPassed&=Changed;
   auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("type"),Type);R->SetStringField(TEXT("state"),State);R->SetStringField(TEXT("rig"),A.Rig);R->SetBoolField(TEXT("changed_pose"),Changed);R->SetNumberField(TEXT("pose_a_hash"),A.PoseHash);R->SetNumberField(TEXT("pose_b_hash"),B.PoseHash);R->SetNumberField(TEXT("non_rigid_rms"),FMath::Max(A.NonRigidDisplacement,B.NonRigidDisplacement));R->SetNumberField(TEXT("triangles"),A.Triangles);PoseResults.Add(MakeShared<FJsonValueObject>(R));
   // Verify the exact render mesh has no inverted cells throughout a full cycle.
   for(int32 Frame=0;Frame<24;++Frame)for(int32 Y=0;Y<6;++Y)for(int32 X=0;X<4;++X){
    const auto P=[&](int32 PX,int32 PY){return FRTSVisualAssets::PoseVertex(Def,State,FVector2D(double(PX)/4,double(PY)/6),Frame/30.f,7);};
    const FVector2D A0=P(X,Y),B0=P(X+1,Y),C0=P(X,Y+1),D0=P(X+1,Y+1);
    const auto Area=[](FVector2D A1,FVector2D B1,FVector2D C1){const auto AB=B1-A1,AC=C1-A1;return AB.X*AC.Y-AB.Y*AC.X;};
    GeometryPassed&=Area(A0,B0,D0)>.0001&&Area(A0,D0,C0)>.0001;
   }
  }
 }
 FRTSEntityView Worker;Worker.Def=Cat.Index(TEXT("industrious_laborer"));Worker.Status=TEXT("repair");bool WorkStates=FRTSVisualAssets::WorkState(Worker)==TEXT("build");Worker.Status=TEXT("gather timber");WorkStates&=FRTSVisualAssets::WorkState(Worker)==TEXT("gather_timber");Worker.Status=TEXT("convert");WorkStates&=FRTSVisualAssets::WorkState(Worker)==TEXT("convert");
 auto PoseReport=MakeShared<FJsonObject>();PoseReport->SetBoolField(TEXT("all_active_poses_non_rigid"),AnimationPassed);PoseReport->SetBoolField(TEXT("mesh_never_folds"),GeometryPassed);PoseReport->SetBoolField(TEXT("all_units_smaller_buildings_unchanged"),ScalePassed);PoseReport->SetBoolField(TEXT("work_state_routing"),WorkStates);PoseReport->SetArrayField(TEXT("poses"),PoseResults);PoseReport->SetStringField(TEXT("method"),TEXT("Same weighted UV mesh used by Canvas drawing; two distinct poses per activity plus 24-sample signed-area checks. World scale 0.66, ships0.70, UI icons unchanged. Rendered pose metrics available via GetPoseMetrics(entityId)."));
 FString PoseJSON;FJsonSerializer::Serialize(PoseReport,TJsonWriterFactory<>::Create(&PoseJSON));const FString PoseDir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*PoseDir,true);FFileHelper::SaveStringToFile(PoseJSON,*(PoseDir/TEXT("visual-v07-animation-tests.json")));
 const bool Passed=Imported&&Retained&&Hit&&Missing&&Restored&&Fallback&&AnimationPassed&&GeometryPassed&&ScalePassed&&WorkStates;
 const FString JSON=FString::Printf(TEXT("{\"passed\":%s,\"png_import\":%s,\"gc_retained\":%s,\"scaled_hit_bounds\":%s,\"unknown_fallback\":%s,\"original_restored\":%s,\"missing_fallback\":%s}"),
  Passed?TEXT("true"):TEXT("false"),Imported?TEXT("true"):TEXT("false"),Retained?TEXT("true"):TEXT("false"),Hit?TEXT("true"):TEXT("false"),Missing?TEXT("true"):TEXT("false"),Restored?TEXT("true"):TEXT("false"),Fallback?TEXT("true"):TEXT("false"));
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("Tests");IFileManager::Get().MakeDirectory(*Dir,true);
 FFileHelper::SaveStringToFile(JSON,*(Dir/TEXT("visual-tests.json")));
 UE_LOG(LogTemp,Display,TEXT("RTS_VISUAL_TESTS %s"),*JSON);
 return Passed;
}
