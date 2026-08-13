#pragma once

#include "../Model/Layer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Core
{
    struct ForwardTraceNeuronSnapshot
    {
        std::uint64_t Id{};
        double WeightedInputSum{};
        double Bias{};
        double PreActivation{};
        double Activation{};
        bool IsInput{};
    };

    struct ForwardTraceLayerSnapshot
    {
        std::uint64_t Id{};
        std::string Name;
        std::uint64_t Order{};
        ActivationType Activation{ ActivationType::Sigmoid };
        std::vector<ForwardTraceNeuronSnapshot> Neurons;
    };

    struct ForwardTraceSnapshot
    {
        std::vector<double> Inputs;
        std::vector<double> Outputs;
        std::vector<ForwardTraceLayerSnapshot> Layers;
    };

    enum class ForwardTraceContributionSort
    {
        ConnectionId,
        Contribution,
        AbsoluteContribution
    };

    struct ForwardTraceContributionPageRequest
    {
        std::size_t Offset{};
        std::size_t Limit{ 25 };
        ForwardTraceContributionSort Sort{
            ForwardTraceContributionSort::ConnectionId };
        bool Descending{};
        double MinimumAbsoluteContribution{};
    };

    struct ForwardTraceConnectionContributionSnapshot
    {
        std::uint64_t ConnectionId{};
        std::uint64_t FromNeuron{};
        std::uint64_t ToNeuron{};
        double SourceActivation{};
        double Weight{};
        double Contribution{};
    };

    struct ForwardTraceContributionPageSnapshot
    {
        ForwardTraceNeuronSnapshot Neuron;
        std::size_t TotalContributionCount{};
        std::size_t FilteredContributionCount{};
        std::size_t Offset{};
        std::size_t Limit{};
        bool HasPrevious{};
        bool HasNext{};
        std::vector<ForwardTraceConnectionContributionSnapshot>
            Contributions;
    };
}
