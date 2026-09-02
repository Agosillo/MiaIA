#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "ProjectState.h"

#include "../../Engine/Analysis/ModelComparator.h"

#include <mutex>
#include <utility>

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

    bool MiaIAClient::TryCompareModelContexts(
        std::uint64_t referenceContextId,
        std::uint64_t currentContextId,
        Core::ModelContextComparisonSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        if (referenceContextId == currentContextId)
        {
            return false;
        }

        const Detail::ProjectState& project =
            Detail::ClientProjectState();
        const Detail::ModelContext* reference =
            project.FindContext(referenceContextId);
        const Detail::ModelContext* current =
            project.FindContext(currentContextId);
        if (reference == nullptr || current == nullptr)
        {
            return false;
        }

        Core::ModelComparisonSnapshot comparison;
        if (!Engine::ModelComparator::Compare(
            reference->Network,
            current->Network,
            comparison))
        {
            return false;
        }

        Core::ModelContextComparisonSnapshot candidate;
        candidate.ReferenceContextId = reference->Id;
        candidate.CurrentContextId = current->Id;
        candidate.ReferenceContextName = reference->Name;
        candidate.CurrentContextName = current->Name;
        candidate.Model = std::move(comparison);
        result = std::move(candidate);
        return true;
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
