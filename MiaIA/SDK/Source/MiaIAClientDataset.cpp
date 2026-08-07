#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Data/CsvDatasetImporter.h"
#include "../../Engine/Data/DatasetInspector.h"
#include "../../Engine/Data/DatasetRuntime.h"
#include "../../Engine/Evaluation/SampleEvaluator.h"
#include "../../Engine/Evaluation/DatasetEvaluator.h"
#include "../../Engine/Differentiation/SampleGradientEvaluator.h"
#include "../../Core/Model/Dataset.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::ImportCsvDataset(
        const std::string& path,
        std::size_t inputCount,
        std::size_t targetCount,
        bool hasHeader)
    {
        return Engine::CsvDatasetImporter::Import(
            Detail::ClientDataset(),
            path,
            inputCount,
            targetCount,
            hasHeader);
    }

    void MiaIAClient::ClearDataset()
    {
        Detail::ClientDataset() = Core::Dataset{};
    }

    Core::DatasetSummary MiaIAClient::GetDatasetSummary()
    {
        return Engine::DatasetInspector::Summary(
            Detail::ClientDataset());
    }

    bool MiaIAClient::TryGetDatasetSample(
        std::size_t index,
        Core::SampleSnapshot& result)
    {
        return Engine::DatasetInspector::TryGetSample(
            Detail::ClientDataset(),
            index,
            result);
    }

    bool MiaIAClient::ApplyDatasetSample(std::size_t index)
    {
        return Engine::DatasetRuntime::ApplySample(
            Detail::ClientDataset(),
            index,
            Detail::ClientNetwork());
    }

    bool MiaIAClient::EvaluateDatasetSample(
        std::size_t index,
        Core::LossType type,
        Core::SampleEvaluationSnapshot& result)
    {
        return Engine::SampleEvaluator::Evaluate(
            Detail::ClientDataset(),
            index,
            Detail::ClientNetwork(),
            type,
            result);
    }

    bool MiaIAClient::EvaluateDataset(
        Core::LossType type,
        Core::DatasetEvaluationSnapshot& result)
    {
        return Engine::DatasetEvaluator::Evaluate(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            type,
            result);
    }

    bool MiaIAClient::EvaluateDatasetSampleGradients(
        std::size_t index,
        Core::LossType type,
        Core::SampleGradientSnapshot& result)
    {
        return Engine::SampleGradientEvaluator::Evaluate(
            Detail::ClientDataset(),
            index,
            Detail::ClientNetwork(),
            type,
            result);
    }
}
