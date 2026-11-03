using UnrealBuildTool;

public class RigAxisEditor : ModuleRules
{
    public RigAxisEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RigAxis"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "AssetTools",
            "AssetRegistry",
            "ImageWrapper"
        });
    }
}
