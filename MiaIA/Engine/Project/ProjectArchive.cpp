#include "ProjectArchive.h"

#include "../Data/CsvDatasetImporter.h"
#include "../Interchange/OnnxExporter.h"
#include "../Interchange/OnnxImporter.h"
#include "../Training/TrainingBreakpointController.h"
#include "../../Core/Model/Dataset.h"
#include "../../Core/Model/Network.h"
#include "../../Core/Model/TrainingSession.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    constexpr std::array<char, 8> ArchiveMagic{
        'M', 'I', 'A', 'I', 'P', 'R', 'J', '\0'
    };
    constexpr std::array<char, 4> ModelSection{ 'O', 'N', 'N', 'X' };
    constexpr std::array<char, 4> DatasetSection{ 'D', 'A', 'T', 'A' };
    constexpr std::array<char, 4> TrainingSection{ 'T', 'R', 'N', 'G' };
    constexpr std::array<char, 4> BreakpointSection{ 'B', 'R', 'K', 'P' };
    constexpr std::uint32_t MaximumSectionCount = 64;
    constexpr std::uint64_t MaximumMetadataSectionSize = 64ull * 1024 * 1024;
    constexpr std::uint64_t MaximumStringSize = 1024ull * 1024;
    constexpr std::uint64_t MaximumBreakpointCount = 1000000;

    using MiaIA::Core::Dataset;
    using MiaIA::Core::LossType;
    using MiaIA::Core::Network;
    using MiaIA::Core::OptimizerType;
    using MiaIA::Core::ProjectInfoSnapshot;
    using MiaIA::Core::TrainingBreakpointKind;
    using MiaIA::Core::TrainingBreakpointSpec;
    using MiaIA::Core::TrainingDebugPhase;
    using MiaIA::Core::TrainingSession;

    class ScopedTemporaryFile final
    {
    public:
        explicit ScopedTemporaryFile(std::filesystem::path path)
            : Path(std::move(path))
        {
        }

        ~ScopedTemporaryFile()
        {
            std::error_code error;
            std::filesystem::remove(Path, error);
        }

        ScopedTemporaryFile(const ScopedTemporaryFile&) = delete;
        ScopedTemporaryFile& operator=(const ScopedTemporaryFile&) = delete;

        const std::filesystem::path& Get() const
        {
            return Path;
        }

        void Release()
        {
            Path.clear();
        }

    private:
        std::filesystem::path Path;
    };

    class PayloadWriter final
    {
    public:
        void WriteByte(std::uint8_t value)
        {
            Bytes.push_back(value);
        }

        void WriteUInt32(std::uint32_t value)
        {
            for (std::size_t index = 0; index < sizeof(value); ++index)
            {
                Bytes.push_back(static_cast<std::uint8_t>(
                    (value >> (index * 8)) & 0xffu));
            }
        }

        void WriteUInt64(std::uint64_t value)
        {
            for (std::size_t index = 0; index < sizeof(value); ++index)
            {
                Bytes.push_back(static_cast<std::uint8_t>(
                    (value >> (index * 8)) & 0xffu));
            }
        }

        void WriteDouble(double value)
        {
            WriteUInt64(std::bit_cast<std::uint64_t>(value));
        }

        void WriteString(const std::string& value)
        {
            WriteUInt64(static_cast<std::uint64_t>(value.size()));
            Bytes.insert(Bytes.end(), value.begin(), value.end());
        }

        const std::vector<std::uint8_t>& Data() const
        {
            return Bytes;
        }

    private:
        std::vector<std::uint8_t> Bytes;
    };

    class PayloadReader final
    {
    public:
        explicit PayloadReader(const std::vector<std::uint8_t>& bytes)
            : Bytes(bytes)
        {
        }

        bool ReadByte(std::uint8_t& value)
        {
            if (Offset >= Bytes.size())
            {
                return false;
            }

            value = Bytes[Offset++];
            return true;
        }

        bool ReadUInt32(std::uint32_t& value)
        {
            std::uint64_t converted{};

            if (!ReadUnsigned(sizeof(value), converted))
            {
                return false;
            }

            value = static_cast<std::uint32_t>(converted);
            return true;
        }

        bool ReadUInt64(std::uint64_t& value)
        {
            return ReadUnsigned(sizeof(value), value);
        }

        bool ReadDouble(double& value)
        {
            std::uint64_t bits{};

            if (!ReadUInt64(bits))
            {
                return false;
            }

            value = std::bit_cast<double>(bits);
            return std::isfinite(value);
        }

        bool ReadString(std::string& value)
        {
            std::uint64_t size{};

            if (!ReadUInt64(size) ||
                size > MaximumStringSize ||
                size > Bytes.size() - Offset)
            {
                return false;
            }

            value.assign(
                reinterpret_cast<const char*>(Bytes.data() + Offset),
                static_cast<std::size_t>(size));
            Offset += static_cast<std::size_t>(size);
            return true;
        }

        bool AtEnd() const
        {
            return Offset == Bytes.size();
        }

    private:
        bool ReadUnsigned(std::size_t size, std::uint64_t& value)
        {
            if (size > Bytes.size() - Offset)
            {
                return false;
            }

            value = 0;

            for (std::size_t index = 0; index < size; ++index)
            {
                value |= static_cast<std::uint64_t>(Bytes[Offset++]) <<
                    (index * 8);
            }

            return true;
        }

        const std::vector<std::uint8_t>& Bytes;
        std::size_t Offset{};
    };

    bool HasMaiExtension(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();

        for (char& character : extension)
        {
            if (character >= 'A' && character <= 'Z')
            {
                character = static_cast<char>(character - 'A' + 'a');
            }
        }

        return extension == ".mai";
    }

    std::filesystem::path NormalizedAbsolutePath(
        const std::filesystem::path& path)
    {
        std::error_code error;
        const std::filesystem::path absolute =
            std::filesystem::absolute(path, error);
        return (error ? path : absolute).lexically_normal();
    }

    std::filesystem::path UniqueTemporaryPath(
        const std::filesystem::path& directory,
        const std::string& prefix,
        const std::string& suffix)
    {
        const auto ticks = std::chrono::high_resolution_clock::now()
            .time_since_epoch().count();

        for (std::uint32_t attempt = 0; attempt < 1000; ++attempt)
        {
            const std::filesystem::path candidate = directory /
                (prefix + "." + std::to_string(ticks) + "." +
                    std::to_string(attempt) + suffix);
            std::error_code error;

            if (!std::filesystem::exists(candidate, error) && !error)
            {
                return candidate;
            }
        }

        return {};
    }

    bool WriteUnsigned(
        std::ostream& output,
        std::uint64_t value,
        std::size_t size)
    {
        std::array<char, sizeof(std::uint64_t)> bytes{};

        for (std::size_t index = 0; index < size; ++index)
        {
            bytes[index] = static_cast<char>(
                (value >> (index * 8)) & 0xffu);
        }

        output.write(bytes.data(), static_cast<std::streamsize>(size));
        return output.good();
    }

    bool ReadUnsigned(
        std::istream& input,
        std::uint64_t& value,
        std::size_t size)
    {
        std::array<unsigned char, sizeof(std::uint64_t)> bytes{};
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(size));

        if (!input)
        {
            return false;
        }

        value = 0;

        for (std::size_t index = 0; index < size; ++index)
        {
            value |= static_cast<std::uint64_t>(bytes[index]) <<
                (index * 8);
        }

        return true;
    }

    bool CopyBytes(
        std::istream& input,
        std::ostream& output,
        std::uint64_t size)
    {
        std::array<char, 64 * 1024> buffer{};

        while (size > 0)
        {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<std::uint64_t>(size, buffer.size()));
            input.read(buffer.data(), static_cast<std::streamsize>(chunk));

            if (input.gcount() != static_cast<std::streamsize>(chunk))
            {
                return false;
            }

            output.write(buffer.data(), static_cast<std::streamsize>(chunk));

            if (!output)
            {
                return false;
            }

            size -= chunk;
        }

        return true;
    }

    bool WritePayloadSection(
        std::ostream& output,
        const std::array<char, 4>& identifier,
        const std::vector<std::uint8_t>& payload)
    {
        output.write(identifier.data(), identifier.size());

        if (!WriteUnsigned(output, payload.size(), sizeof(std::uint64_t)))
        {
            return false;
        }

        output.write(
            reinterpret_cast<const char*>(payload.data()),
            static_cast<std::streamsize>(payload.size()));
        return output.good();
    }

    bool WriteFileSection(
        std::ostream& output,
        const std::array<char, 4>& identifier,
        const std::filesystem::path& path)
    {
        std::error_code error;
        const std::uint64_t size = std::filesystem::file_size(path, error);

        if (error)
        {
            return false;
        }

        std::ifstream input(path, std::ios::binary);

        if (!input)
        {
            return false;
        }

        output.write(identifier.data(), identifier.size());

        return WriteUnsigned(output, size, sizeof(size)) &&
            CopyBytes(input, output, size);
    }

    bool ReadPayload(
        std::istream& input,
        std::uint64_t size,
        std::vector<std::uint8_t>& payload)
    {
        if (size > MaximumMetadataSectionSize ||
            size > std::numeric_limits<std::size_t>::max())
        {
            return false;
        }

        payload.resize(static_cast<std::size_t>(size));
        input.read(
            reinterpret_cast<char*>(payload.data()),
            static_cast<std::streamsize>(payload.size()));
        return input.good();
    }

    bool SkipPayload(std::istream& input, std::uint64_t size)
    {
        if (size > static_cast<std::uint64_t>(
                std::numeric_limits<std::streamoff>::max()))
        {
            return false;
        }

        input.seekg(static_cast<std::streamoff>(size), std::ios::cur);
        return input.good();
    }

    bool ReplaceAtomically(
        const std::filesystem::path& temporary,
        const std::filesystem::path& destination)
    {
        std::error_code error;

        if (!std::filesystem::exists(destination, error))
        {
            if (error)
            {
                return false;
            }

            std::filesystem::rename(temporary, destination, error);
            return !error;
        }

        const std::filesystem::path backup = UniqueTemporaryPath(
            destination.parent_path(),
            destination.filename().string(),
            ".backup");

        if (backup.empty())
        {
            return false;
        }

        std::filesystem::rename(destination, backup, error);

        if (error)
        {
            return false;
        }

        std::filesystem::rename(temporary, destination, error);

        if (error)
        {
            std::error_code restoreError;
            std::filesystem::rename(backup, destination, restoreError);
            return false;
        }

        std::filesystem::remove(backup, error);
        return true;
    }

    std::string StoredDatasetPath(
        const Dataset& dataset,
        const std::filesystem::path& projectPath)
    {
        if (dataset.Source.empty())
        {
            return {};
        }

        const std::filesystem::path source =
            NormalizedAbsolutePath(dataset.Source);
        std::error_code error;
        const std::filesystem::path relative = std::filesystem::relative(
            source,
            projectPath.parent_path(),
            error);

        return (!error && !relative.empty()
            ? relative
            : source).generic_string();
    }

    std::filesystem::path ResolvedDatasetPath(
        const std::string& storedPath,
        const std::filesystem::path& projectPath)
    {
        std::filesystem::path path(storedPath);

        if (path.is_relative())
        {
            path = projectPath.parent_path() / path;
        }

        return NormalizedAbsolutePath(path);
    }

    bool HasTrainingConfiguration(const TrainingSession& session)
    {
        return session.EpochCount > 0 &&
            session.LearningRate > 0.0 &&
            std::isfinite(session.LearningRate);
    }

    std::vector<std::uint8_t> BuildDatasetPayload(
        const Dataset& dataset,
        const std::filesystem::path& projectPath)
    {
        PayloadWriter writer;
        writer.WriteString(StoredDatasetPath(dataset, projectPath));
        writer.WriteUInt64(dataset.InputCount);
        writer.WriteUInt64(dataset.TargetCount);
        writer.WriteByte(dataset.HasHeader ? 1 : 0);
        return writer.Data();
    }

    std::vector<std::uint8_t> BuildTrainingPayload(
        const TrainingSession& session)
    {
        PayloadWriter writer;
        writer.WriteUInt64(session.EpochCount);
        writer.WriteDouble(session.LearningRate);
        writer.WriteUInt32(static_cast<std::uint32_t>(session.Loss));
        writer.WriteUInt32(static_cast<std::uint32_t>(session.Optimizer));
        return writer.Data();
    }

    std::vector<std::uint8_t> BuildBreakpointPayload(
        const TrainingSession& session)
    {
        PayloadWriter writer;
        writer.WriteUInt64(session.Breakpoints.size());

        for (const auto& breakpoint : session.Breakpoints)
        {
            writer.WriteUInt64(breakpoint.Id);
            writer.WriteByte(breakpoint.Enabled ? 1 : 0);
            writer.WriteUInt32(static_cast<std::uint32_t>(
                breakpoint.Spec.Kind));
            writer.WriteUInt32(static_cast<std::uint32_t>(
                breakpoint.Spec.Phase));
            writer.WriteUInt64(breakpoint.Spec.TargetId);
            writer.WriteDouble(breakpoint.Spec.Threshold);
        }

        return writer.Data();
    }

    bool ParseDatasetPayload(
        const std::vector<std::uint8_t>& payload,
        std::string& source,
        std::size_t& inputCount,
        std::size_t& targetCount,
        bool& hasHeader)
    {
        PayloadReader reader(payload);
        std::uint64_t storedInputCount{};
        std::uint64_t storedTargetCount{};
        std::uint8_t storedHasHeader{};

        if (!reader.ReadString(source) || source.empty() ||
            !reader.ReadUInt64(storedInputCount) ||
            !reader.ReadUInt64(storedTargetCount) ||
            !reader.ReadByte(storedHasHeader) ||
            !reader.AtEnd() ||
            storedInputCount == 0 || storedTargetCount == 0 ||
            storedInputCount > std::numeric_limits<std::size_t>::max() ||
            storedTargetCount > std::numeric_limits<std::size_t>::max() ||
            storedHasHeader > 1)
        {
            return false;
        }

        inputCount = static_cast<std::size_t>(storedInputCount);
        targetCount = static_cast<std::size_t>(storedTargetCount);
        hasHeader = storedHasHeader != 0;
        return true;
    }

    bool ParseTrainingPayload(
        const std::vector<std::uint8_t>& payload,
        TrainingSession& session,
        ProjectInfoSnapshot& info)
    {
        PayloadReader reader(payload);
        std::uint64_t epochCount{};
        double learningRate{};
        std::uint32_t loss{};
        std::uint32_t optimizer{};

        if (!reader.ReadUInt64(epochCount) ||
            !reader.ReadDouble(learningRate) ||
            !reader.ReadUInt32(loss) ||
            !reader.ReadUInt32(optimizer) ||
            !reader.AtEnd() ||
            epochCount == 0 ||
            epochCount > std::numeric_limits<std::size_t>::max() ||
            learningRate <= 0.0 ||
            loss != static_cast<std::uint32_t>(
                LossType::MeanSquaredError) ||
            optimizer != static_cast<std::uint32_t>(
                OptimizerType::StochasticGradientDescent))
        {
            return false;
        }

        session.EpochCount = static_cast<std::size_t>(epochCount);
        session.LearningRate = learningRate;
        session.Loss = static_cast<LossType>(loss);
        session.Optimizer = static_cast<OptimizerType>(optimizer);
        info.Training.Available = true;
        info.Training.EpochCount = session.EpochCount;
        info.Training.LearningRate = session.LearningRate;
        info.Training.Loss = session.Loss;
        info.Training.Optimizer = session.Optimizer;
        return true;
    }

    bool ParseBreakpointPayload(
        const std::vector<std::uint8_t>& payload,
        TrainingSession& session)
    {
        PayloadReader reader(payload);
        std::uint64_t count{};

        if (!reader.ReadUInt64(count) || count > MaximumBreakpointCount)
        {
            return false;
        }

        std::unordered_set<std::uint64_t> identifiers;
        std::uint64_t maximumIdentifier{};

        for (std::uint64_t index = 0; index < count; ++index)
        {
            std::uint64_t identifier{};
            std::uint8_t enabled{};
            std::uint32_t kind{};
            std::uint32_t phase{};
            TrainingBreakpointSpec spec;

            if (!reader.ReadUInt64(identifier) || identifier == 0 ||
                !identifiers.insert(identifier).second ||
                !reader.ReadByte(enabled) || enabled > 1 ||
                !reader.ReadUInt32(kind) ||
                kind > static_cast<std::uint32_t>(
                    TrainingBreakpointKind::
                        ConnectionUpdateMagnitudeAbove) ||
                !reader.ReadUInt32(phase) ||
                phase > static_cast<std::uint32_t>(
                    TrainingDebugPhase::Committed) ||
                !reader.ReadUInt64(spec.TargetId) ||
                !reader.ReadDouble(spec.Threshold))
            {
                return false;
            }

            spec.Kind = static_cast<TrainingBreakpointKind>(kind);
            spec.Phase = static_cast<TrainingDebugPhase>(phase);
            MiaIA::Core::TrainingBreakpointSnapshot added;

            if (!MiaIA::Engine::TrainingBreakpointController::Add(
                    session,
                    spec,
                    added))
            {
                return false;
            }

            session.Breakpoints.back().Id = identifier;
            session.Breakpoints.back().Enabled = enabled != 0;
            maximumIdentifier = std::max(maximumIdentifier, identifier);
        }

        if (!reader.AtEnd() ||
            maximumIdentifier == std::numeric_limits<std::uint64_t>::max())
        {
            return false;
        }

        session.NextBreakpointId = maximumIdentifier + 1;
        return true;
    }

    void FillSavedInfo(
        const std::filesystem::path& projectPath,
        const Dataset& dataset,
        const TrainingSession& session,
        ProjectInfoSnapshot& info)
    {
        info = ProjectInfoSnapshot{};
        info.FormatVersion = MiaIA::Engine::ProjectArchive::
            CurrentFormatVersion;
        info.Path = projectPath.string();
        info.HasModel = true;
        info.HasDatasetReference = !dataset.Source.empty();
        info.DatasetLoaded = !dataset.Samples.empty();
        info.DatasetSource = dataset.Source;
        info.DatasetInputCount = dataset.InputCount;
        info.DatasetTargetCount = dataset.TargetCount;
        info.DatasetHasHeader = dataset.HasHeader;
        info.BreakpointCount = session.Breakpoints.size();

        if (HasTrainingConfiguration(session))
        {
            info.Training.Available = true;
            info.Training.EpochCount = session.EpochCount;
            info.Training.LearningRate = session.LearningRate;
            info.Training.Loss = session.Loss;
            info.Training.Optimizer = session.Optimizer;
        }
    }
}

