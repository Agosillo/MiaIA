#pragma once

#include "../../Core/Model/Network.h"

namespace MiaIA::Engine
{
    class NetworkFactory
    {
    public:

        static Core::Network Create();
    };
}
