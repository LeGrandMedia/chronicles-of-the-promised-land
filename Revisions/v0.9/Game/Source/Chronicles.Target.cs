using UnrealBuildTool;
public class ChroniclesTarget : TargetRules {
 public ChroniclesTarget(TargetInfo Target) : base(Target) {
  Type = TargetType.Game; DefaultBuildSettings = BuildSettingsVersion.V4;
  IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
  ExtraModuleNames.Add("Chronicles");
  WindowsPlatform.CompilerVersion = "14.44.35207";
  WindowsPlatform.bStrictConformanceMode = false;
 }
}
