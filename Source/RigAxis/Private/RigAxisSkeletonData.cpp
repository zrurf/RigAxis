#include "RigAxisSkeletonData.h"

#include <cstddef>

#include "RigAxis/RigAxis.h"

namespace
{
    FString ReadUtf8(const char* Text)
    {
        return Text ? FString(UTF8_TO_TCHAR(Text)) : FString();
    }

    FString LastError()
    {
        return ReadUtf8(rigaxis_last_error());
    }
}

FRigAxisSkeletonData::~FRigAxisSkeletonData()
{
    if (Lookup)
    {
        rigaxis_lookup_free(Lookup);
        Lookup = nullptr;
    }
    if (Atlas)
    {
        rigaxis_atlas_free(Atlas);
        Atlas = nullptr;
    }
    if (Skeleton)
    {
        rigaxis_skeleton_free(Skeleton);
        Skeleton = nullptr;
    }
}

TSharedPtr<FRigAxisSkeletonData> FRigAxisSkeletonData::Create(
    const TArray<uint8>& SkeletonBytes,
    const TArray<uint8>& AtlasBytes,
    FString& OutError)
{
    check(IsInGameThread());

    TSharedPtr<FRigAxisSkeletonData> Data(new FRigAxisSkeletonData());
    if (!Data->Initialize(SkeletonBytes, AtlasBytes, OutError))
    {
        return nullptr;
    }
    return Data;
}

bool FRigAxisSkeletonData::Initialize(
    const TArray<uint8>& SkeletonBytes,
    const TArray<uint8>& AtlasBytes,
    FString& OutError)
{
    RigAxisStatus Status = rigaxis_skeleton_create(
        SkeletonBytes.GetData(),
        static_cast<size_t>(SkeletonBytes.Num()),
        &Skeleton);
    if (Status != RIGAXIS_OK)
    {
        OutError = LastError();
        Skeleton = nullptr;
        return false;
    }

    Status = rigaxis_atlas_create(
        AtlasBytes.GetData(),
        static_cast<size_t>(AtlasBytes.Num()),
        &Atlas);
    if (Status != RIGAXIS_OK)
    {
        OutError = LastError();
        rigaxis_skeleton_free(Skeleton);
        Skeleton = nullptr;
        return false;
    }

    Status = rigaxis_lookup_create(Skeleton, Atlas, &Lookup);
    if (Status != RIGAXIS_OK)
    {
        OutError = LastError();
        rigaxis_atlas_free(Atlas);
        Atlas = nullptr;
        rigaxis_skeleton_free(Skeleton);
        Skeleton = nullptr;
        return false;
    }

    Format = ReadUtf8(rigaxis_skeleton_format(Skeleton));

    const uint32 BoneCount = rigaxis_skeleton_bone_count(Skeleton);
    BoneNames.Reserve(static_cast<int32>(BoneCount));
    for (uint32 Index = 0; Index < BoneCount; ++Index)
    {
        BoneNames.Add(ReadUtf8(rigaxis_skeleton_bone_name(Skeleton, Index)));
    }

    const uint32 AnimationCount = rigaxis_skeleton_animation_count(Skeleton);
    AnimationNames.Reserve(static_cast<int32>(AnimationCount));
    AnimationDurations.Reserve(static_cast<int32>(AnimationCount));
    for (uint32 Index = 0; Index < AnimationCount; ++Index)
    {
        AnimationNames.Add(ReadUtf8(rigaxis_skeleton_animation_name(Skeleton, Index)));
        AnimationDurations.Add(static_cast<float>(
            rigaxis_skeleton_animation_duration(Skeleton, Index)));
    }

    const uint32 SkinCount = rigaxis_skeleton_skin_count(Skeleton);
    SkinNames.Reserve(static_cast<int32>(SkinCount));
    for (uint32 Index = 0; Index < SkinCount; ++Index)
    {
        SkinNames.Add(ReadUtf8(rigaxis_skeleton_skin_name(Skeleton, Index)));
    }

    const uint32 WarningCount = rigaxis_skeleton_warning_count(Skeleton);
    Warnings.Reserve(static_cast<int32>(WarningCount));
    for (uint32 Index = 0; Index < WarningCount; ++Index)
    {
        Warnings.Add(ReadUtf8(rigaxis_skeleton_warning_at(Skeleton, Index)));
    }

    const uint32 PageCount = rigaxis_atlas_page_count(Atlas);
    AtlasPageNames.Reserve(static_cast<int32>(PageCount));
    for (uint32 Index = 0; Index < PageCount; ++Index)
    {
        AtlasPageNames.Add(ReadUtf8(rigaxis_atlas_page_name(Atlas, Index)));
    }

    const uint32 MissingCount = rigaxis_lookup_missing_count(Lookup);
    MissingRegions.Reserve(static_cast<int32>(MissingCount));
    for (uint32 Index = 0; Index < MissingCount; ++Index)
    {
        MissingRegions.Add(ReadUtf8(rigaxis_lookup_missing_at(Lookup, Index)));
    }

    for (int32 Index = 0; Index < BoneNames.Num(); ++Index)
    {
        BoneIndexByName.Add(FName(*BoneNames[Index]), Index);
    }
    for (int32 Index = 0; Index < AnimationNames.Num(); ++Index)
    {
        AnimationIndexByName.Add(FName(*AnimationNames[Index]), Index);
    }
    for (int32 Index = 0; Index < SkinNames.Num(); ++Index)
    {
        SkinIndexByName.Add(FName(*SkinNames[Index]), Index);
    }

    return true;
}

int32 FRigAxisSkeletonData::FindBone(FName Name) const
{
    const int32* Index = BoneIndexByName.Find(Name);
    return Index ? *Index : INDEX_NONE;
}

int32 FRigAxisSkeletonData::FindAnimation(FName Name) const
{
    const int32* Index = AnimationIndexByName.Find(Name);
    return Index ? *Index : INDEX_NONE;
}

int32 FRigAxisSkeletonData::FindSkin(FName Name) const
{
    const int32* Index = SkinIndexByName.Find(Name);
    return Index ? *Index : INDEX_NONE;
}

float FRigAxisSkeletonData::GetAnimationDuration(FName Name) const
{
    const int32 Index = FindAnimation(Name);
    return AnimationDurations.IsValidIndex(Index) ? AnimationDurations[Index] : 0.0f;
}
