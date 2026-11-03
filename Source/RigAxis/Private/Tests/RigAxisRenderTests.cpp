#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "RigAxisPlayer.h"
#include "RigAxisRenderData.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const ANSICHAR* RenderSkeleton = R"JSON({
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
      "animations": {}
    })JSON";

    const ANSICHAR* RenderAtlas = "render.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\nimage\n  bounds: 0,0,32,32\n";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigAxisRenderTest, "RigAxis.Native.Render", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisRenderTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URigAxisSkeletonAsset> Asset(NewObject<URigAxisSkeletonAsset>());
    Asset->SkeletonBytes.Append(reinterpret_cast<const uint8*>(RenderSkeleton), FCStringAnsi::Strlen(RenderSkeleton));
    Asset->AtlasBytes.Append(reinterpret_cast<const uint8*>(RenderAtlas), FCStringAnsi::Strlen(RenderAtlas));

    TSharedPtr<FRigAxisSkeletonData> Data = Asset->GetData();
    TestTrue(TEXT("data valid"), Data.IsValid());
    if (!Data.IsValid())
    {
        AddError(FString::Printf(TEXT("asset load failed: %s"), *Asset->GetLoadError()));
        return false;
    }

    FRigAxisPlayer Player;
    FString Error;
    TestTrue(TEXT("player create"), Player.Create(Data, Error));
    if (!Player.IsValid())
    {
        AddError(FString::Printf(TEXT("player create failed: %s"), *Error));
        return false;
    }
    Player.Apply();

    FRigAxisRenderData Render;
    TestTrue(TEXT("render create"), Render.Create(Error));
    if (!Render.IsValid())
    {
        AddError(FString::Printf(TEXT("render create failed: %s"), *Error));
        return false;
    }

    TestTrue(TEXT("render build"), Render.Build(*Data, Player, FLinearColor::White));
    TestEqual(TEXT("batch count"), Render.GetBatchCount(), 1);
    if (Render.GetBatchCount() != 1)
    {
        return false;
    }

    const FRigAxisRenderBatch& Batch = Render.GetBatches()[0];
    TestEqual(TEXT("vertex count"), Batch.Vertices.Num(), 4);
    TestEqual(TEXT("index count"), Batch.Indices.Num(), 6);
    TestEqual(TEXT("page"), int32(Batch.Page), 0);
    TestTrue(TEXT("blend normal"), Batch.Blend == ERigAxisBlend::Normal);

    if (Batch.Indices.Num() == 6)
    {
        const uint32 Expected[] = { 0, 1, 2, 0, 2, 3 };
        for (int32 Index = 0; Index < 6; ++Index)
        {
            TestEqual(FString::Printf(TEXT("index %d"), Index), Batch.Indices[Index], Expected[Index]);
        }
    }

    for (const FRigAxisVertex& Vertex : Batch.Vertices)
    {
        TestTrue(TEXT("x range"), FMath::Abs(Vertex.Position.X) <= 16.001f);
        TestTrue(TEXT("y range"), FMath::Abs(Vertex.Position.Y) <= 16.001f);
        TestTrue(TEXT("u range"), Vertex.UV.X >= 0.0f && Vertex.UV.X <= 1.0f);
        TestTrue(TEXT("v range"), Vertex.UV.Y >= 0.0f && Vertex.UV.Y <= 1.0f);
        TestTrue(TEXT("light"), FMath::IsNearlyEqual(Vertex.Light.R, 1.0f, 1e-5f));
    }

    TestTrue(TEXT("tinted build"), Render.Build(*Data, Player, FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)));
    if (Render.GetBatchCount() == 1)
    {
        const FRigAxisRenderBatch& Tinted = Render.GetBatches()[0];
        if (Tinted.Vertices.Num() > 0)
        {
            TestTrue(TEXT("tint"), FMath::IsNearlyEqual(Tinted.Vertices[0].Light.R, 0.5f, 1e-5f));
        }
    }

    return true;
}

#endif
