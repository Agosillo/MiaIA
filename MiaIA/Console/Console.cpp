#include <filesystem>
#include <iostream>
#include <string>

#include "../CLI/Include/MiaIACommandProcessor.h"

int main()
{
    using MiaIA::CLI::MiaIACommandProcessor;

    std::cout << MiaIACommandProcessor::Banner();
    std::string command;
    const std::string workingDirectory =
        std::filesystem::current_path().string();

    while (true)
    {
        std::cout << "\n> ";

        if (!std::getline(std::cin, command))
        {
            break;
        }

        const auto result = MiaIACommandProcessor::Execute(
            command,
            workingDirectory);
        std::cout << result.Output;

        if (result.ExitRequested)
        {
            break;
        }
    }

    return 0;
}
