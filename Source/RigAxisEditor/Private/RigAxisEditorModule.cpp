#include "RigAxisEditorModule.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "RigAxisLog.h"

void FRigAxisEditorModule::StartupModule()
{
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    AssetTools.RegisterAdvancedAssetCategory(
        FName(TEXT("RigAxis")),
        NSLOCTEXT("RigAxisEditor", "RigAxisCategory", "RigAxis"));

    UE_LOG(LogRigAxis, Display, TEXT("RigAxis editor module loaded."));
}

void FRigAxisEditorModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRigAxisEditorModule, RigAxisEditor)
