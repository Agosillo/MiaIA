#pragma once

#include "TrainingValueComparisonSnapshot.h"

#include <cstdint>

namespace MiaIA::Core
{
    struct TrainingConnectionComparisonSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t FromNeuron{};
        std::uint64_t ToNeuron{};
        bool HasFirstGradient{};
        bool HasSecondGradient{};
        TrainingValueComparisonSnapshot WeightGradient;
        bool HasFirstUpdate{};
        bool HasSecondUpdate{};
        TrainingValueComparisonSnapshot Weight;
    };
}
