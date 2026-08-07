#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace MiaIA::CLI
{
    struct CommandResult
    {
        std::string Output;
        bool ExitRequested{};
    };

    struct CommandSuggestion
    {
        std::string Completion;
        std::string Syntax;
        std::string Description;
    };

    class MiaIACommandProcessor
    {
    public:
        static CommandResult Execute(
            const std::string& command,
            const std::string& workingDirectory = {});

        static std::vector<CommandSuggestion> GetSuggestions(
            const std::string& input,
            std::size_t maximumResults = 16);

        static std::string Banner();
    };
}
