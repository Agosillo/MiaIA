#include "CsvDatasetImporter.h"

#include "../../Core/Model/Dataset.h"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    std::string Trim(const std::string& value)
    {
        const std::size_t first =
            value.find_first_not_of(" \t\r\n");

        if (first == std::string::npos)
        {
            return {};
        }

        const std::size_t last =
            value.find_last_not_of(" \t\r\n");

        return value.substr(first, last - first + 1);
    }

    void RemoveUtf8Bom(std::string& line)
    {
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF)
        {
            line.erase(0, 3);
        }
    }

    bool ParseCsvLine(
        const std::string& line,
        std::vector<std::string>& fields)
    {
        std::string field;
        bool inQuotes{};
        bool closedQuote{};

        for (std::size_t index = 0; index < line.size(); ++index)
        {
            const char character = line[index];

            if (inQuotes)
            {
                if (character != '"')
                {
                    field.push_back(character);
                    continue;
                }

                if (index + 1 < line.size() &&
                    line[index + 1] == '"')
                {
                    field.push_back('"');
                    ++index;
                    continue;
                }

                inQuotes = false;
                closedQuote = true;
                continue;
            }

            if (closedQuote)
            {
                if (character == ',')
                {
                    fields.push_back(Trim(field));
                    field.clear();
                    closedQuote = false;
                }
                else if (character != ' ' && character != '\t')
                {
                    return false;
                }

                continue;
            }

            if (character == ',')
            {
                fields.push_back(Trim(field));
                field.clear();
            }
            else if (character == '"')
            {
                if (!Trim(field).empty())
                {
                    return false;
                }

                field.clear();
                inQuotes = true;
            }
            else
            {
                field.push_back(character);
            }
        }

        if (inQuotes)
        {
            return false;
        }

        fields.push_back(Trim(field));
        return true;
    }

    bool ParseNumber(
        const std::string& field,
        double& result)
    {
        if (field.empty())
        {
            return false;
        }

        const char* begin = field.data();
        const char* end = begin + field.size();
        const auto parsed = std::from_chars(begin, end, result);

        return parsed.ec == std::errc{} &&
            parsed.ptr == end &&
            std::isfinite(result);
    }
}

namespace MiaIA::Engine
{
    bool CsvDatasetImporter::Import(
        Core::Dataset& dataset,
        const std::string& path,
        std::size_t inputCount,
        std::size_t targetCount,
        bool hasHeader)
    {
        if (path.empty() || inputCount == 0 || targetCount == 0)
        {
            return false;
        }

        if (inputCount >
            std::numeric_limits<std::size_t>::max() - targetCount)
        {
            return false;
        }

        std::ifstream input(path);

        if (!input)
        {
            return false;
        }

        const std::size_t expectedColumns =
            inputCount + targetCount;
        Core::Dataset importedDataset;
        importedDataset.Name =
            std::filesystem::path(path).stem().string();
        importedDataset.Source = path;
        importedDataset.InputCount = inputCount;
        importedDataset.TargetCount = targetCount;
        importedDataset.HasHeader = hasHeader;

        std::string line;
        bool firstContentLine = true;
        bool headerRead = !hasHeader;

        while (std::getline(input, line))
        {
            if (firstContentLine)
            {
                RemoveUtf8Bom(line);
            }

            if (Trim(line).empty())
            {
                continue;
            }

            firstContentLine = false;
            std::vector<std::string> fields;

            if (!ParseCsvLine(line, fields) ||
                fields.size() != expectedColumns)
            {
                return false;
            }

            if (!headerRead)
            {
                for (const std::string& field : fields)
                {
                    if (Trim(field).empty())
                    {
                        return false;
                    }
                }

                headerRead = true;
                continue;
            }

            Core::Sample sample;
            sample.Inputs.reserve(inputCount);
            sample.Targets.reserve(targetCount);

            for (std::size_t index = 0;
                index < fields.size();
                ++index)
            {
                double value{};

                if (!ParseNumber(fields[index], value))
                {
                    return false;
                }

                if (index < inputCount)
                {
                    sample.Inputs.push_back(value);
                }
                else
                {
                    sample.Targets.push_back(value);
                }
            }

            importedDataset.Samples.push_back(std::move(sample));
        }

        if (!input.eof() ||
            !headerRead ||
            importedDataset.Samples.empty())
        {
            return false;
        }

        dataset = std::move(importedDataset);
        return true;
    }
}
