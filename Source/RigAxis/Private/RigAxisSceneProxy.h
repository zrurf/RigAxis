#pragma once

#include "CoreMinimal.h"

class FPrimitiveSceneProxy;
class UMaterialInterface;
class URigAxisComponent;
struct FRigAxisMeshSection;

FPrimitiveSceneProxy* CreateRigAxisSceneProxy(
    const URigAxisComponent* Component,
    const TArray<FRigAxisMeshSection>& Sections,
    const TArray<UMaterialInterface*>& Materials,
    const FVector& Normal);
