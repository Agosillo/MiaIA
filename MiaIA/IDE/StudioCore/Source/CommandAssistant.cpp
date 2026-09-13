#include "../Include/CommandAssistant.h"
#include "../Include/AssistantStudioActions.h"
#include "../Include/AssistantParameterCatalog.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

namespace
{
    std::string Lower(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char character)
            {
                if (character >= 'A' && character <= 'Z')
                {
                    return static_cast<char>(character - 'A' + 'a');
                }

                return static_cast<char>(character);
            });
        return value;
    }

    bool Matches(
        const MiaIA::Studio::CommandAssistantEntity& entity,
        std::initializer_list<std::string_view> names)
    {
        const std::string entityName = Lower(entity.Name);
        const std::string entityRole = Lower(entity.Role);

        for (const std::string_view name : names)
        {
            if (entityName == name || entityRole == name)
            {
                return true;
            }
        }

        return false;
    }

    std::optional<std::string> FindValue(
        const MiaIA::Studio::CommandAssistantUnderstanding& understanding,
        std::initializer_list<std::string_view> names)
    {
        for (const auto& entity : understanding.Entities)
        {
            if (Matches(entity, names) && !entity.Value.empty())
            {
                return entity.Value;
            }
        }

        return std::nullopt;
    }

    bool TryUnsigned(const std::string& text, std::uint64_t& value)
    {
        if (text.empty() || text.front() == '-')
        {
            return false;
        }

        const char* begin = text.data();
        const char* end = begin + text.size();
        const auto result = std::from_chars(begin, end, value);
        return result.ec == std::errc{} && result.ptr == end;
    }

    bool TryPositive(const std::string& text, std::uint64_t& value)
    {
        return TryUnsigned(text, value) && value > 0;
    }

    bool TryPositiveDouble(const std::string& text, double& value)
    {
        std::istringstream stream(text);
        stream.imbue(std::locale::classic());
        stream >> value;
        return stream && stream.eof() && std::isfinite(value) && value > 0.0;
    }

    std::string FormatDouble(double value)
    {
        std::ostringstream stream;
        stream.imbue(std::locale::classic());
        // Command text should stay human-readable. Fifteen significant digits
        // preserve practical learning rates without exposing binary rounding
        // artifacts such as 0.0050000000000000001.
        stream << std::setprecision(15) << value;
        return stream.str();
    }

    bool TryQuoted(const std::string& value, std::string& quoted)
    {
        if (value.empty() ||
            value.find_first_of("\r\n\"") != std::string::npos)
        {
            return false;
        }

        quoted = '"' + value + '"';
        return true;
    }

    bool RequirePositiveEntity(
        const MiaIA::Studio::CommandAssistantUnderstanding& understanding,
        std::initializer_list<std::string_view> names,
        const char* label,
        std::uint64_t& value,
        std::string& error)
    {
        const auto text = FindValue(understanding, names);

        if (!text || !TryPositive(*text, value))
        {
            error = std::string("Missing or invalid positive ") + label + '.';
            return false;
        }

        return true;
    }

    bool BuildCommand(
        const MiaIA::Studio::CommandAssistantUnderstanding& understanding,
        std::string& command,
        std::string& error)
    {
        const std::string intent = Lower(understanding.Intent);

        for (const auto& entry : MiaIA::Studio::AssistantBuiltinCommands())
        {
            if (entry.Intent != intent) continue;
            std::map<std::string, std::string> supplied;
            for (const auto& entity : understanding.Entities)
            {
                std::string role = Lower(entity.Role.empty() ? entity.Name : entity.Role);
                if (role.starts_with("miaia_")) role.erase(0, 6);
                const auto* definition = MiaIA::Studio::FindAssistantParameter(intent, role);
                if (!definition || !MiaIA::Studio::ValidAssistantParameter(*definition, entity.Value) || supplied.contains(role))
                {
                    error = "Unknown, repeated or invalid parameter: " + role + ".";
                    return false;
                }
                supplied.emplace(role, entity.Value);
            }
            command = entry.Command;
            for (auto role : entry.Roles)
            {
                if (role.empty()) break;
                const bool optional = role.front() == '?';
                if (optional) role.remove_prefix(1);
                const auto text = FindValue(understanding, {role});
                if (!text && optional) continue;
                std::uint64_t value{};
                const bool zeroAllowed = role.find("index") != std::string_view::npos || role == "sample_limit";
                if (!text || !TryUnsigned(*text, value) || (!zeroAllowed && value == 0))
                {
                    error = "Missing or invalid " + std::string(role) + ".";
                    command.clear();
                    return false;
                }
                command += " " + std::to_string(value);
            }
            if (intent == "miaia_dataset_diagnose")
            {
                // Match SignalHealthConfiguration defaults when one threshold
                // is omitted; these must preserve exploding > vanishing.
                const auto threshold = [&](const char* role, double defaultValue)
                {
                    const auto found = supplied.find(role);
                    if (found == supplied.end()) return defaultValue;
                    std::istringstream stream(found->second);
                    stream.imbue(std::locale::classic());
                    stream >> defaultValue;
                    return defaultValue;
                };
                if (threshold("exploding_magnitude", 100.0) <= threshold("vanishing_magnitude", 1.0e-8))
                {
                    error = "exploding_magnitude must exceed vanishing_magnitude.";
                    command.clear();
                    return false;
                }
                for (const auto& [role, value] : supplied)
                {
                    if (role == "sample_limit") continue;
                    std::string option = role;
                    std::replace(option.begin(), option.end(), '_', '-');
                    command += " --" + option + " " + value;
                }
            }
            return true;
        }

        if (intent == "miaia_help")
        {
            command = "help";
            return true;
        }

        if (intent == "miaia_model_list")
        {
            command = "model list";
            return true;
        }

        if (intent == "miaia_model_create")
        {
            const auto name = FindValue(
                understanding,
                { "model_name", "miaia_model_name" });
            std::string quoted;

            if (!name || !TryQuoted(*name, quoted))
            {
                error = "A non-empty model_name without quotes or newlines is required.";
                return false;
            }

            command = "model create " + quoted;
            return true;
        }

        if (intent == "miaia_model_select")
        {
            std::uint64_t id{};

            if (!RequirePositiveEntity(
                understanding,
                { "model_id", "miaia_model_id" },
                "model_id",
                id,
                error))
            {
                return false;
            }

            command = "model select " + std::to_string(id);
            return true;
        }

        if (intent == "miaia_network_create")
        {
            // All eight create parameters use the same typed catalog as local
            // extraction. Validate again here for every provider, including Wit.
            std::map<std::string, std::string> values;
            for (const auto& entity : understanding.Entities)
            {
                std::string role = Lower(entity.Role.empty() ? entity.Name : entity.Role);
                if (role.starts_with("miaia_")) role.erase(0, 6);
                if (role == "input_count") role = "inputs";
                if (role == "output_count") role = "outputs";
                const auto* definition = MiaIA::Studio::FindAssistantParameter(intent, role);
                if (!definition || !MiaIA::Studio::ValidAssistantParameter(*definition, entity.Value) ||
                    values.contains(role))
                {
                    error = "Unknown, repeated or invalid create parameter: " + role + ".";
                    return false;
                }
                values.emplace(role, entity.Value);
            }
            if (values.empty())
            {
                command = "create";
                return true;
            }
            command = "create";
            for (const auto role : {"inputs", "hidden_width", "hidden_layers", "outputs"})
            {
                const auto found = values.find(role);
                std::uint64_t value{};
                if (found == values.end() || !TryUnsigned(found->second, value) ||
                    value > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
                {
                    command.clear();
                    error = "Missing or out-of-range create parameter: " + std::string(role) + ".";
                    return false;
                }
                command += " " + std::to_string(value);
            }
            for (const auto role : {"hidden_activation", "output_activation", "weight", "bias"})
            {
                const auto found = values.find(role);
                if (found == values.end()) continue;
                std::string option = role;
                std::replace(option.begin(), option.end(), '_', '-');
                command += " --" + option + " " + Lower(found->second);
            }
            return true;
        }

        if (intent == "miaia_training_start")
        {
            std::uint64_t epochs{};

            if (!RequirePositiveEntity(
                understanding,
                { "epochs", "miaia_epochs" },
                "epochs",
                epochs,
                error))
            {
                return false;
            }

            const auto learningRateText = FindValue(
                understanding,
                { "learning_rate", "miaia_learning_rate" });
            double learningRate{};

            if (!learningRateText ||
                !TryPositiveDouble(*learningRateText, learningRate))
            {
                error = "Missing or invalid positive learning_rate.";
                return false;
            }

            command = "train session start " + std::to_string(epochs) +
                ' ' + FormatDouble(learningRate) + " mse";

            const auto orderValue = FindValue(
                understanding,
                { "sample_order", "miaia_sample_order" });

            if (!orderValue ||
                Lower(*orderValue) == "sequential" ||
                Lower(*orderValue) == "sequenziale")
            {
                if (FindValue(understanding, {"seed", "miaia_seed"}))
                {
                    error = "A seed requires shuffle sample_order; it cannot be ignored.";
                    return false;
                }
                return true;
            }

            if (Lower(*orderValue) != "shuffle" &&
                Lower(*orderValue) != "shuffled" &&
                Lower(*orderValue) != "casuale" &&
                Lower(*orderValue) != "mescola" &&
                Lower(*orderValue) != "mescolato" &&
                Lower(*orderValue) != "mischia")
            {
                error = "sample_order must be sequential or shuffle.";
                return false;
            }

            const auto seedText = FindValue(
                understanding,
                { "seed", "miaia_seed" });
            std::uint64_t seed{};

            if (!seedText || !TryUnsigned(*seedText, seed))
            {
                error = "An unsigned seed is required for shuffled training.";
                return false;
            }

            command += " shuffle " + std::to_string(seed);
            return true;
        }

        if (intent == "miaia_training_run")
        {
            const auto steps = FindValue(
                understanding,
                { "steps", "miaia_steps" });

            if (!steps || Lower(*steps) == "all" ||
                Lower(*steps) == "tutti" ||
                Lower(*steps) == "tutto")
            {
                command = "train session run all";
                return true;
            }

            std::uint64_t count{};

            if (!TryPositive(*steps, count))
            {
                error = "steps must be a positive number or all.";
                return false;
            }

            command = "train session run " + std::to_string(count);
            return true;
        }

        if (intent == "miaia_training_pause")
        {
            command = "train session pause";
            return true;
        }

        if (intent == "miaia_training_resume")
        {
            command = "train session resume";
            return true;
        }

        if (intent == "miaia_training_status")
        {
            command = "train session status";
            return true;
        }

        if (intent == "miaia_project_new")
        {
            command = "project new";
            return true;
        }

        if (intent == "miaia_project_open" ||
            intent == "miaia_project_save")
        {
            const auto path = FindValue(
                understanding,
                { "project_path", "miaia_project_path" });

            if (!path && intent == "miaia_project_save")
            {
                command = "project save";
                return true;
            }

            std::string quoted;

            if (!path || !TryQuoted(*path, quoted))
            {
                error = "A project_path without quotes or newlines is required.";
                return false;
            }

            command = intent == "miaia_project_open"
                ? "project open " + quoted
                : "project save " + quoted;
            return true;
        }

        error = "The recognized intent is not supported by MiaIA Studio.";
        return false;
    }
}

