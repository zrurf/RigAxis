#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "RigAxisPlayer.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const ANSICHAR* AnimatedSkeleton = R"JSON({
      "skeleton": {"spine": "4.3"},
      "bones": [
        {"name": "root"},
        {"name": "child", "parent": "root", "x": 10}
      ],
      "slots": [],
      "skins": [],
      "animations": {
        "move": {
          "bones": {
            "child": {
              "translate": [
                {"time": 0, "x": 0, "y": 0},
                {"time": 1, "x": 10, "y": 0}
              ]
            }
          }
        },
        "move2": {
          "bones": {
            "child": {
              "translate": [
                {"time": 0, "x": 0, "y": 0},
                {"time": 1, "x": 100, "y": 0}
              ]
            }
          }
        }
      }
    })JSON";

    const ANSICHAR* MinimalAtlas = "minimal.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\n";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigAxisPlaybackTest, "RigAxis.Native.Playback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisPlaybackTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URigAxisSkeletonAsset> Asset(NewObject<URigAxisSkeletonAsset>());
    Asset->SkeletonBytes.Append(reinterpret_cast<const uint8*>(AnimatedSkeleton), FCStringAnsi::Strlen(AnimatedSkeleton));
    Asset->AtlasBytes.Append(reinterpret_cast<const uint8*>(MinimalAtlas), FCStringAnsi::Strlen(MinimalAtlas));

    TSharedPtr<FRigAxisSkeletonData> Data = Asset->GetData();
    TestTrue(TEXT("data valid"), Data.IsValid());
    if (!Data.IsValid())
    {
        AddError(FString::Printf(TEXT("asset load failed: %s"), *Asset->GetLoadError()));
        return false;
    }

    auto CreatePlayer = [this, &Data](FRigAxisPlayer& Player, const TCHAR* Label) -> bool
    {
        FString Error;
        const bool bCreated = Player.Create(Data, Error);
        TestTrue(Label, bCreated);
        if (!bCreated)
        {
            AddError(FString::Printf(TEXT("%s: %s"), Label, *Error));
        }
        return bCreated;
    };

    FRigAxisPlayer Player;
    if (!CreatePlayer(Player, TEXT("player create")))
    {
        return false;
    }

    TestTrue(TEXT("play move"), Player.Play(FName(TEXT("move")), false, 0.0f));
    Player.Update(0.5);
    Player.Apply();

    RigAxisAffine Affine = {};
    TestTrue(TEXT("bone world"), Player.GetBoneWorldTransform(FName(TEXT("child")), Affine));
    TestTrue(TEXT("move tx"), FMath::IsNearlyEqual(Affine.tx, 15.0, 1e-6));
    TestTrue(TEXT("move ty"), FMath::IsNearlyEqual(Affine.ty, 0.0, 1e-6));
    TestTrue(TEXT("move a"), FMath::IsNearlyEqual(Affine.a, 1.0, 1e-6));
    TestTrue(TEXT("move b"), FMath::IsNearlyEqual(Affine.b, 0.0, 1e-6));
    TestTrue(TEXT("move c"), FMath::IsNearlyEqual(Affine.c, 0.0, 1e-6));
    TestTrue(TEXT("move d"), FMath::IsNearlyEqual(Affine.d, 1.0, 1e-6));
    TestFalse(TEXT("missing bone"), Player.GetBoneWorldTransform(FName(TEXT("missing")), Affine));
    TestFalse(TEXT("missing animation"), Player.Play(FName(TEXT("missing")), false, 0.0f));

    Player.Stop();
    Player.Apply();
    TestTrue(TEXT("stop resets"), Player.GetBoneWorldTransform(FName(TEXT("child")), Affine));
    TestTrue(TEXT("stop tx"), FMath::IsNearlyEqual(Affine.tx, 10.0, 1e-6));

    FRigAxisPlayer Crossfade;
    if (CreatePlayer(Crossfade, TEXT("crossfade create")))
    {
        Crossfade.Play(FName(TEXT("move")), false, 0.0f);
        Crossfade.Update(0.25);
        TestTrue(TEXT("crossfade set"), Crossfade.SetAnimation(0, FName(TEXT("move2")), false, 0.5f));
        Crossfade.Update(0.1);
        Crossfade.Apply();
        TestTrue(TEXT("crossfade bone"), Crossfade.GetBoneWorldTransform(FName(TEXT("child")), Affine));
        TestTrue(TEXT("crossfade tx"), FMath::IsNearlyEqual(Affine.tx, 14.8, 1e-6));
    }

    FRigAxisPlayer Queue;
    if (CreatePlayer(Queue, TEXT("queue create")))
    {
        TestTrue(TEXT("queue play"), Queue.Play(FName(TEXT("move")), false, 0.0f));
        TestTrue(TEXT("queue add"), Queue.QueueAnimation(0, FName(TEXT("move2")), false, 0.0f, -1.0f));
        Queue.Update(1.1);
        Queue.Apply();
        TestTrue(TEXT("queue bone"), Queue.GetBoneWorldTransform(FName(TEXT("child")), Affine));
        TestTrue(TEXT("queue tx"), FMath::IsNearlyEqual(Affine.tx, 20.0, 1e-6));
    }

    FRigAxisPlayer Scrub;
    if (CreatePlayer(Scrub, TEXT("scrub create")))
    {
        Scrub.Play(FName(TEXT("move")), false, 0.0f);
        TestTrue(TEXT("scrub set time"), Scrub.SetTrackTime(0, 0.5f));
        Scrub.Apply();
        TestTrue(TEXT("scrub bone"), Scrub.GetBoneWorldTransform(FName(TEXT("child")), Affine));
        TestTrue(TEXT("scrub tx"), FMath::IsNearlyEqual(Affine.tx, 15.0, 1e-6));
    }

    return true;
}

#endif
