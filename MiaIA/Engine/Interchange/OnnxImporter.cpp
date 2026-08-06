#include "OnnxImporter.h"

#include "../Validation/NetworkValidator.h"
#include "../../Core/Model/Network.h"

#include <onnx/checker.h>
#include <onnx/onnx_pb.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    using MiaIA::Core::ActivationType;
    using MiaIA::Core::Connection;
    using MiaIA::Core::Layer;
    using MiaIA::Core::Network;
    using MiaIA::Core::Neuron;

    struct TensorData
    {
        std::vector<std::int64_t> Dimensions;
        std::vector<double> Values;
    };

    struct DenseLayer
    {
        std::size_t InputCount{};
        std::size_t OutputCount{};
        std::vector<double> Weights;
        std::vector<double> Biases;
        ActivationType Activation{ ActivationType::Linear };
    };

    bool CheckedElementCount(
        const onnx::TensorProto& tensor,
        std::size_t& count)
    {
        count = 1;

        for (const std::int64_t dimension : tensor.dims())
        {
            if (dimension <= 0)
            {
                return false;
            }

            const auto converted =
                static_cast<std::uint64_t>(dimension);

            if (converted >
                std::numeric_limits<std::size_t>::max() / count)
            {
                return false;
            }

            count *= static_cast<std::size_t>(converted);
        }

        return true;
    }

    template<typename TValue>
    bool ReadRawValues(
        const std::string& rawData,
        std::size_t count,
        std::vector<double>& values)
    {
        if (count >
            std::numeric_limits<std::size_t>::max() / sizeof(TValue) ||
            rawData.size() != count * sizeof(TValue))
        {
            return false;
        }

        values.reserve(count);

        for (std::size_t index = 0; index < count; ++index)
        {
            TValue value{};
            std::memcpy(
                &value,
                rawData.data() + index * sizeof(TValue),
                sizeof(TValue));

            const double converted = static_cast<double>(value);

            if (!std::isfinite(converted))
            {
                return false;
            }

            values.push_back(converted);
        }

        return true;
    }

    bool ReadTensor(
        const onnx::TensorProto& tensor,
        TensorData& result)
    {
        std::size_t count{};

        if (!CheckedElementCount(tensor, count))
        {
            return false;
        }

        TensorData data;
        data.Dimensions.assign(
            tensor.dims().begin(),
            tensor.dims().end());

        if (!tensor.raw_data().empty())
        {
            bool read{};

            if (tensor.data_type() ==
                onnx::TensorProto_DataType_FLOAT)
            {
                read = ReadRawValues<float>(
                    tensor.raw_data(),
                    count,
                    data.Values);
            }
            else if (tensor.data_type() ==
                onnx::TensorProto_DataType_DOUBLE)
            {
                read = ReadRawValues<double>(
                    tensor.raw_data(),
                    count,
                    data.Values);
            }

            if (!read)
            {
                return false;
            }
        }
        else if (tensor.data_type() ==
            onnx::TensorProto_DataType_FLOAT)
        {
            if (count > static_cast<std::size_t>(
                    std::numeric_limits<int>::max()) ||
                tensor.float_data_size() !=
                static_cast<int>(count))
            {
                return false;
            }

            data.Values.reserve(count);

            for (const float value : tensor.float_data())
            {
                if (!std::isfinite(value))
                {
                    return false;
                }

                data.Values.push_back(static_cast<double>(value));
            }
        }
        else if (tensor.data_type() ==
            onnx::TensorProto_DataType_DOUBLE)
        {
            if (count > static_cast<std::size_t>(
                    std::numeric_limits<int>::max()) ||
                tensor.double_data_size() !=
                static_cast<int>(count))
            {
                return false;
            }

            data.Values.reserve(count);

            for (const double value : tensor.double_data())
            {
                if (!std::isfinite(value))
                {
                    return false;
                }

                data.Values.push_back(value);
            }
        }
        else
        {
            return false;
        }

        result = std::move(data);
        return true;
    }

    const onnx::TensorProto* FindInitializer(
        const std::unordered_map<std::string,
            const onnx::TensorProto*>& initializers,
        const std::string& name)
    {
        const auto iterator = initializers.find(name);

        if (iterator == initializers.end())
        {
            return nullptr;
        }

        return iterator->second;
    }

    bool ReadGemmAttributes(
        const onnx::NodeProto& node,
        double& alpha,
        double& beta,
        bool& transposeB)
    {
        alpha = 1.0;
        beta = 1.0;
        transposeB = false;
        bool transposeA = false;

        for (const onnx::AttributeProto& attribute : node.attribute())
        {
            if (attribute.name() == "alpha" && attribute.has_f())
            {
                alpha = attribute.f();
            }
            else if (attribute.name() == "beta" && attribute.has_f())
            {
                beta = attribute.f();
            }
            else if (attribute.name() == "transA" && attribute.has_i())
            {
                transposeA = attribute.i() != 0;
            }
            else if (attribute.name() == "transB" && attribute.has_i())
            {
                transposeB = attribute.i() != 0;
            }
            else
            {
                return false;
            }
        }

        return !transposeA &&
            std::isfinite(alpha) &&
            std::isfinite(beta);
    }

    bool ReadBiases(
        const onnx::TensorProto* tensor,
        std::size_t outputCount,
        double beta,
        std::vector<double>& biases)
    {
        biases.assign(outputCount, 0.0);

        if (tensor == nullptr)
        {
            return true;
        }

        TensorData data;

        if (!ReadTensor(*tensor, data))
        {
            return false;
        }

        const bool vectorShape =
            data.Dimensions.size() == 1 &&
            data.Dimensions[0] ==
                static_cast<std::int64_t>(outputCount);
        const bool rowShape =
            data.Dimensions.size() == 2 &&
            data.Dimensions[0] == 1 &&
            data.Dimensions[1] ==
                static_cast<std::int64_t>(outputCount);

        if (!vectorShape && !rowShape)
        {
            return false;
        }

        for (std::size_t index = 0; index < outputCount; ++index)
        {
            biases[index] = data.Values[index] * beta;

            if (!std::isfinite(biases[index]))
            {
                return false;
            }
        }

        return true;
    }

    bool ReadDenseLayer(
        const onnx::NodeProto& node,
        const std::unordered_map<std::string,
            const onnx::TensorProto*>& initializers,
        DenseLayer& result)
    {
        if (node.op_type() != "Gemm" ||
            node.input_size() < 2 ||
            node.input_size() > 3 ||
            node.output_size() != 1)
        {
            return false;
        }

        double alpha{};
        double beta{};
        bool transposeB{};

        if (!ReadGemmAttributes(
            node,
            alpha,
            beta,
            transposeB))
        {
            return false;
        }

        const onnx::TensorProto* weightsTensor =
            FindInitializer(initializers, node.input(1));

        if (weightsTensor == nullptr)
        {
            return false;
        }

        TensorData weightsData;

        if (!ReadTensor(*weightsTensor, weightsData) ||
            weightsData.Dimensions.size() != 2)
        {
            return false;
        }

        const std::int64_t inputDimension =
            transposeB ?
            weightsData.Dimensions[1] :
            weightsData.Dimensions[0];
        const std::int64_t outputDimension =
            transposeB ?
            weightsData.Dimensions[0] :
            weightsData.Dimensions[1];

        if (inputDimension <= 0 || outputDimension <= 0)
        {
            return false;
        }

        DenseLayer layer;
        layer.InputCount = static_cast<std::size_t>(inputDimension);
        layer.OutputCount = static_cast<std::size_t>(outputDimension);
        layer.Weights.resize(layer.InputCount * layer.OutputCount);

        for (std::size_t fromIndex = 0;
            fromIndex < layer.InputCount;
            ++fromIndex)
        {
            for (std::size_t toIndex = 0;
                toIndex < layer.OutputCount;
                ++toIndex)
            {
                const std::size_t sourceIndex =
                    transposeB ?
                    toIndex * layer.InputCount + fromIndex :
                    fromIndex * layer.OutputCount + toIndex;
                const double value =
                    weightsData.Values[sourceIndex] * alpha;

                if (!std::isfinite(value))
                {
                    return false;
                }

                layer.Weights[
                    fromIndex * layer.OutputCount + toIndex] = value;
            }
        }

        const onnx::TensorProto* biasesTensor = nullptr;

        if (node.input_size() == 3 && !node.input(2).empty())
        {
            biasesTensor = FindInitializer(
                initializers,
                node.input(2));

            if (biasesTensor == nullptr)
            {
                return false;
            }
        }

        if (!ReadBiases(
            biasesTensor,
            layer.OutputCount,
            beta,
            layer.Biases))
        {
            return false;
        }

        result = std::move(layer);
        return true;
    }

    bool TryActivation(
        const onnx::NodeProto& node,
        const std::string& input,
        ActivationType& activation,
        std::string& output)
    {
        if (node.input_size() != 1 ||
            node.output_size() != 1 ||
            node.input(0) != input ||
            node.attribute_size() != 0)
        {
            return false;
        }

        if (node.op_type() == "Sigmoid")
        {
            activation = ActivationType::Sigmoid;
        }
        else if (node.op_type() == "Relu")
        {
            activation = ActivationType::ReLU;
        }
        else if (node.op_type() == "Tanh")
        {
            activation = ActivationType::Tanh;
        }
        else
        {
            return false;
        }

        output = node.output(0);
        return !output.empty();
    }

    bool MatchesFeatureCount(
        const onnx::ValueInfoProto& value,
        std::size_t expectedCount)
    {
        if (!value.has_type() ||
            !value.type().has_tensor_type())
        {
            return false;
        }

        const auto& tensorType = value.type().tensor_type();

        if (tensorType.elem_type() !=
                onnx::TensorProto_DataType_FLOAT &&
            tensorType.elem_type() !=
                onnx::TensorProto_DataType_DOUBLE)
        {
            return false;
        }

        if (!tensorType.has_shape() ||
            tensorType.shape().dim_size() != 2)
        {
            return false;
        }

        const auto& featureDimension = tensorType.shape().dim(1);

        return !featureDimension.has_dim_value() ||
            featureDimension.dim_value() ==
                static_cast<std::int64_t>(expectedCount);
    }

    bool ReadGraph(
        const onnx::GraphProto& graph,
        std::vector<DenseLayer>& layers)
    {
        if (graph.output_size() != 1 || graph.node_size() == 0)
        {
            return false;
        }

        std::unordered_map<std::string,
            const onnx::TensorProto*> initializers;

        for (const onnx::TensorProto& initializer : graph.initializer())
        {
            if (initializer.name().empty() ||
                !initializers.emplace(
                    initializer.name(),
                    &initializer).second)
            {
                return false;
            }
        }

        std::string currentValue;
        const onnx::ValueInfoProto* graphInput = nullptr;

        for (const onnx::ValueInfoProto& input : graph.input())
        {
            if (initializers.find(input.name()) == initializers.end())
            {
                if (!currentValue.empty())
                {
                    return false;
                }

                currentValue = input.name();
                graphInput = &input;
            }
        }

        if (currentValue.empty() || graphInput == nullptr)
        {
            return false;
        }

        for (int nodeIndex = 0;
            nodeIndex < graph.node_size();
            ++nodeIndex)
        {
            const onnx::NodeProto& gemm = graph.node(nodeIndex);

            if (gemm.input_size() == 0 ||
                gemm.input(0) != currentValue)
            {
                return false;
            }

            DenseLayer layer;

            if (!ReadDenseLayer(gemm, initializers, layer))
            {
                return false;
            }

            if (!layers.empty() &&
                layers.back().OutputCount != layer.InputCount)
            {
                return false;
            }

            currentValue = gemm.output(0);

            if (currentValue.empty())
            {
                return false;
            }

            if (nodeIndex + 1 < graph.node_size())
            {
                const onnx::NodeProto& possibleActivation =
                    graph.node(nodeIndex + 1);
                ActivationType activation{};
                std::string activatedOutput;

                if (TryActivation(
                    possibleActivation,
                    currentValue,
                    activation,
                    activatedOutput))
                {
                    layer.Activation = activation;
                    currentValue = std::move(activatedOutput);
                    ++nodeIndex;
                }
            }

            layers.push_back(std::move(layer));
        }

        return !layers.empty() &&
            graph.output(0).name() == currentValue &&
            MatchesFeatureCount(
                *graphInput,
                layers.front().InputCount) &&
            MatchesFeatureCount(
                graph.output(0),
                layers.back().OutputCount);
    }

    bool ParseUnsigned(
        const std::string& value,
        std::uint64_t& result)
    {
        if (value.empty())
        {
            return false;
        }

        const char* begin = value.data();
        const char* end = begin + value.size();
        const auto parsed = std::from_chars(begin, end, result);

        return parsed.ec == std::errc{} && parsed.ptr == end;
    }

    bool ParseNeuronIds(
        const std::string& value,
        std::size_t expectedCount,
        std::vector<std::uint64_t>& ids)
    {
        std::size_t begin{};

        while (begin <= value.size())
        {
            const std::size_t separator = value.find(',', begin);
            const std::size_t length =
                separator == std::string::npos ?
                value.size() - begin : separator - begin;
            std::uint64_t id{};

            if (!ParseUnsigned(value.substr(begin, length), id))
            {
                return false;
            }

            ids.push_back(id);

            if (separator == std::string::npos)
            {
                break;
            }

            begin = separator + 1;
        }

        return ids.size() == expectedCount;
    }

    bool ReadMetadata(
        const onnx::ModelProto& model,
        const std::vector<std::size_t>& layerSizes,
        std::vector<std::uint64_t>& layerIds,
        std::vector<std::string>& layerNames,
        std::vector<std::vector<std::uint64_t>>& neuronIds)
    {
        std::unordered_map<std::string, std::string> properties;

        for (const onnx::StringStringEntryProto& property :
            model.metadata_props())
        {
            if (!properties.emplace(
                property.key(),
                property.value()).second)
            {
                return false;
            }
        }

        const auto schema = properties.find("miaia.schema_version");

        if (schema == properties.end())
        {
            std::uint64_t nextNeuronId = 1001;

            for (std::size_t order = 0;
                order < layerSizes.size();
                ++order)
            {
                layerIds.push_back(static_cast<std::uint64_t>(order));

                if (order == 0)
                {
                    layerNames.push_back("Input");
                }
                else if (order + 1 == layerSizes.size())
                {
                    layerNames.push_back("Output");
                }
                else
                {
                    layerNames.push_back("Hidden " +
                        std::to_string(order));
                }

                std::vector<std::uint64_t> currentIds;
                currentIds.reserve(layerSizes[order]);

                for (std::size_t index = 0;
                    index < layerSizes[order];
                    ++index)
                {
                    currentIds.push_back(nextNeuronId++);
                }

                neuronIds.push_back(std::move(currentIds));
            }

            return true;
        }

        if (schema->second != "1")
        {
            return false;
        }

        std::unordered_set<std::uint64_t> uniqueLayerIds;
        std::unordered_set<std::uint64_t> uniqueNeuronIds;

        for (std::size_t order = 0;
            order < layerSizes.size();
            ++order)
        {
            const std::string prefix =
                "miaia.layer." + std::to_string(order);
            const auto id = properties.find(prefix + ".id");
            const auto name = properties.find(prefix + ".name");
            const auto neurons =
                properties.find(prefix + ".neuron_ids");

            if (id == properties.end() ||
                name == properties.end() ||
                neurons == properties.end() ||
                name->second.empty())
            {
                return false;
            }

            std::uint64_t layerId{};

            if (!ParseUnsigned(id->second, layerId) ||
                !uniqueLayerIds.insert(layerId).second)
            {
                return false;
            }

            std::vector<std::uint64_t> currentIds;

            if (!ParseNeuronIds(
                neurons->second,
                layerSizes[order],
                currentIds))
            {
                return false;
            }

            for (const std::uint64_t neuronId : currentIds)
            {
                if (!uniqueNeuronIds.insert(neuronId).second)
                {
                    return false;
                }
            }

            layerIds.push_back(layerId);
            layerNames.push_back(name->second);
            neuronIds.push_back(std::move(currentIds));
        }

        return true;
    }

    bool BuildNetwork(
        const onnx::ModelProto& model,
        const std::vector<DenseLayer>& denseLayers,
        Network& result)
    {
        std::vector<std::size_t> layerSizes;
        layerSizes.reserve(denseLayers.size() + 1);
        layerSizes.push_back(denseLayers.front().InputCount);

        for (const DenseLayer& layer : denseLayers)
        {
            layerSizes.push_back(layer.OutputCount);
        }

        std::vector<std::uint64_t> layerIds;
        std::vector<std::string> layerNames;
        std::vector<std::vector<std::uint64_t>> neuronIds;

        if (!ReadMetadata(
            model,
            layerSizes,
            layerIds,
            layerNames,
            neuronIds))
        {
            return false;
        }

        Network network;
        network.Layers.reserve(layerSizes.size());

        for (std::size_t order = 0;
            order < layerSizes.size();
            ++order)
        {
            Layer layer;
            layer.Id = layerIds[order];
            layer.Name = layerNames[order];
            layer.Order = static_cast<std::uint64_t>(order);

            if (order > 0)
            {
                layer.Activation = denseLayers[order - 1].Activation;
            }

            layer.Neurons.reserve(layerSizes[order]);

            for (std::size_t index = 0;
                index < layerSizes[order];
                ++index)
            {
                Neuron neuron;
                neuron.Id = neuronIds[order][index];

                if (order > 0)
                {
                    neuron.Bias =
                        denseLayers[order - 1].Biases[index];
                }

                layer.Neurons.push_back(neuron);
            }

            network.Layers.push_back(std::move(layer));
        }

        std::uint64_t connectionId = 1;

        for (std::size_t layerIndex = 0;
            layerIndex < denseLayers.size();
            ++layerIndex)
        {
            const DenseLayer& denseLayer = denseLayers[layerIndex];

            for (std::size_t fromIndex = 0;
                fromIndex < denseLayer.InputCount;
                ++fromIndex)
            {
                for (std::size_t toIndex = 0;
                    toIndex < denseLayer.OutputCount;
                    ++toIndex)
                {
                    Connection connection;
                    connection.Id = connectionId++;
                    connection.FromNeuron =
                        neuronIds[layerIndex][fromIndex];
                    connection.ToNeuron =
                        neuronIds[layerIndex + 1][toIndex];
                    connection.Weight = denseLayer.Weights[
                        fromIndex * denseLayer.OutputCount + toIndex];
                    network.Connections.push_back(connection);
                }
            }
        }

        if (!MiaIA::Engine::NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        result = std::move(network);
        return true;
    }
}

namespace MiaIA::Engine
{
    bool OnnxImporter::Import(
        Core::Network& network,
        const std::string& path)
    {
        if (path.empty())
        {
            return false;
        }

        std::ifstream input(path, std::ios::binary);

        if (!input)
        {
            return false;
        }

        onnx::ModelProto model;

        if (!model.ParseFromIstream(&input))
        {
            return false;
        }

        try
        {
            onnx::checker::check_model(model);
        }
        catch (...)
        {
            return false;
        }

        std::vector<DenseLayer> layers;

        if (!ReadGraph(model.graph(), layers))
        {
            return false;
        }

        Core::Network importedNetwork;

        if (!BuildNetwork(model, layers, importedNetwork))
        {
            return false;
        }

        network = std::move(importedNetwork);
        return true;
    }
}
