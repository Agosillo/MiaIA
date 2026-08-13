#pragma once

#include "../Model/Layer.h"
#include "LossType.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Core
{
    struct BackwardTraceNeuronSnapshot
    {
        std::uint64_t Id{};
        double Activation{};
        double ActivationGradient{};
        double PreActivationGradient{};
        double BiasGradient{};
        bool IsInput{};
        bool IsOutput{};
    };

    struct BackwardTraceLayerSnapshot
    {
        std::uint64_t Id{};
        std::string Name;
        std::uint64_t Order{};
        ActivationType Activation{ ActivationType::Sigmoid };
        std::vector<BackwardTraceNeuronSnapshot> Neurons;
    };

    struct BackwardTraceConnectionSnapshot
    {
        std::uint64_t ConnectionId{};
        std::uint64_t FromNeuron{};
        std::uint64_t ToNeuron{};
        double Weight{};
        double WeightGradient{};
        double SourceActivationGradientContribution{};
    };

    struct BackwardTraceSnapshot
    {
        std::vector<double> Inputs;
        std::vector<double> Targets;
        std::vector<double> Predictions;
        std::vector<double> Errors;
        LossType Loss{ LossType::MeanSquaredError };
        double LossValue{};
        std::vector<BackwardTraceLayerSnapshot> Layers;
        std::vector<BackwardTraceConnectionSnapshot> Connections;
    };
}
