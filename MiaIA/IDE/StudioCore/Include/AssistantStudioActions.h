#pragma once

#include "AssistantInspectionCatalog.h"
#include <optional>
#include <vector>

namespace MiaIA::Studio
{
    // Explicit set actions, never toggles. Portable metadata contains no renderer
    // or operating-system dependency; the Studio host supplies the implementation.
    enum class StudioViewAction
    {
        View2D, View3D, LayoutExpanded, LayoutPacked,
        LabelsShow, LabelsHide, ConnectionsShow, ConnectionsHide, ConnectionsAll, ConnectionsSelected, FitView, ResetLayout
    };

    struct AssistantStudioActionEntry
    {
        StudioViewAction Action;
        AssistantInspectionEntry Command;
    };

    inline constexpr std::array AssistantStudioActions{
        AssistantStudioActionEntry{StudioViewAction::View2D, {"miaia_studio_view_2d", "studio view 2d", "Switch to 2D", "Passa alla vista 2D", {}}},
        AssistantStudioActionEntry{StudioViewAction::View3D, {"miaia_studio_view_3d", "studio view 3d", "Switch to 3D", "Passa alla vista 3D", {}}},
        AssistantStudioActionEntry{StudioViewAction::LayoutExpanded, {"miaia_studio_layout_expanded", "studio layout expanded", "Set layout expanded", "Imposta layout expanded", {}}},
        AssistantStudioActionEntry{StudioViewAction::LayoutPacked, {"miaia_studio_layout_packed", "studio layout packed", "Set layout packed", "Imposta layout packed", {}}},
        AssistantStudioActionEntry{StudioViewAction::LabelsShow, {"miaia_studio_labels_show", "studio labels show", "Show neuron labels", "Mostra le etichette dei neuroni", {}}},
        AssistantStudioActionEntry{StudioViewAction::LabelsHide, {"miaia_studio_labels_hide", "studio labels hide", "Hide neuron labels", "Nascondi le etichette dei neuroni", {}}},
        AssistantStudioActionEntry{StudioViewAction::ConnectionsShow, {"miaia_studio_connections_show", "studio connections show", "Show connections", "Mostra le connessioni", {}}},
        AssistantStudioActionEntry{StudioViewAction::ConnectionsHide, {"miaia_studio_connections_hide", "studio connections hide", "Hide connections", "Nascondi le connessioni", {}}},
        AssistantStudioActionEntry{StudioViewAction::ConnectionsAll, {"miaia_studio_connections_all", "studio connections all", "Show all connections", "Mostra tutte le connessioni", {}}},
        AssistantStudioActionEntry{StudioViewAction::ConnectionsSelected, {"miaia_studio_connections_selected", "studio connections selected", "Show only selected connections", "Mostra solo le connessioni selezionate", {}}},
        AssistantStudioActionEntry{StudioViewAction::FitView, {"miaia_studio_fit_view", "studio view fit", "Fit view", "Adatta la vista", {}}},
        AssistantStudioActionEntry{StudioViewAction::ResetLayout, {"miaia_studio_reset_layout", "studio layout reset", "Reset layout", "Ripristina il layout", {}}}
    };

    inline const AssistantStudioActionEntry* FindStudioIntent(std::string_view intent)
    {
        for (const auto& entry : AssistantStudioActions)
            if (entry.Command.Intent == intent) return &entry;
        return nullptr;
    }

    inline std::optional<StudioViewAction> ParseStudioViewAction(std::string_view command)
    {
        for (const auto& entry : AssistantStudioActions)
            if (entry.Command.Command == command) return entry.Action;
        return std::nullopt;
    }

    inline const std::vector<AssistantInspectionEntry>& AssistantBuiltinCommands()
    {
        static const auto entries = []
        {
            std::vector<AssistantInspectionEntry> result(AssistantInspectionCatalog.begin(), AssistantInspectionCatalog.end());
            for (const auto& entry : AssistantStudioActions) result.push_back(entry.Command);
            return result;
        }();
        return entries;
    }
}
