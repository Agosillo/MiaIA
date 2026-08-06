#pragma once

#include "../../Core/Model/Network.h"
#include "../../Core/Public/NetworkSnapshot.h"

namespace MiaIA::Engine
{
    class NetworkRuntime
    {
    public:

        static bool Forward(Core::Network& network);
        static bool Validate(const Core::Network& network);
        static Core::NetworkSnapshot Snapshot(const Core::Network& network);
    };
}
