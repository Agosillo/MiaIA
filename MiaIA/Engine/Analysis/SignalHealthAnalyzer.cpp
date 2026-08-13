#include "SignalHealthAnalyzer.h"

#include "../Inspection/NetworkInspector.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

namespace
{
    struct NeuronAccumulator
    {
        MiaIA::Core::SignalHealthNeuronSnapshot Snapshot;
        std::size_t InactiveSamples{};
        std::size_t SaturatedSamples{};
        std::size_t VanishingGradientSamples{};
        std::size_t ExplodingGradientSamples{};
        double ActivationSum{};
        double AbsoluteActivationSum{};
        double AbsoluteGradientSum{};
    };

    struct ConnectionAccumulator
    {
        MiaIA::Core::SignalHealthConnectionSnapshot Snapshot;
        std::size_t VanishingGradientSamples{};
        std::size_t ExplodingGradientSamples{};
        double AbsoluteGradientSum{};
    };

    bool IsRatio(double value)
    {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    }

    bool IsValidConfiguration(
        const MiaIA::Core::SignalHealthConfiguration& configuration)
    {
        return std::isfinite(configuration.InactiveActivationMagnitude) &&
            configuration.InactiveActivationMagnitude >= 0.0 &&
            IsRatio(configuration.InactiveSampleRatio) &&
            std::isfinite(configuration.SaturationMargin) &&
            configuration.SaturationMargin >= 0.0 &&
            configuration.SaturationMargin <= 0.5 &&
            IsRatio(configuration.SaturationSampleRatio) &&
            std::isfinite(configuration.VanishingGradientMagnitude) &&
            configuration.VanishingGradientMagnitude >= 0.0 &&
            IsRatio(configuration.VanishingGradientSampleRatio) &&
            std::isfinite(configuration.ExplodingGradientMagnitude) &&
            configuration.ExplodingGradientMagnitude >
                configuration.VanishingGradientMagnitude &&
            IsRatio(configuration.ExplodingGradientSampleRatio);
    }

    bool IsSaturated(
        MiaIA::Core::ActivationType type,
        double activation,
        double margin)
    {
        switch (type)
        {
        case MiaIA::Core::ActivationType::Sigmoid:
            return activation <= margin || activation >= 1.0 - margin;
        case MiaIA::Core::ActivationType::Tanh:
            return std::abs(activation) >= 1.0 - margin;
        case MiaIA::Core::ActivationType::ReLU:
        case MiaIA::Core::ActivationType::Linear:
            return false;
        }

        return false;
    }

    double Ratio(std::size_t count, std::size_t total)
    {
        return total == 0
            ? 0.0
            : static_cast<double>(count) / static_cast<double>(total);
    }
}

