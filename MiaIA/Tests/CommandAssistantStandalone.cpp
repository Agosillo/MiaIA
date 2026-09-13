// Dependency-free assistant regression suite. The same tests run in Tests.cpp.
#include "CommandAssistantTests.h"

int main()
{
    MiaIA::Tests::TestRunner runner;
    RunCommandAssistantTests(runner);
    return runner.Finish();
}
