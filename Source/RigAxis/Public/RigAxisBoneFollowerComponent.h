#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RigAxisBoneFollowerComponent.generated.h"

class URigAxisComponent;

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FVector Offset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    FRotator RotationOffset = FRotator::ZeroRotator;

    bool ComputeFollowTransform(FTransform& OutTransform) const;

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    bool bWarned = false;
};
