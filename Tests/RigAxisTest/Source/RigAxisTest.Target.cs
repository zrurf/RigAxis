using UnrealBuildTool;

public class RigAxisTestTarget : TargetRules
{
    public RigAxisTestTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RigAxisTest");
    }
}
