#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

struct IConsoleCommand;

class RIGAXIS_API FRigAxisModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    static void PrintVersion();

    IConsoleCommand* VersionCommand = nullptr;
};
