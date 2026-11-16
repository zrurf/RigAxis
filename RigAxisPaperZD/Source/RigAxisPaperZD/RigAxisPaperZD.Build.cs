using UnrealBuildTool;

public class RigAxisPaperZD : ModuleRules
{
    public RigAxisPaperZD(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RigAxis",
            "PaperZD"
        });
    }
}
