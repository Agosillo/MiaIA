#include "MiaIAStudioGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameUserSettings.h"
#include "Widgets/SMiaIAEditorPanel.h"

void UMiaIAStudioGameInstance::OnStart()
{
    Super::OnStart();

    if (StudioWidget.IsValid() || !GEngine || !GEngine->GameViewport)
    {
        return;
    }

    if (UGameUserSettings* settings = GEngine->GetGameUserSettings();
        settings && settings->GetFullscreenMode() != EWindowMode::Windowed)
    {
        settings->SetFullscreenMode(EWindowMode::Windowed);
        settings->SetScreenResolution(FIntPoint(1600, 900));
        settings->ApplyResolutionSettings(false);
        settings->ConfirmVideoMode();
        settings->SaveSettings();
    }

    StudioWidget = SNew(SMiaIAEditorPanel)
        .StandaloneMode(true);
    GEngine->GameViewport->AddViewportWidgetContent(
        StudioWidget.ToSharedRef(),
        1000);

    if (APlayerController* controller = GetFirstLocalPlayerController())
    {
        FInputModeUIOnly inputMode;
        inputMode.SetWidgetToFocus(StudioWidget);
        controller->SetInputMode(inputMode);
        controller->bShowMouseCursor = true;
    }
}

void UMiaIAStudioGameInstance::Shutdown()
{
    if (StudioWidget.IsValid() && GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(
            StudioWidget.ToSharedRef());
    }

    StudioWidget.Reset();
    Super::Shutdown();
}
