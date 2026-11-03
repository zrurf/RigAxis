#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Misc/AutomationTest.h"
#include "RigAxisMeshBuilder.h"
#include "RigAxisPlayer.h"
#include "RigAxisRenderData.h"
#include "RigAxisSkeletonAsset.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    const ANSICHAR* MeshSkeleton = R"JSON({
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

    const ANSICHAR* MeshAtlas = "render.png\nsize: 64,64\nformat: RGBA8888\nfilter: Linear,Linear\nrepeat: none\nimage\n  bounds: 0,0,32,32\n";
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRigAxisMeshBuilderTest, "RigAxis.Native.Mesh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRigAxisMeshBuilderTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URigAxisSkeletonAsset> Asset(NewObject<URigAxisSkeletonAsset>());
    Asset->SkeletonBytes.Append(reinterpret_cast<const uint8*>(MeshSkeleton), FCStringAnsi::Strlen(MeshSkeleton));
    Asset->AtlasBytes.Append(reinterpret_cast<const uint8*>(MeshAtlas), FCStringAnsi::Strlen(MeshAtlas));

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

    TArray<FRigAxisMeshSection> Sections;
    FRigAxisMeshBuilder::Build(Render, ERigAxisPlane::XZ, 1.0f, false, false, Sections);
    TestEqual(TEXT("section count"), Sections.Num(), 1);
    if (Sections.Num() != 1)
    {
        return false;
    }

    const FRigAxisMeshSection& Section = Sections[0];
    TestEqual(TEXT("position count"), Section.Positions.Num(), 4);
    TestEqual(TEXT("uv count"), Section.UVs.Num(), 4);
    TestEqual(TEXT("color count"), Section.Colors.Num(), 4);
    TestEqual(TEXT("index count"), Section.Indices.Num(), 6);

    if (Section.Positions.Num() == 4)
    {
        TestTrue(TEXT("xz p0"), Section.Positions[0].Equals(FVector(-16.0f, 0.0f, 16.0f), 1e-4f));
        TestTrue(TEXT("xz p1"), Section.Positions[1].Equals(FVector(16.0f, 0.0f, 16.0f), 1e-4f));
        TestTrue(TEXT("xz p2"), Section.Positions[2].Equals(FVector(16.0f, 0.0f, -16.0f), 1e-4f));
        TestTrue(TEXT("xz p3"), Section.Positions[3].Equals(FVector(-16.0f, 0.0f, -16.0f), 1e-4f));
    }

    if (Section.UVs.Num() == 4)
    {
        TestTrue(TEXT("uv0"), Section.UVs[0].Equals(FVector2D(0.0f, 0.0f), 1e-4));
        TestTrue(TEXT("uv1"), Section.UVs[1].Equals(FVector2D(0.5f, 0.0f), 1e-4));
        TestTrue(TEXT("uv2"), Section.UVs[2].Equals(FVector2D(0.5f, 0.5f), 1e-4));
        TestTrue(TEXT("uv3"), Section.UVs[3].Equals(FVector2D(0.0f, 0.5f), 1e-4));
    }

    if (Section.Indices.Num() == 6)
    {
        const int32 Expected[] = { 0, 2, 1, 0, 3, 2 };
        for (int32 Index = 0; Index < 6; ++Index)
        {
            TestEqual(
                *FString::Printf(TEXT("winding %d"), Index),
                Section.Indices[Index],
                Expected[Index]);
        }
    }

    FRigAxisMeshBuilder::Build(Render, ERigAxisPlane::XZ, 1.0f, true, false, Sections);
    if (Sections.Num() == 1 && Sections[0].Positions.Num() == 4)
    {
        TestTrue(TEXT("flip x0"), Sections[0].Positions[0].Equals(FVector(16.0f, 0.0f, 16.0f), 1e-4f));
        TestTrue(TEXT("flip x1"), Sections[0].Positions[1].Equals(FVector(-16.0f, 0.0f, 16.0f), 1e-4f));
    }

    if (Sections.Num() == 1 && Sections[0].Indices.Num() == 6)
    {
        const int32 Expected[] = { 0, 1, 2, 0, 2, 3 };
        for (int32 Index = 0; Index < 6; ++Index)
        {
            TestEqual(
                *FString::Printf(TEXT("flip x winding %d"), Index),
                Sections[0].Indices[Index],
                Expected[Index]);
        }
    }

    FRigAxisMeshBuilder::Build(Render, ERigAxisPlane::XY, 1.0f, false, false, Sections);
    if (Sections.Num() == 1 && Sections[0].Positions.Num() == 4)
    {
        TestTrue(TEXT("xy p0"), Sections[0].Positions[0].Equals(FVector(-16.0f, 16.0f, 0.0f), 1e-4f));
    }

    FRigAxisMeshBuilder::Build(Render, ERigAxisPlane::XZ, 2.0f, false, false, Sections);
    if (Sections.Num() == 1 && Sections[0].Positions.Num() == 4)
    {
        TestTrue(TEXT("scale p0"), Sections[0].Positions[0].Equals(FVector(-8.0f, 0.0f, 8.0f), 1e-4f));
    }

    return true;
}

#endif
