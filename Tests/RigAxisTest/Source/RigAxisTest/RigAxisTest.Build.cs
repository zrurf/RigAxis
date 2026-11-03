using UnrealBuildTool;

public class RigAxisTest : ModuleRules
{
    public RigAxisTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RigAxis"
        });
    }
}
