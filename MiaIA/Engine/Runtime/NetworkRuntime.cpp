#include "NetworkRuntime.h"

#include "../Execution/ForwardEngine.h"
#include "../Validation/NetworkValidator.h"
#include "../../Core/Execution/SnapshotBuilder.h"

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

    Core::NetworkSnapshot NetworkRuntime::Snapshot(
        const Core::Network& network)
    {
        return Core::SnapshotBuilder::Build(network);
    }
}