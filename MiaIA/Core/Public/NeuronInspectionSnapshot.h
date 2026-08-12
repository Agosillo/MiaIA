#pragma once

#include "ConnectionSnapshot.h"
#include "NeuronContextSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    struct NeuronInspectionSnapshot
    {
        NeuronContextSnapshot Context;
        std::size_t IncomingConnectionCount{};
        std::size_t OutgoingConnectionCount{};
        std::vector<ConnectionSnapshot> IncomingConnections;
        std::vector<ConnectionSnapshot> OutgoingConnections;
    };
}
