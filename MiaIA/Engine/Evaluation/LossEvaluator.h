#pragma once

#include "../../Core/Public/LossType.h"

#include <vector>

namespace MiaIA::Engine
{
    class LossEvaluator
    {
    public:
        static bool Evaluate(
            const std::vector<double>& predictions,
            const std::vector<double>& targets,
            Core::LossType type,
            std::vector<double>& errors,
            double& loss);
    };
}
