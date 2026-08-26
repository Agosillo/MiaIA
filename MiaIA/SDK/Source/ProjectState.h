#pragma once

#include "ModelInstance.h"
#include "../../Core/Public/ModelInstanceSnapshot.h"
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
        static constexpr std::uint64_t DefaultModelId = 1;
        static constexpr const char* DefaultModelName = "Model 1";

        ProjectState();

        void Reset();

        [[nodiscard]] ModelInstance& ActiveModel();
        [[nodiscard]] const ModelInstance& ActiveModel() const;
        [[nodiscard]] ModelInstance* FindModel(std::uint64_t modelId);
        [[nodiscard]] const ModelInstance* FindModel(
            std::uint64_t modelId) const;

        bool CreateModel(
            const std::string& name,
            Core::ModelInstanceSnapshot& result);
        bool SelectModel(std::uint64_t modelId);
        bool RenameModel(
            std::uint64_t modelId,
            const std::string& name);
        bool RemoveModel(std::uint64_t modelId);

        [[nodiscard]] std::vector<Core::ModelInstanceSnapshot>
            ModelSnapshots() const;
        [[nodiscard]] Core::ModelInstanceSnapshot
            ActiveModelSnapshot() const;
        [[nodiscard]] std::size_t ModelCount() const;

        Core::ProjectInfoSnapshot Info;

    private:
        static bool IsValidName(const std::string& name);
        static Core::ModelInstanceSnapshot BuildSnapshot(
            const ModelInstance& model,
            bool active);

        std::vector<ModelInstance> Models;
        std::uint64_t ActiveModelId{ DefaultModelId };
        std::uint64_t NextModelId{ DefaultModelId + 1 };
    };
}
