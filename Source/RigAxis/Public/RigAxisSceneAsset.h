#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RigAxisSceneAsset.generated.h"

class URigAxisSkeletonAsset;

/// Imported scene manifest and its stage and actor skeleton assets.
UCLASS(BlueprintType)
class RIGAXIS_API URigAxisSceneAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TArray<uint8> ManifestBytes;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TObjectPtr<URigAxisSkeletonAsset> Stage;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TMap<FString, TObjectPtr<URigAxisSkeletonAsset>> ActorSkeletons;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    FString StagePath;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TMap<FString, FString> ActorSkeletonPaths;

    UPROPERTY(VisibleAnywhere, Category = "RigAxis")
    TArray<FString> AdditionalSourceFiles;

    /// Returns the skeleton asset for a scene actor id, or null.
    URigAxisSkeletonAsset* FindActorSkeleton(const FString& ActorId) const;
};
