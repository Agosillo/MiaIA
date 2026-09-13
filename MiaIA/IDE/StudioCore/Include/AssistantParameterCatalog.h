#pragma once

#include "AssistantInspectionCatalog.h"

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <locale>
#include <sstream>
#include <limits>

namespace MiaIA::Studio
{
    // Parameter aliases belong to (intent, role), not to a global bag of words.
    // Extend this catalog when exposing another console command; never let a
    // language pack change types, CLI switches or the command allowlist.
    enum class AssistantParameterType { Unsigned, PositiveInteger, FiniteNumber, PositiveNumber, Choice, Text };

    struct AssistantParameterDefinition
    {
        std::string Intent;
        std::string Role;
        AssistantParameterType Type;
        std::vector<std::string> Labels;
        std::vector<std::string> Choices;
        double Minimum{-(std::numeric_limits<double>::max)()};
        double Maximum{(std::numeric_limits<double>::max)()};
    };

    inline const std::vector<AssistantParameterDefinition>& AssistantParameters()
    {
        static const auto parameters = []
        {
            using T = AssistantParameterType;
            std::vector<AssistantParameterDefinition> result{
                {"miaia_network_create", "inputs", T::PositiveInteger, {"inputs", "input", "ingressi", "ingresso"}, {}},
                {"miaia_network_create", "hidden_width", T::PositiveInteger, {"hidden_width", "hidden width", "neurons per layer", "neurons per hidden layer", "neuron per layer", "neuron per hidden layer", "neurons in each layer", "neurons in each hidden layer", "neuroni per layer", "neuroni per livello", "neuroni per livello nascosto", "neuroni per strato", "neuroni per strato nascosto", "neuroni per ogni livello", "neuroni per ogni livello nascosto", "neuroni per ogni strato", "neuroni per ogni strato nascosto"}, {}},
                {"miaia_network_create", "hidden_layers", T::Unsigned, {"hidden_layers", "hidden layer", "hidden layers", "layer hidden", "layers hidden", "layer", "layers", "livello nascosto", "livelli nascosti", "strato nascosto", "strati nascosti"}, {}},
                {"miaia_network_create", "outputs", T::PositiveInteger, {"outputs", "output", "uscita", "uscite"}, {}},
                {"miaia_network_create", "hidden_activation", T::Choice, {"hidden_activation", "hidden activation", "hidden layer activation", "attivazione nascosta", "attivazione hidden", "attivazione livelli nascosti"}, {"sigmoid", "relu", "tanh", "linear"}},
                {"miaia_network_create", "output_activation", T::Choice, {"output_activation", "output activation", "output layer activation", "attivazione output", "attivazione di uscita"}, {"sigmoid", "relu", "tanh", "linear"}},
                {"miaia_network_create", "weight", T::FiniteNumber, {"weight", "initial weight", "peso", "peso iniziale"}, {}},
                {"miaia_network_create", "bias", T::FiniteNumber, {"bias", "initial bias", "bias iniziale"}, {}},
                {"miaia_model_create", "model_name", T::Text, {"model_name", "name", "named", "called", "nome", "chiamato"}, {}},
                {"miaia_model_select", "model_id", T::PositiveInteger, {"model_id", "model id", "id modello"}, {}},
                {"miaia_project_open", "project_path", T::Text, {"project_path", "path", "percorso"}, {}},
                {"miaia_project_save", "project_path", T::Text, {"project_path", "path", "percorso"}, {}},
                {"miaia_training_start", "epochs", T::PositiveInteger, {"epochs", "epoch", "epoche"}, {}},
                {"miaia_training_start", "learning_rate", T::PositiveNumber, {"learning_rate", "learning rate", "tasso di apprendimento"}, {}},
                {"miaia_training_start", "sample_order", T::Choice, {"sample_order", "sample order", "ordine campioni"}, {"sequential", "shuffle"}},
                {"miaia_training_start", "seed", T::Unsigned, {"seed", "seme"}, {}},
                {"miaia_training_run", "steps", T::PositiveInteger, {"steps", "passi"}, {"all"}},
                {"miaia_dataset_diagnose", "inactive_magnitude", T::FiniteNumber, {"inactive_magnitude", "inactive magnitude"}, {}, 0.0},
                {"miaia_dataset_diagnose", "inactive_ratio", T::FiniteNumber, {"inactive_ratio", "inactive ratio"}, {}, 0.0, 1.0},
                {"miaia_dataset_diagnose", "saturation_margin", T::FiniteNumber, {"saturation_margin", "saturation margin"}, {}, 0.0, 0.5},
                {"miaia_dataset_diagnose", "saturation_ratio", T::FiniteNumber, {"saturation_ratio", "saturation ratio"}, {}, 0.0, 1.0},
                {"miaia_dataset_diagnose", "vanishing_magnitude", T::FiniteNumber, {"vanishing_magnitude", "vanishing magnitude"}, {}, 0.0},
                {"miaia_dataset_diagnose", "vanishing_ratio", T::FiniteNumber, {"vanishing_ratio", "vanishing ratio"}, {}, 0.0, 1.0},
                {"miaia_dataset_diagnose", "exploding_magnitude", T::PositiveNumber, {"exploding_magnitude", "exploding magnitude"}, {}},
                {"miaia_dataset_diagnose", "exploding_ratio", T::FiniteNumber, {"exploding_ratio", "exploding ratio"}, {}, 0.0, 1.0},
                {"miaia_dataset_diagnose", "max_items", T::PositiveInteger, {"max_items", "max items"}, {}}
            };
            for (const auto& entry : AssistantInspectionCatalog)
                for (auto role : entry.Roles)
                {
                    if (role.empty()) continue;
                    if (role.front() == '?') role.remove_prefix(1);
                    std::string label(role);
                    for (auto& c : label) if (c == '_') c = ' ';
                    result.push_back({std::string(entry.Intent), std::string(role),
                        role.find("index") != std::string_view::npos || role == "sample_limit" ? T::Unsigned : T::PositiveInteger,
                        {std::string(role), label}, {}});
                }
            return result;
        }();
        return parameters;
    }

