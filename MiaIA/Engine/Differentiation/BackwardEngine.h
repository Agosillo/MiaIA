#pragma once

#include "../../Core/Public/SampleEvaluationSnapshot.h"
#include "../../Core/Public/SampleGradientSnapshot.h"

namespace MiaIA::Core
{
    struct Network;
}

namespace MiaIA::Engine
{
    class BackwardEngine
    {
    public:
        static bool Run(
            const Core::Network& network,
            const Core::SampleEvaluationSnapshot& evaluation,
            Core::SampleGradientSnapshot& result);
    };
}
