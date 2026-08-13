#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"

#include "../../Engine/Checkpoint/ModelCheckpointStore.h"
#include "../../Core/Model/Network.h"

#include <mutex>
#include <utility>

namespace MiaIA::SDK
{
    bool MiaIAClient::CaptureModelCheckpoint(
        const std::string& name,
        Core::ModelCheckpointSummarySnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Detail::ClientCheckpointStore().Capture(
            Detail::ClientNetwork(),
            name,
            result);
    }

    std::vector<Core::ModelCheckpointSummarySnapshot>
    MiaIAClient::GetModelCheckpoints()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientCheckpointStore().List();
    }

    bool MiaIAClient::TryGetModelCheckpoint(
        std::uint64_t checkpointId,
        Core::ModelCheckpointSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientCheckpointStore().TryGet(checkpointId, result);
    }

    bool MiaIAClient::TryCompareModelCheckpoints(
        std::uint64_t firstCheckpointId,
        std::uint64_t secondCheckpointId,
        Core::ModelCheckpointComparisonSnapshot& result)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Detail::ClientCheckpointStore().Compare(
            firstCheckpointId,
            secondCheckpointId,
            result);
    }

    bool MiaIAClient::RestoreModelCheckpoint(std::uint64_t checkpointId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        Core::Network candidate;
        if (!Detail::ClientCheckpointStore().TryRestore(
            checkpointId,
            candidate))
        {
            return false;
        }

        Detail::ClientNetwork() = std::move(candidate);
        return true;
    }

    bool MiaIAClient::RemoveModelCheckpoint(std::uint64_t checkpointId)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        return Detail::ClientCheckpointStore().Remove(checkpointId);
    }

    bool MiaIAClient::ClearModelCheckpoints()
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        if (Detail::IsClientMutationBlocked())
        {
            return false;
        }

        Detail::ClientCheckpointStore().Clear();
        return true;
    }
}
