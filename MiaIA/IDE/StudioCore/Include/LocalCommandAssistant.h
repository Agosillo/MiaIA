#pragma once

#include "CommandAssistant.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace MiaIA::Studio
{
    enum class CommandAssistantLanguage
    {
        Automatic,
        English,
        Italian
    };

    struct LocalCommandAssistantExample
    {
        CommandAssistantLanguage Language{CommandAssistantLanguage::Automatic};
        std::string Text;
        std::string Intent;
        std::uint32_t Confirmations{1};
    };

    struct LocalCommandAssistantPendingPhrase
    {
        CommandAssistantLanguage Language{CommandAssistantLanguage::Automatic};
        std::string Text;
    };

    // Offline, deterministic NLU for MiaIA commands. This provider only uses
    // the C++ standard library so it can be reused outside the Windows Studio.
    class LocalCommandAssistant final : public ICommandAssistantProvider
    {
    public:
        explicit LocalCommandAssistant(
            CommandAssistantLanguage language =
                CommandAssistantLanguage::Automatic);

        bool IsAvailable() const override;
        std::string AvailabilityMessage() const override;
        bool Interpret(
            const std::string& text,
            CommandAssistantCompletion completion) override;

        CommandAssistantLanguage Language() const;
        void SetLanguage(CommandAssistantLanguage language);

        bool LearnValidated(std::string text, std::string intent);
        bool RecordUnknown(std::string text);
        bool MarkIncorrect(std::string text);
        bool ClassifyPending(
            std::string_view text,
            std::string intent);
        bool RemovePending(std::string_view text);

        const std::vector<LocalCommandAssistantExample>&
            LearnedExamples() const;
        const std::vector<LocalCommandAssistantPendingPhrase>&
            PendingPhrases() const;

        std::string ExportCorpus() const;
        bool ImportCorpus(
            std::string_view serialized,
            std::string& error);

        static const std::vector<std::string_view>& SupportedIntents();

    private:
        CommandAssistantLanguage InferLanguage(
            std::string_view text,
            std::string_view intent = {}) const;

        CommandAssistantLanguage language_;
        std::vector<LocalCommandAssistantExample> learnedExamples_;
        std::vector<LocalCommandAssistantPendingPhrase> pendingPhrases_;
    };
}
