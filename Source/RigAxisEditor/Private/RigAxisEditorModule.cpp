#include "RigAxisEditorModule.h"

#include "RigAxisLog.h"

void FRigAxisEditorModule::StartupModule()
{
    UE_LOG(LogRigAxis, Display, TEXT("RigAxis editor module loaded."));
}

void FRigAxisEditorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRigAxisEditorModule, RigAxisEditor)
