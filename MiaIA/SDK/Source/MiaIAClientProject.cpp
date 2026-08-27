#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "ProjectState.h"

#include "../../Engine/Project/ProjectArchive.h"
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

        Engine::ProjectArchiveState archive;
        Core::ProjectInfoSnapshot info;

        if (!Engine::ProjectArchive::Load(
                path,
                archive,
                info))
        {
            return false;
        }

        Detail::ProjectState project;

        if (!project.ReplaceArchiveState(std::move(archive)))
        {
            return false;
        }

        project.Info = std::move(info);
        Detail::ClientProjectState() = std::move(project);
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
                Detail::ClientProjectState().BuildArchiveView(),
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
        return Detail::ClientProjectState().InfoSnapshot();
    }
}
