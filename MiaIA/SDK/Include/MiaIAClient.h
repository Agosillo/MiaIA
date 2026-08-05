#pragma once

#include "../../Core/Public/NetworkSnapshot.h"

namespace MiaIA::SDK
{
    class MiaIAClient
    {
    public:
        [[nodiscard]]
        static Core::NetworkSnapshot GetSnapshot();
        static int TestConnection();
    };
}
