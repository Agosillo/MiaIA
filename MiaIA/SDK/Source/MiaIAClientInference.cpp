#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Inference/PredictionEvaluator.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::Predict(
        const std::vector<double>& inputs,
        Core::PredictionSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsTrainingSessionRunning())
        {
            return false;
        }

        return Engine::PredictionEvaluator::Predict(
            Detail::ClientNetwork(),
            inputs,
            result);
    }
}
