#pragma once

#include "../../Core/Public/DatasetSummary.h"
#include "../../Core/Public/SampleSnapshot.h"

#include <cstddef>

namespace MiaIA::Core
{
    struct Dataset;
}

namespace MiaIA::Engine
{
    class DatasetInspector
    {
    public:
        static Core::DatasetSummary Summary(
            const Core::Dataset& dataset);

        static bool TryGetSample(
            const Core::Dataset& dataset,
            std::size_t index,
            Core::SampleSnapshot& result);
    };
}