namespace MiaIA::Engine
{
    bool SignalHealthAnalyzer::Analyze(
        const Core::Dataset& dataset,
        const Core::Network& network,
        Core::LossType loss,
        const Core::SignalHealthConfiguration& configuration,
        Core::SignalHealthSnapshot& result)
    {
        if (dataset.Samples.empty() || network.Layers.empty() ||
            loss != Core::LossType::MeanSquaredError ||
            !IsValidConfiguration(configuration))
        {
            return false;
        }

        const std::size_t sampleCount = configuration.MaximumSamples == 0
            ? dataset.Samples.size()
            : std::min(configuration.MaximumSamples, dataset.Samples.size());

        if (sampleCount == 0)
        {
            return false;
        }

        std::vector<NeuronAccumulator> neuronAccumulators;
        std::vector<ConnectionAccumulator> connectionAccumulators;
        std::unordered_map<std::uint64_t, std::size_t> neuronIndices;
        std::unordered_map<std::uint64_t, std::size_t> connectionIndices;

        for (std::size_t sampleIndex = 0; sampleIndex < sampleCount;
            ++sampleIndex)
        {
            Core::BackwardTraceSnapshot trace;
            const Core::Sample& sample = dataset.Samples[sampleIndex];

            if (!NetworkInspector::TraceBackward(
                network,
                sample.Inputs,
                sample.Targets,
                loss,
                trace))
            {
                return false;
            }

            for (const Core::BackwardTraceLayerSnapshot& layer : trace.Layers)
            {
                for (const Core::BackwardTraceNeuronSnapshot& neuron :
                    layer.Neurons)
                {
                    auto found = neuronIndices.find(neuron.Id);

                    if (found == neuronIndices.end())
                    {
                        NeuronAccumulator accumulator;
                        accumulator.Snapshot.Id = neuron.Id;
                        accumulator.Snapshot.LayerId = layer.Id;
                        accumulator.Snapshot.LayerOrder = layer.Order;
                        accumulator.Snapshot.LayerName = layer.Name;
                        accumulator.Snapshot.Activation = layer.Activation;
                        accumulator.Snapshot.IsInput = neuron.IsInput;
                        accumulator.Snapshot.IsOutput = neuron.IsOutput;
                        accumulator.Snapshot.MinimumActivation =
                            std::numeric_limits<double>::infinity();
                        accumulator.Snapshot.MaximumActivation =
                            -std::numeric_limits<double>::infinity();
                        found = neuronIndices.emplace(
                            neuron.Id,
                            neuronAccumulators.size()).first;
                        neuronAccumulators.push_back(std::move(accumulator));
                    }

                    NeuronAccumulator& accumulator =
                        neuronAccumulators[found->second];
                    const double activation = neuron.Activation;
                    const double gradient = neuron.IsInput
                        ? neuron.ActivationGradient
                        : neuron.PreActivationGradient;
                    const double absoluteActivation = std::abs(activation);
                    const double absoluteGradient = std::abs(gradient);

                    if (!std::isfinite(activation) ||
                        !std::isfinite(absoluteGradient))
                    {
                        return false;
                    }

                    ++accumulator.Snapshot.SampleCount;
                    accumulator.ActivationSum += activation;
                    accumulator.AbsoluteActivationSum += absoluteActivation;
                    accumulator.AbsoluteGradientSum += absoluteGradient;
                    accumulator.Snapshot.MinimumActivation = std::min(
                        accumulator.Snapshot.MinimumActivation,
                        activation);
                    accumulator.Snapshot.MaximumActivation = std::max(
                        accumulator.Snapshot.MaximumActivation,
                        activation);
                    accumulator.Snapshot.MaximumAbsoluteGradient = std::max(
                        accumulator.Snapshot.MaximumAbsoluteGradient,
                        absoluteGradient);

                    if (absoluteActivation <=
                        configuration.InactiveActivationMagnitude)
                    {
                        ++accumulator.InactiveSamples;
                    }
                    if (!neuron.IsInput && IsSaturated(
                        layer.Activation,
                        activation,
                        configuration.SaturationMargin))
                    {
                        ++accumulator.SaturatedSamples;
                    }
                    if (absoluteGradient <=
                        configuration.VanishingGradientMagnitude)
                    {
                        ++accumulator.VanishingGradientSamples;
                    }
                    if (absoluteGradient >=
                        configuration.ExplodingGradientMagnitude)
                    {
                        ++accumulator.ExplodingGradientSamples;
                    }
                }
            }

            for (const Core::BackwardTraceConnectionSnapshot& connection :
                trace.Connections)
            {
                auto found = connectionIndices.find(connection.ConnectionId);

                if (found == connectionIndices.end())
                {
                    ConnectionAccumulator accumulator;
                    accumulator.Snapshot.Id = connection.ConnectionId;
                    accumulator.Snapshot.FromNeuron = connection.FromNeuron;
                    accumulator.Snapshot.ToNeuron = connection.ToNeuron;
                    found = connectionIndices.emplace(
                        connection.ConnectionId,
                        connectionAccumulators.size()).first;
                    connectionAccumulators.push_back(std::move(accumulator));
                }

                ConnectionAccumulator& accumulator =
                    connectionAccumulators[found->second];
                const double absoluteGradient =
                    std::abs(connection.WeightGradient);

                if (!std::isfinite(absoluteGradient))
                {
                    return false;
                }

                ++accumulator.Snapshot.SampleCount;
                accumulator.AbsoluteGradientSum += absoluteGradient;
                accumulator.Snapshot.MaximumAbsoluteGradient = std::max(
                    accumulator.Snapshot.MaximumAbsoluteGradient,
                    absoluteGradient);

                if (absoluteGradient <=
                    configuration.VanishingGradientMagnitude)
                {
                    ++accumulator.VanishingGradientSamples;
                }
                if (absoluteGradient >=
                    configuration.ExplodingGradientMagnitude)
                {
                    ++accumulator.ExplodingGradientSamples;
                }
            }
        }

        Core::SignalHealthSnapshot snapshot;
        snapshot.Configuration = configuration;
        snapshot.Loss = loss;
        snapshot.DatasetSampleCount = dataset.Samples.size();
        snapshot.AnalyzedSampleCount = sampleCount;
        snapshot.Neurons.reserve(neuronAccumulators.size());
        snapshot.Connections.reserve(connectionAccumulators.size());

        for (NeuronAccumulator& accumulator : neuronAccumulators)
        {
            Core::SignalHealthNeuronSnapshot& neuron = accumulator.Snapshot;
            const double divisor = static_cast<double>(neuron.SampleCount);
            neuron.MeanActivation = accumulator.ActivationSum / divisor;
            neuron.MeanAbsoluteActivation =
                accumulator.AbsoluteActivationSum / divisor;
            neuron.MeanAbsoluteGradient =
                accumulator.AbsoluteGradientSum / divisor;
            neuron.InactiveSampleRatio = Ratio(
                accumulator.InactiveSamples,
                neuron.SampleCount);
            neuron.SaturatedSampleRatio = Ratio(
                accumulator.SaturatedSamples,
                neuron.SampleCount);
            neuron.VanishingGradientSampleRatio = Ratio(
                accumulator.VanishingGradientSamples,
                neuron.SampleCount);
            neuron.ExplodingGradientSampleRatio = Ratio(
                accumulator.ExplodingGradientSamples,
                neuron.SampleCount);
            neuron.ConsistentlyInactive = neuron.InactiveSampleRatio >=
                configuration.InactiveSampleRatio;
            neuron.ConsistentlySaturated = !neuron.IsInput &&
                neuron.SaturatedSampleRatio >=
                    configuration.SaturationSampleRatio;
            neuron.VanishingGradient =
                neuron.VanishingGradientSampleRatio >=
                    configuration.VanishingGradientSampleRatio;
            neuron.ExplodingGradient =
                neuron.ExplodingGradientSampleRatio >=
                    configuration.ExplodingGradientSampleRatio;

            snapshot.InactiveNeuronCount += neuron.ConsistentlyInactive;
            snapshot.SaturatedNeuronCount += neuron.ConsistentlySaturated;
            snapshot.VanishingGradientNeuronCount +=
                neuron.VanishingGradient;
            snapshot.ExplodingGradientNeuronCount +=
                neuron.ExplodingGradient;
            snapshot.HealthyNeuronCount +=
                !neuron.ConsistentlyInactive &&
                !neuron.ConsistentlySaturated &&
                !neuron.VanishingGradient &&
                !neuron.ExplodingGradient;
            snapshot.Neurons.push_back(std::move(neuron));
        }

        for (ConnectionAccumulator& accumulator : connectionAccumulators)
        {
            Core::SignalHealthConnectionSnapshot& connection =
                accumulator.Snapshot;
            const double divisor = static_cast<double>(
                connection.SampleCount);
            connection.MeanAbsoluteGradient =
                accumulator.AbsoluteGradientSum / divisor;
            connection.VanishingGradientSampleRatio = Ratio(
                accumulator.VanishingGradientSamples,
                connection.SampleCount);
            connection.ExplodingGradientSampleRatio = Ratio(
                accumulator.ExplodingGradientSamples,
                connection.SampleCount);
            connection.VanishingGradient =
                connection.VanishingGradientSampleRatio >=
                    configuration.VanishingGradientSampleRatio;
            connection.ExplodingGradient =
                connection.ExplodingGradientSampleRatio >=
                    configuration.ExplodingGradientSampleRatio;
            snapshot.VanishingGradientConnectionCount +=
                connection.VanishingGradient;
            snapshot.ExplodingGradientConnectionCount +=
                connection.ExplodingGradient;
            snapshot.HealthyConnectionCount +=
                !connection.VanishingGradient &&
                !connection.ExplodingGradient;
            snapshot.Connections.push_back(std::move(connection));
        }

        result = std::move(snapshot);
        return true;
    }
}
