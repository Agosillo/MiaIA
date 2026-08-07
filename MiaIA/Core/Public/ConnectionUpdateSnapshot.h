#pragma once

#include <cstdint>

namespace MiaIA::Core
{
    struct ConnectionUpdateSnapshot
    {
        std::uint64_t Id{};
        double PreviousWeight{};
        double Gradient{};
        double Delta{};
        double UpdatedWeight{};
    };
}
