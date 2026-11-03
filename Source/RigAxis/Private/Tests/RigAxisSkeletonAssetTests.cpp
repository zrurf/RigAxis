#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const ANSICHAR* MinimalSkeleton = R"JSON({"skeleton":{"spine":"4.3"},"bones":[{"name":"root"},{"name":"child","parent":"root","x":10}],"slots":[],"skins":[],"animations":{}})JSON";
    const ANSICHAR* MinimalAtlas = "minimal.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\n";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigAxisSkeletonAssetTest, "RigAxis.Native.Asset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisSkeletonAssetTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URigAxisSkeletonAsset> Asset(NewObject<URigAxisSkeletonAsset>());
    Asset->SkeletonBytes.Append(reinterpret_cast<const uint8*>(MinimalSkeleton), FCStringAnsi::Strlen(MinimalSkeleton));
    Asset->AtlasBytes.Append(reinterpret_cast<const uint8*>(MinimalAtlas), FCStringAnsi::Strlen(MinimalAtlas));

    TSharedPtr<FRigAxisSkeletonData> Data = Asset->GetData();
    TestTrue(TEXT("data valid"), Data.IsValid());
    if (!Data.IsValid())
    {
        AddError(FString::Printf(TEXT("asset load failed: %s"), *Asset->GetLoadError()));
        return false;
    }

    TestEqual(TEXT("version"), Asset->GetVersion(), 1);
    TestTrue(TEXT("load error empty"), Asset->GetLoadError().IsEmpty());
    TestEqual(TEXT("format"), Data->GetFormat(), FString(TEXT("4.3")));

    const TArray<FString>& BoneNames = Data->GetBoneNames();
    TestEqual(TEXT("bone count"), BoneNames.Num(), 2);
    if (BoneNames.Num() > 0)
    {
        TestEqual(TEXT("first bone"), BoneNames[0], FString(TEXT("root")));
    }
    TestEqual(TEXT("find child"), Data->FindBone(FName(TEXT("child"))), 1);
    TestEqual(TEXT("find missing bone"), Data->FindBone(FName(TEXT("missing"))), INDEX_NONE);
    TestEqual(TEXT("animation count"), Data->GetAnimationNames().Num(), 0);
    TestEqual(TEXT("skin count"), Data->GetSkinNames().Num(), 0);

    const TArray<FString>& PageNames = Data->GetAtlasPageNames();
    TestEqual(TEXT("atlas pages"), PageNames.Num(), 1);
    if (PageNames.Num() > 0)
    {
        TestEqual(TEXT("atlas page name"), PageNames[0], FString(TEXT("minimal.png")));
    }
    TestEqual(TEXT("missing regions"), Data->GetMissingRegions().Num(), 0);

    AddExpectedError(TEXT("invalid JSON"), EAutomationExpectedErrorFlags::Contains, 1);

    const ANSICHAR* Invalid = "not a skeleton";
    Asset->SkeletonBytes.Reset();
    Asset->SkeletonBytes.Append(reinterpret_cast<const uint8*>(Invalid), FCStringAnsi::Strlen(Invalid));
    Asset->RebuildData();

    TestEqual(TEXT("version after rebuild"), Asset->GetVersion(), 2);
    TestFalse(TEXT("data invalid after rebuild"), Asset->GetData().IsValid());
    TestFalse(TEXT("load error set after rebuild"), Asset->GetLoadError().IsEmpty());
    return true;
}

#endif
