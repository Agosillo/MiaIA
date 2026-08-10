#pragma once

#include "../../Core/Public/ProjectInfoSnapshot.h"

#include <cstdint>
#include <string>

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
    struct TrainingSession;
}

namespace MiaIA::Engine
{
    class ProjectArchive
    {
    public:
        static constexpr std::uint32_t CurrentFormatVersion =
            Core::ProjectFormatVersion;

        static bool Save(
            const Core::Network& network,
            const Core::Dataset& dataset,
            const Core::TrainingSession& trainingSession,
            const std::string& path,
            Core::ProjectInfoSnapshot& result);

        static bool Load(
            const std::string& path,
            Core::Network& network,
            Core::Dataset& dataset,
            Core::TrainingSession& trainingSession,
            Core::ProjectInfoSnapshot& result);
    };
}
