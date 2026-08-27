#include "ProjectState.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_set>
#include <utility>

namespace
{
    constexpr std::size_t MaximumContextNameLength = 128;
}

namespace MiaIA::SDK::Detail
{
    ProjectState::ProjectState()
    {
        Reset();
    }

    void ProjectState::Reset()
    {
        Contexts.clear();
        Contexts.push_back({ DefaultContextId, DefaultContextName });
        ActiveContextId = DefaultContextId;
        NextContextId = DefaultContextId + 1;
        Info = Core::ProjectInfoSnapshot{};
    }

    ModelContext& ProjectState::ActiveContext()
    {
        return *FindContext(ActiveContextId);
    }

    const ModelContext& ProjectState::ActiveContext() const
    {
        return *FindContext(ActiveContextId);
    }

    ModelContext* ProjectState::FindContext(std::uint64_t contextId)
    {
        const auto model = std::find_if(
            Contexts.begin(),
            Contexts.end(),
            [contextId](const ModelContext& candidate)
            {
                return candidate.Id == contextId;
            });
        return model == Contexts.end() ? nullptr : &*model;
    }

    const ModelContext* ProjectState::FindContext(
        std::uint64_t contextId) const
    {
        const auto model = std::find_if(
            Contexts.begin(),
            Contexts.end(),
            [contextId](const ModelContext& candidate)
            {
                return candidate.Id == contextId;
            });
        return model == Contexts.end() ? nullptr : &*model;
    }

    bool ProjectState::CreateContext(
        const std::string& name,
        Core::ModelContextSnapshot& result)
    {
        if (!IsValidName(name) ||
            NextContextId == std::numeric_limits<std::uint64_t>::max())
        {
            return false;
        }

        const std::uint64_t contextId = NextContextId++;
        Contexts.push_back({ contextId, name });
        ActiveContextId = contextId;
        Info = Core::ProjectInfoSnapshot{};
        result = BuildSnapshot(Contexts.back(), true);
        return true;
    }

    bool ProjectState::SelectContext(std::uint64_t contextId)
    {
        if (FindContext(contextId) == nullptr)
        {
            return false;
        }

        ActiveContextId = contextId;
        return true;
    }

    bool ProjectState::RenameContext(
        std::uint64_t contextId,
        const std::string& name)
    {
        ModelContext* model = FindContext(contextId);

        if (model == nullptr || !IsValidName(name))
        {
            return false;
        }

        model->Name = name;
        return true;
    }

    bool ProjectState::RemoveContext(std::uint64_t contextId)
    {
        if (Contexts.size() <= 1)
        {
            return false;
        }

        const auto model = std::find_if(
            Contexts.begin(),
            Contexts.end(),
            [contextId](const ModelContext& candidate)
            {
                return candidate.Id == contextId;
            });

        if (model == Contexts.end())
        {
            return false;
        }

        const bool removingActive = model->Id == ActiveContextId;
        Contexts.erase(model);

        if (removingActive)
        {
            ActiveContextId = Contexts.front().Id;
        }

        Info = Core::ProjectInfoSnapshot{};
        return true;
    }

    std::vector<Core::ModelContextSnapshot>
    ProjectState::ContextSnapshots() const
    {
        std::vector<Core::ModelContextSnapshot> snapshots;
        snapshots.reserve(Contexts.size());

        for (const ModelContext& model : Contexts)
        {
            snapshots.push_back(BuildSnapshot(
                model,
                model.Id == ActiveContextId));
        }

        return snapshots;
    }

    Core::ModelContextSnapshot
    ProjectState::ActiveContextSnapshot() const
    {
        return BuildSnapshot(ActiveContext(), true);
    }

    std::size_t ProjectState::ContextCount() const
    {
        return Contexts.size();
    }

    Core::ProjectInfoSnapshot ProjectState::InfoSnapshot() const
    {
        Core::ProjectInfoSnapshot result = Info;
        const ModelContext& active = ActiveContext();
        result.ContextCount = Contexts.size();
        result.ActiveContextId = active.Id;
        result.ActiveContextName = active.Name;
        result.HasModel = !active.Network.Layers.empty() ||
            !active.Network.Connections.empty();
        result.HasDatasetReference = !active.Dataset.Source.empty();
        result.DatasetLoaded = !active.Dataset.Samples.empty();
        result.DatasetSource = active.Dataset.Source;
        result.DatasetInputCount = active.Dataset.InputCount;
        result.DatasetTargetCount = active.Dataset.TargetCount;
        result.DatasetHasHeader = active.Dataset.HasHeader;
        result.Training = {};

        if (active.TrainingSession.EpochCount > 0 &&
            active.TrainingSession.LearningRate > 0.0)
        {
            result.Training.Available = true;
            result.Training.EpochCount = active.TrainingSession.EpochCount;
            result.Training.LearningRate =
                active.TrainingSession.LearningRate;
            result.Training.Loss = active.TrainingSession.Loss;
            result.Training.Optimizer = active.TrainingSession.Optimizer;
        }

        result.BreakpointCount =
            active.TrainingSession.Breakpoints.size();
        result.CheckpointCount = active.Checkpoints.List().size();
        return result;
    }

    Engine::ProjectArchiveView ProjectState::BuildArchiveView() const
    {
        Engine::ProjectArchiveView result;
        result.ActiveContextId = ActiveContextId;
        result.NextContextId = NextContextId;
        result.Contexts.reserve(Contexts.size());

        for (const ModelContext& model : Contexts)
        {
            result.Contexts.push_back({
                model.Id,
                &model.Name,
                &model.Network,
                &model.Dataset,
                &model.TrainingSession,
                &model.Checkpoints
            });
        }

        return result;
    }

    bool ProjectState::ReplaceArchiveState(
        Engine::ProjectArchiveState state)
    {
        if (state.Contexts.empty() || state.ActiveContextId == 0 ||
            state.NextContextId == 0)
        {
            return false;
        }

        std::vector<ModelContext> replacements;
        replacements.reserve(state.Contexts.size());
        std::unordered_set<std::uint64_t> identifiers;
        bool activeFound{};
        std::uint64_t maximumIdentifier{};

        for (Engine::ProjectArchiveContextState& source : state.Contexts)
        {
            if (source.Id == 0 || source.Id >= state.NextContextId ||
                !identifiers.insert(source.Id).second ||
                !IsValidName(source.Name))
            {
                return false;
            }

            ModelContext model;
            model.Id = source.Id;
            model.Name = std::move(source.Name);
            model.Network = std::move(source.Network);
            model.Dataset = std::move(source.Dataset);
            model.TrainingSession = std::move(source.TrainingSession);
            model.Checkpoints = std::move(source.Checkpoints);
            activeFound = activeFound || model.Id == state.ActiveContextId;
            maximumIdentifier = std::max(maximumIdentifier, model.Id);
            replacements.push_back(std::move(model));
        }

        if (!activeFound || maximumIdentifier >= state.NextContextId)
        {
            return false;
        }

        Contexts = std::move(replacements);
        ActiveContextId = state.ActiveContextId;
        NextContextId = state.NextContextId;
        Info = Core::ProjectInfoSnapshot{};
        return true;
    }

    bool ProjectState::IsValidName(const std::string& name)
    {
        if (name.empty() || name.size() > MaximumContextNameLength)
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

    Core::ModelContextSnapshot ProjectState::BuildSnapshot(
        const ModelContext& model,
        bool active)
    {
        Core::ModelContextSnapshot snapshot;
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
