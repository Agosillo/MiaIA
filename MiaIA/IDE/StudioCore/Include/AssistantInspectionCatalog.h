#pragma once

#include <array>
#include <string_view>

namespace MiaIA::Studio
{
    // Shared by recognition, command validation and the example sidebar.
    // A leading '?' marks an optional positional argument.
    struct AssistantInspectionEntry
    {
        std::string_view Intent;
        std::string_view Command;
        std::string_view English;
        std::string_view Italian;
        std::array<std::string_view, 3> Roles;
    };

    inline constexpr std::array AssistantInspectionCatalog{
        AssistantInspectionEntry{"miaia_project_info", "project info", "Show project information", "Mostra informazioni sul progetto", {}},
        AssistantInspectionEntry{"miaia_network_summary", "summary", "Show network summary", "Mostra riepilogo della rete", {}},
        AssistantInspectionEntry{"miaia_network_inspect", "inspect", "Inspect network values", "Ispeziona valori della rete", {}},
        AssistantInspectionEntry{"miaia_neuron_inspect", "inspect neuron", "Inspect neuron 2", "Ispeziona neurone 2", {"neuron_id", "?maximum_connections"}},
        AssistantInspectionEntry{"miaia_connection_inspect", "inspect connection", "Inspect connection 2", "Ispeziona connessione 2", {"connection_id"}},
        AssistantInspectionEntry{"miaia_model_compare", "model compare", "Compare models 1 and 2", "Confronta modelli 1 e 2", {"reference_id", "current_id", "?maximum_items"}},
        AssistantInspectionEntry{"miaia_checkpoint_list", "checkpoint list", "List checkpoints", "Elenca checkpoint", {}},
        AssistantInspectionEntry{"miaia_checkpoint_inspect", "checkpoint inspect", "Inspect checkpoint 1", "Ispeziona checkpoint 1", {"checkpoint_id"}},
        AssistantInspectionEntry{"miaia_checkpoint_compare", "checkpoint compare", "Compare checkpoints 1 and 2", "Confronta checkpoint 1 e 2", {"first_id", "second_id", "?maximum_items"}},
        AssistantInspectionEntry{"miaia_dataset_summary", "dataset summary", "Show dataset summary", "Mostra riepilogo del dataset", {}},
        AssistantInspectionEntry{"miaia_dataset_inspect", "dataset inspect", "Inspect sample 0", "Ispeziona campione 0", {"sample_index"}},
        AssistantInspectionEntry{"miaia_dataset_diagnose", "dataset diagnose", "Diagnose dataset", "Diagnostica dataset", {"?sample_limit"}},
        AssistantInspectionEntry{"miaia_training_history", "train session history", "Show training history", "Mostra cronologia del training", {}},
        AssistantInspectionEntry{"miaia_training_inspect", "train session inspect", "Inspect training step 0", "Ispeziona passo training 0", {"step_index"}},
        AssistantInspectionEntry{"miaia_training_compare", "train session compare", "Compare training steps 0 and 1", "Confronta passi training 0 e 1", {"first_step_index", "second_step_index", "?maximum_items"}},
        AssistantInspectionEntry{"miaia_debug_status", "train debug status", "Show debug status", "Mostra stato del debug", {}},
        AssistantInspectionEntry{"miaia_breakpoint_list", "train breakpoint list", "List breakpoints", "Elenca breakpoint", {}}
    };
}
