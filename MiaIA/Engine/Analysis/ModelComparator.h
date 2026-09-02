#pragma once

#include "../../Core/Model/Network.h"
#include "../../Core/Public/ModelComparisonSnapshot.h"

namespace MiaIA::Engine
{
    class ModelComparator final
    {
    public:
        static bool Compare(
            const Core::Network& reference,
            const Core::Network& current,
            Core::ModelComparisonSnapshot& result);
    };
}
