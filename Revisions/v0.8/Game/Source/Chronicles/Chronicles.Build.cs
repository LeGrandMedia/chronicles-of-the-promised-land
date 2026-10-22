using UnrealBuildTool;
public class Chronicles : ModuleRules {
 public Chronicles(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "Slate", "SlateCore", "UMG", "Networking", "Sockets", "ApplicationCore", "ImageCore"});
 }
}
