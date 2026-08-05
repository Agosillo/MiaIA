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
	/*
	auto Snapshot = MiaIA::SDK::MiaIAClient::CreateDemoSnapshot();

	UE_LOG(LogTemp, Warning,
		TEXT("MiaIA Neuron ID: %d"),
		Snapshot.Layers[0].Neurons[0].Id
	);
	*/
	int id = MiaIA::SDK::MiaIAClient::TestConnection();

	UE_LOG(LogTemp, Warning, TEXT("MiaIA Neuron ID: %d"), id);
}

// Called every frame
void ANeuronActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

