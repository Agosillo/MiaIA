#pragma once

#include <cstddef>
#include <string>

namespace MiaIA::Core
{
    struct Dataset;
}

namespace MiaIA::Engine
{
    class CsvDatasetImporter
    {
    public:
        static bool Import(
            Core::Dataset& dataset,
            const std::string& path,
            std::size_t inputCount,
            std::size_t targetCount,
            bool hasHeader);
    };
}
