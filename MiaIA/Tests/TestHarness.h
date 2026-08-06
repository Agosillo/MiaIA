#pragma once

#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace MiaIA::Tests
{
    namespace Console
    {
        inline constexpr std::string_view Green = "\x1b[32m";
        inline constexpr std::string_view Red = "\x1b[31m";
        inline constexpr std::string_view Reset = "\x1b[0m";

        inline bool EnableColors()
        {
#ifdef _WIN32
            const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

            if (output == INVALID_HANDLE_VALUE || output == nullptr)
            {
                return false;
            }

            DWORD mode{};

            if (!GetConsoleMode(output, &mode))
            {
                return false;
            }

            if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
            {
                return true;
            }

            return SetConsoleMode(
                output,
                mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
            return isatty(STDOUT_FILENO) == 1;
#endif
        }
    }

    class TestFailure final : public std::runtime_error
    {
    public:
        TestFailure(
            const char* expression,
            const char* file,
            int line)
            : std::runtime_error(BuildMessage(expression, file, line))
        {
        }

    private:
        static std::string BuildMessage(
            const char* expression,
            const char* file,
            int line)
        {
            std::ostringstream message;
            message
                << "CHECK failed: " << expression
                << '\n'
                << file << ':' << line;

            return message.str();
        }
    };

    inline void Check(
        bool condition,
        const char* expression,
        const char* file,
        int line)
    {
        if (!condition)
        {
            throw TestFailure(expression, file, line);
        }
    }

    class TestRunner
    {
    public:
        TestRunner()
            : colorsEnabled(Console::EnableColors())
        {
        }

        template<typename Test>
        void Run(std::string_view name, Test&& test)
        {
            try
            {
                std::forward<Test>(test)();
                ++passed;

                std::cout
                    << Color(Console::Green)
                    << "[PASS]"
                    << Color(Console::Reset)
                    << ' '
                    << name
                    << '\n';
            }
            catch (const std::exception& exception)
            {
                ++failed;

                std::cerr
                    << Color(Console::Red)
                    << "[FAIL]"
                    << Color(Console::Reset)
                    << ' '
                    << name
                    << '\n'
                    << "       "
                    << exception.what()
                    << '\n';
            }
            catch (...)
            {
                ++failed;

                std::cerr
                    << Color(Console::Red)
                    << "[FAIL]"
                    << Color(Console::Reset)
                    << ' '
                    << name
                    << '\n'
                    << "       Unknown exception"
                    << '\n';
            }
        }

        [[nodiscard]]
        int Finish() const
        {
            std::cout
                << '\n'
                << Color(failed == 0 ? Console::Green : Console::Red)
                << "Test summary: "
                << passed << " passed, "
                << failed << " failed"
                << Color(Console::Reset)
                << '\n';

            return failed == 0 ? 0 : 1;
        }

    private:
        [[nodiscard]]
        std::string_view Color(std::string_view color) const
        {
            return colorsEnabled ? color : std::string_view{};
        }

        bool colorsEnabled{};
        int passed{};
        int failed{};
    };
}

#define MIAIA_CHECK(expression) \
    ::MiaIA::Tests::Check(      \
        static_cast<bool>(expression), \
        #expression,            \
        __FILE__,               \
        __LINE__)
