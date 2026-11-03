#include "RigAxisSceneAsset.h"

URigAxisSkeletonAsset* URigAxisSceneAsset::FindActorSkeleton(const FString& ActorId) const
{
    const TObjectPtr<URigAxisSkeletonAsset>* Found = ActorSkeletons.Find(ActorId);
    return Found ? Found->Get() : nullptr;
}
