#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "ProjectState.h"

#include <mutex>

namespace MiaIA::SDK
{
    bool MiaIAClient::CreateModelContext(
        const std::string& name,
        Core::ModelContextSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Detail::ClientProjectState().CreateContext(name, result);
    }

    std::vector<Core::ModelContextSnapshot>
    MiaIAClient::GetModelContexts()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientProjectState().ContextSnapshots();
    }

    Core::ModelContextSnapshot MiaIAClient::GetActiveModelContext()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientProjectState().ActiveContextSnapshot();
    }

    bool MiaIAClient::SelectModelContext(std::uint64_t contextId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Detail::ClientProjectState().SelectContext(contextId);
    }

    bool MiaIAClient::RenameModelContext(
        std::uint64_t contextId,
        const std::string& name)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientProjectState().RenameContext(contextId, name);
    }

    bool MiaIAClient::RemoveModelContext(std::uint64_t contextId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Detail::ClientProjectState().RemoveContext(contextId);
    }
}
