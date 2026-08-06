#include "../Include/MiaIAClient.h"
#include "MiaIAClientState.h"
#include "../../Engine/Interchange/OnnxExporter.h"

namespace MiaIA::SDK
{
    bool MiaIAClient::ExportOnnx(const std::string& path)
    {
        return Engine::OnnxExporter::Export(
            Detail::ClientNetwork(),
            path);
    }
}
