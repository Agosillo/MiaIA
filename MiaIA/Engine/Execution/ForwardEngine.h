#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class ForwardEngine
    {
    public:
        [[nodiscard]]
        static bool Run(Core::Network& network);
    };
}
