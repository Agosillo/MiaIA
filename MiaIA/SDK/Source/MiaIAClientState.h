#pragma once

namespace MiaIA::Core
{
    struct Dataset;
    struct Network;
}

namespace MiaIA::SDK::Detail
{
    Core::Dataset& ClientDataset();
    Core::Network& ClientNetwork();
}
