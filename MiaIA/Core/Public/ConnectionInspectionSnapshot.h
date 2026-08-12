#pragma once

#include "ConnectionSnapshot.h"
#include "NeuronContextSnapshot.h"

namespace MiaIA::Core
{
    struct ConnectionInspectionSnapshot
    {
        ConnectionSnapshot Connection;
        NeuronContextSnapshot FromNeuron;
        NeuronContextSnapshot ToNeuron;
    };
}
