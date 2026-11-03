#include "RigAxisModule.h"

#include "HAL/IConsoleManager.h"
#include "Misc/EngineVersion.h"
#include "RigAxisLog.h"
#include "RigAxisVersion.h"

DEFINE_LOG_CATEGORY(LogRigAxis);

void FRigAxisModule::StartupModule()
{
    VersionCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("RigAxis.Version"),
        TEXT("Print the RigAxis plugin and engine versions."),
        FConsoleCommandDelegate::CreateStatic(&FRigAxisModule::PrintVersion));
}

void FRigAxisModule::ShutdownModule()
{
    if (VersionCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(VersionCommand);
        VersionCommand = nullptr;
    }
}

void FRigAxisModule::PrintVersion()
{
    UE_LOG(LogRigAxis, Display, TEXT("RigAxis %s | Engine %s | Native ABI not linked"),
        TEXT(RIGAXIS_VERSION_STRING),
        *FEngineVersion::Current().ToString());
}

IMPLEMENT_MODULE(FRigAxisModule, RigAxis)
