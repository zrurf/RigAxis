#pragma once

#include "CoreMinimal.h"
#include "RigAxis/RigAxis.h"

class URigAxisSceneAsset;

class FRigAxisScenePlayer
{
public:
    ~FRigAxisScenePlayer();

    bool Create(const URigAxisSceneAsset& Asset, FString& OutError);
    void Destroy();
    bool IsValid() const { return Scene != nullptr; }
    RigAxisScene* GetScene() const { return Scene; }

    void Play(const FString& Shot, bool bLoop);
    void Update(double DeltaSeconds);
    void SetTime(double Time);
    void ResetPhysics();
    double GetDuration() const;
    double GetTime() const;
    FString GetCurrentShot() const;
    uint32 GetSourceCount() const;
    FString GetSourceId(uint32 Source) const;
    uint32 GetActorCount() const;
    FString GetActorId(uint32 Actor) const;
    FString GetActorName(uint32 Actor) const;
    bool GetActorBoneWorld(uint32 Actor, FName Bone, RigAxisAffine& Out) const;
    bool GetCameraWorld(RigAxisAffine& Out) const;

private:
    RigAxisScene* Scene = nullptr;
};
