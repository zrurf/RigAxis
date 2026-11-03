#pragma once

#include "CoreMinimal.h"
#include "RigAxis/RigAxis.h"

class FRigAxisSkeletonData;

class FRigAxisPlayer
{
public:
    ~FRigAxisPlayer();

    bool Create(const TSharedPtr<FRigAxisSkeletonData>& InData, FString& OutError);
    void Destroy();

    bool Play(FName Animation, bool bLoop, float MixDuration);
    bool SetAnimation(int32 Track, FName Animation, bool bLoop, float MixDuration);
    bool QueueAnimation(int32 Track, FName Animation, bool bLoop, float MixDuration, float Delay);
    void ClearTrack(int32 Track);
    void Stop();

    bool SetTrackAlpha(int32 Track, float Alpha);
    bool SetTrackTimeScale(int32 Track, float Scale);
    bool SetTrackTime(int32 Track, float Time);
    void SetSpeed(float Speed);

    void Update(double DeltaSeconds);
    void Apply();

    bool GetBoneWorldTransform(FName Bone, RigAxisAffine& Out) const;
    bool GetBoneWorldTransform(int32 BoneIndex, RigAxisAffine& Out) const;

    const TSharedPtr<FRigAxisSkeletonData>& GetData() const { return Data; }
    RigAxisPose* GetPose() const { return Pose; }
    bool IsValid() const;

private:
    RigAxisSkeleton* GetSkeleton() const;

    TSharedPtr<FRigAxisSkeletonData> Data;
    RigAxisPose* Pose = nullptr;
    RigAxisState* State = nullptr;
};
