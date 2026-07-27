#pragma once

#include "CoreMinimal.h"
#include "DemocracyGameState.h"

struct FDemocracyLoadedSaveState
{
    int32 FormatVersion = 0;
    FString SaveId;
    FString StateName;
    FString Mode;
    FString Difficulty;
    FString Climate;
    FString LeaderGender;
    FString AddressTitle;
    FString CreatedAtUtc;
    FString LastPlayedAtUtc;
    FString SavePath;
    FDemocracySimulationState RuntimeState;

    FString ToSummaryText() const;
};

class FDemocracySaveGameRuntime
{
public:
    static bool LoadSinglePlayerSave(const FString& SavePath, FDemocracyLoadedSaveState& OutLoadedSave, FString& OutError);
    static bool LoadSinglePlayerSaveWithFallback(const FString& SavePath, FDemocracyLoadedSaveState& OutLoadedSave, FString& OutError);
    static bool SaveSinglePlayerRuntimeState(FDemocracyLoadedSaveState& LoadedSave, FString& OutError);
    static bool SaveSinglePlayerAutosave(FDemocracyLoadedSaveState& LoadedSave, FString& OutError);
    static bool GetProtectedReloadSavePath(const FString& SavePath, FString& OutReloadPath, FString& OutError);
    static bool RunAutosaveRecoverySelfTest(FString& OutReport, FString& OutError);
};

