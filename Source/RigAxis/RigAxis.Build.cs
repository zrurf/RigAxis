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
            "Engine"
        });

        string ThirdPartyDir = Path.Combine(ModuleDirectory, "..", "ThirdParty", "RigAxis");
        string LibRoot = Path.Combine(ThirdPartyDir, "lib");
        PublicIncludePaths.Add(Path.Combine(ThirdPartyDir, "include"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string Library = Path.Combine(LibRoot, "Win64", "Release", "rigaxis_ffi.lib");
            RequireLibrary(Library, "Scripts/BuildRust.ps1 -Target win64");
            PublicSystemLibraries.AddRange(new string[]
            {
                "ntdll.lib",
                "userenv.lib",
                "ws2_32.lib",
                "dbghelp.lib"
            });
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            string Arch = Target.Architecture == UnrealArch.Arm64 ? "arm64" : "x86_64";
            string Library = Path.Combine(LibRoot, "Android", Arch, "rigaxis_ffi.a");
            RequireLibrary(Library, "Scripts/BuildRust.ps1 -Target android");
            PublicSystemLibraries.AddRange(new string[] { "log", "dl", "m" });
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            string Arch = Target.Architecture == UnrealArch.IOSSimulator ? "simulator" : "arm64";
            string Library = Path.Combine(LibRoot, "iOS", Arch, "rigaxis_ffi.a");
            RequireLibrary(Library, "a macOS/iOS Rust build");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string Library = Path.Combine(LibRoot, "Mac", "rigaxis_ffi.a");
            RequireLibrary(Library, "a macOS universal Rust build");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string Library = Path.Combine(LibRoot, "Linux", "x64", "rigaxis_ffi.a");
            RequireLibrary(Library, "a Linux x64 Rust build");
            PublicSystemLibraries.AddRange(new string[] { "pthread", "dl", "m" });
        }
        else
        {
            throw new BuildException($"RigAxis does not support {Target.Platform}.");
        }
    }

    private void RequireLibrary(string Library, string Hint)
    {
        if (!File.Exists(Library))
        {
            throw new BuildException($"RigAxis: missing '{Library}'. Build it with {Hint}.");
        }
        PublicAdditionalLibraries.Add(Library);
    }
}
