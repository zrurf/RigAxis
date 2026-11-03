#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RigAxisSkeletonData.h"
#include "RigAxisSkeletonAsset.generated.h"

class UTexture2D;

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

    TSharedPtr<FRigAxisSkeletonData> GetData() const;
    int32 GetVersion() const { return Version; }
    const FString& GetLoadError() const { return LoadError; }

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
