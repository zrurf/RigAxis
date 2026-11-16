#include "RigAxisModule.h"

#include "HAL/IConsoleManager.h"
#include "Misc/EngineVersion.h"
#include "RigAxis/RigAxis.h"
#include "RigAxisLog.h"
#include "RigAxisVersion.h"

DEFINE_LOG_CATEGORY(LogRigAxis);

void FRigAxisModule::StartupModule()
{
    const uint32 NativeAbi = rigaxis_abi_version();
    if (NativeAbi != RIGAXIS_ABI_VERSION)
    {
        UE_LOG(LogRigAxis, Error,
            TEXT("RigAxis ABI mismatch: plugin %u, native %u."),
            static_cast<uint32>(RIGAXIS_ABI_VERSION),
            NativeAbi);
    }

    VersionCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("RigAxis.Version"),
        TEXT("Print the RigAxis plugin, native, and engine versions."),
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
    const char* NativeVersion = rigaxis_version_string();
    UE_LOG(LogRigAxis, Display,
        TEXT("RigAxis %s | Native %s (ABI %u/%u) | Engine %s"),
        TEXT(RIGAXIS_VERSION_STRING),
        UTF8_TO_TCHAR(NativeVersion ? NativeVersion : "unknown"),
        static_cast<uint32>(rigaxis_abi_version()),
        static_cast<uint32>(RIGAXIS_ABI_VERSION),
        *FEngineVersion::Current().ToString());
}

IMPLEMENT_MODULE(FRigAxisModule, RigAxis)
