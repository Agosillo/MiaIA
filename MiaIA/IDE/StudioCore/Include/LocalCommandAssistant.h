#pragma once

#include "CommandAssistant.h"

#include <cstdint>
#include <string>
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
        std::string LanguageCode{"auto"};
        std::string Text;
        std::string Intent;
        std::uint32_t Confirmations{1};
    };

    struct LocalCommandAssistantPendingPhrase
    {
        std::string LanguageCode{"auto"};
        std::string Text;
    };

    struct LocalCommandAssistantLanguagePackExample
    {
        std::string Intent;
        std::string Text;
    };

    struct LocalCommandAssistantLanguagePack
    {
        std::string Code;
        std::string DisplayName;
        std::vector<LocalCommandAssistantLanguagePackExample> Examples;
        struct ParameterAlias
        {
            std::string Intent;
            std::string Role;
            std::string Text;
        };
        std::vector<ParameterAlias> ParameterAliases;
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
        const std::string& LanguageCode() const;
        bool SetLanguageCode(std::string code);

        bool LearnValidated(std::string text, std::string intent);
        bool RecordUnknown(std::string text);
        bool MarkIncorrect(std::string text);
        bool ClassifyPending(
            std::string_view text,
            std::string intent);
        bool RemovePending(std::string_view text);
        bool ReassignValidated(
            std::string_view text,
            std::string intent);
        bool RemoveValidated(std::string_view text);
        void ClearCorpus();

        const std::vector<LocalCommandAssistantExample>&
            LearnedExamples() const;
        const std::vector<LocalCommandAssistantPendingPhrase>&
            PendingPhrases() const;

        std::string ExportCorpus() const;
        bool ImportCorpus(
            std::string_view serialized,
            std::string& error);

        static std::string ExportLanguageTemplate();
        // Export the installed pack, or an editable built-in-language supplement.
        std::string ExportParameterTemplate(std::string_view code) const;
        bool ImportLanguagePack(
            std::string_view serialized,
            std::string& error);
        std::string ExportLanguagePack(std::string_view code) const;
        const std::vector<LocalCommandAssistantLanguagePack>&
            LanguagePacks() const;

        static const std::vector<std::string_view>& SupportedIntents();

    private:
        std::string InferLanguageCode(
            std::string_view text,
            std::string_view intent = {}) const;

        std::string languageCode_;
        std::vector<LocalCommandAssistantExample> learnedExamples_;
        std::vector<LocalCommandAssistantPendingPhrase> pendingPhrases_;
        std::vector<LocalCommandAssistantLanguagePack> languagePacks_;
    };
}
