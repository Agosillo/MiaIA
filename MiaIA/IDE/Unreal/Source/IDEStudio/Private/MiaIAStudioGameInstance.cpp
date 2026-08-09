#include "MiaIAStudioGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SMiaIAEditorPanel.h"

void UMiaIAStudioGameInstance::OnStart()
{
    Super::OnStart();

    if (StudioWidget.IsValid() || !GEngine || !GEngine->GameViewport)
    {
        return;
    }

    StudioWidget = SNew(SMiaIAEditorPanel);
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
