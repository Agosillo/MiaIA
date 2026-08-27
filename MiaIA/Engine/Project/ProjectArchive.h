#pragma once

#include "ProjectArchiveState.h"
#include "../../Core/Public/ProjectInfoSnapshot.h"

#include <cstdint>
#include <string>

namespace MiaIA::Engine
{
    class ProjectArchive
    {
    public:
        static constexpr std::uint32_t CurrentFormatVersion =
            Core::ProjectFormatVersion;

        static bool Save(
            const ProjectArchiveView& project,
            const std::string& path,
            Core::ProjectInfoSnapshot& result);

        static bool SaveVersion1(
            const Core::Network& network,
            const Core::Dataset& dataset,
            const Core::TrainingSession& trainingSession,
            const std::string& path,
            Core::ProjectInfoSnapshot& result);

        static bool Load(
            const std::string& path,
            ProjectArchiveState& project,
            Core::ProjectInfoSnapshot& result);
    };
}
