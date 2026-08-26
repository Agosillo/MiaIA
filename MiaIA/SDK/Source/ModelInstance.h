#pragma once

#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingDebugSession.h"
#include "../../Core/Model/TrainingSession.h"
#include "../../Engine/Checkpoint/ModelCheckpointStore.h"

#include <cstdint>
#include <string>

namespace MiaIA::SDK::Detail
{
    struct ModelInstance final
    {
        std::uint64_t Id{};
        std::string Name;
        Core::Network Network;
        Core::Dataset Dataset;
        Core::TrainingSession TrainingSession;
        Core::TrainingDebugSession TrainingDebugSession;
        Engine::ModelCheckpointStore Checkpoints;
    };
}
