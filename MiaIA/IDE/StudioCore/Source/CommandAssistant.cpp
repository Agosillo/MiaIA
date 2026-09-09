#include "../Include/CommandAssistant.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
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
            const auto inputsEntity = FindValue(
                understanding,
                { "inputs", "input_count", "miaia_inputs" });
            const auto hiddenWidthEntity = FindValue(
                understanding,
                { "hidden_width", "miaia_hidden_width" });
            const auto hiddenLayersEntity = FindValue(
                understanding,
                { "hidden_layers", "miaia_hidden_layers" });
            const auto outputsEntity = FindValue(
                understanding,
                { "outputs", "output_count", "miaia_outputs" });

            // The console intentionally supports bare `create`, which uses
            // its documented 10/32/2/3 defaults.  Once any topology value is
            // supplied, however, require the complete shape so the assistant
            // never guesses the remaining dimensions.
            if (!inputsEntity && !hiddenWidthEntity &&
                !hiddenLayersEntity && !outputsEntity)
            {
                command = "create";
                return true;
            }

            std::uint64_t inputs{};
            std::uint64_t hiddenWidth{};
            std::uint64_t hiddenLayers{};
            std::uint64_t outputs{};

            if (!RequirePositiveEntity(
                    understanding,
                    { "inputs", "input_count", "miaia_inputs" },
                    "inputs",
                    inputs,
                    error) ||
                !RequirePositiveEntity(
                    understanding,
                    { "hidden_width", "miaia_hidden_width" },
                    "hidden_width",
                    hiddenWidth,
                    error) ||
                !RequirePositiveEntity(
                    understanding,
                    { "hidden_layers", "miaia_hidden_layers" },
                    "hidden_layers",
                    hiddenLayers,
                    error) ||
                !RequirePositiveEntity(
                    understanding,
                    { "outputs", "output_count", "miaia_outputs" },
                    "outputs",
                    outputs,
                    error))
            {
                return false;
            }

            command = "create " + std::to_string(inputs) + ' ' +
                std::to_string(hiddenWidth) + ' ' +
                std::to_string(hiddenLayers) + ' ' +
                std::to_string(outputs);
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
