#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class NetworkValidator
    {
    public:
        [[nodiscard]]
        static bool ValidateForForward(const Core::Network& network);
    };
}
