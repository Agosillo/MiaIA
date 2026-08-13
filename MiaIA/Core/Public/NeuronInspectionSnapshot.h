#pragma once

#include "ConnectionSnapshot.h"
#include "NeuronContextSnapshot.h"

#include <cstddef>
#include <vector>

namespace MiaIA::Core
{
    enum class NeuronRelationshipDirection
    {
        Incoming,
        Outgoing
    };

    enum class NeuronRelationshipSort
    {
        ConnectionId,
        Weight,
        AbsoluteWeight
    };

    struct NeuronRelationshipPageRequest
    {
        NeuronRelationshipDirection Direction{
            NeuronRelationshipDirection::Incoming};
        std::size_t Offset{};
        std::size_t Limit{25};
        NeuronRelationshipSort Sort{
            NeuronRelationshipSort::ConnectionId};
        bool Descending{};
        double MinimumAbsoluteWeight{};
    };

    struct NeuronRelationshipPageSnapshot
    {
        NeuronContextSnapshot Context;
        NeuronRelationshipDirection Direction{
            NeuronRelationshipDirection::Incoming};
        std::size_t TotalConnectionCount{};
        std::size_t FilteredConnectionCount{};
        std::size_t Offset{};
        std::size_t Limit{};
        bool HasPrevious{};
        bool HasNext{};
        std::vector<ConnectionSnapshot> Connections;
    };

    struct NeuronInspectionSnapshot
    {
        NeuronContextSnapshot Context;
        std::size_t IncomingConnectionCount{};
        std::size_t OutgoingConnectionCount{};
        std::vector<ConnectionSnapshot> IncomingConnections;
        std::vector<ConnectionSnapshot> OutgoingConnections;
    };
}
