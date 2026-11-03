#include "RigAxisPlayer.h"

#include "Containers/StringConv.h"
#include "RigAxisSkeletonData.h"

namespace
{
    FString LastError()
    {
        const char* Text = rigaxis_last_error();
        return Text ? FString(UTF8_TO_TCHAR(Text)) : FString();
    }
}

FRigAxisPlayer::~FRigAxisPlayer()
{
    Destroy();
}

bool FRigAxisPlayer::Create(const TSharedPtr<FRigAxisSkeletonData>& InData, FString& OutError)
{
    check(IsInGameThread());
    Destroy();

    if (!InData.IsValid())
    {
        OutError = TEXT("Invalid skeleton data.");
        return false;
    }

    Data = InData;

    RigAxisStatus Status = rigaxis_pose_create(Data->GetSkeleton(), &Pose);
    if (Status != RIGAXIS_OK)
    {
        OutError = LastError();
        Destroy();
        return false;
    }

    Status = rigaxis_state_create(&State);
    if (Status != RIGAXIS_OK)
    {
        OutError = LastError();
        Destroy();
        return false;
    }

    return true;
}

void FRigAxisPlayer::Destroy()
{
    if (State)
    {
        rigaxis_state_free(State);
        State = nullptr;
    }
    if (Pose)
    {
        rigaxis_pose_free(Pose);
        Pose = nullptr;
    }
    Data.Reset();
}

bool FRigAxisPlayer::IsValid() const
{
    return Data.IsValid() && Pose != nullptr && State != nullptr;
}

RigAxisSkeleton* FRigAxisPlayer::GetSkeleton() const
{
    return Data.IsValid() ? Data->GetSkeleton() : nullptr;
}

bool FRigAxisPlayer::Play(FName Animation, bool bLoop, float MixDuration)
{
    if (!IsValid())
    {
        return false;
    }

    FTCHARToUTF8 Name(*Animation.ToString());
    return rigaxis_state_play(State, GetSkeleton(), Name.Get(), bLoop, MixDuration, nullptr);
}

bool FRigAxisPlayer::SetAnimation(int32 Track, FName Animation, bool bLoop, float MixDuration)
{
    if (!IsValid() || Track < 0)
    {
        return false;
    }

    FTCHARToUTF8 Name(*Animation.ToString());
    return rigaxis_state_set_animation(
        State,
        GetSkeleton(),
        static_cast<uint32>(Track),
        Name.Get(),
        bLoop,
        MixDuration,
        nullptr);
}

bool FRigAxisPlayer::QueueAnimation(
    int32 Track,
    FName Animation,
    bool bLoop,
    float MixDuration,
    float Delay)
{
    if (!IsValid() || Track < 0)
    {
        return false;
    }

    const bool bHasDelay = Delay >= 0.0f;
    FTCHARToUTF8 Name(*Animation.ToString());
    return rigaxis_state_add_animation(
        State,
        GetSkeleton(),
        static_cast<uint32>(Track),
        Name.Get(),
        bLoop,
        MixDuration,
        Delay,
        bHasDelay,
        nullptr);
}

void FRigAxisPlayer::ClearTrack(int32 Track)
{
    if (IsValid() && Track >= 0)
    {
        rigaxis_state_clear_track(State, static_cast<uint32>(Track));
    }
}

void FRigAxisPlayer::Stop()
{
    if (IsValid())
    {
        rigaxis_state_stop(State);
    }
}

bool FRigAxisPlayer::SetTrackAlpha(int32 Track, float Alpha)
{
    return IsValid() && Track >= 0
        && rigaxis_state_set_track_alpha(State, static_cast<uint32>(Track), Alpha);
}

bool FRigAxisPlayer::SetTrackTimeScale(int32 Track, float Scale)
{
    return IsValid() && Track >= 0
        && rigaxis_state_set_track_time_scale(State, static_cast<uint32>(Track), Scale);
}

bool FRigAxisPlayer::SetTrackTime(int32 Track, float Time)
{
    return IsValid() && Track >= 0
        && rigaxis_state_set_track_time(State, static_cast<uint32>(Track), Time);
}

void FRigAxisPlayer::SetSpeed(float Speed)
{
    if (IsValid())
    {
        rigaxis_state_set_speed(State, Speed);
    }
}

void FRigAxisPlayer::Update(double DeltaSeconds)
{
    if (IsValid())
    {
        rigaxis_state_update(State, GetSkeleton(), DeltaSeconds);
    }
}

void FRigAxisPlayer::Apply()
{
    if (IsValid())
    {
        rigaxis_state_apply(State, GetSkeleton(), Pose);
    }
}

int32 FRigAxisPlayer::GetEventCount() const
{
    return IsValid() ? static_cast<int32>(rigaxis_state_event_count(State)) : 0;
}

bool FRigAxisPlayer::GetEvent(int32 Index, FRigAxisEvent& Out) const
{
    if (!IsValid() || Index < 0)
    {
        return false;
    }

    RigAxisEvent Native = {};
    if (!rigaxis_state_event_at(State, static_cast<uint32>(Index), &Native))
    {
        return false;
    }

    Out.Kind = Native.kind;
    Out.Track = Native.track;
    Out.Animation = Native.animation_name ? FName(UTF8_TO_TCHAR(Native.animation_name)) : NAME_None;
    Out.Name = Native.event_name ? FName(UTF8_TO_TCHAR(Native.event_name)) : NAME_None;
    Out.Time = static_cast<float>(Native.time);
    Out.LoopIndex = static_cast<int32>(Native.loop_index);
    Out.IntValue = static_cast<int32>(Native.int_value);
    Out.FloatValue = static_cast<float>(Native.float_value);
    Out.StringValue = Native.string_value ? FString(UTF8_TO_TCHAR(Native.string_value)) : FString();
    Out.Audio = Native.audio ? FName(UTF8_TO_TCHAR(Native.audio)) : NAME_None;
    Out.Volume = static_cast<float>(Native.volume);
    Out.Balance = static_cast<float>(Native.balance);
    return true;
}

void FRigAxisPlayer::ClearEvents()
{
    if (IsValid())
    {
        rigaxis_state_clear_events(State);
    }
}

bool FRigAxisPlayer::SetSkin(FName Skin)
{
    if (!IsValid())
    {
        return false;
    }

    FTCHARToUTF8 Name(*Skin.ToString());
    return rigaxis_pose_set_skin(GetSkeleton(), Pose, Name.Get());
}

void FRigAxisPlayer::ResetPhysics()
{
    if (IsValid())
    {
        rigaxis_pose_reset_physics(Pose);
    }
}

bool FRigAxisPlayer::GetBoneWorldTransform(FName Bone, RigAxisAffine& Out) const
{
    if (!IsValid())
    {
        return false;
    }

    const int32 BoneIndex = Data->FindBone(Bone);
    return BoneIndex != INDEX_NONE && GetBoneWorldTransform(BoneIndex, Out);
}

bool FRigAxisPlayer::GetBoneWorldTransform(int32 BoneIndex, RigAxisAffine& Out) const
{
    if (!IsValid() || BoneIndex < 0)
    {
        return false;
    }

    return rigaxis_pose_bone_world(Pose, static_cast<uint32>(BoneIndex), &Out);
}
