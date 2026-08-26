#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "ProjectState.h"

#include <mutex>

namespace MiaIA::SDK
{
    bool MiaIAClient::CreateModelInstance(
        const std::string& name,
        Core::ModelInstanceSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Detail::ClientProjectState().CreateModel(name, result);
    }

    std::vector<Core::ModelInstanceSnapshot>
    MiaIAClient::GetModelInstances()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientProjectState().ModelSnapshots();
    }

    Core::ModelInstanceSnapshot MiaIAClient::GetActiveModelInstance()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientProjectState().ActiveModelSnapshot();
    }

    bool MiaIAClient::SelectModelInstance(std::uint64_t modelId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Detail::ClientProjectState().SelectModel(modelId);
    }

    bool MiaIAClient::RenameModelInstance(
        std::uint64_t modelId,
        const std::string& name)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientProjectState().RenameModel(modelId, name);
    }

    bool MiaIAClient::RemoveModelInstance(std::uint64_t modelId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Detail::ClientProjectState().RemoveModel(modelId);
    }
}
