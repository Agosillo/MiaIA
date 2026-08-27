#pragma once

#include "../Checkpoint/ModelCheckpointStore.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingSession.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::Engine
{
    struct ProjectArchiveContextView
    {
        std::uint64_t Id{};
        const std::string* Name{};
        const Core::Network* Network{};
        const Core::Dataset* Dataset{};
        const Core::TrainingSession* TrainingSession{};
        const ModelCheckpointStore* Checkpoints{};
    };

    struct ProjectArchiveView
    {
        std::vector<ProjectArchiveContextView> Contexts;
        std::uint64_t ActiveContextId{};
        std::uint64_t NextContextId{};
    };

    struct ProjectArchiveContextState
    {
        std::uint64_t Id{};
        std::string Name;
        Core::Network Network;
        Core::Dataset Dataset;
        Core::TrainingSession TrainingSession;
        ModelCheckpointStore Checkpoints;
    };

    struct ProjectArchiveState
    {
        std::vector<ProjectArchiveContextState> Contexts;
        std::uint64_t ActiveContextId{};
        std::uint64_t NextContextId{};
    };
}
