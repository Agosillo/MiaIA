#pragma once

#include "LossType.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct SampleEvaluationSnapshot
    {
        std::size_t SampleIndex{};
        LossType Type{ LossType::MeanSquaredError };
        std::vector<double> Targets;
        std::vector<double> Predictions;
        std::vector<double> Errors;
        double Loss{};
    };
}
