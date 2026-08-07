#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintEditorLibrary.h"
#include "Containers/Ticker.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MiaIABlueprintLibrary.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NeuronActor.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    constexpr const TCHAR* DemoAssetPath =
        TEXT("/Game/MiaIA/Demo/BP_MiaIADemo.BP_MiaIADemo");
    constexpr const TCHAR* DemoPackageName =
        TEXT("/Game/MiaIA/Demo/BP_MiaIADemo");
    constexpr const TCHAR* DemoMapPackageName =
        TEXT("/Game/Maps/MiaIAMain");
    constexpr const TCHAR* DemoActorLabel =
        TEXT("MiaIA Blueprint Demo");

    UEdGraphPin* FindPinChecked(
        UEdGraphNode& node,
        const FName pinName)
    {
        return node.FindPinChecked(pinName);
    }

    UK2Node_CallFunction* AddFunctionNode(
        UEdGraph& graph,
        UFunction& function,
        const int32 x,
        const int32 y,
        const TCHAR* comment)
    {
        auto* node = FEdGraphSchemaAction_K2NewNode::
            SpawnNode<UK2Node_CallFunction>(
                &graph,
                FVector2D(x, y),
                EK2NewNodeFlags::None,
                [&function](UK2Node_CallFunction* newNode)
                {
                    newNode->SetFromFunction(&function);
                });

        node->NodeComment = comment;
        node->bCommentBubbleVisible = true;
        return node;
    }

    UFunction& FindFunctionChecked(
        UClass& owner,
        const FName functionName)
    {
        UFunction* function = owner.FindFunctionByName(functionName);
        check(function);
        return *function;
    }

    void SetDefaultValue(
        UEdGraphNode& node,
        const FName pinName,
        const FString& value)
    {
        UEdGraphPin* pin = FindPinChecked(node, pinName);
        pin->DefaultValue = value;
    }

    void Connect(
        const UEdGraphSchema& schema,
        UEdGraphPin& from,
        UEdGraphPin& to)
    {
        check(schema.TryCreateConnection(&from, &to));
    }

    void ConnectExecution(
        const UEdGraphSchema& schema,
        UEdGraphNode& from,
        UEdGraphNode& to)
    {
        Connect(
            schema,
            *FindPinChecked(from, UEdGraphSchema_K2::PN_Then),
            *FindPinChecked(to, UEdGraphSchema_K2::PN_Execute));
    }

    UK2Node_CallFunction* AddAdvanceNode(
        UEdGraph& graph,
        const int32 x,
        const TCHAR* comment)
    {
        return AddFunctionNode(
            graph,
            FindFunctionChecked(
                *UMiaIABlueprintLibrary::StaticClass(),
                GET_FUNCTION_NAME_CHECKED(
                    UMiaIABlueprintLibrary,
                    AdvanceDebugPhase)),
            x,
            0,
            comment);
    }

    UK2Node_CallFunction* AddDelayNode(
        UEdGraph& graph,
        const int32 x)
    {
        auto* node = AddFunctionNode(
            graph,
            FindFunctionChecked(
                *UKismetSystemLibrary::StaticClass(),
                GET_FUNCTION_NAME_CHECKED(
                    UKismetSystemLibrary,
                    Delay)),
            x,
            0,
            TEXT("Pause so the inspected phase remains observable."));
        SetDefaultValue(*node, TEXT("Duration"), TEXT("0.75"));
        return node;
    }

    UBlueprint* CreateDemoBlueprint()
    {
        UPackage* package = CreatePackage(DemoPackageName);

        if (!package)
        {
            return nullptr;
        }

        UBlueprint* blueprint = FKismetEditorUtilities::CreateBlueprint(
            AActor::StaticClass(),
            package,
            TEXT("BP_MiaIADemo"),
            BPTYPE_Normal,
            UBlueprint::StaticClass(),
            UBlueprintGeneratedClass::StaticClass());

        if (!blueprint)
        {
            return nullptr;
        }

        FAssetRegistryModule::AssetCreated(blueprint);

        UEdGraph* graph = FBlueprintEditorUtils::FindEventGraph(blueprint);

        if (!graph)
        {
            return nullptr;
        }

        UK2Node_Event* beginPlay = UBlueprintEditorLibrary::AddEventOverride(
            blueprint,
            TEXT("ReceiveBeginPlay"),
            FIntPoint(0, 0));

        if (!beginPlay)
        {
            return nullptr;
        }

        beginPlay->NodeComment =
            TEXT("Temporary MiaIA Blueprint integration demo.");
        beginPlay->bCommentBubbleVisible = true;

        const UEdGraphSchema& schema = *graph->GetSchema();

        auto* projectDirectory = AddFunctionNode(
            *graph,
            FindFunctionChecked(
                *UKismetSystemLibrary::StaticClass(),
                GET_FUNCTION_NAME_CHECKED(
                    UKismetSystemLibrary,
                    GetProjectDirectory)),
            300,
            -250,
            TEXT("Resolve the demo data without an absolute path."));

        auto* appendPath = AddFunctionNode(
            *graph,
            FindFunctionChecked(
                *UKismetStringLibrary::StaticClass(),
                GET_FUNCTION_NAME_CHECKED(
                    UKismetStringLibrary,
                    Concat_StrStr)),
            600,
            -250,
            TEXT("Build DemoData/and.csv outside Unreal Content."));
        SetDefaultValue(
            *appendPath,
            TEXT("B"),
            TEXT("DemoData/and.csv"));

        Connect(
            schema,
            *FindPinChecked(*projectDirectory, UEdGraphSchema_K2::PN_ReturnValue),
            *FindPinChecked(*appendPath, TEXT("A")));

        auto* createNetwork = AddFunctionNode(
            *graph,
            FindFunctionChecked(
                *UMiaIABlueprintLibrary::StaticClass(),
                GET_FUNCTION_NAME_CHECKED(
                    UMiaIABlueprintLibrary,
                    CreateDenseNetwork)),
            300,
            0,
            TEXT("Create a 2-2-1-1 dense network."));
        SetDefaultValue(*createNetwork, TEXT("InputCount"), TEXT("2"));
        SetDefaultValue(*createNetwork, TEXT("HiddenCount"), TEXT("2"));
        SetDefaultValue(*createNetwork, TEXT("HiddenLayers"), TEXT("1"));
        SetDefaultValue(*createNetwork, TEXT("OutputCount"), TEXT("1"));

        auto* importDataset = AddFunctionNode(
            *graph,
            FindFunctionChecked(
                *UMiaIABlueprintLibrary::StaticClass(),
                GET_FUNCTION_NAME_CHECKED(
                    UMiaIABlueprintLibrary,
                    ImportCsvDataset)),
            650,
            0,
            TEXT("Import the bundled AND dataset."));
        SetDefaultValue(*importDataset, TEXT("InputCount"), TEXT("2"));
        SetDefaultValue(*importDataset, TEXT("TargetCount"), TEXT("1"));
        SetDefaultValue(*importDataset, TEXT("bHasHeader"), TEXT("true"));

        Connect(
            schema,
            *FindPinChecked(*appendPath, UEdGraphSchema_K2::PN_ReturnValue),
            *FindPinChecked(*importDataset, TEXT("Path")));

        auto* startSession = AddFunctionNode(
            *graph,
            FindFunctionChecked(
                *UMiaIABlueprintLibrary::StaticClass(),
                GET_FUNCTION_NAME_CHECKED(
                    UMiaIABlueprintLibrary,
                    StartTrainingSession)),
            1000,
            0,
            TEXT("Create a controlled one-epoch training session."));
        SetDefaultValue(*startSession, TEXT("EpochCount"), TEXT("1"));
        SetDefaultValue(*startSession, TEXT("LearningRate"), TEXT("0.25"));

        auto* startDebug = AddFunctionNode(
            *graph,
            FindFunctionChecked(
                *UMiaIABlueprintLibrary::StaticClass(),
                GET_FUNCTION_NAME_CHECKED(
                    UMiaIABlueprintLibrary,
                    StartSessionDebug)),
            1350,
            0,
            TEXT("Attach phase debugging to the next training step."));

        ConnectExecution(schema, *beginPlay, *createNetwork);
        ConnectExecution(schema, *createNetwork, *importDataset);
        ConnectExecution(schema, *importDataset, *startSession);
        ConnectExecution(schema, *startSession, *startDebug);

        TArray<UK2Node_CallFunction*> delayNodes;
        TArray<UK2Node_CallFunction*> advanceNodes;

        for (int32 index = 0; index < 5; ++index)
        {
            delayNodes.Add(AddDelayNode(
                *graph,
                1700 + index * 650));
        }

        advanceNodes.Add(AddAdvanceNode(
            *graph,
            2000,
            TEXT("Forward pass complete.")));
        advanceNodes.Add(AddAdvanceNode(
            *graph,
            2650,
            TEXT("Backward pass complete.")));
        advanceNodes.Add(AddAdvanceNode(
            *graph,
            3300,
            TEXT("Candidate update complete.")));
        advanceNodes.Add(AddAdvanceNode(
            *graph,
            3950,
            TEXT("Candidate update verified.")));
        advanceNodes.Add(AddAdvanceNode(
            *graph,
            4600,
            TEXT("Commit the inspected training step.")));

        ConnectExecution(schema, *startDebug, *delayNodes[0]);

        for (int32 index = 0; index < advanceNodes.Num(); ++index)
        {
            ConnectExecution(
                schema,
                *delayNodes[index],
                *advanceNodes[index]);

            if (index + 1 < delayNodes.Num())
            {
                ConnectExecution(
                    schema,
                    *advanceNodes[index],
                    *delayNodes[index + 1]);
            }
        }

        auto* branch = FEdGraphSchemaAction_K2NewNode::
            SpawnNode<UK2Node_IfThenElse>(
                graph,
                FVector2D(4950, 0),
                EK2NewNodeFlags::None);
        branch->NodeComment =
            TEXT("Report whether the final commit phase succeeded.");
        branch->bCommentBubbleVisible = true;

        ConnectExecution(schema, *advanceNodes.Last(), *branch);
        Connect(
            schema,
            *FindPinChecked(
                *advanceNodes.Last(),
                UEdGraphSchema_K2::PN_ReturnValue),
            *FindPinChecked(*branch, UEdGraphSchema_K2::PN_Condition));

        UFunction& printFunction = FindFunctionChecked(
            *UKismetSystemLibrary::StaticClass(),
            GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
        auto* printSuccess = AddFunctionNode(
            *graph,
            printFunction,
            5300,
            -100,
            TEXT("Visible confirmation of the Blueprint path."));
        SetDefaultValue(
            *printSuccess,
            TEXT("InString"),
            TEXT("MiaIA Blueprint demo: training debug step committed."));
        SetDefaultValue(*printSuccess, TEXT("Duration"), TEXT("8.0"));
        SetDefaultValue(
            *printSuccess,
            TEXT("TextColor"),
            TEXT("(R=0.0,G=1.0,B=0.15,A=1.0)"));

        auto* printFailure = AddFunctionNode(
            *graph,
            printFunction,
            5300,
            250,
            TEXT("A failed result remains visible for diagnosis."));
        SetDefaultValue(
            *printFailure,
            TEXT("InString"),
            TEXT("MiaIA Blueprint demo failed. Check the Output Log."));
        SetDefaultValue(*printFailure, TEXT("Duration"), TEXT("8.0"));
        SetDefaultValue(
            *printFailure,
            TEXT("TextColor"),
            TEXT("(R=1.0,G=0.05,B=0.05,A=1.0)"));

        Connect(
            schema,
            *FindPinChecked(*branch, UEdGraphSchema_K2::PN_Then),
            *FindPinChecked(*printSuccess, UEdGraphSchema_K2::PN_Execute));
        Connect(
            schema,
            *FindPinChecked(*branch, UEdGraphSchema_K2::PN_Else),
            *FindPinChecked(*printFailure, UEdGraphSchema_K2::PN_Execute));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(blueprint);
        FKismetEditorUtilities::CompileBlueprint(blueprint);

        if (blueprint->Status == BS_Error)
        {
            return nullptr;
        }

        package->SetDirtyFlag(true);
        const FString filename = FPackageName::LongPackageNameToFilename(
            DemoPackageName,
            FPackageName::GetAssetPackageExtension());
        FSavePackageArgs saveArgs;
        saveArgs.TopLevelFlags = RF_Public | RF_Standalone;

        if (!UPackage::SavePackage(
            package,
            blueprint,
            *filename,
            saveArgs))
        {
            return nullptr;
        }

        return blueprint;
    }

    UBlueprint* LoadOrCreateDemoBlueprint()
    {
        if (auto* existing = LoadObject<UBlueprint>(nullptr, DemoAssetPath))
        {
            return existing;
        }

        return CreateDemoBlueprint();
    }

    bool IsDemoActor(const AActor& actor, const UClass& demoClass)
    {
        return actor.IsA(&demoClass) ||
            actor.GetActorLabel() == DemoActorLabel;
    }

    void InstallDemoInCurrentMap(UBlueprint& blueprint)
    {
        if (!GEditor || !blueprint.GeneratedClass)
        {
            return;
        }

        UWorld* world = GEditor->GetEditorWorldContext().World();

        if (!world ||
            world->GetPackage()->GetName() != DemoMapPackageName)
        {
            return;
        }

        bool hasDemoActor = false;
        TArray<AActor*> oldDemoActors;

        for (AActor* actor : world->PersistentLevel->Actors)
        {
            if (!actor)
            {
                continue;
            }

            if (IsDemoActor(
                *actor,
                *blueprint.GeneratedClass.Get()))
            {
                hasDemoActor = true;
            }
            else if (actor->IsA<ANeuronActor>())
            {
                oldDemoActors.Add(actor);
            }
        }

        for (AActor* actor : oldDemoActors)
        {
            world->EditorDestroyActor(actor, true);
        }

        if (!hasDemoActor)
        {
            AActor* demoActor = GEditor->AddActor(
                world->GetCurrentLevel(),
                blueprint.GeneratedClass,
                FTransform(FVector(0.0, 0.0, 100.0)),
                false,
                RF_Transactional,
                false);

            if (demoActor)
            {
                demoActor->SetActorLabel(DemoActorLabel);
            }
        }

        if (oldDemoActors.Num() > 0 || !hasDemoActor)
        {
            const FString mapFilename =
                FPackageName::LongPackageNameToFilename(
                    DemoMapPackageName,
                    FPackageName::GetMapPackageExtension());
            FEditorFileUtils::SaveMap(world, mapFilename);
            FEditorFileUtils::SaveMapDataPackages(world, false);
        }
    }
}

class FIDEEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (IsRunningCommandlet())
        {
            return;
        }

        FEditorDelegates::OnMapOpened.AddRaw(
            this,
            &FIDEEditorModule::OnMapOpened);

        auto& assetRegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
                TEXT("AssetRegistry"));
        auto& assetRegistry = assetRegistryModule.Get();

        if (assetRegistry.IsLoadingAssets())
        {
            assetRegistry.OnFilesLoaded().AddRaw(
                this,
                &FIDEEditorModule::OnAssetRegistryFilesLoaded);
        }
        else
        {
            ScheduleInitialization();
        }
    }

    virtual void ShutdownModule() override
    {
        FEditorDelegates::OnMapOpened.RemoveAll(this);

        if (auto* assetRegistryModule =
            FModuleManager::GetModulePtr<FAssetRegistryModule>(
                TEXT("AssetRegistry")))
        {
            assetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
        }

        if (InitializationTickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(
                InitializationTickerHandle);
            InitializationTickerHandle.Reset();
        }

        DemoBlueprint.Reset();
    }

private:
    void OnAssetRegistryFilesLoaded()
    {
        auto& assetRegistry =
            FModuleManager::GetModuleChecked<FAssetRegistryModule>(
                TEXT("AssetRegistry")).Get();
        assetRegistry.OnFilesLoaded().RemoveAll(this);
        ScheduleInitialization();
    }

    void ScheduleInitialization()
    {
        if (InitializationTickerHandle.IsValid())
        {
            return;
        }

        InitializationTickerHandle =
            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateRaw(
                    this,
                    &FIDEEditorModule::InitializeDemo));
    }

    bool InitializeDemo(float)
    {
        InitializationTickerHandle.Reset();
        bAssetRegistryReady = true;
        DemoBlueprint = LoadOrCreateDemoBlueprint();

        if (DemoBlueprint.IsValid())
        {
            InstallDemoInCurrentMap(*DemoBlueprint.Get());
        }

        return false;
    }

    void OnMapOpened(const FString&, const bool)
    {
        if (!bAssetRegistryReady)
        {
            return;
        }

        if (!DemoBlueprint.IsValid())
        {
            DemoBlueprint = LoadOrCreateDemoBlueprint();
        }

        if (DemoBlueprint.IsValid())
        {
            InstallDemoInCurrentMap(*DemoBlueprint.Get());
        }
    }

    bool bAssetRegistryReady{};
    FTSTicker::FDelegateHandle InitializationTickerHandle;
    TWeakObjectPtr<UBlueprint> DemoBlueprint;
};

IMPLEMENT_MODULE(FIDEEditorModule, IDEEditor)
