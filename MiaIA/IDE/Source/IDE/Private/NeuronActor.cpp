// Fill out your copyright notice in the Description page of Project Settings.


#include "NeuronActor.h"
#include "../../SDK/Include/MiaIAClient.h"

// Sets default values
ANeuronActor::ANeuronActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ANeuronActor::BeginPlay()
{
	Super::BeginPlay();
    auto Snapshot = MiaIA::SDK::MiaIAClient::GetSnapshot();

    if (!Snapshot.Layers.empty() && !Snapshot.Layers[0].Neurons.empty())
    {
        for (const auto& Layer : Snapshot.Layers)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Layer: %s"),
                *FString(Layer.Name.c_str()));

            for (const auto& Neuron : Layer.Neurons)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("Neuron %llu Bias %.2f Activation %.2f"),
                    Neuron.Id,
                    Neuron.Bias,
                    Neuron.Activation);
            }
        }

        for (const auto& Connection : Snapshot.Connections)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Connection: %llu -> %llu Weight: %.2f"),
                Connection.FromNeuron,
                Connection.ToNeuron,
                Connection.Weight);
        }
    }
}

// Called every frame
void ANeuronActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

