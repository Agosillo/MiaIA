#pragma once

#include "ConnectionGradientSnapshot.h"
#include "NeuronGradientSnapshot.h"
#include "SampleEvaluationSnapshot.h"

#include <vector>

namespace MiaIA::Core
{
    struct SampleGradientSnapshot
    {
        SampleEvaluationSnapshot Evaluation;
        std::vector<NeuronGradientSnapshot> Neurons;
        std::vector<ConnectionGradientSnapshot> Connections;
    };
}
