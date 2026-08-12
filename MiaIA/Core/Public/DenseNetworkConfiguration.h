#pragma once

#include "../Model/Layer.h"

namespace MiaIA::Core
{
    struct DenseNetworkConfiguration
    {
        ActivationType HiddenActivation{ ActivationType::Sigmoid };
        ActivationType OutputActivation{ ActivationType::Sigmoid };
        double InitialWeight{ 0.1 };
        double InitialBias{};
    };
}
