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
    constexpr std::array<char, 4> ProjectSection{ 'P', 'R', 'J', 'T' };
    constexpr std::array<char, 4> ContextStateSection{ 'M', 'O', 'D', 'L' };
    constexpr std::array<char, 4> CheckpointSection{ 'C', 'K', 'P', 'T' };
    constexpr std::uint32_t Version1 = 1;
    constexpr std::uint32_t Version2 = 2;
    constexpr std::uint64_t MaximumSectionCount = 4000000;
    constexpr std::uint64_t MaximumContextCount = 100000;
    constexpr std::uint64_t MaximumMetadataSectionSize = 64ull * 1024 * 1024;
    constexpr std::uint64_t MaximumStringSize = 1024ull * 1024;
    constexpr std::size_t MaximumContextNameLength = 128;
    constexpr std::uint64_t MaximumBreakpointCount = 1000000;
    constexpr std::uint64_t MaximumCheckpointCount = 1000000;

    using MiaIA::Core::Dataset;
    using MiaIA::Core::LossType;
    using MiaIA::Core::Network;
    using MiaIA::Core::OptimizerType;
    using MiaIA::Core::ProjectInfoSnapshot;
    using MiaIA::Core::TrainingBreakpointKind;
    using MiaIA::Core::TrainingBreakpointSpec;
    using MiaIA::Core::TrainingDebugPhase;
    using MiaIA::Core::TrainingSession;
    using MiaIA::Engine::ModelCheckpointArchiveEntry;
    using MiaIA::Engine::ModelCheckpointArchiveEntryView;
    using MiaIA::Engine::ProjectArchiveContextState;
    using MiaIA::Engine::ProjectArchiveContextView;
    using MiaIA::Engine::ProjectArchiveState;
    using MiaIA::Engine::ProjectArchiveView;

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
        std::uint32_t formatVersion,
        std::size_t contextCount,
        std::uint64_t activeContextId,
        const std::string& activeContextName,
        bool hasActiveContextNetwork,
        const Dataset& dataset,
        const TrainingSession& session,
        std::size_t checkpointCount,
        ProjectInfoSnapshot& info)
    {
        info = ProjectInfoSnapshot{};
        info.FormatVersion = formatVersion;
        info.Path = projectPath.string();
        info.ContextCount = contextCount;
        info.ActiveContextId = activeContextId;
        info.ActiveContextName = activeContextName;
        info.HasModel = hasActiveContextNetwork;
        info.HasDatasetReference = !dataset.Source.empty();
        info.DatasetLoaded = !dataset.Samples.empty();
        info.DatasetSource = dataset.Source;
        info.DatasetInputCount = dataset.InputCount;
        info.DatasetTargetCount = dataset.TargetCount;
        info.DatasetHasHeader = dataset.HasHeader;
        info.BreakpointCount = session.Breakpoints.size();
        info.CheckpointCount = checkpointCount;

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

bool MiaIA::Engine::ProjectArchive::SaveVersion1(
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

    if (!WriteUnsigned(output, Version1, sizeof(std::uint32_t)) ||
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
    FillSavedInfo(
        projectPath,
        Version1,
        1,
        1,
        "Model 1",
        true,
        dataset,
        trainingSession,
        0,
        result);
    return true;
}

namespace
{
bool LoadVersion1(
    const std::string& path,
    Network& network,
    Dataset& dataset,
    TrainingSession& trainingSession,
    ProjectInfoSnapshot& result)
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
        version != Version1 ||
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

    if (!MiaIA::Engine::OnnxImporter::Import(
            importedNetwork,
            onnxPath.string()))
    {
        return false;
    }

    TrainingSession importedSession;

    if (!ParseBreakpointPayload(breakpointPayload, importedSession))
    {
        return false;
    }

    ProjectInfoSnapshot info;
    info.FormatVersion = Version1;
    info.Path = projectPath.string();
    info.ContextCount = 1;
    info.ActiveContextId = 1;
    info.ActiveContextName = "Model 1";
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
        info.DatasetLoaded = MiaIA::Engine::CsvDatasetImporter::Import(
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
}

namespace
{
    constexpr std::uint8_t ContextHasDataset = 1u << 0;
    constexpr std::uint8_t ContextHasTraining = 1u << 1;
    constexpr std::uint8_t ContextHasNetwork = 1u << 2;
    constexpr std::uint8_t KnownContextFlags =
        ContextHasDataset | ContextHasTraining | ContextHasNetwork;

    class ScopedTemporaryFiles final
    {
    public:
        ~ScopedTemporaryFiles()
        {
            for (const std::filesystem::path& path : Paths)
            {
                std::error_code error;
                std::filesystem::remove(path, error);
            }
        }

        void Add(const std::filesystem::path& path)
        {
            Paths.push_back(path);
        }

    private:
        std::vector<std::filesystem::path> Paths;
    };

    struct PreparedCheckpoint final
    {
        ModelCheckpointArchiveEntryView View;
        std::filesystem::path OnnxPath;
    };

    struct PreparedContext final
    {
        const ProjectArchiveContextView* View{};
        bool HasNetwork{};
        bool HasDataset{};
        bool HasTraining{};
        std::filesystem::path OnnxPath;
        std::vector<PreparedCheckpoint> Checkpoints;
    };

    bool HasText(const std::string& value)
    {
        return value.find_first_not_of(" \t\r\n") != std::string::npos;
    }

    bool ReadSectionHeader(
        std::istream& input,
        std::array<char, 4>& identifier,
        std::uint64_t& payloadSize)
    {
        input.read(identifier.data(), identifier.size());
        return input.good() && ReadUnsigned(
            input,
            payloadSize,
            sizeof(payloadSize));
    }

    bool ReadExpectedPayloadSection(
        std::istream& input,
        const std::array<char, 4>& expected,
        std::vector<std::uint8_t>& payload)
    {
        std::array<char, 4> identifier{};
        std::uint64_t payloadSize{};
        return ReadSectionHeader(input, identifier, payloadSize) &&
            identifier == expected &&
            ReadPayload(input, payloadSize, payload);
    }

    bool ImportNetworkSection(
        std::istream& input,
        const std::string& temporaryPrefix,
        Network& network)
    {
        std::array<char, 4> identifier{};
        std::uint64_t payloadSize{};

        if (!ReadSectionHeader(input, identifier, payloadSize) ||
            identifier != ModelSection || payloadSize == 0)
        {
            return false;
        }

        std::error_code error;
        const std::filesystem::path temporaryPath = UniqueTemporaryPath(
            std::filesystem::temp_directory_path(error),
            temporaryPrefix,
            ".onnx");

        if (error || temporaryPath.empty())
        {
            return false;
        }

        ScopedTemporaryFile temporaryFile(temporaryPath);
        std::ofstream output(
            temporaryPath,
            std::ios::binary | std::ios::trunc);

        if (!output || !CopyBytes(input, output, payloadSize))
        {
            return false;
        }

        output.close();
        return output.good() &&
            MiaIA::Engine::OnnxImporter::Import(
                network,
                temporaryPath.string());
    }

    std::vector<std::uint8_t> BuildProjectPayload(
        const ProjectArchiveView& project)
    {
        PayloadWriter writer;
        writer.WriteUInt64(project.ActiveContextId);
        writer.WriteUInt64(project.NextContextId);
        writer.WriteUInt64(project.Contexts.size());
        return writer.Data();
    }

    std::vector<std::uint8_t> BuildContextStatePayload(
        const PreparedContext& model)
    {
        PayloadWriter writer;
        std::uint8_t flags{};
        flags |= model.HasDataset ? ContextHasDataset : 0;
        flags |= model.HasTraining ? ContextHasTraining : 0;
        flags |= model.HasNetwork ? ContextHasNetwork : 0;
        writer.WriteUInt64(model.View->Id);
        writer.WriteString(*model.View->Name);
        writer.WriteUInt64(model.View->Checkpoints->NextIdentifier());
        writer.WriteByte(flags);
        writer.WriteUInt64(model.Checkpoints.size());
        return writer.Data();
    }

    std::vector<std::uint8_t> BuildCheckpointPayload(
        const ModelCheckpointArchiveEntryView& checkpoint)
    {
        PayloadWriter writer;
        writer.WriteUInt64(checkpoint.Id);
        writer.WriteString(*checkpoint.Name);
        return writer.Data();
    }

    bool ParseProjectPayload(
        const std::vector<std::uint8_t>& payload,
        std::uint64_t& activeContextId,
        std::uint64_t& nextContextId,
        std::uint64_t& contextCount)
    {
        PayloadReader reader(payload);
        return reader.ReadUInt64(activeContextId) && activeContextId != 0 &&
            reader.ReadUInt64(nextContextId) && nextContextId != 0 &&
            reader.ReadUInt64(contextCount) && contextCount > 0 &&
            contextCount <= MaximumContextCount && reader.AtEnd();
    }

    bool ParseContextStatePayload(
        const std::vector<std::uint8_t>& payload,
        std::uint64_t& contextId,
        std::string& name,
        std::uint64_t& nextCheckpointId,
        std::uint8_t& flags,
        std::uint64_t& checkpointCount)
    {
        PayloadReader reader(payload);
        return reader.ReadUInt64(contextId) && contextId != 0 &&
            reader.ReadString(name) && HasText(name) &&
            name.size() <= MaximumContextNameLength &&
            reader.ReadUInt64(nextCheckpointId) && nextCheckpointId != 0 &&
            reader.ReadByte(flags) && (flags & ~KnownContextFlags) == 0 &&
            reader.ReadUInt64(checkpointCount) &&
            checkpointCount <= MaximumCheckpointCount && reader.AtEnd();
    }

    bool ParseCheckpointPayload(
        const std::vector<std::uint8_t>& payload,
        std::uint64_t& checkpointId,
        std::string& name)
    {
        PayloadReader reader(payload);
        return reader.ReadUInt64(checkpointId) && checkpointId != 0 &&
            reader.ReadString(name) && HasText(name) && reader.AtEnd();
    }

    bool RestoreDataset(
        const std::vector<std::uint8_t>& payload,
        const std::filesystem::path& projectPath,
        Dataset& dataset)
    {
        std::string storedSource;
        std::size_t inputCount{};
        std::size_t targetCount{};
        bool hasHeader{};

        if (!ParseDatasetPayload(
                payload,
                storedSource,
                inputCount,
                targetCount,
                hasHeader))
        {
            return false;
        }

        const std::filesystem::path resolvedSource =
            ResolvedDatasetPath(storedSource, projectPath);

        if (!MiaIA::Engine::CsvDatasetImporter::Import(
                dataset,
                resolvedSource.string(),
                inputCount,
                targetCount,
                hasHeader))
        {
            dataset = Dataset{};
            dataset.Name = resolvedSource.stem().string();
            dataset.Source = resolvedSource.string();
            dataset.InputCount = inputCount;
            dataset.TargetCount = targetCount;
            dataset.HasHeader = hasHeader;
        }

        return true;
    }

    bool ReadArchiveVersion(
        const std::filesystem::path& projectPath,
        std::uint32_t& version)
    {
        std::ifstream input(projectPath, std::ios::binary);
        std::array<char, ArchiveMagic.size()> magic{};
        std::uint64_t storedVersion{};

        if (!input)
        {
            return false;
        }

        input.read(magic.data(), magic.size());
        if (!input || magic != ArchiveMagic ||
            !ReadUnsigned(input, storedVersion, sizeof(std::uint32_t)) ||
            storedVersion > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        version = static_cast<std::uint32_t>(storedVersion);
        return true;
    }

    bool PrepareProject(
        const ProjectArchiveView& project,
        std::vector<PreparedContext>& prepared,
        ScopedTemporaryFiles& temporaryFiles,
        std::uint64_t& sectionCount)
    {
        if (project.Contexts.empty() ||
            project.Contexts.size() > MaximumContextCount ||
            project.ActiveContextId == 0 || project.NextContextId == 0)
        {
            return false;
        }

        std::unordered_set<std::uint64_t> contextIdentifiers;
        bool activeFound{};
        sectionCount = 1;
        prepared.reserve(project.Contexts.size());

        for (const ProjectArchiveContextView& view : project.Contexts)
        {
            if (view.Id == 0 || view.Id >= project.NextContextId ||
                !contextIdentifiers.insert(view.Id).second ||
                view.Name == nullptr || !HasText(*view.Name) ||
                view.Name->size() > MaximumContextNameLength ||
                view.Network == nullptr || view.Dataset == nullptr ||
                view.TrainingSession == nullptr || view.Checkpoints == nullptr)
            {
                return false;
            }

            PreparedContext model;
            model.View = &view;
            model.HasNetwork = !view.Network->Layers.empty() ||
                !view.Network->Connections.empty();
            model.HasDataset = !view.Dataset->Source.empty();
            model.HasTraining = HasTrainingConfiguration(
                *view.TrainingSession);

            if (model.HasDataset &&
                (view.Dataset->InputCount == 0 ||
                    view.Dataset->TargetCount == 0))
            {
                return false;
            }

            std::error_code error;
            if (model.HasNetwork)
            {
                model.OnnxPath = UniqueTemporaryPath(
                    std::filesystem::temp_directory_path(error),
                    "miaia-project-model",
                    ".onnx");
                if (error || model.OnnxPath.empty() ||
                    !MiaIA::Engine::OnnxExporter::Export(
                        *view.Network,
                        model.OnnxPath.string()))
                {
                    return false;
                }
                temporaryFiles.Add(model.OnnxPath);
            }

            const auto checkpoints = view.Checkpoints->ArchiveEntries();
            if (checkpoints.size() > MaximumCheckpointCount)
            {
                return false;
            }

            std::unordered_set<std::uint64_t> checkpointIdentifiers;
            for (const ModelCheckpointArchiveEntryView& checkpoint :
                checkpoints)
            {
                if (checkpoint.Id == 0 ||
                    checkpoint.Id >= view.Checkpoints->NextIdentifier() ||
                    !checkpointIdentifiers.insert(checkpoint.Id).second ||
                    checkpoint.Name == nullptr ||
                    !HasText(*checkpoint.Name) ||
                    checkpoint.Name->size() > MaximumStringSize ||
                    checkpoint.Network == nullptr)
                {
                    return false;
                }

                PreparedCheckpoint preparedCheckpoint;
                preparedCheckpoint.View = checkpoint;
                preparedCheckpoint.OnnxPath = UniqueTemporaryPath(
                    std::filesystem::temp_directory_path(error),
                    "miaia-project-checkpoint",
                    ".onnx");
                if (error || preparedCheckpoint.OnnxPath.empty() ||
                    !MiaIA::Engine::OnnxExporter::Export(
                        *checkpoint.Network,
                        preparedCheckpoint.OnnxPath.string()))
                {
                    return false;
                }
                temporaryFiles.Add(preparedCheckpoint.OnnxPath);
                model.Checkpoints.push_back(std::move(preparedCheckpoint));
            }

            const std::uint64_t contextSections = 2 +
                (model.HasNetwork ? 1ull : 0ull) +
                (model.HasDataset ? 1ull : 0ull) +
                (model.HasTraining ? 1ull : 0ull) +
                static_cast<std::uint64_t>(model.Checkpoints.size()) * 2ull;
            if (contextSections > MaximumSectionCount - sectionCount)
            {
                return false;
            }
            sectionCount += contextSections;
            activeFound = activeFound || view.Id == project.ActiveContextId;
            prepared.push_back(std::move(model));
        }

        return activeFound && sectionCount <= MaximumSectionCount;
    }

    bool LoadVersion2(
        const std::string& path,
        ProjectArchiveState& project,
        ProjectInfoSnapshot& result)
    {
        const std::filesystem::path projectPath =
            NormalizedAbsolutePath(path);
        std::ifstream input(projectPath, std::ios::binary);
        std::array<char, ArchiveMagic.size()> magic{};
        std::uint64_t version{};
        std::uint64_t sectionCount{};

        if (!input)
        {
            return false;
        }

        input.read(magic.data(), magic.size());
        if (!input || magic != ArchiveMagic ||
            !ReadUnsigned(input, version, sizeof(std::uint32_t)) ||
            version != Version2 ||
            !ReadUnsigned(input, sectionCount, sizeof(std::uint32_t)) ||
            sectionCount == 0 || sectionCount > MaximumSectionCount)
        {
            return false;
        }

        std::vector<std::uint8_t> payload;
        if (!ReadExpectedPayloadSection(input, ProjectSection, payload))
        {
            return false;
        }

        ProjectArchiveState imported;
        std::uint64_t contextCount{};
        if (!ParseProjectPayload(
                payload,
                imported.ActiveContextId,
                imported.NextContextId,
                contextCount))
        {
            return false;
        }

        std::uint64_t consumedSections = 1;
        std::unordered_set<std::uint64_t> contextIdentifiers;
        imported.Contexts.reserve(static_cast<std::size_t>(contextCount));

        for (std::uint64_t contextIndex = 0;
            contextIndex < contextCount;
            ++contextIndex)
        {
            if (!ReadExpectedPayloadSection(input, ContextStateSection, payload))
            {
                return false;
            }
            ++consumedSections;

            ProjectArchiveContextState model;
            std::uint64_t nextCheckpointId{};
            std::uint64_t checkpointCount{};
            std::uint8_t flags{};
            if (!ParseContextStatePayload(
                    payload,
                    model.Id,
                    model.Name,
                    nextCheckpointId,
                    flags,
                    checkpointCount) ||
                model.Id >= imported.NextContextId ||
                !contextIdentifiers.insert(model.Id).second)
            {
                return false;
            }

            if ((flags & ContextHasNetwork) != 0)
            {
                if (!ImportNetworkSection(
                        input,
                        "miaia-project-model",
                        model.Network))
                {
                    return false;
                }
                ++consumedSections;
            }

            if ((flags & ContextHasDataset) != 0)
            {
                if (!ReadExpectedPayloadSection(
                        input,
                        DatasetSection,
                        payload) ||
                    !RestoreDataset(payload, projectPath, model.Dataset))
                {
                    return false;
                }
                ++consumedSections;
            }

            if ((flags & ContextHasTraining) != 0)
            {
                ProjectInfoSnapshot ignoredInfo;
                if (!ReadExpectedPayloadSection(
                        input,
                        TrainingSection,
                        payload) ||
                    !ParseTrainingPayload(
                        payload,
                        model.TrainingSession,
                        ignoredInfo))
                {
                    return false;
                }
                ++consumedSections;
            }

            if (!ReadExpectedPayloadSection(
                    input,
                    BreakpointSection,
                    payload) ||
                !ParseBreakpointPayload(payload, model.TrainingSession))
            {
                return false;
            }
            ++consumedSections;

            std::vector<ModelCheckpointArchiveEntry> checkpoints;
            checkpoints.reserve(static_cast<std::size_t>(checkpointCount));
            std::unordered_set<std::uint64_t> checkpointIdentifiers;
            for (std::uint64_t checkpointIndex = 0;
                checkpointIndex < checkpointCount;
                ++checkpointIndex)
            {
                ModelCheckpointArchiveEntry checkpoint;
                if (!ReadExpectedPayloadSection(
                        input,
                        CheckpointSection,
                        payload) ||
                    !ParseCheckpointPayload(
                        payload,
                        checkpoint.Id,
                        checkpoint.Name) ||
                    checkpoint.Id >= nextCheckpointId ||
                    !checkpointIdentifiers.insert(checkpoint.Id).second ||
                    !ImportNetworkSection(
                        input,
                        "miaia-project-checkpoint",
                        checkpoint.Network))
                {
                    return false;
                }
                consumedSections += 2;
                checkpoints.push_back(std::move(checkpoint));
            }

            if (!model.Checkpoints.ReplaceArchiveEntries(
                    std::move(checkpoints),
                    nextCheckpointId))
            {
                return false;
            }

            model.TrainingSession.SampleCount = model.Dataset.Samples.size();
            imported.Contexts.push_back(std::move(model));
        }

        if (consumedSections != sectionCount ||
            input.peek() != std::char_traits<char>::eof())
        {
            return false;
        }

        const auto active = std::find_if(
            imported.Contexts.begin(),
            imported.Contexts.end(),
            [&imported](const ProjectArchiveContextState& model)
            {
                return model.Id == imported.ActiveContextId;
            });
        if (active == imported.Contexts.end())
        {
            return false;
        }

        ProjectInfoSnapshot info;
        FillSavedInfo(
            projectPath,
            Version2,
            imported.Contexts.size(),
            active->Id,
            active->Name,
            !active->Network.Layers.empty() ||
                !active->Network.Connections.empty(),
            active->Dataset,
            active->TrainingSession,
            active->Checkpoints.List().size(),
            info);
        project = std::move(imported);
        result = std::move(info);
        return true;
    }
}

bool MiaIA::Engine::ProjectArchive::Save(
    const ProjectArchiveView& project,
    const std::string& path,
    Core::ProjectInfoSnapshot& result)
{
    if (path.empty())
    {
        return false;
    }

    const std::filesystem::path projectPath = NormalizedAbsolutePath(path);
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

    ScopedTemporaryFiles exportedFiles;
    std::vector<PreparedContext> models;
    std::uint64_t sectionCount{};
    if (!PrepareProject(project, models, exportedFiles, sectionCount) ||
        sectionCount > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

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
    if (!WriteUnsigned(output, Version2, sizeof(std::uint32_t)) ||
        !WriteUnsigned(output, sectionCount, sizeof(std::uint32_t)) ||
        !WritePayloadSection(output, ProjectSection, BuildProjectPayload(project)))
    {
        return false;
    }

    for (const PreparedContext& model : models)
    {
        if (!WritePayloadSection(
                output,
                ContextStateSection,
                BuildContextStatePayload(model)) ||
            (model.HasNetwork && !WriteFileSection(
                output,
                ModelSection,
                model.OnnxPath)) ||
            (model.HasDataset && !WritePayloadSection(
                output,
                DatasetSection,
                BuildDatasetPayload(*model.View->Dataset, projectPath))) ||
            (model.HasTraining && !WritePayloadSection(
                output,
                TrainingSection,
                BuildTrainingPayload(*model.View->TrainingSession))) ||
            !WritePayloadSection(
                output,
                BreakpointSection,
                BuildBreakpointPayload(*model.View->TrainingSession)))
        {
            return false;
        }

        for (const PreparedCheckpoint& checkpoint : model.Checkpoints)
        {
            if (!WritePayloadSection(
                    output,
                    CheckpointSection,
                    BuildCheckpointPayload(checkpoint.View)) ||
                !WriteFileSection(
                    output,
                    ModelSection,
                    checkpoint.OnnxPath))
            {
                return false;
            }
        }
    }

    output.flush();
    output.close();
    if (!output || !ReplaceAtomically(temporaryPath, projectPath))
    {
        return false;
    }
    temporaryFile.Release();

    const auto active = std::find_if(
        project.Contexts.begin(),
        project.Contexts.end(),
        [&project](const ProjectArchiveContextView& model)
        {
            return model.Id == project.ActiveContextId;
        });
    FillSavedInfo(
        projectPath,
        Version2,
        project.Contexts.size(),
        active->Id,
        *active->Name,
        !active->Network->Layers.empty() ||
            !active->Network->Connections.empty(),
        *active->Dataset,
        *active->TrainingSession,
        active->Checkpoints->List().size(),
        result);
    return true;
}

bool MiaIA::Engine::ProjectArchive::Load(
    const std::string& path,
    ProjectArchiveState& project,
    Core::ProjectInfoSnapshot& result)
{
    if (path.empty())
    {
        return false;
    }

    const std::filesystem::path projectPath = NormalizedAbsolutePath(path);
    if (!HasMaiExtension(projectPath))
    {
        return false;
    }

    std::uint32_t version{};
    if (!ReadArchiveVersion(projectPath, version))
    {
        return false;
    }

    if (version == Version2)
    {
        return LoadVersion2(path, project, result);
    }

    if (version != Version1)
    {
        return false;
    }

    Network network;
    Dataset dataset;
    TrainingSession trainingSession;
    ProjectInfoSnapshot info;
    if (!LoadVersion1(
            path,
            network,
            dataset,
            trainingSession,
            info))
    {
        return false;
    }

    if (info.HasDatasetReference && dataset.Source.empty())
    {
        dataset.Name = std::filesystem::path(
            info.DatasetSource).stem().string();
        dataset.Source = info.DatasetSource;
        dataset.InputCount = info.DatasetInputCount;
        dataset.TargetCount = info.DatasetTargetCount;
        dataset.HasHeader = info.DatasetHasHeader;
    }

    ProjectArchiveState imported;
    ProjectArchiveContextState model;
    model.Id = 1;
    model.Name = "Model 1";
    model.Network = std::move(network);
    model.Dataset = std::move(dataset);
    model.TrainingSession = std::move(trainingSession);
    imported.Contexts.push_back(std::move(model));
    imported.ActiveContextId = 1;
    imported.NextContextId = 2;
    project = std::move(imported);
    result = std::move(info);
    return true;
}
