#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "RigAxisTypes.h"

class FPrimitiveSceneProxy;
class UMaterialInterface;
class UMeshComponent;

struct FRigAxisSceneState
{
    FCriticalSection Lock;
    TArray<FRigAxisMeshSection> Sections;
    FVector Normal = FVector(0.0f, 0.0f, 1.0f);
};

using FRigAxisSceneStateRef = TSharedRef<FRigAxisSceneState, ESPMode::ThreadSafe>;

FPrimitiveSceneProxy* CreateRigAxisSceneProxy(
    const UMeshComponent* Component,
    const FRigAxisSceneStateRef& State,
    const TArray<UMaterialInterface*>& Materials);
