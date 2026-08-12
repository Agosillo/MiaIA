#pragma once

namespace MiaIA::Core
{
    struct TrainingValueComparisonSnapshot
    {
        double FirstValue{};
        double SecondValue{};
        double Delta{};
        double AbsoluteDelta{};
    };
}
