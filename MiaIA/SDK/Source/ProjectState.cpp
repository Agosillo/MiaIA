#include "ProjectState.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace
{
    constexpr std::size_t MaximumModelNameLength = 128;
}

namespace MiaIA::SDK::Detail
{
    ProjectState::ProjectState()
    {
        Reset();
    }

    void ProjectState::Reset()
    {
        Models.clear();
        Models.push_back({ DefaultModelId, DefaultModelName });
        ActiveModelId = DefaultModelId;
        NextModelId = DefaultModelId + 1;
        Info = Core::ProjectInfoSnapshot{};
    }

    ModelInstance& ProjectState::ActiveModel()
    {
        return *FindModel(ActiveModelId);
    }

    const ModelInstance& ProjectState::ActiveModel() const
    {
        return *FindModel(ActiveModelId);
    }

    ModelInstance* ProjectState::FindModel(std::uint64_t modelId)
    {
        const auto model = std::find_if(
            Models.begin(),
            Models.end(),
            [modelId](const ModelInstance& candidate)
            {
                return candidate.Id == modelId;
            });
        return model == Models.end() ? nullptr : &*model;
    }

    const ModelInstance* ProjectState::FindModel(
        std::uint64_t modelId) const
    {
        const auto model = std::find_if(
            Models.begin(),
            Models.end(),
            [modelId](const ModelInstance& candidate)
            {
                return candidate.Id == modelId;
            });
        return model == Models.end() ? nullptr : &*model;
    }

    bool ProjectState::CreateModel(
        const std::string& name,
        Core::ModelInstanceSnapshot& result)
    {
        if (!IsValidName(name) ||
            NextModelId == std::numeric_limits<std::uint64_t>::max())
        {
            return false;
        }

        const std::uint64_t modelId = NextModelId++;
        Models.push_back({ modelId, name });
        ActiveModelId = modelId;
        Info = Core::ProjectInfoSnapshot{};
        result = BuildSnapshot(Models.back(), true);
        return true;
    }

    bool ProjectState::SelectModel(std::uint64_t modelId)
    {
        if (FindModel(modelId) == nullptr)
        {
            return false;
        }

        ActiveModelId = modelId;
        return true;
    }

    bool ProjectState::RenameModel(
        std::uint64_t modelId,
        const std::string& name)
    {
        ModelInstance* model = FindModel(modelId);

        if (model == nullptr || !IsValidName(name))
        {
            return false;
        }

        model->Name = name;
        return true;
    }

    bool ProjectState::RemoveModel(std::uint64_t modelId)
    {
        if (Models.size() <= 1)
        {
            return false;
        }

        const auto model = std::find_if(
            Models.begin(),
            Models.end(),
            [modelId](const ModelInstance& candidate)
            {
                return candidate.Id == modelId;
            });

        if (model == Models.end())
        {
            return false;
        }

        const bool removingActive = model->Id == ActiveModelId;
        Models.erase(model);

        if (removingActive)
        {
            ActiveModelId = Models.front().Id;
        }

        Info = Core::ProjectInfoSnapshot{};
        return true;
    }

    std::vector<Core::ModelInstanceSnapshot>
    ProjectState::ModelSnapshots() const
    {
        std::vector<Core::ModelInstanceSnapshot> snapshots;
        snapshots.reserve(Models.size());

        for (const ModelInstance& model : Models)
        {
            snapshots.push_back(BuildSnapshot(
                model,
                model.Id == ActiveModelId));
        }

        return snapshots;
    }

    Core::ModelInstanceSnapshot
    ProjectState::ActiveModelSnapshot() const
    {
        return BuildSnapshot(ActiveModel(), true);
    }

    std::size_t ProjectState::ModelCount() const
    {
        return Models.size();
    }

    bool ProjectState::IsValidName(const std::string& name)
    {
        if (name.empty() || name.size() > MaximumModelNameLength)
        {
            return false;
        }

        return std::any_of(
            name.begin(),
            name.end(),
            [](unsigned char character)
            {
                return !std::isspace(character);
            });
    }

    Core::ModelInstanceSnapshot ProjectState::BuildSnapshot(
        const ModelInstance& model,
        bool active)
    {
        Core::ModelInstanceSnapshot snapshot;
        snapshot.Id = model.Id;
        snapshot.Name = model.Name;
        snapshot.Active = active;
        snapshot.LayerCount = model.Network.Layers.size();
        snapshot.ConnectionCount = model.Network.Connections.size();
        snapshot.DatasetSampleCount = model.Dataset.Samples.size();
        snapshot.TrainingStatus = model.TrainingSession.Status;
        snapshot.CheckpointCount = model.Checkpoints.List().size();

        for (const Core::Layer& layer : model.Network.Layers)
        {
            snapshot.NeuronCount += layer.Neurons.size();
        }

        return snapshot;
    }
}
