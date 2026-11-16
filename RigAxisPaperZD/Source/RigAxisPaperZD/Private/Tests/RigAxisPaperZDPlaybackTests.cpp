#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "AnimSequences/Players/PaperZDAnimationPlaybackData.h"
#include "Misc/AutomationTest.h"
#include "RigAxisComponent.h"
#include "RigAxisPaperZDAnimSequence.h"
#include "RigAxisPaperZDPlaybackHandle.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const ANSICHAR* PaperZDSkeleton = R"JSON({
      "skeleton": {"spine": "4.3"},
      "bones": [
        {"name": "root"},
        {"name": "bone", "parent": "root"}
      ],
      "slots": [],
      "skins": [],
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

    const ANSICHAR* PaperZDAtlas =
        "empty.png\nsize: 8,8\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\n";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRigAxisPaperZDPlaybackTest,
    "RigAxis.PaperZD.PlaybackHandle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisPaperZDPlaybackTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URigAxisSkeletonAsset> Asset(NewObject<URigAxisSkeletonAsset>());
    Asset->SkeletonBytes.Append(
        reinterpret_cast<const uint8*>(PaperZDSkeleton),
        FCStringAnsi::Strlen(PaperZDSkeleton));
    Asset->AtlasBytes.Append(
        reinterpret_cast<const uint8*>(PaperZDAtlas),
        FCStringAnsi::Strlen(PaperZDAtlas));

    TStrongObjectPtr<URigAxisComponent> Component(NewObject<URigAxisComponent>());
    Component->SkeletonAsset = Asset.Get();
    Component->PixelsPerUnrealUnit = 1.0f;

    TStrongObjectPtr<URigAxisPaperZDAnimSequence> Sequence(
        NewObject<URigAxisPaperZDAnimSequence>());
    Sequence->AnimationName = TEXT("move");
    Sequence->Duration = 1.0f;

    TStrongObjectPtr<URigAxisPaperZDPlaybackHandle> Handle(
        NewObject<URigAxisPaperZDPlaybackHandle>());
    Handle->ConfigureRenderComponent(Component.Get());

    TestTrue(TEXT("manual update"), Component->UpdateMode == ERigAxisUpdateMode::Manual);
    TestFalse(TEXT("autoplay disabled"), Component->bAutoPlay);

    FPaperZDAnimationPlaybackData Playback;
    Playback.WeightedAnimations.Add(FPaperZDWeightedAnimation(Sequence.Get(), 0.5f, 1.0f));
    Handle->UpdateRenderPlayback(Component.Get(), Playback);

    FTransform Transform;
    TestTrue(TEXT("bone transform"), Component->GetBoneWorldTransform(TEXT("bone"), Transform));
    TestTrue(
        TEXT("time sampled"),
        Transform.GetLocation().Equals(FVector(5.0f, 0.0f, 0.0f), 0.01f));

    Playback.WeightedAnimations[0].PlaybackTime = 0.75f;
    Handle->UpdateRenderPlayback(Component.Get(), Playback);
    TestTrue(TEXT("second transform"), Component->GetBoneWorldTransform(TEXT("bone"), Transform));
    TestTrue(
        TEXT("second time sampled"),
        Transform.GetLocation().Equals(FVector(7.5f, 0.0f, 0.0f), 0.01f));

    return true;
}

#endif
