using UnrealBuildTool;

public class RigAxisTestEditorTarget : TargetRules
{
    public RigAxisTestEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RigAxisTest");
    }
}
