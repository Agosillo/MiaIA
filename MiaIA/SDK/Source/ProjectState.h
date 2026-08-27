#pragma once

#include "ModelContext.h"
#include "../../Engine/Project/ProjectArchiveState.h"
#include "../../Core/Public/ModelContextSnapshot.h"
#include "../../Core/Public/ProjectInfoSnapshot.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MiaIA::SDK::Detail
{
    class ProjectState final
    {
    public:
        static constexpr std::uint64_t DefaultContextId = 1;
        static constexpr const char* DefaultContextName = "Model 1";

        ProjectState();

        void Reset();

        [[nodiscard]] ModelContext& ActiveContext();
        [[nodiscard]] const ModelContext& ActiveContext() const;
        [[nodiscard]] ModelContext* FindContext(std::uint64_t contextId);
        [[nodiscard]] const ModelContext* FindContext(
            std::uint64_t contextId) const;

        bool CreateContext(
            const std::string& name,
            Core::ModelContextSnapshot& result);
        bool SelectContext(std::uint64_t contextId);
        bool RenameContext(
            std::uint64_t contextId,
            const std::string& name);
        bool RemoveContext(std::uint64_t contextId);

        [[nodiscard]] std::vector<Core::ModelContextSnapshot>
            ContextSnapshots() const;
        [[nodiscard]] Core::ModelContextSnapshot
            ActiveContextSnapshot() const;
        [[nodiscard]] std::size_t ContextCount() const;
        [[nodiscard]] Core::ProjectInfoSnapshot InfoSnapshot() const;
        [[nodiscard]] Engine::ProjectArchiveView BuildArchiveView() const;
        bool ReplaceArchiveState(Engine::ProjectArchiveState state);

        Core::ProjectInfoSnapshot Info;

    private:
        static bool IsValidName(const std::string& name);
        static Core::ModelContextSnapshot BuildSnapshot(
            const ModelContext& model,
            bool active);

        std::vector<ModelContext> Contexts;
        std::uint64_t ActiveContextId{ DefaultContextId };
        std::uint64_t NextContextId{ DefaultContextId + 1 };
    };
}
