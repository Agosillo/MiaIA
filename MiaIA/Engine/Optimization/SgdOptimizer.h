#pragma once

#include "../../Core/Public/ConnectionUpdateSnapshot.h"
#include "../../Core/Public/NeuronUpdateSnapshot.h"
#include "../../Core/Public/SampleGradientSnapshot.h"

#include <vector>

namespace MiaIA::Core
{
    struct Network;
}

namespace MiaIA::Engine
{
    class SgdOptimizer
    {
    public:
        static bool Apply(
            Core::Network& network,
            const Core::SampleGradientSnapshot& gradients,
            double learningRate,
            std::vector<Core::ConnectionUpdateSnapshot>& connectionUpdates,
            std::vector<Core::NeuronUpdateSnapshot>& neuronUpdates);
    };
}
