#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RigAxisSkeletonData.h"
#include "RigAxisSkeletonAsset.generated.h"

class UTexture2D;

/// Imported skeleton and atlas data. Runtime data is parsed on first use and rebuilt after changes.
UCLASS(BlueprintType)
class RIGAXIS_API URigAxisSkeletonAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TArray<uint8> SkeletonBytes;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TArray<uint8> AtlasBytes;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TArray<FString> AtlasPageNames;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TArray<TObjectPtr<UTexture2D>> AtlasPages;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    bool bPremultipliedAlpha = false;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TArray<FString> AdditionalSourceFiles;

    /// Returns the parsed runtime data, parsing it on first use.
    TSharedPtr<FRigAxisSkeletonData> GetData() const;
    /// Increments whenever the runtime data is rebuilt.
    int32 GetVersion() const { return Version; }
    /// Returns the parse error when GetData() is invalid.
    const FString& GetLoadError() const { return LoadError; }

    /// Returns the bone names, or an empty array when the asset has no valid data.
    UFUNCTION(BlueprintPure, Category = "RigAxis|Skeleton")
    TArray<FName> GetBoneNames() const;

    /// Returns the animation names, or an empty array when the asset has no valid data.
    UFUNCTION(BlueprintPure, Category = "RigAxis|Skeleton")
    TArray<FName> GetAnimationNames() const;

    /// Returns the skin names, or an empty array when the asset has no valid data.
    UFUNCTION(BlueprintPure, Category = "RigAxis|Skeleton")
    TArray<FName> GetSkinNames() const;

    /// Returns the animation duration in seconds, or 0 when the animation is missing.
    UFUNCTION(BlueprintPure, Category = "RigAxis|Skeleton")
    float GetAnimationDuration(FName Animation) const;

    /// Rebuilds the runtime data from the stored bytes and bumps GetVersion().
    void RebuildData() const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    mutable TSharedPtr<FRigAxisSkeletonData> Data;
    mutable FString LoadError;
    mutable int32 Version = 0;
    mutable bool bParsed = false;
};
