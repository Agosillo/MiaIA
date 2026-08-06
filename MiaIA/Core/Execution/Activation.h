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

        [[nodiscard]]
        static double ReLU(double value)
        {
            return value > 0.0 ? value : 0.0;
        }

        [[nodiscard]]
        static double Tanh(double value)
        {
            return std::tanh(value);
        }

        [[nodiscard]]
        static double Linear(double value)
        {
            return value;
        }
    };
}