bool MiaIA::Engine::ProjectArchive::Save(
    const Core::Network& network,
    const Core::Dataset& dataset,
    const Core::TrainingSession& trainingSession,
    const std::string& path,
    Core::ProjectInfoSnapshot& result)
{
    if (path.empty())
    {
        return false;
    }

    const std::filesystem::path projectPath =
        NormalizedAbsolutePath(path);

    if (!HasMaiExtension(projectPath))
    {
        return false;
    }

    std::filesystem::path directory = projectPath.parent_path();

    if (directory.empty())
    {
        directory = std::filesystem::current_path();
    }

    std::error_code error;
    std::filesystem::create_directories(directory, error);

    if (error)
    {
        return false;
    }

    const std::filesystem::path onnxPath = UniqueTemporaryPath(
        std::filesystem::temp_directory_path(error),
        "miaia-project-model",
        ".onnx");

    if (error || onnxPath.empty())
    {
        return false;
    }

    ScopedTemporaryFile onnxFile(onnxPath);

    if (!OnnxExporter::Export(network, onnxPath.string()))
    {
        return false;
    }

    const bool hasDataset = !dataset.Source.empty() &&
        dataset.InputCount > 0 && dataset.TargetCount > 0;
    const bool hasTraining = HasTrainingConfiguration(trainingSession);
    const std::uint32_t sectionCount = 2 +
        (hasDataset ? 1u : 0u) +
        (hasTraining ? 1u : 0u);
    const std::filesystem::path temporaryPath = UniqueTemporaryPath(
        directory,
        projectPath.filename().string(),
        ".tmp");

    if (temporaryPath.empty())
    {
        return false;
    }

    ScopedTemporaryFile temporaryFile(temporaryPath);
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);

    if (!output)
    {
        return false;
    }

    output.write(ArchiveMagic.data(), ArchiveMagic.size());

    if (!WriteUnsigned(output, CurrentFormatVersion, sizeof(std::uint32_t)) ||
        !WriteUnsigned(output, sectionCount, sizeof(std::uint32_t)) ||
        !WriteFileSection(output, ModelSection, onnxPath) ||
        (hasDataset && !WritePayloadSection(
            output,
            DatasetSection,
            BuildDatasetPayload(dataset, projectPath))) ||
        (hasTraining && !WritePayloadSection(
            output,
            TrainingSection,
            BuildTrainingPayload(trainingSession))) ||
        !WritePayloadSection(
            output,
            BreakpointSection,
            BuildBreakpointPayload(trainingSession)))
    {
        return false;
    }

    output.flush();
    output.close();

    if (!output || !ReplaceAtomically(temporaryPath, projectPath))
    {
        return false;
    }

    temporaryFile.Release();
    FillSavedInfo(projectPath, dataset, trainingSession, result);
    return true;
}

