#include "DatasetInspector.h"

#include "../../Core/Model/Dataset.h"

namespace MiaIA::Engine
{
    Core::DatasetSummary DatasetInspector::Summary(
        const Core::Dataset& dataset)
    {
        return Core::DatasetSummary{
            dataset.Name,
            dataset.Source,
            dataset.Samples.size(),
            dataset.InputCount,
            dataset.TargetCount
        };
    }

    bool DatasetInspector::TryGetSample(
        const Core::Dataset& dataset,
        std::size_t index,
        Core::SampleSnapshot& result)
    {
        if (index >= dataset.Samples.size())
        {
            return false;
        }

        const Core::Sample& sample = dataset.Samples[index];

        result = Core::SampleSnapshot{
            index,
            sample.Inputs,
            sample.Targets
        };

        return true;
    }
}
