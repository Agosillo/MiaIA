#pragma once

#include <string>

namespace MiaIA::Core
{
    struct Network;
}

namespace MiaIA::Engine
{
    class OnnxImporter
    {
    public:
        static bool Import(
            Core::Network& network,
            const std::string& path);
    };
}