bool MiaIA::Engine::ProjectArchive::Load(
    const std::string& path,
    Core::Network& network,
    Core::Dataset& dataset,
    Core::TrainingSession& trainingSession,
    Core::ProjectInfoSnapshot& result)
{
    if (path.empty())
    {
        return false;
    }

    const std::filesystem::path projectPath =
        NormalizedAbsolutePath(path);

    if (!HasMaiExtension(projectPath))
    {
        return false;
    }

    std::ifstream input(projectPath, std::ios::binary);

    if (!input)
    {
        return false;
    }

    std::array<char, ArchiveMagic.size()> magic{};
    input.read(magic.data(), magic.size());
    std::uint64_t version{};
    std::uint64_t sectionCount{};

    if (!input || magic != ArchiveMagic ||
        !ReadUnsigned(input, version, sizeof(std::uint32_t)) ||
        version != CurrentFormatVersion ||
        !ReadUnsigned(input, sectionCount, sizeof(std::uint32_t)) ||
        sectionCount == 0 || sectionCount > MaximumSectionCount)
    {
        return false;
    }

    std::error_code error;
    const std::filesystem::path onnxPath = UniqueTemporaryPath(
        std::filesystem::temp_directory_path(error),
        "miaia-project-model",
        ".onnx");

    if (error || onnxPath.empty())
    {
        return false;
    }

    ScopedTemporaryFile onnxFile(onnxPath);
    bool modelFound{};
    bool datasetFound{};
    bool trainingFound{};
    bool breakpointsFound{};
    std::vector<std::uint8_t> datasetPayload;
    std::vector<std::uint8_t> trainingPayload;
    std::vector<std::uint8_t> breakpointPayload;

    for (std::uint64_t sectionIndex = 0;
        sectionIndex < sectionCount;
        ++sectionIndex)
    {
        std::array<char, 4> identifier{};
        input.read(identifier.data(), identifier.size());
        std::uint64_t payloadSize{};

        if (!input || !ReadUnsigned(
                input,
                payloadSize,
                sizeof(payloadSize)))
        {
            return false;
        }

        if (identifier == ModelSection)
        {
            if (modelFound)
            {
                return false;
            }

            std::ofstream modelOutput(
                onnxPath,
                std::ios::binary | std::ios::trunc);

            if (!modelOutput ||
                !CopyBytes(input, modelOutput, payloadSize))
            {
                return false;
            }

            modelOutput.close();
            modelFound = modelOutput.good();
        }
        else if (identifier == DatasetSection)
        {
            if (datasetFound ||
                !ReadPayload(input, payloadSize, datasetPayload))
            {
                return false;
            }

            datasetFound = true;
        }
        else if (identifier == TrainingSection)
        {
            if (trainingFound ||
                !ReadPayload(input, payloadSize, trainingPayload))
            {
                return false;
            }

            trainingFound = true;
        }
        else if (identifier == BreakpointSection)
        {
            if (breakpointsFound ||
                !ReadPayload(input, payloadSize, breakpointPayload))
            {
                return false;
            }

            breakpointsFound = true;
        }
        else if (!SkipPayload(input, payloadSize))
        {
            return false;
        }
    }

    if (!modelFound || !breakpointsFound ||
        input.peek() != std::char_traits<char>::eof())
    {
        return false;
    }

    Network importedNetwork;

    if (!OnnxImporter::Import(importedNetwork, onnxPath.string()))
    {
        return false;
    }

    TrainingSession importedSession;

    if (!ParseBreakpointPayload(breakpointPayload, importedSession))
    {
        return false;
    }

    ProjectInfoSnapshot info;
    info.FormatVersion = CurrentFormatVersion;
    info.Path = projectPath.string();
    info.HasModel = true;
    info.BreakpointCount = importedSession.Breakpoints.size();

    if (trainingFound &&
        !ParseTrainingPayload(trainingPayload, importedSession, info))
    {
        return false;
    }

    Dataset importedDataset;

    if (datasetFound)
    {
        std::string storedSource;
        std::size_t inputCount{};
        std::size_t targetCount{};
        bool hasHeader{};

        if (!ParseDatasetPayload(
                datasetPayload,
                storedSource,
                inputCount,
                targetCount,
                hasHeader))
        {
            return false;
        }

        const std::filesystem::path resolvedSource =
            ResolvedDatasetPath(storedSource, projectPath);
        info.HasDatasetReference = true;
        info.DatasetSource = resolvedSource.string();
        info.DatasetInputCount = inputCount;
        info.DatasetTargetCount = targetCount;
        info.DatasetHasHeader = hasHeader;
        info.DatasetLoaded = CsvDatasetImporter::Import(
            importedDataset,
            resolvedSource.string(),
            inputCount,
            targetCount,
            hasHeader);

        if (!info.DatasetLoaded)
        {
            importedDataset = Dataset{};
        }
    }

    importedSession.SampleCount = importedDataset.Samples.size();
    network = std::move(importedNetwork);
    dataset = std::move(importedDataset);
    trainingSession = std::move(importedSession);
    result = std::move(info);
    return true;
}
