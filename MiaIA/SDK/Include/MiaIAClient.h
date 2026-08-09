#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../../Core/Public/NetworkSnapshot.h"
#include "../../Core/Public/DatasetSummary.h"
#include "../../Core/Public/DatasetEvaluationSnapshot.h"
#include "../../Core/Public/LossType.h"
#include "../../Core/Public/OptimizerType.h"
#include "../../Core/Public/PredictionSnapshot.h"
#include "../../Core/Public/SampleEvaluationSnapshot.h"
#include "../../Core/Public/SampleGradientSnapshot.h"
#include "../../Core/Public/SampleSnapshot.h"
#include "../../Core/Public/TrainingStepSnapshot.h"
#include "../../Core/Public/TrainingEpochSnapshot.h"
#include "../../Core/Public/TrainingSessionSnapshot.h"
#include "../../Core/Public/TrainingRunSnapshot.h"
#include "../../Core/Public/TrainingHistoryEntrySnapshot.h"
#include "../../Core/Public/TrainingDebugSnapshot.h"
#include "../../Core/Public/TrainingDebugNeuronSnapshot.h"
#include "../../Core/Public/TrainingDebugConnectionSnapshot.h"

namespace MiaIA::SDK
{
    class MiaIAClient
    {
    public:
        [[nodiscard]]
        static Core::NetworkSnapshot GetSnapshot();
        [[nodiscard]]
        static Core::NetworkOverviewSnapshot GetNetworkOverview();
        static bool ClearNetwork();
        static bool AddLayer(std::uint64_t id, const std::string& name, std::uint64_t order);
        static bool AddNeuron(std::uint64_t layerId, std::uint64_t neuronId, double bias, double activation);
        static bool AddConnection(std::uint64_t id, std::uint64_t fromNeuron, std::uint64_t toNeuron, double weight);
        static bool SetNeuronActivation(std::uint64_t neuronId, double activation);
        static bool SetInputValues(const std::vector<double>& values);
        static bool SetNeuronBias(std::uint64_t neuronId, double bias);
        static bool SetConnectionWeight(std::uint64_t connectionId, double weight);
        static bool TryGetNeuron(std::uint64_t neuronId, Core::NeuronSnapshot& result);
        static bool TryGetConnection(std::uint64_t connectionId, Core::ConnectionSnapshot& result);
        static bool TryGetLayer(std::uint64_t layerId, Core::LayerSnapshot& result);
        static bool RemoveConnection(std::uint64_t connectionId);
        static bool RemoveNeuron(std::uint64_t neuronId);
        static bool RemoveLayer(std::uint64_t layerId);
        static bool SetLayerActivation(std::uint64_t layerId, Core::ActivationType activation);
        static bool GetConnectionWeight(std::uint64_t connectionId, double& weight);
        static bool CreateDenseNetwork(int inputCount, int hiddenCount, int hiddenLayers, int outputCount);
        static bool ImportOnnx(const std::string& path);
        static bool ExportOnnx(const std::string& path);
        static bool ImportCsvDataset(
            const std::string& path,
            std::size_t inputCount,
            std::size_t targetCount,
            bool hasHeader = true);
        static bool ClearDataset();
        [[nodiscard]]
        static Core::DatasetSummary GetDatasetSummary();
        static bool TryGetDatasetSample(
            std::size_t index,
            Core::SampleSnapshot& result);
        static bool ApplyDatasetSample(std::size_t index);
        static bool EvaluateDatasetSample(
            std::size_t index,
            Core::LossType type,
            Core::SampleEvaluationSnapshot& result);
        static bool EvaluateDataset(
            Core::LossType type,
            Core::DatasetEvaluationSnapshot& result);
        static bool EvaluateDatasetSampleGradients(
            std::size_t index,
            Core::LossType type,
            Core::SampleGradientSnapshot& result);
        static bool TrainDatasetSample(
            std::size_t index,
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingStepSnapshot& result);
        static bool TrainDatasetEpoch(
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingEpochSnapshot& result);
        static bool StartTrainingSession(
            std::size_t epochCount,
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingSessionSnapshot& result);
        [[nodiscard]]
        static Core::TrainingSessionSnapshot GetTrainingSession();
        [[nodiscard]]
        static std::vector<Core::TrainingHistoryEntrySnapshot>
            GetTrainingSessionHistory();
        static bool TryGetTrainingSessionStep(
            std::size_t stepIndex,
            Core::TrainingStepSnapshot& result);
        static bool AdvanceTrainingSession(
            Core::TrainingStepSnapshot& result);
        static bool RunTrainingSession(
            std::size_t maximumSteps,
            Core::TrainingRunSnapshot& result);
        static bool ResumeTrainingSession();
        static bool PauseTrainingSession();
        static bool CancelTrainingSession();
        static bool StartTrainingDebug(
            std::size_t sampleIndex,
            double learningRate,
            Core::LossType lossType,
            Core::OptimizerType optimizerType,
            Core::TrainingDebugSnapshot& result);
        static bool StartTrainingSessionDebug(
            Core::TrainingDebugSnapshot& result);
        [[nodiscard]]
        static Core::TrainingDebugSnapshot GetTrainingDebug();
        static bool AdvanceTrainingDebug(
            Core::TrainingDebugSnapshot& result);
        static bool CancelTrainingDebug();
        static bool TryGetTrainingDebugNeuron(
            std::uint64_t neuronId,
            Core::TrainingDebugNeuronSnapshot& result);
        static bool TryGetTrainingDebugConnection(
            std::uint64_t connectionId,
            Core::TrainingDebugConnectionSnapshot& result);
        static bool Predict(
            const std::vector<double>& inputs,
            Core::PredictionSnapshot& result);
        static bool Forward();
    };
}
