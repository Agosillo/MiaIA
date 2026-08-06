#pragma once

#include <cmath>

namespace MiaIA::Core
{
    class Activation
    {
    public:
        [[nodiscard]]
        static double Sigmoid(double value)
        {
            return 1.0 / (1.0 + std::exp(-value));
        }
    };
}
