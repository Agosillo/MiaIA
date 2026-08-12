#pragma once

#include "TrainingValueComparisonSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct TrainingOutputComparisonSnapshot
    {
        std::size_t OutputIndex{};
        bool HasFirstPrediction{};
        bool HasSecondPrediction{};
        TrainingValueComparisonSnapshot BeforePrediction;
        TrainingValueComparisonSnapshot AfterPrediction;
    };
}
