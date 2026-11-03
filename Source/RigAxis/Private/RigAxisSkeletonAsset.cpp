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
