#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "RigAxisSkeletonFactory.generated.h"

UCLASS()
class URigAxisSkeletonFactory : public UFactory
{
    GENERATED_BODY()

public:
    URigAxisSkeletonFactory();

    virtual UObject* FactoryCreateFile(
        UClass* InClass,
        UObject* InParent,
        FName InName,
        EObjectFlags Flags,
        const FString& Filename,
        const TCHAR* Parms,
        FFeedbackContext* Warn,
        bool& bOutOperationCanceled) override;
};
