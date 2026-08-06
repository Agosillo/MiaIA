#pragma once

#include <string>

namespace MiaIA::Core
{
    struct Network;
}

namespace MiaIA::Engine
{
    class OnnxExporter
    {
    public:
        static bool Export(
            const Core::Network& network,
            const std::string& path);
    };
}
