#pragma once

#include "../../Core/Public/SignalHealthSnapshot.h"

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
}

namespace MiaIA::Engine
{
    class SignalHealthAnalyzer
    {
    public:
        static bool Analyze(
            const Core::Dataset& dataset,
            const Core::Network& network,
            Core::LossType loss,
            const Core::SignalHealthConfiguration& configuration,
            Core::SignalHealthSnapshot& result);
    };
}
