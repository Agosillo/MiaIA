#include "DatasetRuntime.h"

#include "../Input/NetworkInput.h"
#include "../../Core/Model/Dataset.h"

namespace MiaIA::Engine
{
    bool DatasetRuntime::ApplySample(
        const Core::Dataset& dataset,
        std::size_t index,
        Core::Network& network)
    {
        if (index >= dataset.Samples.size())
        {
            return false;
        }

        return NetworkInput::SetValues(
            network,
            dataset.Samples[index].Inputs);
    }
}
