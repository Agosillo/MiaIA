#include "NetworkRuntime.h"

#include "../Execution/ForwardEngine.h"
#include "../Validation/NetworkValidator.h"

namespace MiaIA::Engine
{
    bool NetworkRuntime::Forward(Core::Network& network)
    {
        if (!NetworkValidator::ValidateForForward(network))
        {
            return false;
        }

        return ForwardEngine::Run(network);
    }

    bool NetworkRuntime::Validate(const Core::Network& network)
    {
        return NetworkValidator::ValidateForForward(network);
    }
}
