#include "RigAxisSkeletonAsset.h"

#include "RigAxisLog.h"

#if WITH_EDITOR
void URigAxisSkeletonAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    RebuildData();
}
#endif

TSharedPtr<FRigAxisSkeletonData> URigAxisSkeletonAsset::GetData() const
{
    if (!bParsed)
    {
        RebuildData();
    }
    return Data;
}

TArray<FName> URigAxisSkeletonAsset::GetBoneNames() const
{
    TArray<FName> Names;
    const TSharedPtr<FRigAxisSkeletonData> Skeleton = GetData();
    if (!Skeleton.IsValid())
    {
        return Names;
    }

    Names.Reserve(Skeleton->GetBoneNames().Num());
    for (const FString& Name : Skeleton->GetBoneNames())
    {
        Names.Add(FName(*Name));
    }
    return Names;
}

TArray<FName> URigAxisSkeletonAsset::GetAnimationNames() const
{
    TArray<FName> Names;
    const TSharedPtr<FRigAxisSkeletonData> Skeleton = GetData();
    if (!Skeleton.IsValid())
    {
        return Names;
    }

    Names.Reserve(Skeleton->GetAnimationNames().Num());
    for (const FString& Name : Skeleton->GetAnimationNames())
    {
        Names.Add(FName(*Name));
    }
    return Names;
}

TArray<FName> URigAxisSkeletonAsset::GetSkinNames() const
{
    TArray<FName> Names;
    const TSharedPtr<FRigAxisSkeletonData> Skeleton = GetData();
    if (!Skeleton.IsValid())
    {
        return Names;
    }

    Names.Reserve(Skeleton->GetSkinNames().Num());
    for (const FString& Name : Skeleton->GetSkinNames())
    {
        Names.Add(FName(*Name));
    }
    return Names;
}

float URigAxisSkeletonAsset::GetAnimationDuration(FName Animation) const
{
    const TSharedPtr<FRigAxisSkeletonData> Skeleton = GetData();
    return Skeleton.IsValid() ? Skeleton->GetAnimationDuration(Animation) : 0.0f;
}

void URigAxisSkeletonAsset::RebuildData() const
{
    Data.Reset();
    LoadError.Empty();
    bParsed = true;
    ++Version;

    if (SkeletonBytes.Num() == 0)
    {
        LoadError = TEXT("No skeleton data.");
        UE_LOG(LogRigAxis, Error, TEXT("%s: %s"), *GetPathName(), *LoadError);
        return;
    }

    FString Error;
    Data = FRigAxisSkeletonData::Create(SkeletonBytes, AtlasBytes, Error);
    if (!Data.IsValid())
    {
        LoadError = Error;
        UE_LOG(LogRigAxis, Error, TEXT("%s: %s"), *GetPathName(), *LoadError);
        return;
    }

    for (const FString& Warning : Data->GetWarnings())
    {
        UE_LOG(LogRigAxis, Warning, TEXT("%s: %s"), *GetPathName(), *Warning);
    }
    for (const FString& Missing : Data->GetMissingRegions())
    {
        UE_LOG(LogRigAxis, Warning, TEXT("%s: missing atlas region '%s'"), *GetPathName(), *Missing);
    }
}
