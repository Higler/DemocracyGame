#pragma once

#include "CoreMinimal.h"

struct FDifficultyResourceAmounts
{
    int32 Food = 0;
    int32 GasOil = 0;
    int32 Wood = 0;
    int32 Metals = 0;
    int32 Water = 0;
};

struct FDifficultyProfile
{
    FString Name = TEXT("Normal");
    FString CountrySize = TEXT("Medium");
    int32 CountrySizeScore = 2;
    FString PlayerHelpLevel = TEXT("Standard");
    FString AiSupportLevel = TEXT("Moderate");
    int32 AdvisorCount = 4;
    FString TipFrequency = TEXT("Standard");
    TArray<FString> StartingResourceTypes;
    FDifficultyResourceAmounts StartingResources;
    int32 StartingApproval = 60;
    int32 StartingTreasury = 1000;
    FString StartingCrisisPressure = TEXT("Moderate");

    FString ToSummaryText() const;
    FString ToJson(int32 IndentSpaces = 2) const;
};

class FDifficultyProfileLibrary
{
public:
    static FDifficultyProfile GetProfile(const FString& DifficultyName);
    static TArray<FString> GetDifficultyNames();

private:
    static FDifficultyProfile BuildDefaultProfile(const FString& DifficultyName);
    static void ApplyConfigOverrides(FDifficultyProfile& Profile);
};
