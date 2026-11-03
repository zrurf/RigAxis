using UnrealBuildTool;
using System.IO;

public class RigAxis : ModuleRules
{
    public RigAxis(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ProceduralMeshComponent"
        });

        string ThirdPartyDir = Path.Combine(ModuleDirectory, "..", "ThirdParty", "RigAxis");
        PublicIncludePaths.Add(Path.Combine(ThirdPartyDir, "include"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string LibDir = Path.Combine(ThirdPartyDir, "lib", "Win64", "Release");
            PublicAdditionalLibraries.Add(Path.Combine(LibDir, "rigaxis_ffi.lib"));
            PublicSystemLibraries.AddRange(new string[]
            {
                "ntdll.lib",
                "userenv.lib",
                "ws2_32.lib",
                "dbghelp.lib"
            });
        }
    }
}
