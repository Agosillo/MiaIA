#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Project/ProjectArchive.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingDebugSession.h"
#include "../../Core/Model/TrainingSession.h"

#include <utility>

namespace MiaIA::SDK
{
    bool MiaIAClient::NewProject()
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        Detail::ClientNetwork() = Core::Network{};
        Detail::ClientDataset() = Core::Dataset{};
        Detail::ClientTrainingSession() = Core::TrainingSession{};
        Detail::ClientTrainingDebugSession() =
            Core::TrainingDebugSession{};
        Detail::ClientProjectInfo() = Core::ProjectInfoSnapshot{};
        return true;
    }

    bool MiaIAClient::OpenProject(const std::string& path)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        Core::Network network;
        Core::Dataset dataset;
        Core::TrainingSession trainingSession;
        Core::ProjectInfoSnapshot info;

        if (!Engine::ProjectArchive::Load(
                path,
                network,
                dataset,
                trainingSession,
                info))
        {
            return false;
        }

        Detail::ClientNetwork() = std::move(network);
        Detail::ClientDataset() = std::move(dataset);
        Detail::ClientTrainingSession() = std::move(trainingSession);
        Detail::ClientTrainingDebugSession() =
            Core::TrainingDebugSession{};
        Detail::ClientProjectInfo() = std::move(info);
        return true;
    }

    bool MiaIAClient::SaveProject(const std::string& path)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        Core::ProjectInfoSnapshot info;

        if (!Engine::ProjectArchive::Save(
                Detail::ClientNetwork(),
                Detail::ClientDataset(),
                Detail::ClientTrainingSession(),
                path,
                info))
        {
            return false;
        }

        Detail::ClientProjectInfo() = std::move(info);
        return true;
    }

    Core::ProjectInfoSnapshot MiaIAClient::GetProjectInfo()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientProjectInfo();
    }
}
