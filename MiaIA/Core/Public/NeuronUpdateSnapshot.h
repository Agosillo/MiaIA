#pragma once

#include <cstdint>

namespace MiaIA::Core
{
    struct NeuronUpdateSnapshot
    {
        std::uint64_t Id{};
        double PreviousBias{};
        double Gradient{};
        double Delta{};
        double UpdatedBias{};
    };
}
