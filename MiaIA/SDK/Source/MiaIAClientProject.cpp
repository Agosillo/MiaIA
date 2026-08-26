#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "ProjectState.h"

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

        Detail::ClientProjectState().Reset();
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

        Detail::ProjectState project;
        Detail::ModelInstance& model = project.ActiveModel();
        model.Network = std::move(network);
        model.Dataset = std::move(dataset);
        model.TrainingSession = std::move(trainingSession);
        project.Info = std::move(info);
        Detail::ClientProjectState() = std::move(project);
        return true;
    }

    bool MiaIAClient::SaveProject(const std::string& path)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked() ||
            Detail::ClientProjectState().ModelCount() != 1)
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
