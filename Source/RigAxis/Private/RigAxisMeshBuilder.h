#pragma once

#include "CoreMinimal.h"
#include "RigAxisTypes.h"

class FRigAxisRenderData;

class FRigAxisMeshBuilder
{
public:
    static void Build(
        const FRigAxisRenderData& RenderData,
        ERigAxisPlane Plane,
        float PixelsPerUnrealUnit,
        bool bFlipX,
        bool bFlipY,
        TArray<FRigAxisMeshSection>& OutSections);
};
