#pragma once

#include "CoreMinimal.h"
#include "RigAxisTypes.generated.h"

UENUM(BlueprintType)
enum class ERigAxisPlane : uint8
{
    XZ UMETA(DisplayName = "XZ (side view)"),
    XY UMETA(DisplayName = "XY (top-down)")
};

UENUM(BlueprintType)
enum class ERigAxisUpdateMode : uint8
{
    Scaled,
    Unscaled,
    Manual
};

UENUM(BlueprintType)
enum class ERigAxisBlend : uint8
{
    Normal = 0,
    Additive = 1,
    Multiply = 2,
    Screen = 3
};

USTRUCT(BlueprintType)
struct FRigAxisAnimationEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    FName Name;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    FName Animation;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    int32 Track = 0;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    float Time = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    int32 LoopIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    int32 IntValue = 0;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    float FloatValue = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    FString StringValue;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    FName Audio;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    float Volume = 1.0f;

    UPROPERTY(BlueprintReadOnly, Category = "RigAxis")
    float Balance = 0.0f;
};

struct FRigAxisMeshSection
{
    uint32 Source = 0;
    uint32 Page = 0;
    ERigAxisBlend Blend = ERigAxisBlend::Normal;
    TArray<FVector> Positions;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<int32> Indices;
};
