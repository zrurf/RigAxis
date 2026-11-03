#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "RigAxisLog.h"
#include "RigAxisMeshBuilder.h"
#include "RigAxisPlayer.h"
#include "RigAxisRenderData.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const ANSICHAR* PerformanceSkeleton = R"JSON({
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

    const ANSICHAR* PerformanceAtlas = "render.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\nimage\n  bounds: 0,0,32,32\n";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigAxisPerformanceTest, "RigAxis.Native.Performance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisPerformanceTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URigAxisSkeletonAsset> Asset(NewObject<URigAxisSkeletonAsset>());
    Asset->SkeletonBytes.Append(reinterpret_cast<const uint8*>(PerformanceSkeleton), FCStringAnsi::Strlen(PerformanceSkeleton));
    Asset->AtlasBytes.Append(reinterpret_cast<const uint8*>(PerformanceAtlas), FCStringAnsi::Strlen(PerformanceAtlas));

    TSharedPtr<FRigAxisSkeletonData> Data = Asset->GetData();
    TestTrue(TEXT("data valid"), Data.IsValid());
    if (!Data.IsValid())
    {
        return false;
    }

    FRigAxisPlayer Player;
    FString Error;
    TestTrue(TEXT("player create"), Player.Create(Data, Error));
    if (!Player.IsValid())
    {
        return false;
    }

    FRigAxisRenderData Render;
    TestTrue(TEXT("render create"), Render.Create(Error));
    if (!Render.IsValid())
    {
        return false;
    }

    TestTrue(TEXT("play"), Player.Play(TEXT("move"), true, 0.0f));

    constexpr int32 Frames = 600;
    TArray<FRigAxisMeshSection> Sections;
    const double Start = FPlatformTime::Seconds();
    for (int32 Frame = 0; Frame < Frames; ++Frame)
    {
        Player.Update(1.0 / 60.0);
        Player.Apply();
        if (Render.Build(*Data, Player, FLinearColor::White))
        {
            FRigAxisMeshBuilder::Build(Render, ERigAxisPlane::XZ, 1.0f, false, false, Sections);
        }
    }
    const double Elapsed = FPlatformTime::Seconds() - Start;
    const double PerFrameMs = Elapsed / Frames * 1000.0;

    UE_LOG(LogRigAxis, Display, TEXT("Performance: %d frames in %.2f ms (%.3f ms/frame)"),
        Frames,
        Elapsed * 1000.0,
        PerFrameMs);
    TestTrue(TEXT("per-frame budget"), PerFrameMs < 10.0);
    return true;
}

#endif
