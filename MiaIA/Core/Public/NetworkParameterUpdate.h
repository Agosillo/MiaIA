#pragma once

#include "../Model/Layer.h"

#include <cstddef>
#include <optional>

namespace MiaIA::Core
{
    struct NetworkParameterUpdate
    {
        std::optional<ActivationType> HiddenActivation;
        std::optional<ActivationType> OutputActivation;
        std::optional<double> ConnectionWeight;
        std::optional<double> NonInputBias;

        [[nodiscard]]
        bool HasRequestedChanges() const noexcept
        {
            return HiddenActivation.has_value() ||
                OutputActivation.has_value() ||
                ConnectionWeight.has_value() ||
                NonInputBias.has_value();
        }
    };

    struct NetworkParameterUpdateSnapshot
    {
        std::size_t HiddenLayersChanged{};
        bool OutputLayerChanged{};
        std::size_t ConnectionWeightsChanged{};
        std::size_t NeuronBiasesChanged{};
    };
}
