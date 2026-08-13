#pragma once

#include "../Model/Layer.h"
#include "LossType.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Core
{
    struct SignalHealthConfiguration
    {
        std::size_t MaximumSamples{};
        double InactiveActivationMagnitude{ 1.0e-6 };
        double InactiveSampleRatio{ 0.95 };
        double SaturationMargin{ 1.0e-2 };
        double SaturationSampleRatio{ 0.95 };
        double VanishingGradientMagnitude{ 1.0e-8 };
        double VanishingGradientSampleRatio{ 0.95 };
        double ExplodingGradientMagnitude{ 100.0 };
        double ExplodingGradientSampleRatio{ 0.05 };
    };

    struct SignalHealthNeuronSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t LayerId{};
        std::uint64_t LayerOrder{};
        std::string LayerName;
        ActivationType Activation{ ActivationType::Sigmoid };
        bool IsInput{};
        bool IsOutput{};
        std::size_t SampleCount{};
        double MeanActivation{};
        double MeanAbsoluteActivation{};
        double MinimumActivation{};
        double MaximumActivation{};
        double MeanAbsoluteGradient{};
        double MaximumAbsoluteGradient{};
        double InactiveSampleRatio{};
        double SaturatedSampleRatio{};
        double VanishingGradientSampleRatio{};
        double ExplodingGradientSampleRatio{};
        bool ConsistentlyInactive{};
        bool ConsistentlySaturated{};
        bool VanishingGradient{};
        bool ExplodingGradient{};
    };

    struct SignalHealthConnectionSnapshot
    {
        std::uint64_t Id{};
        std::uint64_t FromNeuron{};
        std::uint64_t ToNeuron{};
        std::size_t SampleCount{};
        double MeanAbsoluteGradient{};
        double MaximumAbsoluteGradient{};
        double VanishingGradientSampleRatio{};
        double ExplodingGradientSampleRatio{};
        bool VanishingGradient{};
        bool ExplodingGradient{};
    };

    struct SignalHealthSnapshot
    {
        SignalHealthConfiguration Configuration;
        LossType Loss{ LossType::MeanSquaredError };
        std::size_t DatasetSampleCount{};
        std::size_t AnalyzedSampleCount{};
        std::size_t HealthyNeuronCount{};
        std::size_t InactiveNeuronCount{};
        std::size_t SaturatedNeuronCount{};
        std::size_t VanishingGradientNeuronCount{};
        std::size_t ExplodingGradientNeuronCount{};
        std::size_t HealthyConnectionCount{};
        std::size_t VanishingGradientConnectionCount{};
        std::size_t ExplodingGradientConnectionCount{};
        std::vector<SignalHealthNeuronSnapshot> Neurons;
        std::vector<SignalHealthConnectionSnapshot> Connections;
    };
}
