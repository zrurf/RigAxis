#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RigAxisBoneFollowerComponent.generated.h"

class URigAxisComponent;

/// Follows a bone of a URigAxisComponent.
/// Position, rotation and scale can be followed independently.
UCLASS(ClassGroup = (RigAxis), meta = (BlueprintSpawnableComponent))
class RIGAXIS_API URigAxisBoneFollowerComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    URigAxisBoneFollowerComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    TObjectPtr<URigAxisComponent> Target;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FName Bone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bFollowPosition = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bFollowRotation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    bool bFollowScale = false;

    /// Offset in the bone's local space.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FVector Offset = FVector::ZeroVector;

    /// Rotation offset in the bone's local space.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FRotator RotationOffset = FRotator::ZeroRotator;

    /// Computes the followed transform without moving the component. Returns false when the target or bone is unavailable.
    bool ComputeFollowTransform(FTransform& OutTransform) const;

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    void RefreshTarget();

    TWeakObjectPtr<URigAxisComponent> BoundTarget;
    FName BoundBone;
    bool bWarned = false;
};
