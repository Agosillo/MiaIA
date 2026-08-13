#pragma once

#include "../../Core/Model/Network.h"
#include "../../Core/Public/ForwardTraceSnapshot.h"

namespace MiaIA::Engine
{
    class ForwardEngine
    {
    public:
        [[nodiscard]]
        static bool Run(Core::Network& network);

        [[nodiscard]]
        static bool Run(
            Core::Network& network,
            Core::ForwardTraceSnapshot& trace);
    };
}
