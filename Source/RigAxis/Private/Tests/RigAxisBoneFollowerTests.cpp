#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "RigAxisBoneFollowerComponent.h"
#include "RigAxisComponent.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const ANSICHAR* BoneSkeleton = R"JSON({
      "skeleton": {"spine": "4.3"},
      "bones": [
        {"name": "root"},
        {"name": "bone", "parent": "root"}
      ],
      "slots": [
        {"name": "slot", "bone": "bone", "attachment": "image"}
      ],
      "skins": [
        {
          "name": "default",
          "attachments": {
            "slot": {
              "image": {"type": "region", "x": 0, "y": 0, "width": 32, "height": 32}
            }
          }
        }
      ],
      "animations": {
        "move": {
          "bones": {
            "bone": {
              "translate": [
                {"time": 0, "x": 0, "y": 0},
                {"time": 1, "x": 10, "y": 0}
              ]
            }
          }
        }
      }
    })JSON";

    const ANSICHAR* BoneAtlas = "render.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\nimage\n  bounds: 0,0,32,32\n";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigAxisBoneTransformTest, "RigAxis.Native.BoneTransform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisBoneTransformTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URigAxisSkeletonAsset> Asset(NewObject<URigAxisSkeletonAsset>());
    Asset->SkeletonBytes.Append(reinterpret_cast<const uint8*>(BoneSkeleton), FCStringAnsi::Strlen(BoneSkeleton));
    Asset->AtlasBytes.Append(reinterpret_cast<const uint8*>(BoneAtlas), FCStringAnsi::Strlen(BoneAtlas));

    TStrongObjectPtr<URigAxisComponent> Component(NewObject<URigAxisComponent>());
    Component->SkeletonAsset = Asset.Get();
    Component->PixelsPerUnrealUnit = 1.0f;

    TestTrue(TEXT("play"), Component->Play(TEXT("move")));
    Component->Advance(0.5f);

    FTransform Transform;
    TestTrue(TEXT("bone transform"), Component->GetBoneWorldTransform(TEXT("bone"), Transform));
    TestTrue(TEXT("location"), Transform.GetLocation().Equals(FVector(5.0f, 0.0f, 0.0f), 0.01f));
    TestTrue(TEXT("rotation"), Transform.Rotator().IsNearlyZero(0.01f));
    TestTrue(TEXT("scale"), Transform.GetScale3D().Equals(FVector::OneVector, 0.01f));
    TestTrue(
        TEXT("world position"),
        Component->GetBoneWorldPosition(TEXT("bone")).Equals(Transform.GetLocation(), 0.01f));

    Component->SetTrackTime(0, 0.5f);
    TestTrue(
        TEXT("seek updates pose"),
        Component->GetBoneWorldPosition(TEXT("bone")).Equals(FVector(5.0f, 0.0f, 0.0f), 0.01f));
    TestTrue(TEXT("track time"), FMath::IsNearlyEqual(Component->GetTrackTime(0), 0.5f, 1e-4f));
    TestEqual(TEXT("current animation"), Component->GetCurrentAnimation(0), FName(TEXT("move")));
    TestTrue(
        TEXT("animation duration"),
        FMath::IsNearlyEqual(Component->GetAnimationDuration(TEXT("move")), 1.0f, 1e-4f));
    TestTrue(
        TEXT("current animation duration"),
        FMath::IsNearlyEqual(Component->GetCurrentAnimationDuration(0), 1.0f, 1e-4f));

    TestTrue(TEXT("set track alpha"), Component->SetTrackAlpha(0, 0.0f));
    TestTrue(
        TEXT("zero alpha uses setup"),
        Component->GetBoneWorldPosition(TEXT("bone")).Equals(FVector::ZeroVector, 0.01f));
    TestTrue(TEXT("restore track alpha"), Component->SetTrackAlpha(0, 1.0f));

    TestTrue(TEXT("set time scale"), Component->SetTrackTimeScale(0, 2.0f));
    Component->SetTrackTime(0, 0.0f);
    Component->Advance(0.25f);
    TestTrue(
        TEXT("time scale applied"),
        Component->GetBoneWorldPosition(TEXT("bone")).Equals(FVector(5.0f, 0.0f, 0.0f), 0.01f));
    Component->SetTrackTimeScale(0, 1.0f);

    TestFalse(TEXT("missing bone"), Component->GetBoneWorldTransform(TEXT("missing"), Transform));

    Component->bFlipX = true;
    FTransform FlippedX;
    TestTrue(TEXT("flipped bone transform"), Component->GetBoneWorldTransform(TEXT("bone"), FlippedX));
    TestTrue(
        TEXT("flipped location"),
        FlippedX.GetLocation().Equals(FVector(-5.0f, 0.0f, 0.0f), 0.01f));
    TestTrue(
        TEXT("flipped rotation"),
        FlippedX.GetRotation().RotateVector(FVector::ForwardVector).Equals(
            FVector(-1.0f, 0.0f, 0.0f),
            0.01f));
    Component->bFlipX = false;
    Component->bFlipY = true;
    FTransform FlippedY;
    TestTrue(TEXT("flipped Y bone transform"), Component->GetBoneWorldTransform(TEXT("bone"), FlippedY));
    TestTrue(TEXT("flipped Y scale"), FMath::Abs(FlippedY.GetScale3D().Z + 1.0f) < 0.01f);
    Component->bFlipY = false;

    TStrongObjectPtr<URigAxisBoneFollowerComponent> Follower(NewObject<URigAxisBoneFollowerComponent>());
    Follower->Target = Component.Get();
    Follower->Bone = TEXT("bone");
    Follower->bFollowScale = true;
    Follower->Offset = FVector(0.0f, 0.0f, 10.0f);

    FTransform Follow;
    TestTrue(TEXT("follow transform"), Follower->ComputeFollowTransform(Follow));
    TestTrue(TEXT("follower location"), Follow.GetLocation().Equals(FVector(5.0f, 0.0f, 10.0f), 0.01f));
    TestTrue(TEXT("follower rotation"), Follow.Rotator().IsNearlyZero(0.01f));
    return true;
}

#endif
