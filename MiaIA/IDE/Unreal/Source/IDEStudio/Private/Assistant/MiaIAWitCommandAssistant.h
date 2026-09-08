#pragma once

#if MIAIA_WITH_WIT_AI

#include "CommandAssistant.h"

class FMiaIAWitCommandAssistant final :
    public MiaIA::Studio::ICommandAssistantProvider
{
public:
    explicit FMiaIAWitCommandAssistant(
        FString ClientAccessToken,
        FString MissingConfigurationMessage);

    bool IsAvailable() const override;
    std::string AvailabilityMessage() const override;
    bool Interpret(
        const std::string& Text,
        MiaIA::Studio::CommandAssistantCompletion Completion) override;

private:
    FString Token;
    FString MissingConfigurationMessage;
};

#endif
