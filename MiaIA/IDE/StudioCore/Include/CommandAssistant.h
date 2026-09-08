#pragma once

#include <functional>
#include <string>
#include <vector>

namespace MiaIA::Studio
{
    struct CommandAssistantEntity
    {
        std::string Name;
        std::string Role;
        std::string Value;
        double Confidence{};
    };

    struct CommandAssistantUnderstanding
    {
        std::string Text;
        std::string Intent;
        double Confidence{};
        std::vector<CommandAssistantEntity> Entities;
        std::string Error;
    };

    struct CommandProposal
    {
        std::string SourceText;
        std::string Intent;
        std::string Command;
        double Confidence{};
        bool FullyConfident{};
        std::string Error;
    };

    using CommandAssistantCompletion =
        std::function<void(CommandAssistantUnderstanding)>;

    class ICommandAssistantProvider
    {
    public:
        virtual ~ICommandAssistantProvider() = default;

        virtual bool IsAvailable() const = 0;
        virtual std::string AvailabilityMessage() const = 0;
        virtual bool Interpret(
            const std::string& text,
            CommandAssistantCompletion completion) = 0;
    };

    class CommandAssistant final
    {
    public:
        static constexpr double MinimumIntentConfidence = 0.70;

        static bool Propose(
            const CommandAssistantUnderstanding& understanding,
            CommandProposal& proposal);
    };
}
