#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Data/CsvDatasetImporter.h"
#include "../../Engine/Data/DatasetInspector.h"
#include "../../Engine/Data/DatasetRuntime.h"
#include "../../Engine/Evaluation/SampleEvaluator.h"
#include "../../Engine/Evaluation/DatasetEvaluator.h"
#include "../../Engine/Differentiation/SampleGradientEvaluator.h"
#include "../../Engine/Analysis/SignalHealthAnalyzer.h"
#include "../../Core/Model/Dataset.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::ImportCsvDataset(
        const std::string& path,
        std::size_t inputCount,
        std::size_t targetCount,
        bool hasHeader)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::CsvDatasetImporter::Import(
            Detail::ClientDataset(),
            path,
            inputCount,
            targetCount,
            hasHeader);
    }

    bool MiaIAClient::ClearDataset()
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        Detail::ClientDataset() = Core::Dataset{};
        return true;
    }

    Core::DatasetSummary MiaIAClient::GetDatasetSummary()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::DatasetInspector::Summary(
            Detail::ClientDataset());
    }

    bool MiaIAClient::TryGetDatasetSample(
        std::size_t index,
        Core::SampleSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::DatasetInspector::TryGetSample(
            Detail::ClientDataset(),
            index,
            result);
    }

    bool MiaIAClient::ApplyDatasetSample(std::size_t index)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

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
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

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
        const std::scoped_lock lock(Detail::ClientMutex());
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
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Engine::SampleGradientEvaluator::Evaluate(
            Detail::ClientDataset(),
            index,
            Detail::ClientNetwork(),
            type,
            result);
    }

    bool MiaIAClient::DiagnoseDataset(
        Core::LossType type,
        const Core::SignalHealthConfiguration& configuration,
        Core::SignalHealthSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::SignalHealthAnalyzer::Analyze(
            Detail::ClientDataset(),
            Detail::ClientNetwork(),
            type,
            configuration,
            result);
    }
}
