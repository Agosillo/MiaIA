#pragma once

#include "LossType.h"
#include "SampleEvaluationSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct DatasetEvaluationSnapshot
    {
        std::size_t SampleCount{};
        LossType Type{ LossType::MeanSquaredError };
        double MeanLoss{};
        std::vector<SampleEvaluationSnapshot> Evaluations;
    };
}
