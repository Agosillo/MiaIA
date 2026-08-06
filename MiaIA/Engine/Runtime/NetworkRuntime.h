#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class NetworkRuntime
    {
    public:

        static bool Forward(Core::Network& network);
        static bool Validate(const Core::Network& network);
    };
}