    inline const AssistantParameterDefinition* FindAssistantParameter(
        std::string_view intent, std::string_view role)
    {
        for (const auto& parameter : AssistantParameters())
            if (parameter.Intent == intent && parameter.Role == role) return &parameter;
        return nullptr;
    }

    inline std::string ParameterLabel(std::string_view text)
    {
        std::string result;
        for (unsigned char c : text)
        {
            if (c == ' ' || c == '\t')
            {
                if (!result.empty() && result.back() != ' ') result += ' ';
            }
            else result += static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
        }
        if (!result.empty() && result.back() == ' ') result.pop_back();
        return result;
    }

    inline bool ValidAssistantParameter(const AssistantParameterDefinition& parameter, const std::string& value)
    {
        using T = AssistantParameterType;
        if (value.empty() || value.find_first_of("\r\n\t") != std::string::npos) return false;
        if (parameter.Type == T::Text) return value.find('"') == std::string::npos;
        if (std::find(parameter.Choices.begin(), parameter.Choices.end(), ParameterLabel(value)) != parameter.Choices.end()) return true;
        if (parameter.Type == T::Choice)
            return std::find(parameter.Choices.begin(), parameter.Choices.end(), ParameterLabel(value)) != parameter.Choices.end();
        if (parameter.Type == T::Unsigned || parameter.Type == T::PositiveInteger)
        {
            std::uint64_t number{};
            auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
            return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() &&
                (parameter.Type != T::PositiveInteger || number > 0);
        }
        std::istringstream stream(value);
        stream.imbue(std::locale::classic());
        double number{};
        stream >> number;
        return stream && stream.eof() && std::isfinite(number) &&
            number >= parameter.Minimum && number <= parameter.Maximum &&
            (parameter.Type != T::PositiveNumber || number > 0.0);
    }
}
