#pragma once

#include "CoreMinimal.h"
#include "RigAxis/RigAxis.h"

class FRigAxisPlayer;
class FRigAxisSkeletonData;

enum class ERigAxisBlend : uint8
{
    Normal = 0,
    Additive = 1,
    Multiply = 2,
    Screen = 3
};

struct FRigAxisVertex
{
    FVector2f Position = FVector2f(0.0f, 0.0f);
    FVector2f UV = FVector2f(0.0f, 0.0f);
    FLinearColor Light = FLinearColor::White;
    FLinearColor Dark = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
};

struct FRigAxisRenderBatch
{
    uint32 Source = 0;
    uint32 Page = 0;
    ERigAxisBlend Blend = ERigAxisBlend::Normal;
    TArray<FRigAxisVertex> Vertices;
    TArray<uint32> Indices;
};

class FRigAxisRenderData
{
public:
    ~FRigAxisRenderData();

    bool Create(FString& OutError);
    void Destroy();

    bool Build(const FRigAxisSkeletonData& Data, const FRigAxisPlayer& Player, const FLinearColor& Tint);

    int32 GetBatchCount() const { return Batches.Num(); }
    const TArray<FRigAxisRenderBatch>& GetBatches() const { return Batches; }
    bool IsValid() const { return Render != nullptr; }

private:
    RigAxisRender* Render = nullptr;
    TArray<FRigAxisRenderBatch> Batches;
};
