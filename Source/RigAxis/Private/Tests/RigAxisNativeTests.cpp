#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "RigAxis/RigAxis.h"

namespace
{
    const ANSICHAR* MinimalSkeleton = R"JSON({"skeleton":{"spine":"4.3"},"bones":[{"name":"root"},{"name":"child","parent":"root","x":10}],"slots":[],"skins":[],"animations":{}})JSON";
    const ANSICHAR* MinimalAtlas = "minimal.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\n";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigAxisNativeLoadTest, "RigAxis.Native.LoadSkeleton", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisNativeLoadTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("ABI version"), int32(rigaxis_abi_version()), int32(RIGAXIS_ABI_VERSION));
    TestEqual(TEXT("last error is empty"), FString(ANSI_TO_TCHAR(rigaxis_last_error())), FString());

    RigAxisSkeleton* Skeleton = nullptr;
    const SIZE_T SkeletonSize = static_cast<SIZE_T>(FCStringAnsi::Strlen(MinimalSkeleton));
    RigAxisStatus Status = rigaxis_skeleton_create(reinterpret_cast<const uint8*>(MinimalSkeleton), SkeletonSize, &Skeleton);
    TestEqual(TEXT("skeleton create"), int32(Status), int32(RIGAXIS_OK));
    if (Status != RIGAXIS_OK)
    {
        AddError(FString::Printf(TEXT("skeleton create failed: %s"), ANSI_TO_TCHAR(rigaxis_last_error())));
        return false;
    }

    TestNotNull(TEXT("skeleton handle"), Skeleton);
    TestEqual(TEXT("bone count"), int32(rigaxis_skeleton_bone_count(Skeleton)), 2);
    TestEqual(TEXT("first bone"), FString(ANSI_TO_TCHAR(rigaxis_skeleton_bone_name(Skeleton, 0))), FString(TEXT("root")));
    TestEqual(TEXT("child bone"), FString(ANSI_TO_TCHAR(rigaxis_skeleton_bone_name(Skeleton, 1))), FString(TEXT("child")));
    TestEqual(TEXT("find child"), int32(rigaxis_skeleton_find_bone(Skeleton, "child")), 1);
    TestEqual(TEXT("find missing"), int32(rigaxis_skeleton_find_bone(Skeleton, "missing")), -1);

    RigAxisAtlas* Atlas = nullptr;
    const SIZE_T AtlasSize = static_cast<SIZE_T>(FCStringAnsi::Strlen(MinimalAtlas));
    Status = rigaxis_atlas_create(reinterpret_cast<const uint8*>(MinimalAtlas), AtlasSize, &Atlas);
    TestEqual(TEXT("atlas create"), int32(Status), int32(RIGAXIS_OK));
    if (Status != RIGAXIS_OK)
    {
        AddError(FString::Printf(TEXT("atlas create failed: %s"), ANSI_TO_TCHAR(rigaxis_last_error())));
        rigaxis_skeleton_free(Skeleton);
        return false;
    }

    TestNotNull(TEXT("atlas handle"), Atlas);
    TestEqual(TEXT("atlas pages"), int32(rigaxis_atlas_page_count(Atlas)), 1);
    TestEqual(TEXT("atlas page name"), FString(ANSI_TO_TCHAR(rigaxis_atlas_page_name(Atlas, 0))), FString(TEXT("minimal.png")));

    RigAxisLookup* Lookup = nullptr;
    Status = rigaxis_lookup_create(Skeleton, Atlas, &Lookup);
    TestEqual(TEXT("lookup create"), int32(Status), int32(RIGAXIS_OK));
    if (Status == RIGAXIS_OK)
    {
        TestNotNull(TEXT("lookup handle"), Lookup);
        TestEqual(TEXT("missing regions"), int32(rigaxis_lookup_missing_count(Lookup)), 0);
    }
    else
    {
        AddError(FString::Printf(TEXT("lookup create failed: %s"), ANSI_TO_TCHAR(rigaxis_last_error())));
    }

    rigaxis_lookup_free(Lookup);
    rigaxis_atlas_free(Atlas);
    rigaxis_skeleton_free(Skeleton);
    return true;
}

#endif
