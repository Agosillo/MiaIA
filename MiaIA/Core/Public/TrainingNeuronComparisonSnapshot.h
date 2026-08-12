#pragma once

#include "TrainingValueComparisonSnapshot.h"

#include <cstdint>

namespace MiaIA::Core
{
    struct TrainingNeuronComparisonSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t LayerOrder{};
        bool HasFirstGradient{};
        bool HasSecondGradient{};
        TrainingValueComparisonSnapshot ActivationGradient;
        TrainingValueComparisonSnapshot PreActivationGradient;
        TrainingValueComparisonSnapshot BiasGradient;
        bool HasFirstUpdate{};
        bool HasSecondUpdate{};
        TrainingValueComparisonSnapshot Bias;
    };
}
