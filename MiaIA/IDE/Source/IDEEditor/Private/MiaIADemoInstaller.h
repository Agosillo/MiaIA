#pragma once

class UBlueprint;

class FMiaIADemoInstaller
{
public:
    static UBlueprint* LoadOrCreateBlueprint();
    static void InstallInCurrentMap(UBlueprint& Blueprint);
};
