#include "../Core/Execution/SnapshotBuilder.h"
#include "../SDK/Include/MiaIAClient.h"

#include <cassert>
#include <iostream>

int main()
{
    const MiaIA::Core::NetworkSnapshot snapshot =
        MiaIA::SDK::MiaIAClient::CreateDemoSnapshot();

    assert(snapshot.Layers.size() == 1);
    assert(snapshot.Layers[0].Id == 1);
    assert(snapshot.Layers[0].Neurons.size() == 1);
    assert(snapshot.Layers[0].Neurons[0].Id == 1001);
    assert(snapshot.Layers[0].Neurons[0].Activation == 0.75);

    std::cout << "SDK snapshot test passed\n";

    return 0;
}