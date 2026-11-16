#pragma once

#include "CoreMinimal.h"
#include "RigAxisTypes.generated.h"

class UMaterialInterface;

/** Selects the plane a skeleton mesh is built in. */
UENUM(BlueprintType)
enum class ERigAxisPlane : uint8
{
    /** XZ plane, facing -Y. */
    XZ UMETA(DisplayName = "XZ (side view)"),
    /** XY plane, facing +Z. */
    XY UMETA(DisplayName = "XY (top-down)")
};

/** Selects the clock used to advance playback. */
UENUM(BlueprintType)
enum class ERigAxisUpdateMode : uint8
{
    /** Advances with the game delta time. */
    Scaled,
    /** Advances with real time, ignoring time dilation. */
    Unscaled,
    /** Does not advance automatically; call Advance(). */
    Manual
};

/** Blend mode of an atlas attachment. */
UENUM(BlueprintType)
enum class ERigAxisBlend : uint8
{
    Normal = 0,
    Additive = 1,
    Multiply = 2,
    Screen = 3
};

/** Animation event delivered by URigAxisComponent. */
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

/** Materials per blend mode. Unset entries use the plugin default or MeshMaterial. */
USTRUCT(BlueprintType)
struct FRigAxisMaterialSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    TObjectPtr<UMaterialInterface> Normal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    TObjectPtr<UMaterialInterface> Additive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    TObjectPtr<UMaterialInterface> Multiply;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RigAxis")
    TObjectPtr<UMaterialInterface> Screen;
};

struct FRigAxisMeshSection
{
    uint32 Source = 0;
    uint32 Page = 0;
    ERigAxisBlend Blend = ERigAxisBlend::Normal;
    FLinearColor Dark = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
    TArray<FVector> Positions;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<int32> Indices;
};
