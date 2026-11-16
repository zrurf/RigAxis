#pragma once

#include "CoreMinimal.h"

struct RigAxisSkeleton;
struct RigAxisAtlas;
struct RigAxisLookup;

/** Owns the native runtime handles. Create and use on the game thread. */
class RIGAXIS_API FRigAxisSkeletonData
{
public:
    ~FRigAxisSkeletonData();

    FRigAxisSkeletonData(const FRigAxisSkeletonData&) = delete;
    FRigAxisSkeletonData& operator=(const FRigAxisSkeletonData&) = delete;

    /// Parses skeleton and atlas bytes. Returns null and fills OutError on failure.
    static TSharedPtr<FRigAxisSkeletonData> Create(
        const TArray<uint8>& SkeletonBytes,
        const TArray<uint8>& AtlasBytes,
        FString& OutError);

    RigAxisSkeleton* GetSkeleton() const { return Skeleton; }
    RigAxisAtlas* GetAtlas() const { return Atlas; }
    RigAxisLookup* GetLookup() const { return Lookup; }

    /// Skeleton format version reported by the export.
    const FString& GetFormat() const { return Format; }
    const TArray<FString>& GetBoneNames() const { return BoneNames; }
    const TArray<FString>& GetAnimationNames() const { return AnimationNames; }
    /// Animation durations in seconds, in GetAnimationNames order.
    const TArray<float>& GetAnimationDurations() const { return AnimationDurations; }
    const TArray<FString>& GetSkinNames() const { return SkinNames; }
    const TArray<FString>& GetAtlasPageNames() const { return AtlasPageNames; }
    /// Non-fatal parse warnings.
    const TArray<FString>& GetWarnings() const { return Warnings; }
    /// Atlas image names referenced by the skeleton but missing from the atlas.
    const TArray<FString>& GetMissingRegions() const { return MissingRegions; }

    /// Returns the index, or INDEX_NONE when the name is missing.
    int32 FindBone(FName Name) const;
    int32 FindAnimation(FName Name) const;
    int32 FindSkin(FName Name) const;

    /// Returns the animation duration in seconds, or 0 when the animation is missing.
    float GetAnimationDuration(FName Name) const;

private:
    FRigAxisSkeletonData() = default;

    bool Initialize(
        const TArray<uint8>& SkeletonBytes,
        const TArray<uint8>& AtlasBytes,
        FString& OutError);

    RigAxisSkeleton* Skeleton = nullptr;
    RigAxisAtlas* Atlas = nullptr;
    RigAxisLookup* Lookup = nullptr;

    FString Format;
    TArray<FString> BoneNames;
    TArray<FString> AnimationNames;
    TArray<float> AnimationDurations;
    TArray<FString> SkinNames;
    TArray<FString> AtlasPageNames;
    TArray<FString> Warnings;
    TArray<FString> MissingRegions;

    TMap<FName, int32> BoneIndexByName;
    TMap<FName, int32> AnimationIndexByName;
    TMap<FName, int32> SkinIndexByName;
};
