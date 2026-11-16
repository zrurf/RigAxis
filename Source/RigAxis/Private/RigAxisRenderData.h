#pragma once

#include "CoreMinimal.h"
#include "RigAxis/RigAxis.h"
#include "RigAxisTypes.h"

class FRigAxisPlayer;
class FRigAxisSkeletonData;

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
    bool BuildScene(RigAxisScene* Scene, FString& OutError);

    int32 GetBatchCount() const { return Batches.Num(); }
    const TArray<FRigAxisRenderBatch>& GetBatches() const { return Batches; }
    bool IsValid() const { return Render != nullptr; }

private:
    bool CopyBatches();

    RigAxisRender* Render = nullptr;
    TArray<FRigAxisRenderBatch> Batches;
};