bool MiaIA::Studio::CommandAssistant::Propose(
    const CommandAssistantUnderstanding& understanding,
    CommandProposal& proposal)
{
    CommandProposal next;
    next.SourceText = understanding.Text;
    next.Intent = understanding.Intent;
    next.Confidence = understanding.Confidence;
    next.FullyConfident = understanding.Confidence == 1.0 &&
        std::all_of(
            understanding.Entities.begin(),
            understanding.Entities.end(),
            [](const CommandAssistantEntity& entity)
            {
                return entity.Confidence == 1.0;
            });

    if (!understanding.Error.empty())
    {
        next.Error = understanding.Error;
        proposal = std::move(next);
        return false;
    }

    if (understanding.Text.empty())
    {
        next.Error = "The assistant returned no transcription or text.";
        proposal = std::move(next);
        return false;
    }

    if (understanding.Intent.empty())
    {
        next.Error = "No MiaIA intent was recognized.";
        proposal = std::move(next);
        return false;
    }

    if (!std::isfinite(understanding.Confidence) ||
        understanding.Confidence < MinimumIntentConfidence ||
        understanding.Confidence > 1.0)
    {
        next.Error = "The recognized intent confidence is too low.";
        proposal = std::move(next);
        return false;
    }

    if (!BuildCommand(understanding, next.Command, next.Error))
    {
        proposal = std::move(next);
        return false;
    }

    proposal = std::move(next);
    return true;
}
