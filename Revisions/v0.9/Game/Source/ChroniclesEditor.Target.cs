using UnrealBuildTool;
public class ChroniclesEditorTarget : TargetRules {
 public ChroniclesEditorTarget(TargetInfo Target) : base(Target) {
  Type = TargetType.Editor; DefaultBuildSettings = BuildSettingsVersion.V4;
  IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
  ExtraModuleNames.Add("Chronicles");
  WindowsPlatform.CompilerVersion = "14.44.35207";
  WindowsPlatform.bStrictConformanceMode = false;
 }
}
