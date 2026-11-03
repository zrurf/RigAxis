#include "RigAxisBoneFollowerComponent.h"

#include "RigAxisComponent.h"
#include "RigAxisLog.h"

URigAxisBoneFollowerComponent::URigAxisBoneFollowerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void URigAxisBoneFollowerComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!Target)
    {
        Target = GetOwner() ? GetOwner()->FindComponentByClass<URigAxisComponent>() : nullptr;
    }

    RefreshTarget();
}

void URigAxisBoneFollowerComponent::RefreshTarget()
{
    URigAxisComponent* Current = Target.Get();
    if (BoundTarget.Get() == Current)
    {
        return;
    }

    if (BoundTarget.IsValid())
    {
        RemoveTickPrerequisiteComponent(BoundTarget.Get());
    }
    BoundTarget = Target;
    if (Current)
    {
        AddTickPrerequisiteComponent(Current);
    }
    bWarned = false;
}

bool URigAxisBoneFollowerComponent::ComputeFollowTransform(FTransform& OutTransform) const
{
    if (!Target || Bone.IsNone())
    {
        return false;
    }

    FTransform BoneTransform;
    if (!Target->GetBoneWorldTransform(Bone, BoneTransform))
    {
        return false;
    }

    OutTransform = FTransform(RotationOffset, Offset) * BoneTransform;
    return true;
}

void URigAxisBoneFollowerComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!IsRegistered())
    {
        return;
    }

    RefreshTarget();

    FTransform Result;
    if (!ComputeFollowTransform(Result))
    {
        if (!bWarned && Target && !Bone.IsNone())
        {
            bWarned = true;
            UE_LOG(LogRigAxis, Warning, TEXT("%s: bone '%s' not found."), *GetPathName(), *Bone.ToString());
        }
        return;
    }

    const FVector Location = bFollowPosition ? Result.GetLocation() : GetComponentLocation();
    const FRotator Rotation = bFollowRotation ? Result.Rotator() : GetComponentRotation();
    const FVector Scale = bFollowScale ? Result.GetScale3D() : GetComponentScale();

    SetWorldLocationAndRotation(Location, Rotation);
    SetWorldScale3D(Scale);
}
