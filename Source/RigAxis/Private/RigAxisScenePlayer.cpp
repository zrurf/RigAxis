#include "RigAxisScenePlayer.h"

#include "RigAxisSceneAsset.h"
#include "RigAxisSkeletonAsset.h"

namespace
{
    FString LastError()
    {
        const char* Text = rigaxis_last_error();
        return Text ? FString(UTF8_TO_TCHAR(Text)) : FString();
    }

    FString ToFString(const char* Text)
    {
        return Text ? FString(UTF8_TO_TCHAR(Text)) : FString();
    }
}

FRigAxisScenePlayer::~FRigAxisScenePlayer()
{
    Destroy();
}

bool FRigAxisScenePlayer::Create(const URigAxisSceneAsset& Asset, FString& OutError)
{
    check(IsInGameThread());
    Destroy();

    if (!Asset.Stage || Asset.ManifestBytes.Num() == 0)
    {
        OutError = TEXT("The scene asset has no stage or manifest.");
        return false;
    }

    struct FSource
    {
        FString Path;
        const URigAxisSkeletonAsset* Skeleton = nullptr;
    };

    TArray<FSource> Sources;
    Sources.Add({ Asset.StagePath, Asset.Stage.Get() });
    for (const TPair<FString, TObjectPtr<URigAxisSkeletonAsset>>& Pair : Asset.ActorSkeletons)
    {
        const FString* Path = Asset.ActorSkeletonPaths.Find(Pair.Key);
        if (Pair.Value && Path)
        {
            Sources.Add({ *Path, Pair.Value.Get() });
        }
    }

    TArray<TUniquePtr<FTCHARToUTF8>> PathStorage;
    TArray<RigAxisSceneSource> NativeSources;
    NativeSources.Reserve(Sources.Num());
    for (const FSource& Source : Sources)
    {
        TUniquePtr<FTCHARToUTF8> Path = MakeUnique<FTCHARToUTF8>(*Source.Path);
        RigAxisSceneSource Native = {};
        Native.path = Path->Get();
        Native.skeleton = Source.Skeleton->SkeletonBytes.GetData();
        Native.skeleton_size = Source.Skeleton->SkeletonBytes.Num();
        Native.atlas = Source.Skeleton->AtlasBytes.GetData();
        Native.atlas_size = Source.Skeleton->AtlasBytes.Num();
        NativeSources.Add(Native);
        PathStorage.Add(MoveTemp(Path));
    }

    const RigAxisStatus Status = rigaxis_scene_create(
        Asset.ManifestBytes.GetData(),
        Asset.ManifestBytes.Num(),
        NativeSources.GetData(),
        static_cast<uint32>(NativeSources.Num()),
        &Scene);
    if (Status != RIGAXIS_OK)
    {
        OutError = LastError();
        Scene = nullptr;
        return false;
    }

    return true;
}

void FRigAxisScenePlayer::Destroy()
{
    if (Scene)
    {
        rigaxis_scene_free(Scene);
        Scene = nullptr;
    }
}

void FRigAxisScenePlayer::Play(const FString& Shot, bool bLoop)
{
    if (!Scene)
    {
        return;
    }

    FTCHARToUTF8 ShotUtf8(*Shot);
    rigaxis_scene_play(Scene, ShotUtf8.Get(), bLoop);
}

void FRigAxisScenePlayer::Update(double DeltaSeconds)
{
    if (Scene)
    {
        rigaxis_scene_update(Scene, DeltaSeconds);
    }
}

void FRigAxisScenePlayer::SetTime(double Time)
{
    if (Scene)
    {
        rigaxis_scene_set_time(Scene, Time, 0.0);
    }
}

double FRigAxisScenePlayer::GetDuration() const
{
    return Scene ? rigaxis_scene_duration(Scene) : 0.0;
}

double FRigAxisScenePlayer::GetTime() const
{
    return Scene ? rigaxis_scene_time(Scene) : 0.0;
}

FString FRigAxisScenePlayer::GetCurrentShot() const
{
    return Scene ? ToFString(rigaxis_scene_current_shot(Scene)) : FString();
}

uint32 FRigAxisScenePlayer::GetSourceCount() const
{
    return Scene ? rigaxis_scene_source_count(Scene) : 0;
}

FString FRigAxisScenePlayer::GetSourceId(uint32 Source) const
{
    return Scene ? ToFString(rigaxis_scene_source_id(Scene, Source)) : FString();
}

uint32 FRigAxisScenePlayer::GetActorCount() const
{
    return Scene ? rigaxis_scene_actor_count(Scene) : 0;
}

FString FRigAxisScenePlayer::GetActorId(uint32 Actor) const
{
    return Scene ? ToFString(rigaxis_scene_actor_id(Scene, Actor)) : FString();
}

FString FRigAxisScenePlayer::GetActorName(uint32 Actor) const
{
    return Scene ? ToFString(rigaxis_scene_actor_name(Scene, Actor)) : FString();
}

bool FRigAxisScenePlayer::GetActorBoneWorld(
    uint32 Actor,
    FName Bone,
    RigAxisAffine& Out) const
{
    if (!Scene)
    {
        return false;
    }

    FTCHARToUTF8 BoneUtf8(*Bone.ToString());
    return rigaxis_scene_actor_bone_world(Scene, Actor, BoneUtf8.Get(), &Out);
}

bool FRigAxisScenePlayer::GetCameraWorld(RigAxisAffine& Out) const
{
    return Scene && rigaxis_scene_camera_world(Scene, &Out);
}
