#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MiaIAStudioGameInstance.generated.h"

class SWidget;

UCLASS()
class IDESTUDIO_API UMiaIAStudioGameInstance final : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void OnStart() override;
    virtual void Shutdown() override;

private:
    TSharedPtr<SWidget> StudioWidget;
};
