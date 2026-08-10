#pragma once

namespace MiaIA::Core
{
    enum class TrainingBreakpointKind
    {
        Phase,
        NeuronActivationAbove,
        NeuronActivationBelow,
        NeuronGradientMagnitudeAbove,
        ConnectionUpdateMagnitudeAbove
    };
}
