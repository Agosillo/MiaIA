#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Framework/Docking/TabManager.h"
#include "MiaIADemoInstaller.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SMiaIAEditorPanel.h"

#define LOCTEXT_NAMESPACE "IDEEditorModule"

namespace
{
    const FName MiaIATabName(TEXT("MiaIAEditor"));
}

class FIDEEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (IsRunningCommandlet())
        {
            return;
        }

        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
            MiaIATabName,
            FOnSpawnTab::CreateRaw(this, &FIDEEditorModule::SpawnMiaIATab))
            .SetDisplayName(LOCTEXT(
                "MiaIATabTitle",
                "MiaIA Studio"))
            .SetTooltipText(LOCTEXT(
                "MiaIATabTooltip",
                "Inspect and control the active model in MiaIA Studio."));
        bTabRegistered = true;

        FEditorDelegates::OnMapOpened.AddRaw(
            this,
            &FIDEEditorModule::OnMapOpened);

        auto& assetRegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
                TEXT("AssetRegistry"));
        auto& assetRegistry = assetRegistryModule.Get();

        if (assetRegistry.IsLoadingAssets())
        {
            assetRegistry.OnFilesLoaded().AddRaw(
                this,
                &FIDEEditorModule::OnAssetRegistryFilesLoaded);
        }
        else
        {
            ScheduleInitialization();
        }
    }

    virtual void ShutdownModule() override
    {
        FEditorDelegates::OnMapOpened.RemoveAll(this);

        if (auto* assetRegistryModule =
            FModuleManager::GetModulePtr<FAssetRegistryModule>(
                TEXT("AssetRegistry")))
        {
            assetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
        }

        if (InitializationTickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(
                InitializationTickerHandle);
            InitializationTickerHandle.Reset();
        }

        if (bTabRegistered)
        {
            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MiaIATabName);
            bTabRegistered = false;
        }
        DemoBlueprint.Reset();
    }

private:
    TSharedRef<SDockTab> SpawnMiaIATab(const FSpawnTabArgs&) const
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                SNew(SMiaIAEditorPanel)
            ];
    }

    void OnAssetRegistryFilesLoaded()
    {
        auto& assetRegistry =
            FModuleManager::GetModuleChecked<FAssetRegistryModule>(
                TEXT("AssetRegistry")).Get();
        assetRegistry.OnFilesLoaded().RemoveAll(this);
        ScheduleInitialization();
    }

    void ScheduleInitialization()
    {
        if (InitializationTickerHandle.IsValid())
        {
            return;
        }

        InitializationTickerHandle =
            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateRaw(
                    this,
                    &FIDEEditorModule::InitializeDemo));
    }

    bool InitializeDemo(float)
    {
        InitializationTickerHandle.Reset();
        bAssetRegistryReady = true;
        DemoBlueprint = FMiaIADemoInstaller::LoadOrCreateBlueprint();

        if (DemoBlueprint.IsValid())
        {
            FMiaIADemoInstaller::InstallInCurrentMap(*DemoBlueprint.Get());
        }

        return false;
    }

    void OnMapOpened(const FString&, bool)
    {
        if (!bAssetRegistryReady)
        {
            return;
        }

        if (!DemoBlueprint.IsValid())
        {
            DemoBlueprint = FMiaIADemoInstaller::LoadOrCreateBlueprint();
        }

        if (DemoBlueprint.IsValid())
        {
            FMiaIADemoInstaller::InstallInCurrentMap(*DemoBlueprint.Get());
        }
    }

    bool bAssetRegistryReady{};
    bool bTabRegistered{};
    FTSTicker::FDelegateHandle InitializationTickerHandle;
    TWeakObjectPtr<UBlueprint> DemoBlueprint;
};

IMPLEMENT_MODULE(FIDEEditorModule, IDEEditor)

#undef LOCTEXT_NAMESPACE
