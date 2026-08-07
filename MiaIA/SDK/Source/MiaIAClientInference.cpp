#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Inference/PredictionEvaluator.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::Predict(
        const std::vector<double>& inputs,
        Core::PredictionSnapshot& result)
    {
        return Engine::PredictionEvaluator::Predict(
            Detail::ClientNetwork(),
            inputs,
            result);
    }
}
