#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "../../Engine/Interchange/OnnxImporter.h"
#include "../../Engine/Interchange/OnnxExporter.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::ImportOnnx(const std::string& path)
    {
        const std::scoped_lock lock(Detail::ClientMutex());

        if (Detail::IsTrainingSessionRunning())
        {
            return false;
        }

        return Engine::OnnxImporter::Import(
            Detail::ClientNetwork(),
            path);
    }

    bool MiaIAClient::ExportOnnx(const std::string& path)
    {
        const std::scoped_lock lock(Detail::ClientMutex());
        return Engine::OnnxExporter::Export(
            Detail::ClientNetwork(),
            path);
    }
}
