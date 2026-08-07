#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include <onnx/checker.h>
#include <onnx/onnx_pb.h>
#include "TestHarness.h"
#include "../SDK/Include/MiaIAClient.h"
#include "../Core/Execution/Activation.h"
#include "../Engine/Validation/NetworkValidator.h"

// Keep the existing checks active in every build configuration.
#ifdef assert
#undef assert
#endif
#define assert(expression) MIAIA_CHECK(expression)

namespace
{
    const onnx::TensorProto* FindInitializer(
        const onnx::GraphProto& graph,
        const std::string& name)
    {
        for (const onnx::TensorProto& tensor : graph.initializer())
        {
            if (tensor.name() == name)
            {
                return &tensor;
            }
        }

        return nullptr;
    }
}

int main()
{
    using MiaIA::SDK::MiaIAClient;

    MiaIA::Tests::TestRunner runner;

    runner.Run("Network editing and snapshots", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(!MiaIAClient::AddLayer(0, "Duplicate", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25));
    assert(!MiaIAClient::AddNeuron(0, 1001, 0.10, 0.10));

    const double nan =
        std::numeric_limits<double>::quiet_NaN();

    const double infinity =
        std::numeric_limits<double>::infinity();

    assert(!MiaIAClient::AddNeuron(0, 1002, nan, 0.0));
    assert(!MiaIAClient::AddNeuron(0, 1002, infinity, 0.0));
    assert(!MiaIAClient::AddNeuron(0, 1002, 0.0, nan));
    assert(!MiaIAClient::AddNeuron(0, 1002, 0.0, infinity));

    assert(MiaIAClient::AddLayer(1, "Hidden", 1));
    assert(!MiaIAClient::AddNeuron(1, 1001, 0.60, 0.30));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30));

    const auto afterInvalidNeurons = MiaIAClient::GetSnapshot();

    assert(afterInvalidNeurons.Layers[0].Neurons.size() == 1);
    assert(afterInvalidNeurons.Layers[1].Neurons.size() == 1);

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));
    assert(!MiaIAClient::AddConnection(1, 1001, 2001, 0.8));
    assert(!MiaIAClient::AddConnection(2, 1001, 9999, 0.5));
    
    assert(MiaIAClient::SetNeuronActivation(1001, 0.95));
    assert(MiaIAClient::SetNeuronBias(1001, 0.85));
    assert(MiaIAClient::SetConnectionWeight(1, 0.65));

    const auto snapshot = MiaIAClient::GetSnapshot();

    assert(snapshot.Layers[0].Neurons[0].Activation == 0.95);
    assert(snapshot.Layers[0].Neurons[0].Bias == 0.85);
    assert(snapshot.Connections[0].Weight == 0.65);


    assert(snapshot.Layers.size() == 2);
    assert(snapshot.Connections.size() == 1);
    assert(snapshot.Connections[0].Id == 1);
    assert(snapshot.Connections[0].FromNeuron == 1001);
    assert(snapshot.Connections[0].ToNeuron == 2001);
    assert(snapshot.Connections[0].Weight == 0.65);

    assert(MiaIAClient::RemoveNeuron(1001));

    const auto afterNeuronRemoval = MiaIAClient::GetSnapshot();

    assert(afterNeuronRemoval.Layers[0].Neurons.empty());
    assert(afterNeuronRemoval.Connections.empty());

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Hidden", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));

    assert(MiaIAClient::RemoveLayer(0));

    const auto afterLayerRemoval = MiaIAClient::GetSnapshot();

    assert(afterLayerRemoval.Layers.size() == 1);
    assert(afterLayerRemoval.Layers[0].Id == 1);
    assert(afterLayerRemoval.Connections.empty());

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Hidden", 1));
    assert(MiaIAClient::AddNeuron(0, 1001, 0.75, 0.25));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.60, 0.30));
    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));

    MiaIA::Core::NeuronSnapshot neuron;
    MiaIA::Core::ConnectionSnapshot connection;
    MiaIA::Core::LayerSnapshot layer;

    assert(MiaIAClient::TryGetNeuron(1001, neuron));
    assert(neuron.Id == 1001);
    assert(neuron.Bias == 0.75);
    assert(neuron.Activation == 0.25);

    assert(MiaIAClient::TryGetConnection(1, connection));
    assert(connection.Id == 1);
    assert(connection.FromNeuron == 1001);
    assert(connection.ToNeuron == 2001);
    assert(connection.Weight == 0.8);

    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Tanh));

    assert(!MiaIAClient::SetLayerActivation(
        9999,
        MiaIA::Core::ActivationType::Linear));

    layer.Order = 999;
    layer.Activation = MiaIA::Core::ActivationType::Linear;

    assert(MiaIAClient::TryGetLayer(1, layer));
    assert(layer.Id == 1);
    assert(layer.Name == "Hidden");
    assert(layer.Order == 1);
    assert(layer.Activation == MiaIA::Core::ActivationType::Tanh);
    assert(layer.Neurons.size() == 1);
    assert(layer.Neurons[0].Id == 2001);

    assert(!MiaIAClient::TryGetNeuron(9999, neuron));
    assert(!MiaIAClient::TryGetConnection(9999, connection));
    assert(!MiaIAClient::TryGetLayer(9999, layer));

    assert(neuron.Id == 1001);
    assert(connection.Id == 1);
    assert(layer.Id == 1);

    const auto afterInspection = MiaIAClient::GetSnapshot();

    assert(afterInspection.Layers.size() == 2);
    assert(afterInspection.Connections.size() == 1);

    assert(MiaIAClient::RemoveConnection(1));
    assert(!MiaIAClient::RemoveConnection(1));

    const auto afterConnectionRemoval = MiaIAClient::GetSnapshot();

    assert(afterConnectionRemoval.Connections.empty());

    });

    runner.Run("CSV dataset pipeline", [&]()
    {
    const std::filesystem::path headerPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_header_test.csv";
    const std::filesystem::path noHeaderPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_no_header_test.csv";
    const std::filesystem::path invalidPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_invalid_test.csv";
    const std::filesystem::path nonFinitePath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_non_finite_test.csv";

    {
        std::ofstream output(headerPath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0,0,0\n"
            << "0,1,1\n"
            << "1,0,1\n"
            << "1,1,0\n";
    }

    {
        std::ofstream output(noHeaderPath);
        assert(output.good());
        output
            << "0.25,-0.5,1\n"
            << "0.75,0.5,0\n";
    }

    {
        std::ofstream output(invalidPath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0,1\n";
    }

    {
        std::ofstream output(nonFinitePath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0,nan,1\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        headerPath.string(),
        2,
        1));

    const auto summary = MiaIAClient::GetDatasetSummary();

    assert(summary.Name == "miaia_dataset_header_test");
    assert(summary.Source == headerPath.string());
    assert(summary.SampleCount == 4);
    assert(summary.InputCount == 2);
    assert(summary.TargetCount == 1);

    MiaIA::Core::SampleSnapshot sample;

    assert(MiaIAClient::TryGetDatasetSample(2, sample));
    assert(sample.Index == 2);
    assert(sample.Inputs.size() == 2);
    assert(sample.Targets.size() == 1);
    assert(sample.Inputs[0] == 1.0);
    assert(sample.Inputs[1] == 0.0);
    assert(sample.Targets[0] == 1.0);
    assert(!MiaIAClient::TryGetDatasetSample(4, sample));

    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));
    assert(MiaIAClient::ApplyDatasetSample(2));

    const auto appliedSnapshot = MiaIAClient::GetSnapshot();

    assert(appliedSnapshot.Layers[0].Neurons[0].Activation == 1.0);
    assert(appliedSnapshot.Layers[0].Neurons[1].Activation == 0.0);

    assert(!MiaIAClient::ImportCsvDataset(
        invalidPath.string(),
        2,
        1));
    assert(!MiaIAClient::ImportCsvDataset(
        nonFinitePath.string(),
        2,
        1));
    assert(!MiaIAClient::ImportCsvDataset("", 2, 1));
    assert(!MiaIAClient::ImportCsvDataset(
        headerPath.string(),
        0,
        1));

    const auto afterFailedImport = MiaIAClient::GetDatasetSummary();

    assert(afterFailedImport.Source == summary.Source);
    assert(afterFailedImport.SampleCount == summary.SampleCount);

    assert(MiaIAClient::ImportCsvDataset(
        noHeaderPath.string(),
        2,
        1,
        false));

    const auto noHeaderSummary = MiaIAClient::GetDatasetSummary();

    assert(noHeaderSummary.SampleCount == 2);

    assert(MiaIAClient::CreateDenseNetwork(3, 2, 1, 1));
    assert(MiaIAClient::SetInputValues({ 0.1, 0.2, 0.3 }));
    assert(!MiaIAClient::ApplyDatasetSample(0));

    const auto afterFailedApply = MiaIAClient::GetSnapshot();

    assert(afterFailedApply.Layers[0].Neurons[0].Activation == 0.1);
    assert(afterFailedApply.Layers[0].Neurons[1].Activation == 0.2);
    assert(afterFailedApply.Layers[0].Neurons[2].Activation == 0.3);

    MiaIAClient::ClearDataset();

    const auto clearedSummary = MiaIAClient::GetDatasetSummary();

    assert(clearedSummary.SampleCount == 0);
    assert(!MiaIAClient::TryGetDatasetSample(0, sample));
    assert(!MiaIAClient::ApplyDatasetSample(0));

    std::filesystem::remove(headerPath);
    std::filesystem::remove(noHeaderPath);
    std::filesystem::remove(invalidPath);
    std::filesystem::remove(nonFinitePath);

    });

    runner.Run("Dataset sample evaluation", [&]()
    {
    const std::filesystem::path evaluationPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_evaluation_test.csv";
    const std::filesystem::path incompatiblePath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_evaluation_incompatible_test.csv";

    {
        std::ofstream output(evaluationPath);
        assert(output.good());
        output
            << "x1,x2,target1,target2\n"
            << "0.5,-1,1,2\n";
    }

    {
        std::ofstream output(incompatiblePath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0.5,-1,1\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        evaluationPath.string(),
        2,
        2));
    assert(MiaIAClient::CreateDenseNetwork(2, 2, 0, 2));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));

    assert(MiaIAClient::SetConnectionWeight(1, 2.0));
    assert(MiaIAClient::SetConnectionWeight(2, -1.0));
    assert(MiaIAClient::SetConnectionWeight(3, 3.0));
    assert(MiaIAClient::SetConnectionWeight(4, 0.5));
    assert(MiaIAClient::SetNeuronBias(1003, 0.25));
    assert(MiaIAClient::SetNeuronBias(1004, -0.5));

    const auto beforeEvaluation = MiaIAClient::GetSnapshot();

    MiaIA::Core::SampleEvaluationSnapshot evaluation;

    assert(MiaIAClient::EvaluateDatasetSample(
        0,
        MiaIA::Core::LossType::MeanSquaredError,
        evaluation));

    assert(evaluation.SampleIndex == 0);
    assert(evaluation.Type == MiaIA::Core::LossType::MeanSquaredError);
    assert(evaluation.Targets.size() == 2);
    assert(evaluation.Predictions.size() == 2);
    assert(evaluation.Errors.size() == 2);
    assert(std::abs(evaluation.Targets[0] - 1.0) < 1e-12);
    assert(std::abs(evaluation.Targets[1] - 2.0) < 1e-12);
    assert(std::abs(evaluation.Predictions[0] - (-1.75)) < 1e-12);
    assert(std::abs(evaluation.Predictions[1] - (-1.5)) < 1e-12);
    assert(std::abs(evaluation.Errors[0] - (-2.75)) < 1e-12);
    assert(std::abs(evaluation.Errors[1] - (-3.5)) < 1e-12);
    assert(std::abs(evaluation.Loss - 9.90625) < 1e-12);

    const auto afterEvaluation = MiaIAClient::GetSnapshot();

    assert(afterEvaluation.Connections.size() ==
        beforeEvaluation.Connections.size());

    for (std::size_t index = 0;
        index < afterEvaluation.Connections.size();
        ++index)
    {
        assert(afterEvaluation.Connections[index].Weight ==
            beforeEvaluation.Connections[index].Weight);
    }

    assert(afterEvaluation.Layers[1].Neurons[0].Bias ==
        beforeEvaluation.Layers[1].Neurons[0].Bias);
    assert(afterEvaluation.Layers[1].Neurons[1].Bias ==
        beforeEvaluation.Layers[1].Neurons[1].Bias);

    const double successfulLoss = evaluation.Loss;

    assert(!MiaIAClient::EvaluateDatasetSample(
        1,
        MiaIA::Core::LossType::MeanSquaredError,
        evaluation));
    assert(evaluation.Loss == successfulLoss);

    assert(!MiaIAClient::EvaluateDatasetSample(
        0,
        static_cast<MiaIA::Core::LossType>(999),
        evaluation));
    assert(evaluation.Loss == successfulLoss);

    assert(MiaIAClient::ImportCsvDataset(
        incompatiblePath.string(),
        2,
        1));
    assert(MiaIAClient::SetInputValues({ 0.25, 0.75 }));

    const auto beforeIncompatibleEvaluation =
        MiaIAClient::GetSnapshot();

    assert(!MiaIAClient::EvaluateDatasetSample(
        0,
        MiaIA::Core::LossType::MeanSquaredError,
        evaluation));
    assert(evaluation.Loss == successfulLoss);

    const auto afterIncompatibleEvaluation =
        MiaIAClient::GetSnapshot();

    assert(afterIncompatibleEvaluation.Layers[0].Neurons[0].Activation ==
        beforeIncompatibleEvaluation.Layers[0].Neurons[0].Activation);
    assert(afterIncompatibleEvaluation.Layers[0].Neurons[1].Activation ==
        beforeIncompatibleEvaluation.Layers[0].Neurons[1].Activation);

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    std::filesystem::remove(evaluationPath);
    std::filesystem::remove(incompatiblePath);

    });

    runner.Run("Dataset sample gradients", [&]()
    {
    const std::filesystem::path analyticalPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_gradient_analytical_test.csv";
    const std::filesystem::path numericalPath =
        std::filesystem::temp_directory_path() /
        "miaia_dataset_gradient_numerical_test.csv";

    {
        std::ofstream output(analyticalPath);
        assert(output.good());
        output
            << "x1,x2,target1,target2\n"
            << "0.5,-1,1,2\n";
    }

    {
        std::ofstream output(numericalPath);
        assert(output.good());
        output
            << "x1,x2,target\n"
            << "0.3,-0.2,0.7\n";
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::ImportCsvDataset(
        analyticalPath.string(),
        2,
        2));
    assert(MiaIAClient::CreateDenseNetwork(2, 2, 0, 2));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));
    assert(MiaIAClient::SetConnectionWeight(1, 2.0));
    assert(MiaIAClient::SetConnectionWeight(2, -1.0));
    assert(MiaIAClient::SetConnectionWeight(3, 3.0));
    assert(MiaIAClient::SetConnectionWeight(4, 0.5));
    assert(MiaIAClient::SetNeuronBias(1003, 0.25));
    assert(MiaIAClient::SetNeuronBias(1004, -0.5));

    const auto beforeAnalyticalGradients =
        MiaIAClient::GetSnapshot();

    MiaIA::Core::SampleGradientSnapshot analyticalGradients;

    assert(MiaIAClient::EvaluateDatasetSampleGradients(
        0,
        MiaIA::Core::LossType::MeanSquaredError,
        analyticalGradients));

    assert(analyticalGradients.Neurons.size() == 4);
    assert(analyticalGradients.Connections.size() == 4);
    assert(std::abs(
        analyticalGradients.Evaluation.Loss - 9.90625) < 1e-12);

    assert(analyticalGradients.Neurons[0].Id == 1001);
    assert(analyticalGradients.Neurons[0].LayerOrder == 0);
    assert(std::abs(
        analyticalGradients.Neurons[0].ActivationGradient - (-2.0)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Neurons[1].ActivationGradient - (-10.0)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Neurons[2].BiasGradient - (-2.75)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Neurons[3].BiasGradient - (-3.5)) <
        1e-12);

    assert(analyticalGradients.Connections[0].Id == 1);
    assert(std::abs(
        analyticalGradients.Connections[0].WeightGradient - (-1.375)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Connections[1].WeightGradient - (-1.75)) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Connections[2].WeightGradient - 2.75) <
        1e-12);
    assert(std::abs(
        analyticalGradients.Connections[3].WeightGradient - 3.5) <
        1e-12);

    const auto afterAnalyticalGradients =
        MiaIAClient::GetSnapshot();

    for (std::size_t index = 0;
        index < beforeAnalyticalGradients.Connections.size();
        ++index)
    {
        assert(beforeAnalyticalGradients.Connections[index].Weight ==
            afterAnalyticalGradients.Connections[index].Weight);
    }

    assert(beforeAnalyticalGradients.Layers[1].Neurons[0].Bias ==
        afterAnalyticalGradients.Layers[1].Neurons[0].Bias);
    assert(beforeAnalyticalGradients.Layers[1].Neurons[1].Bias ==
        afterAnalyticalGradients.Layers[1].Neurons[1].Bias);

    const double analyticalLoss =
        analyticalGradients.Evaluation.Loss;

    assert(!MiaIAClient::EvaluateDatasetSampleGradients(
        1,
        MiaIA::Core::LossType::MeanSquaredError,
        analyticalGradients));
    assert(analyticalGradients.Evaluation.Loss == analyticalLoss);

    assert(!MiaIAClient::EvaluateDatasetSampleGradients(
        0,
        static_cast<MiaIA::Core::LossType>(999),
        analyticalGradients));
    assert(analyticalGradients.Evaluation.Loss == analyticalLoss);

    assert(MiaIAClient::ImportCsvDataset(
        numericalPath.string(),
        2,
        1));
    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Tanh));
    assert(MiaIAClient::SetLayerActivation(
        2,
        MiaIA::Core::ActivationType::Sigmoid));

    assert(MiaIAClient::SetConnectionWeight(1, 0.4));
    assert(MiaIAClient::SetConnectionWeight(2, -0.3));
    assert(MiaIAClient::SetConnectionWeight(3, 0.2));
    assert(MiaIAClient::SetConnectionWeight(4, 0.1));
    assert(MiaIAClient::SetConnectionWeight(5, -0.5));
    assert(MiaIAClient::SetConnectionWeight(6, 0.6));
    assert(MiaIAClient::SetNeuronBias(1003, 0.05));
    assert(MiaIAClient::SetNeuronBias(1004, -0.02));
    assert(MiaIAClient::SetNeuronBias(1005, 0.1));

    const auto beforeNumericalGradients =
        MiaIAClient::GetSnapshot();

    MiaIA::Core::SampleGradientSnapshot gradients;

    assert(MiaIAClient::EvaluateDatasetSampleGradients(
        0,
        MiaIA::Core::LossType::MeanSquaredError,
        gradients));

    const auto afterNumericalGradients =
        MiaIAClient::GetSnapshot();

    for (std::size_t index = 0;
        index < beforeNumericalGradients.Connections.size();
        ++index)
    {
        assert(beforeNumericalGradients.Connections[index].Weight ==
            afterNumericalGradients.Connections[index].Weight);
    }

    for (std::size_t layerIndex = 1;
        layerIndex < beforeNumericalGradients.Layers.size();
        ++layerIndex)
    {
        for (std::size_t neuronIndex = 0;
            neuronIndex <
                beforeNumericalGradients.Layers[layerIndex].Neurons.size();
            ++neuronIndex)
        {
            assert(
                beforeNumericalGradients.Layers[layerIndex]
                    .Neurons[neuronIndex].Bias ==
                afterNumericalGradients.Layers[layerIndex]
                    .Neurons[neuronIndex].Bias);
        }
    }

    const auto findConnectionGradient =
        [&](std::uint64_t id) -> const MiaIA::Core::ConnectionGradientSnapshot*
        {
            for (const auto& connection : gradients.Connections)
            {
                if (connection.Id == id)
                {
                    return &connection;
                }
            }

            return nullptr;
        };

    const auto findNeuronGradient =
        [&](std::uint64_t id) -> const MiaIA::Core::NeuronGradientSnapshot*
        {
            for (const auto& neuron : gradients.Neurons)
            {
                if (neuron.Id == id)
                {
                    return &neuron;
                }
            }

            return nullptr;
        };

    constexpr double Epsilon = 1e-6;
    constexpr double GradientTolerance = 1e-6;

    for (std::uint64_t connectionId = 1;
        connectionId <= 6;
        ++connectionId)
    {
        MiaIA::Core::ConnectionSnapshot connection;
        assert(MiaIAClient::TryGetConnection(connectionId, connection));

        MiaIA::Core::SampleEvaluationSnapshot plusEvaluation;
        MiaIA::Core::SampleEvaluationSnapshot minusEvaluation;

        assert(MiaIAClient::SetConnectionWeight(
            connectionId,
            connection.Weight + Epsilon));
        assert(MiaIAClient::EvaluateDatasetSample(
            0,
            MiaIA::Core::LossType::MeanSquaredError,
            plusEvaluation));

        assert(MiaIAClient::SetConnectionWeight(
            connectionId,
            connection.Weight - Epsilon));
        assert(MiaIAClient::EvaluateDatasetSample(
            0,
            MiaIA::Core::LossType::MeanSquaredError,
            minusEvaluation));

        assert(MiaIAClient::SetConnectionWeight(
            connectionId,
            connection.Weight));

        const double numericalGradient =
            (plusEvaluation.Loss - minusEvaluation.Loss) /
            (2.0 * Epsilon);

        const auto* analyticalGradient =
            findConnectionGradient(connectionId);

        assert(analyticalGradient != nullptr);
        assert(std::abs(
            analyticalGradient->WeightGradient - numericalGradient) <
            GradientTolerance);
    }

    for (const std::uint64_t neuronId :
        std::vector<std::uint64_t>{ 1003, 1004, 1005 })
    {
        MiaIA::Core::NeuronSnapshot neuron;
        assert(MiaIAClient::TryGetNeuron(neuronId, neuron));

        MiaIA::Core::SampleEvaluationSnapshot plusEvaluation;
        MiaIA::Core::SampleEvaluationSnapshot minusEvaluation;

        assert(MiaIAClient::SetNeuronBias(
            neuronId,
            neuron.Bias + Epsilon));
        assert(MiaIAClient::EvaluateDatasetSample(
            0,
            MiaIA::Core::LossType::MeanSquaredError,
            plusEvaluation));

        assert(MiaIAClient::SetNeuronBias(
            neuronId,
            neuron.Bias - Epsilon));
        assert(MiaIAClient::EvaluateDatasetSample(
            0,
            MiaIA::Core::LossType::MeanSquaredError,
            minusEvaluation));

        assert(MiaIAClient::SetNeuronBias(
            neuronId,
            neuron.Bias));

        const double numericalGradient =
            (plusEvaluation.Loss - minusEvaluation.Loss) /
            (2.0 * Epsilon);

        const auto* analyticalGradient =
            findNeuronGradient(neuronId);

        assert(analyticalGradient != nullptr);
        assert(std::abs(
            analyticalGradient->BiasGradient - numericalGradient) <
            GradientTolerance);
    }

    MiaIAClient::ClearDataset();
    MiaIAClient::ClearNetwork();

    std::filesystem::remove(analyticalPath);
    std::filesystem::remove(numericalPath);

    });

    runner.Run("Network input", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(3, 2, 1, 1));

    const std::vector<double> inputValues{
        0.25,
        -0.5,
        1.0
    };

    assert(MiaIAClient::SetInputValues(inputValues));

    const auto inputSnapshot = MiaIAClient::GetSnapshot();

    assert(inputSnapshot.Layers[0].Neurons.size() == 3);
    assert(inputSnapshot.Layers[0].Neurons[0].Activation == 0.25);
    assert(inputSnapshot.Layers[0].Neurons[1].Activation == -0.5);
    assert(inputSnapshot.Layers[0].Neurons[2].Activation == 1.0);

    assert(!MiaIAClient::SetInputValues({ 0.1, 0.2 }));
    assert(!MiaIAClient::SetInputValues({ 0.1, 0.2, 0.3, 0.4 }));
    assert(!MiaIAClient::SetInputValues({
        0.1,
        std::numeric_limits<double>::quiet_NaN(),
        0.3
        }));
    assert(!MiaIAClient::SetInputValues({
        0.1,
        std::numeric_limits<double>::infinity(),
        0.3
        }));

    const auto afterInvalidInput = MiaIAClient::GetSnapshot();

    assert(afterInvalidInput.Layers[0].Neurons[0].Activation == 0.25);
    assert(afterInvalidInput.Layers[0].Neurons[1].Activation == -0.5);
    assert(afterInvalidInput.Layers[0].Neurons[2].Activation == 1.0);

    assert(MiaIAClient::Forward());

    MiaIAClient::ClearNetwork();

    assert(!MiaIAClient::SetInputValues({ 1.0 }));

    });

    runner.Run("Forward propagation and activations", [&]()
    {

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));

    assert(MiaIAClient::Forward());

    const auto forwardSnapshot = MiaIAClient::GetSnapshot();

    const double expected = MiaIA::Core::Activation::Sigmoid(1.0);

    assert(std::abs(forwardSnapshot.Layers[1].Neurons[0].Activation - expected) < 0.000001);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(0, 1002, 0.0, 0.5));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.2, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.8));
    assert(MiaIAClient::AddConnection(2, 1002, 2001, 0.4));

    assert(MiaIAClient::Forward());

    const auto multiInputSnapshot = MiaIAClient::GetSnapshot();

    const double multiInputExpected =
        MiaIA::Core::Activation::Sigmoid(
            0.2 + (1.0 * 0.8) + (0.5 * 0.4));

    assert(std::abs(
        multiInputSnapshot.Layers[1].Neurons[0].Activation -
        multiInputExpected
    ) < 0.000001);


    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Hidden", 1));
    assert(MiaIAClient::AddLayer(2, "Output", 2));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));
    assert(MiaIAClient::AddNeuron(2, 3001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));
    assert(MiaIAClient::AddConnection(2, 2001, 3001, 1.0));

    assert(MiaIAClient::Forward());

    const auto threeLayerSnapshot = MiaIAClient::GetSnapshot();

    const double hiddenExpected =
        MiaIA::Core::Activation::Sigmoid(1.0);

    const double outputExpected =
        MiaIA::Core::Activation::Sigmoid(hiddenExpected);

    assert(std::abs(
        threeLayerSnapshot.Layers[1].Neurons[0].Activation -
        hiddenExpected
    ) < 0.000001);

    assert(std::abs(
        threeLayerSnapshot.Layers[2].Neurons[0].Activation -
        outputExpected
    ) < 0.000001);

    MiaIAClient::ClearNetwork();

    assert(!MiaIAClient::Forward());

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(!MiaIAClient::Forward());


    assert(MiaIA::Core::Activation::ReLU(2.5) == 2.5);
    assert(MiaIA::Core::Activation::ReLU(0.0) == 0.0);
    assert(MiaIA::Core::Activation::ReLU(-3.0) == 0.0);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::SetLayerActivation(1, MiaIA::Core::ActivationType::ReLU));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, -2.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));

    assert(MiaIAClient::Forward());

    const auto reluSnapshot = MiaIAClient::GetSnapshot();

    assert(reluSnapshot.Layers[1].Neurons[0].Activation == 0.0);

    assert(reluSnapshot.Layers[1].Activation ==  MiaIA::Core::ActivationType::ReLU);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Tanh));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));

    assert(MiaIAClient::Forward());

    const auto tanhSnapshot = MiaIAClient::GetSnapshot();

    const double tanhExpected =
        MiaIA::Core::Activation::Tanh(1.0);

    assert(std::abs(
        tanhSnapshot.Layers[1].Neurons[0].Activation -
        tanhExpected
    ) < 0.000001);

    assert(
        tanhSnapshot.Layers[1].Activation ==
        MiaIA::Core::ActivationType::Tanh);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::Linear));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 2.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.5, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.5));

    assert(MiaIAClient::Forward());

    const auto linearSnapshot = MiaIAClient::GetSnapshot();

    assert(std::abs(
        linearSnapshot.Layers[1].Neurons[0].Activation - 3.5
    ) < 0.000001);

    assert(
        linearSnapshot.Layers[1].Activation ==
        MiaIA::Core::ActivationType::Linear);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(20, "Output", 2));
    assert(MiaIAClient::AddLayer(10, "Input", 0));
    assert(MiaIAClient::AddLayer(15, "Hidden", 1));

    assert(MiaIAClient::AddNeuron(10, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(15, 2001, 0.0, 0.0));
    assert(MiaIAClient::AddNeuron(20, 3001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));
    assert(MiaIAClient::AddConnection(2, 2001, 3001, 1.0));

    assert(MiaIAClient::Forward());

    const auto orderedSnapshot = MiaIAClient::GetSnapshot();

    assert(orderedSnapshot.Layers[0].Order == 0);
    assert(orderedSnapshot.Layers[1].Order == 1);
    assert(orderedSnapshot.Layers[2].Order == 2);

    assert(orderedSnapshot.Layers[0].Name == "Input");
    assert(orderedSnapshot.Layers[1].Name == "Hidden");
    assert(orderedSnapshot.Layers[2].Name == "Output");

    });

    runner.Run("Topology editing and layer ordering", [&]()
    {

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(100, "Input", 0));
    assert(MiaIAClient::AddLayer(10, "Output", 1));

    assert(MiaIAClient::AddNeuron(100, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(10, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(1, 1001, 2001, 1.0));
    assert(!MiaIAClient::AddConnection(2, 2001, 1001, 1.0));

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(10, "Input", 0));
    assert(MiaIAClient::AddLayer(20, "Hidden", 1));
    assert(MiaIAClient::AddLayer(30, "Output", 2));

    assert(MiaIAClient::RemoveLayer(20));

    const auto compactedSnapshot = MiaIAClient::GetSnapshot();

    assert(compactedSnapshot.Layers.size() == 2);
    assert(compactedSnapshot.Layers[0].Order == 0);
    assert(compactedSnapshot.Layers[1].Order == 1);
    assert(compactedSnapshot.Layers[1].Name == "Output");

    assert(!MiaIAClient::Forward());

    assert(MiaIAClient::AddNeuron(10, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(30, 3001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(10, 1001, 3001, 1.0));

    assert(MiaIAClient::Forward());

    const auto rewiredSnapshot = MiaIAClient::GetSnapshot();

    const double rewiredExpected =
        MiaIA::Core::Activation::Sigmoid(1.0);

    assert(std::abs(
        rewiredSnapshot.Layers[1].Neurons[0].Activation -
        rewiredExpected
    ) < 0.000001);

    });

    runner.Run("Network validation", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(!MiaIAClient::Forward());

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(!MiaIAClient::AddConnection(1, 2001, 1001, 1.0));

    MiaIA::Core::Network invalidNetwork;

    assert(!MiaIA::Engine::NetworkValidator::ValidateForForward(
        invalidNetwork));

    MiaIA::Core::Network validNetwork;

    MiaIA::Core::Layer inputLayer;
    inputLayer.Id = 10;
    inputLayer.Name = "Input";
    inputLayer.Order = 0;
    inputLayer.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer outputLayer;
    outputLayer.Id = 20;
    outputLayer.Name = "Output";
    outputLayer.Order = 1;
    outputLayer.Neurons.push_back({ 2001, 0.0, 0.0 });

    validNetwork.Layers.push_back(inputLayer);
    validNetwork.Layers.push_back(outputLayer);

    validNetwork.Connections.push_back({
        1,
        1001,
        2001,
        1.0
        });

    assert(
        MiaIA::Engine::NetworkValidator::ValidateForForward(
            validNetwork));

    MiaIA::Core::Network invalidOrderNetwork;

    MiaIA::Core::Layer firstLayer;
    firstLayer.Id = 10;
    firstLayer.Name = "Input";
    firstLayer.Order = 0;
    firstLayer.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer secondLayer;
    secondLayer.Id = 20;
    secondLayer.Name = "Output";
    secondLayer.Order = 2;
    secondLayer.Neurons.push_back({ 2001, 0.0, 0.0 });

    invalidOrderNetwork.Layers.push_back(firstLayer);
    invalidOrderNetwork.Layers.push_back(secondLayer);

    invalidOrderNetwork.Connections.push_back({
        1,
        1001,
        2001,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            invalidOrderNetwork));

    MiaIA::Core::Network invalidConnectionNetwork;

    MiaIA::Core::Layer input;
    input.Id = 10;
    input.Name = "Input";
    input.Order = 0;
    input.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer output;
    output.Id = 20;
    output.Name = "Output";
    output.Order = 1;
    output.Neurons.push_back({ 2001, 0.0, 0.0 });

    invalidConnectionNetwork.Layers.push_back(input);
    invalidConnectionNetwork.Layers.push_back(output);

    invalidConnectionNetwork.Connections.push_back({
        1,
        1001,
        9999,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            invalidConnectionNetwork));


    MiaIA::Core::Network backwardConnectionNetwork;

    MiaIA::Core::Layer backwardInput;
    backwardInput.Id = 10;
    backwardInput.Name = "Input";
    backwardInput.Order = 0;
    backwardInput.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer backwardOutput;
    backwardOutput.Id = 20;
    backwardOutput.Name = "Output";
    backwardOutput.Order = 1;
    backwardOutput.Neurons.push_back({ 2001, 0.0, 0.0 });

    backwardConnectionNetwork.Layers.push_back(backwardInput);
    backwardConnectionNetwork.Layers.push_back(backwardOutput);

    backwardConnectionNetwork.Connections.push_back({
        1,
        2001,
        1001,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            backwardConnectionNetwork));


    MiaIA::Core::Network disconnectedNeuronNetwork;

    MiaIA::Core::Layer disconnectedInput;
    disconnectedInput.Id = 10;
    disconnectedInput.Name = "Input";
    disconnectedInput.Order = 0;
    disconnectedInput.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer disconnectedOutput;
    disconnectedOutput.Id = 20;
    disconnectedOutput.Name = "Output";
    disconnectedOutput.Order = 1;
    disconnectedOutput.Neurons.push_back({ 2001, 0.0, 0.0 });
    disconnectedOutput.Neurons.push_back({ 2002, 0.0, 0.0 });

    disconnectedNeuronNetwork.Layers.push_back(disconnectedInput);
    disconnectedNeuronNetwork.Layers.push_back(disconnectedOutput);

    disconnectedNeuronNetwork.Connections.push_back({
        1,
        1001,
        2001,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            disconnectedNeuronNetwork));


    MiaIA::Core::Network duplicateNeuronNetwork;

    MiaIA::Core::Layer duplicateInput;
    duplicateInput.Id = 10;
    duplicateInput.Name = "Input";
    duplicateInput.Order = 0;
    duplicateInput.Neurons.push_back({ 1001, 0.0, 1.0 });

    MiaIA::Core::Layer duplicateOutput;
    duplicateOutput.Id = 20;
    duplicateOutput.Name = "Output";
    duplicateOutput.Order = 1;
    duplicateOutput.Neurons.push_back({ 1001, 0.0, 0.0 });

    duplicateNeuronNetwork.Layers.push_back(duplicateInput);
    duplicateNeuronNetwork.Layers.push_back(duplicateOutput);

    duplicateNeuronNetwork.Connections.push_back({
        1,
        1001,
        1001,
        1.0
        });

    assert(
        !MiaIA::Engine::NetworkValidator::ValidateForForward(
            duplicateNeuronNetwork));

    });

    runner.Run("Dense network factory", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(2, 3, 2, 1));

    const auto denseSnapshot = MiaIAClient::GetSnapshot();

    assert(denseSnapshot.Layers.size() == 4);
    assert(denseSnapshot.Layers[0].Name == "Input");
    assert(denseSnapshot.Layers[1].Name == "Hidden");
    assert(denseSnapshot.Layers[2].Name == "Hidden");
    assert(denseSnapshot.Layers[3].Name == "Output");
    assert(denseSnapshot.Layers[0].Order == 0);
    assert(denseSnapshot.Layers[1].Order == 1);
    assert(denseSnapshot.Layers[2].Order == 2);
    assert(denseSnapshot.Layers[3].Order == 3);
    assert(denseSnapshot.Layers[0].Neurons.size() == 2);
    assert(denseSnapshot.Layers[1].Neurons.size() == 3);
    assert(denseSnapshot.Layers[2].Neurons.size() == 3);
    assert(denseSnapshot.Layers[3].Neurons.size() == 1);
    assert(denseSnapshot.Connections.size() == 18);

    assert(!MiaIAClient::CreateDenseNetwork(0, 3, 2, 1));
    assert(!MiaIAClient::CreateDenseNetwork(2, 0, 2, 1));
    assert(!MiaIAClient::CreateDenseNetwork(2, 3, -1, 1));
    assert(!MiaIAClient::CreateDenseNetwork(2, 3, 2, 0));

    const auto afterInvalidCreation = MiaIAClient::GetSnapshot();

    assert(afterInvalidCreation.Layers.size() == 4);
    assert(afterInvalidCreation.Connections.size() == 18);

    assert(MiaIAClient::CreateDenseNetwork(2, 1, 0, 1));

    const auto directSnapshot = MiaIAClient::GetSnapshot();

    assert(directSnapshot.Layers.size() == 2);
    assert(directSnapshot.Layers[0].Name == "Input");
    assert(directSnapshot.Layers[1].Name == "Output");
    assert(directSnapshot.Connections.size() == 2);

    });

    runner.Run("Connection weights", [&]()
    {
    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(
        1,
        1001,
        2001,
        0.5));

    assert(MiaIAClient::SetConnectionWeight(
        1,
        0.8));

    const auto weightSnapshot = MiaIAClient::GetSnapshot();

    assert(weightSnapshot.Connections[0].Weight == 0.8);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Output", 1));

    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 1.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));

    assert(MiaIAClient::AddConnection(
        1,
        1001,
        2001,
        0.5));

    assert(MiaIAClient::SetConnectionWeight(
        1,
        0.8));

    double readWeight = 0.0;

    assert(MiaIAClient::GetConnectionWeight(
        1,
        readWeight));

    assert(readWeight == 0.8);

    double missingWeight = 0.0;

    assert(!MiaIAClient::GetConnectionWeight(
        9999,
        missingWeight));

    const auto afterWeightChangeSnapshot = MiaIAClient::GetSnapshot();

    assert(afterWeightChangeSnapshot.Connections[0].Weight == 0.8);

    });

    runner.Run("ONNX export", [&]()
    {
    const std::filesystem::path exportPath =
        std::filesystem::temp_directory_path() /
        "miaia_export_test.onnx";
    const std::filesystem::path invalidExportPath =
        std::filesystem::temp_directory_path() /
        "miaia_invalid_export_test.onnx";

    std::filesystem::remove(exportPath);
    std::filesystem::remove(invalidExportPath);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::ReLU));
    assert(MiaIAClient::SetLayerActivation(
        2,
        MiaIA::Core::ActivationType::Tanh));

    assert(MiaIAClient::SetNeuronBias(1003, 0.3));
    assert(MiaIAClient::SetNeuronBias(1004, -0.2));
    assert(MiaIAClient::SetNeuronBias(1005, 0.7));

    assert(MiaIAClient::SetConnectionWeight(1, 0.1));
    assert(MiaIAClient::SetConnectionWeight(2, 0.2));
    assert(MiaIAClient::SetConnectionWeight(3, 0.3));
    assert(MiaIAClient::SetConnectionWeight(4, 0.4));
    assert(MiaIAClient::SetConnectionWeight(5, 0.5));
    assert(MiaIAClient::SetConnectionWeight(6, 0.6));

    assert(MiaIAClient::ExportOnnx(exportPath.string()));
    assert(std::filesystem::exists(exportPath));
    assert(std::filesystem::file_size(exportPath) > 0);

    onnx::ModelProto model;
    std::ifstream input(exportPath, std::ios::binary);

    assert(input.good());
    assert(model.ParseFromIstream(&input));

    onnx::checker::check_model(model);

    assert(model.ir_version() == 8);
    assert(model.producer_name() == "MiaIA");
    assert(model.opset_import_size() == 1);
    assert(model.opset_import(0).version() == 18);

    const onnx::GraphProto& graph = model.graph();

    assert(graph.input_size() == 1);
    assert(graph.output_size() == 1);
    assert(graph.node_size() == 4);
    assert(graph.initializer_size() == 4);
    assert(graph.node(0).op_type() == "Gemm");
    assert(graph.node(1).op_type() == "Relu");
    assert(graph.node(2).op_type() == "Gemm");
    assert(graph.node(3).op_type() == "Tanh");

    const onnx::TensorProto* hiddenWeights =
        FindInitializer(graph, "layer_1_weights");
    const onnx::TensorProto* hiddenBiases =
        FindInitializer(graph, "layer_1_biases");
    const onnx::TensorProto* outputWeights =
        FindInitializer(graph, "layer_2_weights");
    const onnx::TensorProto* outputBiases =
        FindInitializer(graph, "layer_2_biases");

    assert(hiddenWeights != nullptr);
    assert(hiddenBiases != nullptr);
    assert(outputWeights != nullptr);
    assert(outputBiases != nullptr);

    assert(hiddenWeights->dims_size() == 2);
    assert(hiddenWeights->dims(0) == 2);
    assert(hiddenWeights->dims(1) == 2);
    assert(hiddenWeights->double_data_size() == 4);
    assert(hiddenWeights->double_data(0) == 0.1);
    assert(hiddenWeights->double_data(1) == 0.2);
    assert(hiddenWeights->double_data(2) == 0.3);
    assert(hiddenWeights->double_data(3) == 0.4);

    assert(hiddenBiases->double_data_size() == 2);
    assert(hiddenBiases->double_data(0) == 0.3);
    assert(hiddenBiases->double_data(1) == -0.2);
    assert(outputWeights->double_data_size() == 2);
    assert(outputWeights->double_data(0) == 0.5);
    assert(outputWeights->double_data(1) == 0.6);
    assert(outputBiases->double_data_size() == 1);
    assert(outputBiases->double_data(0) == 0.7);

    assert(model.metadata_props_size() == 10);

    input.close();

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::AddLayer(0, "Input", 0));
    assert(MiaIAClient::AddLayer(1, "Hidden", 1));
    assert(MiaIAClient::AddLayer(2, "Output", 2));
    assert(MiaIAClient::AddNeuron(0, 1001, 0.0, 0.0));
    assert(MiaIAClient::AddNeuron(1, 2001, 0.0, 0.0));
    assert(MiaIAClient::AddNeuron(2, 3001, 0.0, 0.0));
    assert(MiaIAClient::AddConnection(1, 1001, 2001, 0.5));
    assert(MiaIAClient::AddConnection(2, 2001, 3001, 0.5));
    assert(MiaIAClient::AddConnection(3, 1001, 3001, 0.5));

    assert(!MiaIAClient::ExportOnnx(invalidExportPath.string()));
    assert(!std::filesystem::exists(invalidExportPath));
    assert(!MiaIAClient::ExportOnnx(""));

    std::filesystem::remove(exportPath);
    std::filesystem::remove(invalidExportPath);

    });

    runner.Run("ONNX import and round trip", [&]()
    {
    const std::filesystem::path roundTripPath =
        std::filesystem::temp_directory_path() /
        "miaia_round_trip_test.onnx";
    const std::filesystem::path floatPath =
        std::filesystem::temp_directory_path() /
        "miaia_float_import_test.onnx";
    const std::filesystem::path unsupportedPath =
        std::filesystem::temp_directory_path() /
        "miaia_unsupported_import_test.onnx";

    std::filesystem::remove(roundTripPath);
    std::filesystem::remove(floatPath);
    std::filesystem::remove(unsupportedPath);

    MiaIAClient::ClearNetwork();

    assert(MiaIAClient::CreateDenseNetwork(2, 2, 1, 1));
    assert(MiaIAClient::SetLayerActivation(
        1,
        MiaIA::Core::ActivationType::ReLU));
    assert(MiaIAClient::SetLayerActivation(
        2,
        MiaIA::Core::ActivationType::Tanh));
    assert(MiaIAClient::SetNeuronBias(1003, 0.3));
    assert(MiaIAClient::SetNeuronBias(1004, -0.2));
    assert(MiaIAClient::SetNeuronBias(1005, 0.7));
    assert(MiaIAClient::SetConnectionWeight(1, 0.1));
    assert(MiaIAClient::SetConnectionWeight(2, 0.2));
    assert(MiaIAClient::SetConnectionWeight(3, 0.3));
    assert(MiaIAClient::SetConnectionWeight(4, 0.4));
    assert(MiaIAClient::SetConnectionWeight(5, 0.5));
    assert(MiaIAClient::SetConnectionWeight(6, 0.6));

    const auto original = MiaIAClient::GetSnapshot();

    assert(MiaIAClient::SetInputValues({ 0.25, -0.5 }));
    assert(MiaIAClient::Forward());

    const auto originalForward = MiaIAClient::GetSnapshot();
    const double expectedOutput =
        originalForward.Layers.back().Neurons.front().Activation;

    assert(MiaIAClient::ExportOnnx(roundTripPath.string()));

    MiaIAClient::ClearNetwork();
    assert(MiaIAClient::ImportOnnx(roundTripPath.string()));

    const auto imported = MiaIAClient::GetSnapshot();

    assert(imported.Layers.size() == original.Layers.size());
    assert(imported.Connections.size() == original.Connections.size());

    for (std::size_t layerIndex = 0;
        layerIndex < original.Layers.size();
        ++layerIndex)
    {
        const auto& expectedLayer = original.Layers[layerIndex];
        const auto& actualLayer = imported.Layers[layerIndex];

        assert(actualLayer.Id == expectedLayer.Id);
        assert(actualLayer.Name == expectedLayer.Name);
        assert(actualLayer.Order == expectedLayer.Order);
        assert(actualLayer.Activation == expectedLayer.Activation);
        assert(actualLayer.Neurons.size() ==
            expectedLayer.Neurons.size());

        for (std::size_t neuronIndex = 0;
            neuronIndex < expectedLayer.Neurons.size();
            ++neuronIndex)
        {
            assert(actualLayer.Neurons[neuronIndex].Id ==
                expectedLayer.Neurons[neuronIndex].Id);
            assert(actualLayer.Neurons[neuronIndex].Bias ==
                expectedLayer.Neurons[neuronIndex].Bias);
        }
    }

    for (std::size_t connectionIndex = 0;
        connectionIndex < original.Connections.size();
        ++connectionIndex)
    {
        const auto& expectedConnection =
            original.Connections[connectionIndex];
        const auto& actualConnection =
            imported.Connections[connectionIndex];

        assert(actualConnection.Id == expectedConnection.Id);
        assert(actualConnection.FromNeuron ==
            expectedConnection.FromNeuron);
        assert(actualConnection.ToNeuron ==
            expectedConnection.ToNeuron);
        assert(actualConnection.Weight == expectedConnection.Weight);
    }

    assert(MiaIAClient::SetInputValues({ 0.25, -0.5 }));
    assert(MiaIAClient::Forward());

    const auto importedForward = MiaIAClient::GetSnapshot();
    const double actualOutput =
        importedForward.Layers.back().Neurons.front().Activation;

    assert(std::abs(actualOutput - expectedOutput) < 1e-12);

    onnx::ModelProto floatModel;
    floatModel.set_ir_version(8);

    auto* floatOpset = floatModel.add_opset_import();
    floatOpset->set_domain("");
    floatOpset->set_version(18);

    auto* floatGraph = floatModel.mutable_graph();
    floatGraph->set_name("Float dense model");

    auto setFloatValueInfo = [](
        onnx::ValueInfoProto& value,
        const std::string& name,
        std::int64_t features)
    {
        value.set_name(name);
        auto* tensorType = value.mutable_type()->mutable_tensor_type();
        tensorType->set_elem_type(onnx::TensorProto_DataType_FLOAT);
        auto* shape = tensorType->mutable_shape();
        shape->add_dim()->set_dim_param("batch");
        shape->add_dim()->set_dim_value(features);
    };

    setFloatValueInfo(*floatGraph->add_input(), "input", 2);
    setFloatValueInfo(*floatGraph->add_output(), "output", 1);

    auto* floatWeights = floatGraph->add_initializer();
    floatWeights->set_name("weights");
    floatWeights->set_data_type(onnx::TensorProto_DataType_FLOAT);
    floatWeights->add_dims(2);
    floatWeights->add_dims(1);
    floatWeights->add_float_data(0.25f);
    floatWeights->add_float_data(-0.5f);

    auto* floatBiases = floatGraph->add_initializer();
    floatBiases->set_name("biases");
    floatBiases->set_data_type(onnx::TensorProto_DataType_FLOAT);
    floatBiases->add_dims(1);
    floatBiases->add_float_data(0.1f);

    auto* floatGemm = floatGraph->add_node();
    floatGemm->set_name("dense");
    floatGemm->set_op_type("Gemm");
    floatGemm->add_input("input");
    floatGemm->add_input("weights");
    floatGemm->add_input("biases");
    floatGemm->add_output("linear");

    auto* floatRelu = floatGraph->add_node();
    floatRelu->set_name("activation");
    floatRelu->set_op_type("Relu");
    floatRelu->add_input("linear");
    floatRelu->add_output("output");

    onnx::checker::check_model(floatModel);

    {
        std::ofstream output(floatPath, std::ios::binary);
        assert(output.good());
        assert(floatModel.SerializeToOstream(&output));
    }

    assert(MiaIAClient::ImportOnnx(floatPath.string()));

    const auto floatSnapshot = MiaIAClient::GetSnapshot();

    assert(floatSnapshot.Layers.size() == 2);
    assert(floatSnapshot.Layers[0].Name == "Input");
    assert(floatSnapshot.Layers[1].Name == "Output");
    assert(floatSnapshot.Layers[0].Neurons.size() == 2);
    assert(floatSnapshot.Layers[1].Neurons.size() == 1);
    assert(floatSnapshot.Layers[1].Activation ==
        MiaIA::Core::ActivationType::ReLU);
    assert(std::abs(
        floatSnapshot.Layers[1].Neurons[0].Bias - 0.1) < 1e-6);
    assert(floatSnapshot.Connections.size() == 2);
    assert(std::abs(
        floatSnapshot.Connections[0].Weight - 0.25) < 1e-6);
    assert(std::abs(
        floatSnapshot.Connections[1].Weight + 0.5) < 1e-6);

    assert(MiaIAClient::SetInputValues({ 2.0, 1.0 }));
    assert(MiaIAClient::Forward());

    const auto floatForward = MiaIAClient::GetSnapshot();

    assert(std::abs(
        floatForward.Layers[1].Neurons[0].Activation - 0.1) < 1e-6);

    const auto beforeFailedImport = MiaIAClient::GetSnapshot();

    onnx::ModelProto unsupportedModel;
    unsupportedModel.set_ir_version(8);

    auto* unsupportedOpset = unsupportedModel.add_opset_import();
    unsupportedOpset->set_domain("");
    unsupportedOpset->set_version(18);

    auto* unsupportedGraph = unsupportedModel.mutable_graph();
    unsupportedGraph->set_name("Unsupported model");
    setFloatValueInfo(*unsupportedGraph->add_input(), "input", 2);
    setFloatValueInfo(*unsupportedGraph->add_output(), "output", 2);

    auto* identity = unsupportedGraph->add_node();
    identity->set_name("identity");
    identity->set_op_type("Identity");
    identity->add_input("input");
    identity->add_output("output");

    onnx::checker::check_model(unsupportedModel);

    {
        std::ofstream output(unsupportedPath, std::ios::binary);
        assert(output.good());
        assert(unsupportedModel.SerializeToOstream(&output));
    }

    assert(!MiaIAClient::ImportOnnx(unsupportedPath.string()));
    assert(!MiaIAClient::ImportOnnx(""));

    const auto afterFailedImport = MiaIAClient::GetSnapshot();

    assert(afterFailedImport.Layers.size() ==
        beforeFailedImport.Layers.size());
    assert(afterFailedImport.Connections.size() ==
        beforeFailedImport.Connections.size());
    assert(afterFailedImport.Layers[0].Neurons[0].Id ==
        beforeFailedImport.Layers[0].Neurons[0].Id);
    assert(afterFailedImport.Connections[0].Weight ==
        beforeFailedImport.Connections[0].Weight);

    std::filesystem::remove(roundTripPath);
    std::filesystem::remove(floatPath);
    std::filesystem::remove(unsupportedPath);

    });

    return runner.Finish();
}
