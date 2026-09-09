#pragma once

#include "CommandAssistant.h"

namespace MiaIA::Studio
{
    enum class CommandAssistantLanguage
    {
        English,
        Italian
    };

    // Offline, deterministic NLU for MiaIA commands. This provider only uses
    // the C++ standard library so it can be reused outside the Windows Studio.
    class LocalCommandAssistant final : public ICommandAssistantProvider
    {
    public:
        explicit LocalCommandAssistant(
            CommandAssistantLanguage language =
                CommandAssistantLanguage::English);

        bool IsAvailable() const override;
        std::string AvailabilityMessage() const override;
        bool Interpret(
            const std::string& text,
            CommandAssistantCompletion completion) override;

        CommandAssistantLanguage Language() const;
        void SetLanguage(CommandAssistantLanguage language);

    private:
        CommandAssistantLanguage language_;
    };
}
