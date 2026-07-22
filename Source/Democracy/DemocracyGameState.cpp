#include "DemocracyGameState.h"

namespace
{
    FString JsonEscape(FString Text)
    {
        Text.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Text.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Text.ReplaceInline(TEXT("\r"), TEXT("\\r"));
        Text.ReplaceInline(TEXT("\n"), TEXT("\\n"));
        return Text;
    }

    FString Indent(int32 Count)
    {
        return FString::ChrN(Count, TEXT(' '));
    }

    FString StringArrayToJson(const TArray<FString>& Values)
    {
        FString Output = TEXT("[");
        for (int32 Index = 0; Index < Values.Num(); ++Index)
        {
            if (Index > 0)
            {
                Output += TEXT(", ");
            }
            Output += FString::Printf(TEXT("\"%s\""), *JsonEscape(Values[Index]));
        }
        Output += TEXT("]");
        return Output;
    }

    TArray<FString> GetAdvisorWarnings(const FDifficultyProfile& DifficultyProfile)
    {
        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("High"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Advisor warning: Stability below the warning line can trigger organized unrest."),
                TEXT("Advisor warning: Rising unrest increases assassination risk if ignored."),
                TEXT("Advisor warning: Use policy, resource relief, and public approval actions before the crisis becomes critical.")
            };
        }

        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("Standard"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Advisor warning: Stability and unrest are linked to assassination risk."),
                TEXT("Advisor warning: Resolve shortages and unpopular policies before unrest becomes critical.")
            };
        }

        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Advisor warning: Public order is weakening. Risk may rise quickly.")
            };
        }

        return {
            TEXT("Minimal warning: Watch stability and unrest.")
        };
    }

    TArray<FString> GetRecoveryTips(const FDifficultyProfile& DifficultyProfile)
    {
        if (DifficultyProfile.TipFrequency.Equals(TEXT("Frequent"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Increase food and water security to reduce household pressure."),
                TEXT("Use emergency funding or tax relief to stabilize approval."),
                TEXT("Avoid aggressive military or austerity policies while unrest is high."),
                TEXT("Open advisor briefings from the office phone or computer when warnings appear.")
            };
        }

        if (DifficultyProfile.TipFrequency.Equals(TEXT("Standard"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Address shortages before unrest crosses the critical threshold."),
                TEXT("Balance treasury actions against public approval and stability.")
            };
        }

        if (DifficultyProfile.TipFrequency.Equals(TEXT("Sparse"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Find the cause of unrest before acting.")
            };
        }

        return {};
    }

    FString WarningLevelForDifficulty(const FDifficultyProfile& DifficultyProfile)
    {
        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("High"), ESearchCase::IgnoreCase))
        {
            return TEXT("Early Detailed");
        }

        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("Standard"), ESearchCase::IgnoreCase))
        {
            return TEXT("Standard");
        }

        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
        {
            return TEXT("Late Limited");
        }

        return TEXT("Minimal");
    }

    int32 InitialUnrestForDifficulty(const FDifficultyProfile& DifficultyProfile)
    {
        if (DifficultyProfile.StartingCrisisPressure.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
        {
            return 14;
        }
        if (DifficultyProfile.StartingCrisisPressure.Equals(TEXT("High"), ESearchCase::IgnoreCase))
        {
            return 34;
        }
        if (DifficultyProfile.StartingCrisisPressure.Equals(TEXT("Severe"), ESearchCase::IgnoreCase))
        {
            return 46;
        }
        return 24;
    }


    TArray<FString> GetInvasionAdvisorWarnings(const FDifficultyProfile& DifficultyProfile)
    {
        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("High"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Defense warning: Border pressure is rising before enemy forces cross critical thresholds."),
                TEXT("Defense warning: Low military readiness can invite invasion or forced regime change."),
                TEXT("Defense warning: Diplomacy, alliances, readiness spending, and resource stockpiles can lower takeover risk.")
            };
        }

        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("Standard"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Defense warning: Rival pressure and weak readiness can lead to foreign takeover."),
                TEXT("Defense warning: Improve readiness or reduce tensions before territory is lost.")
            };
        }

        if (DifficultyProfile.PlayerHelpLevel.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Defense warning: Border pressure is becoming dangerous.")
            };
        }

        return {
            TEXT("Minimal warning: Watch borders and readiness.")
        };
    }

    TArray<FString> GetInvasionRecoveryTips(const FDifficultyProfile& DifficultyProfile)
    {
        if (DifficultyProfile.TipFrequency.Equals(TEXT("Frequent"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Raise military readiness before border pressure reaches critical levels."),
                TEXT("Use diplomacy or trade concessions to reduce immediate rival pressure."),
                TEXT("Secure gas/oil and metals to sustain defense production."),
                TEXT("Ask advisors for threat briefings through the office phone or computer.")
            };
        }

        if (DifficultyProfile.TipFrequency.Equals(TEXT("Standard"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Balance military readiness, diplomacy, and resource reserves."),
                TEXT("Do not ignore repeated border-pressure warnings.")
            };
        }

        if (DifficultyProfile.TipFrequency.Equals(TEXT("Sparse"), ESearchCase::IgnoreCase))
        {
            return {
                TEXT("Strengthen readiness or de-escalate rivals.")
            };
        }

        return {};
    }

    FDemocracyInvasionRiskState BuildInvasionRiskState(const FDifficultyProfile& DifficultyProfile)
    {
        FDemocracyInvasionRiskState Risk;
        Risk.WarningLevel = WarningLevelForDifficulty(DifficultyProfile);
        Risk.MilitaryReadinessWarningThreshold = FMath::Clamp(52 - DifficultyProfile.CountrySizeScore * 3, 34, 52);
        Risk.MilitaryReadinessCriticalThreshold = FMath::Clamp(32 - DifficultyProfile.CountrySizeScore * 2, 18, 32);
        Risk.BorderPressureWarningThreshold = FMath::Clamp(38 + DifficultyProfile.CountrySizeScore * 5, 38, 62);
        Risk.BorderPressureCriticalThreshold = FMath::Clamp(62 + DifficultyProfile.CountrySizeScore * 5, 62, 86);
        Risk.TerritorialLossWarningThreshold = FMath::Clamp(1 + DifficultyProfile.CountrySizeScore / 2, 1, 3);
        Risk.TerritorialLossCriticalThreshold = FMath::Clamp(3 + DifficultyProfile.CountrySizeScore, 3, 7);
        Risk.CurrentInvasionRisk = FMath::Clamp((DifficultyProfile.CountrySizeScore - 1) * 7, 0, 28);
        Risk.InvasionRiskTrigger = FMath::Clamp(125 - DifficultyProfile.CountrySizeScore * 10, 85, 125);
        Risk.GameOverReason = TEXT("Foreign Takeover");
        Risk.bGameOverOnTakeover = true;
        Risk.ActiveInvasionCauses = {
            TEXT("Low military readiness"),
            TEXT("High rival border pressure"),
            TEXT("Loss of border territories"),
            TEXT("Resource shortages affecting defense"),
            TEXT("Failed diplomacy or hostile alliances")
        };
        Risk.AdvisorWarnings = GetInvasionAdvisorWarnings(DifficultyProfile);
        Risk.RecoveryTips = GetInvasionRecoveryTips(DifficultyProfile);
        return Risk;
    }


    int32 StartingCountryCountForDifficulty(const FDifficultyProfile& DifficultyProfile)
    {
        if (DifficultyProfile.Name.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
        {
            return 50;
        }
        if (DifficultyProfile.Name.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
        {
            return 100;
        }
        if (DifficultyProfile.Name.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
        {
            return 150;
        }
        if (DifficultyProfile.Name.Equals(TEXT("Expert"), ESearchCase::IgnoreCase))
        {
            return 200;
        }
        return FMath::Clamp(DifficultyProfile.CountrySizeScore, 1, 4) * 50;
    }

    FString PoliticalTypeForDifficulty(const FDifficultyProfile& DifficultyProfile, int32 CountryIndex)
    {
        const int32 DifficultyScore = FMath::Clamp(DifficultyProfile.CountrySizeScore, 1, 4);
        const int32 Roll = (CountryIndex * 37 + DifficultyScore * 11) % 100;

        if (DifficultyScore == 1)
        {
            if (Roll < 54) { return TEXT("Democratic Ally"); }
            if (Roll < 74) { return TEXT("Democratic Neutral"); }
            if (Roll < 89) { return TEXT("Non-Aligned Republic"); }
            if (Roll < 97) { return TEXT("Authoritarian State"); }
            return TEXT("Hostile Bloc");
        }

        if (DifficultyScore == 2)
        {
            if (Roll < 38) { return TEXT("Democratic Ally"); }
            if (Roll < 62) { return TEXT("Democratic Neutral"); }
            if (Roll < 82) { return TEXT("Non-Aligned Republic"); }
            if (Roll < 94) { return TEXT("Authoritarian State"); }
            return TEXT("Hostile Bloc");
        }

        if (DifficultyScore == 3)
        {
            if (Roll < 24) { return TEXT("Democratic Ally"); }
            if (Roll < 45) { return TEXT("Democratic Neutral"); }
            if (Roll < 69) { return TEXT("Non-Aligned Republic"); }
            if (Roll < 88) { return TEXT("Authoritarian State"); }
            return TEXT("Hostile Bloc");
        }

        if (Roll < 14) { return TEXT("Democratic Ally"); }
        if (Roll < 32) { return TEXT("Democratic Neutral"); }
        if (Roll < 58) { return TEXT("Non-Aligned Republic"); }
        if (Roll < 82) { return TEXT("Authoritarian State"); }
        return TEXT("Hostile Bloc");
    }

    FString DiplomaticAlignmentForPoliticalType(const FString& PoliticalType)
    {
        if (PoliticalType.Equals(TEXT("Democratic Ally"), ESearchCase::IgnoreCase))
        {
            return TEXT("Allied");
        }
        if (PoliticalType.Equals(TEXT("Democratic Neutral"), ESearchCase::IgnoreCase) ||
            PoliticalType.Equals(TEXT("Non-Aligned Republic"), ESearchCase::IgnoreCase))
        {
            return TEXT("Neutral");
        }
        if (PoliticalType.Equals(TEXT("Authoritarian State"), ESearchCase::IgnoreCase))
        {
            return TEXT("Tense");
        }
        return TEXT("Hostile");
    }

    FString GeneratedCountryName(int32 CountryIndex, int32 ContinentIndex)
    {
        static const TCHAR* Prefixes[] = {
            TEXT("Aster"), TEXT("Bren"), TEXT("Coro"), TEXT("Daven"), TEXT("Eld"), TEXT("Faren"), TEXT("Galen"), TEXT("Harth"),
            TEXT("Istr"), TEXT("Jor"), TEXT("Kelm"), TEXT("Lumo"), TEXT("Maren"), TEXT("Noro"), TEXT("Orin"), TEXT("Pryth")
        };
        static const TCHAR* Middles[] = {
            TEXT("vale"), TEXT("mere"), TEXT("holm"), TEXT("port"), TEXT("crest"), TEXT("ford"), TEXT("mont"), TEXT("fall"),
            TEXT("ridge"), TEXT("wick"), TEXT("haven"), TEXT("reach"), TEXT("field"), TEXT("glen"), TEXT("shore"), TEXT("mark")
        };
        static const TCHAR* Suffixes[] = {
            TEXT("Union"), TEXT("Republic"), TEXT("Commonwealth"), TEXT("Federation"), TEXT("Assembly"), TEXT("Territory"),
            TEXT("League"), TEXT("Accord")
        };

        return FString::Printf(
            TEXT("%s%s %s %03d"),
            Prefixes[(CountryIndex + ContinentIndex * 3) % UE_ARRAY_COUNT(Prefixes)],
            Middles[(CountryIndex * 5 + ContinentIndex) % UE_ARRAY_COUNT(Middles)],
            Suffixes[(CountryIndex * 7 + ContinentIndex) % UE_ARRAY_COUNT(Suffixes)],
            CountryIndex);
    }

    FDemocracyWorldMapState BuildWorldMapState(const FDifficultyProfile& DifficultyProfile, const FString& PlayerCountryName, const FString& PlayerClimate)
    {
        static const TCHAR* ContinentNames[] = {
            TEXT("Aurelian Reach"),
            TEXT("Borealis Crown"),
            TEXT("Cindervale Expanse"),
            TEXT("Driftmarch Isles"),
            TEXT("Ebon Coast"),
            TEXT("Frostmere Shelf"),
            TEXT("Gilded Steppe"),
            TEXT("Halcyon Basin")
        };

        static const TCHAR* ContinentClimates[] = {
            TEXT("Northern Cold"),
            TEXT("Northern Cold"),
            TEXT("Middle Moderate"),
            TEXT("Middle Moderate"),
            TEXT("Middle Moderate"),
            TEXT("Southern Tropical"),
            TEXT("Southern Tropical"),
            TEXT("Mixed Transitional")
        };

        FDemocracyWorldMapState WorldMap;
        WorldMap.TotalCountryCount = StartingCountryCountForDifficulty(DifficultyProfile);
        WorldMap.GenerationRule = FString::Printf(
            TEXT("Eight permanent continents. Starting countries scale by difficulty: Easy 50, Normal 100, Hard 150, Expert 200. Democratic allies become less common as difficulty increases."));

        const int32 BaseCountriesPerContinent = WorldMap.TotalCountryCount / 8;
        const int32 RemainderCountries = WorldMap.TotalCountryCount % 8;
        int32 GlobalCountryIndex = 0;

        for (int32 ContinentIndex = 0; ContinentIndex < 8; ++ContinentIndex)
        {
            FDemocracyContinentState Continent;
            Continent.ContinentName = ContinentNames[ContinentIndex];
            Continent.Climate = ContinentClimates[ContinentIndex];
            Continent.CountryCount = BaseCountriesPerContinent + (ContinentIndex < RemainderCountries ? 1 : 0);

            for (int32 LocalCountryIndex = 0; LocalCountryIndex < Continent.CountryCount; ++LocalCountryIndex)
            {
                FDemocracyGeneratedCountryState Country;
                Country.ContinentName = Continent.ContinentName;
                Country.Climate = Continent.Climate;

                if (GlobalCountryIndex == 0)
                {
                    Country.CountryName = PlayerCountryName;
                    Country.Climate = PlayerClimate;
                    Country.PoliticalType = TEXT("Player Democracy");
                    Country.DiplomaticAlignment = TEXT("Player");
                    Country.bAlliedWithPlayer = true;
                    Country.PowerScore = 45 + DifficultyProfile.CountrySizeScore * 6;
                    Country.Stability = FMath::Clamp(DifficultyProfile.StartingApproval, 25, 85);
                    Country.BorderPressure = 0;
                }
                else
                {
                    Country.CountryName = GeneratedCountryName(GlobalCountryIndex, ContinentIndex);
                    Country.PoliticalType = PoliticalTypeForDifficulty(DifficultyProfile, GlobalCountryIndex);
                    Country.DiplomaticAlignment = DiplomaticAlignmentForPoliticalType(Country.PoliticalType);
                    Country.bAlliedWithPlayer = Country.PoliticalType.Equals(TEXT("Democratic Ally"), ESearchCase::IgnoreCase);
                    Country.PowerScore = FMath::Clamp(28 + ((GlobalCountryIndex * 13 + ContinentIndex * 9) % 55) + DifficultyProfile.CountrySizeScore * 3, 20, 95);
                    Country.Stability = FMath::Clamp(35 + ((GlobalCountryIndex * 17 + ContinentIndex * 5) % 45), 20, 90);
                    Country.BorderPressure = Country.DiplomaticAlignment.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase)
                        ? FMath::Clamp(30 + DifficultyProfile.CountrySizeScore * 10 + (GlobalCountryIndex % 18), 30, 90)
                        : FMath::Clamp(5 + DifficultyProfile.CountrySizeScore * 4 + (GlobalCountryIndex % 12), 0, 45);

                    if (Country.bAlliedWithPlayer)
                    {
                        ++WorldMap.DemocraticAllyCount;
                    }
                    else if (!Country.PoliticalType.Contains(TEXT("Democratic")))
                    {
                        ++WorldMap.NonDemocraticCountryCount;
                    }
                }

                Continent.Countries.Add(Country);
                ++GlobalCountryIndex;
            }

            WorldMap.Continents.Add(Continent);
        }

        return WorldMap;
    }

    FDemocracyFailureRiskState BuildFailureRiskState(const FDifficultyProfile& DifficultyProfile)
    {
        FDemocracyFailureRiskState Risk;
        Risk.WarningLevel = WarningLevelForDifficulty(DifficultyProfile);
        Risk.StabilityWarningThreshold = FMath::Clamp(42 - DifficultyProfile.CountrySizeScore * 3, 24, 42);
        Risk.StabilityCriticalThreshold = FMath::Clamp(24 - DifficultyProfile.CountrySizeScore * 2, 12, 24);
        Risk.UnrestWarningThreshold = FMath::Clamp(48 + DifficultyProfile.CountrySizeScore * 4, 48, 68);
        Risk.UnrestCriticalThreshold = FMath::Clamp(70 + DifficultyProfile.CountrySizeScore * 3, 70, 88);
        Risk.CurrentAssassinationRisk = FMath::Clamp((DifficultyProfile.CountrySizeScore - 1) * 6, 0, 24);
        Risk.AssassinationRiskTrigger = FMath::Clamp(120 - DifficultyProfile.CountrySizeScore * 10, 80, 120);
        Risk.GameOverReason = TEXT("Assassination");
        Risk.bGameOverOnAssassination = true;
        Risk.ActiveUnrestCauses = {
            TEXT("Resource shortages"),
            TEXT("Public approval collapse"),
            TEXT("Policy backlash"),
            TEXT("Rival-country pressure"),
            TEXT("Scandals and opposition movements")
        };
        Risk.AdvisorWarnings = GetAdvisorWarnings(DifficultyProfile);
        Risk.RecoveryTips = GetRecoveryTips(DifficultyProfile);
        return Risk;
    }
}

FString FDemocracyResourceInventory::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"food\": %d,\n")
        TEXT("%s\"gasOil\": %d,\n")
        TEXT("%s\"wood\": %d,\n")
        TEXT("%s\"metals\": %d,\n")
        TEXT("%s\"water\": %d\n")
        TEXT("%s}"),
        *Pad, Food,
        *Pad, GasOil,
        *Pad, Wood,
        *Pad, Metals,
        *Pad, Water,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyResourceChainEntry::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"resourceName\": \"%s\",\n")
        TEXT("%s\"production\": %d,\n")
        TEXT("%s\"consumption\": %d,\n")
        TEXT("%s\"imports\": %d,\n")
        TEXT("%s\"exports\": %d,\n")
        TEXT("%s\"reserve\": %d,\n")
        TEXT("%s\"reserveTarget\": %d,\n")
        TEXT("%s\"shortage\": %d,\n")
        TEXT("%s\"surplus\": %d,\n")
        TEXT("%s\"strategicValue\": %d,\n")
        TEXT("%s\"role\": \"%s\",\n")
        TEXT("%s\"status\": \"%s\",\n")
        TEXT("%s\"drivers\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ResourceName),
        *Pad, Production,
        *Pad, Consumption,
        *Pad, Imports,
        *Pad, Exports,
        *Pad, Reserve,
        *Pad, ReserveTarget,
        *Pad, Shortage,
        *Pad, Surplus,
        *Pad, StrategicValue,
        *Pad, *JsonEscape(Role),
        *Pad, *JsonEscape(Status),
        *Pad, *StringArrayToJson(Drivers),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyResourceProductionChainState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString ChainPad = Indent(IndentSpaces + 2);
    FString ChainJson = TEXT("[");
    for (int32 Index = 0; Index < Chains.Num(); ++Index)
    {
        ChainJson += FString::Printf(TEXT("\n%s%s"), *ChainPad, *Chains[Index].ToJson(IndentSpaces + 4));
        if (Index < Chains.Num() - 1)
        {
            ChainJson += TEXT(",");
        }
    }
    ChainJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"totalShortagePressure\": %d,\n")
        TEXT("%s\"tradeBalance\": %d,\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"chains\": %s\n")
        TEXT("%s}"),
        *Pad, TotalShortagePressure,
        *Pad, TradeBalance,
        *Pad, LastUpdatedTurn,
        *Pad, *JsonEscape(Summary),
        *Pad, *ChainJson,
        *Indent(IndentSpaces - 2));
}
FString FDemocracyPolicyState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"economicPolicy\": \"%s\",\n")
        TEXT("%s\"environmentalPolicy\": \"%s\",\n")
        TEXT("%s\"militaryPolicy\": \"%s\",\n")
        TEXT("%s\"diplomacyPolicy\": \"%s\",\n")
        TEXT("%s\"civilPolicy\": \"%s\",\n")
        TEXT("%s\"policyChangeCount\": %d,\n")
        TEXT("%s\"lastPolicyChangeSummary\": \"%s\",\n")
        TEXT("%s\"activePolicyEffects\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(EconomicPolicy),
        *Pad, *JsonEscape(EnvironmentalPolicy),
        *Pad, *JsonEscape(MilitaryPolicy),
        *Pad, *JsonEscape(DiplomacyPolicy),
        *Pad, *JsonEscape(CivilPolicy),
        *Pad, PolicyChangeCount,
        *Pad, *JsonEscape(LastPolicyChangeSummary),
        *Pad, *StringArrayToJson(ActivePolicyEffects),
        *Indent(IndentSpaces - 2));
}
FString FDemocracyCountryState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"countryName\": \"%s\",\n")
        TEXT("%s\"leaderGender\": \"%s\",\n")
        TEXT("%s\"addressTitle\": \"%s\",\n")
        TEXT("%s\"climate\": \"%s\",\n")
        TEXT("%s\"difficulty\": \"%s\",\n")
        TEXT("%s\"countrySize\": \"%s\",\n")
        TEXT("%s\"countrySizeScore\": %d,\n")
        TEXT("%s\"publicApproval\": %d,\n")
        TEXT("%s\"stability\": %d,\n")
        TEXT("%s\"unrest\": %d,\n")
        TEXT("%s\"treasury\": %d,\n")
        TEXT("%s\"economicHealth\": %d,\n")
        TEXT("%s\"diplomaticStanding\": %d,\n")
        TEXT("%s\"technology\": %d,\n")
        TEXT("%s\"militaryReadiness\": %d,\n")
        TEXT("%s\"infrastructure\": %d,\n")
        TEXT("%s\"environmentalHealth\": %d,\n")
        TEXT("%s\"resources\": %s,\n")
        TEXT("%s\"policies\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CountryName),
        *Pad, *JsonEscape(LeaderGender),
        *Pad, *JsonEscape(AddressTitle),
        *Pad, *JsonEscape(Climate),
        *Pad, *JsonEscape(Difficulty),
        *Pad, *JsonEscape(CountrySize),
        *Pad, CountrySizeScore,
        *Pad, PublicApproval,
        *Pad, Stability,
        *Pad, Unrest,
        *Pad, Treasury,
        *Pad, EconomicHealth,
        *Pad, DiplomaticStanding,
        *Pad, Technology,
        *Pad, MilitaryReadiness,
        *Pad, Infrastructure,
        *Pad, EnvironmentalHealth,
        *Pad, *Resources.ToJson(IndentSpaces + 2),
        *Pad, *Policies.ToJson(IndentSpaces + 2),
        *Indent(IndentSpaces - 2));
}
FString FDemocracyFailureRiskState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"stabilityWarningThreshold\": %d,\n")
        TEXT("%s\"stabilityCriticalThreshold\": %d,\n")
        TEXT("%s\"unrestWarningThreshold\": %d,\n")
        TEXT("%s\"unrestCriticalThreshold\": %d,\n")
        TEXT("%s\"currentAssassinationRisk\": %d,\n")
        TEXT("%s\"assassinationRiskTrigger\": %d,\n")
        TEXT("%s\"warningLevel\": \"%s\",\n")
        TEXT("%s\"gameOverReason\": \"%s\",\n")
        TEXT("%s\"gameOverOnAssassination\": %s,\n")
        TEXT("%s\"activeUnrestCauses\": %s,\n")
        TEXT("%s\"advisorWarnings\": %s,\n")
        TEXT("%s\"recoveryTips\": %s\n")
        TEXT("%s}"),
        *Pad, StabilityWarningThreshold,
        *Pad, StabilityCriticalThreshold,
        *Pad, UnrestWarningThreshold,
        *Pad, UnrestCriticalThreshold,
        *Pad, CurrentAssassinationRisk,
        *Pad, AssassinationRiskTrigger,
        *Pad, *JsonEscape(WarningLevel),
        *Pad, *JsonEscape(GameOverReason),
        *Pad, bGameOverOnAssassination ? TEXT("true") : TEXT("false"),
        *Pad, *StringArrayToJson(ActiveUnrestCauses),
        *Pad, *StringArrayToJson(AdvisorWarnings),
        *Pad, *StringArrayToJson(RecoveryTips),
        *Indent(IndentSpaces - 2));
}


FString FDemocracyInvasionRiskState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"militaryReadinessWarningThreshold\": %d,\n")
        TEXT("%s\"militaryReadinessCriticalThreshold\": %d,\n")
        TEXT("%s\"borderPressureWarningThreshold\": %d,\n")
        TEXT("%s\"borderPressureCriticalThreshold\": %d,\n")
        TEXT("%s\"territorialLossWarningThreshold\": %d,\n")
        TEXT("%s\"territorialLossCriticalThreshold\": %d,\n")
        TEXT("%s\"currentInvasionRisk\": %d,\n")
        TEXT("%s\"invasionRiskTrigger\": %d,\n")
        TEXT("%s\"warningLevel\": \"%s\",\n")
        TEXT("%s\"gameOverReason\": \"%s\",\n")
        TEXT("%s\"gameOverOnTakeover\": %s,\n")
        TEXT("%s\"activeInvasionCauses\": %s,\n")
        TEXT("%s\"advisorWarnings\": %s,\n")
        TEXT("%s\"recoveryTips\": %s\n")
        TEXT("%s}"),
        *Pad, MilitaryReadinessWarningThreshold,
        *Pad, MilitaryReadinessCriticalThreshold,
        *Pad, BorderPressureWarningThreshold,
        *Pad, BorderPressureCriticalThreshold,
        *Pad, TerritorialLossWarningThreshold,
        *Pad, TerritorialLossCriticalThreshold,
        *Pad, CurrentInvasionRisk,
        *Pad, InvasionRiskTrigger,
        *Pad, *JsonEscape(WarningLevel),
        *Pad, *JsonEscape(GameOverReason),
        *Pad, bGameOverOnTakeover ? TEXT("true") : TEXT("false"),
        *Pad, *StringArrayToJson(ActiveInvasionCauses),
        *Pad, *StringArrayToJson(AdvisorWarnings),
        *Pad, *StringArrayToJson(RecoveryTips),
        *Indent(IndentSpaces - 2));
}


FString FDemocracyAdvisorReport::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"advisorName\": \"%s\",\n")
        TEXT("%s\"category\": \"%s\",\n")
        TEXT("%s\"issueReport\": \"%s\",\n")
        TEXT("%s\"recommendation\": \"%s\",\n")
        TEXT("%s\"warning\": \"%s\",\n")
        TEXT("%s\"tradeoffExplanation\": \"%s\",\n")
        TEXT("%s\"guidanceLevel\": \"%s\",\n")
        TEXT("%s\"severity\": %d\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(AdvisorName),
        *Pad, *JsonEscape(Category),
        *Pad, *JsonEscape(IssueReport),
        *Pad, *JsonEscape(Recommendation),
        *Pad, *JsonEscape(Warning),
        *Pad, *JsonEscape(TradeoffExplanation),
        *Pad, *JsonEscape(GuidanceLevel),
        *Pad, Severity,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyAdvisorSystemState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString ReportPad = Indent(IndentSpaces + 2);
    FString ReportJson = TEXT("[");
    for (int32 Index = 0; Index < Reports.Num(); ++Index)
    {
        ReportJson += FString::Printf(TEXT("\n%s%s"), *ReportPad, *Reports[Index].ToJson(IndentSpaces + 4));
        if (Index < Reports.Num() - 1)
        {
            ReportJson += TEXT(",");
        }
    }
    ReportJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"guidanceLevel\": \"%s\",\n")
        TEXT("%s\"advisorCount\": %d,\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"reports\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(GuidanceLevel),
        *Pad, AdvisorCount,
        *Pad, LastUpdatedTurn,
        *Pad, *ReportJson,
        *Indent(IndentSpaces - 2));
}
FString FDemocracyEventChoiceState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"choiceId\": \"%s\",\n")
        TEXT("%s\"label\": \"%s\",\n")
        TEXT("%s\"description\": \"%s\",\n")
        TEXT("%s\"consequencePreview\": \"%s\",\n")
        TEXT("%s\"approvalDelta\": %d,\n")
        TEXT("%s\"stabilityDelta\": %d,\n")
        TEXT("%s\"unrestDelta\": %d,\n")
        TEXT("%s\"treasuryDelta\": %d,\n")
        TEXT("%s\"economicDelta\": %d,\n")
        TEXT("%s\"diplomacyDelta\": %d,\n")
        TEXT("%s\"militaryDelta\": %d,\n")
        TEXT("%s\"infrastructureDelta\": %d,\n")
        TEXT("%s\"environmentDelta\": %d,\n")
        TEXT("%s\"foodDelta\": %d,\n")
        TEXT("%s\"waterDelta\": %d,\n")
        TEXT("%s\"gasOilDelta\": %d,\n")
        TEXT("%s\"woodDelta\": %d,\n")
        TEXT("%s\"metalsDelta\": %d,\n")
        TEXT("%s\"assassinationRiskDelta\": %d,\n")
        TEXT("%s\"invasionRiskDelta\": %d\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ChoiceId), *Pad, *JsonEscape(Label), *Pad, *JsonEscape(Description), *Pad, *JsonEscape(ConsequencePreview),
        *Pad, ApprovalDelta, *Pad, StabilityDelta, *Pad, UnrestDelta, *Pad, TreasuryDelta, *Pad, EconomicDelta, *Pad, DiplomacyDelta,
        *Pad, MilitaryDelta, *Pad, InfrastructureDelta, *Pad, EnvironmentDelta, *Pad, FoodDelta, *Pad, WaterDelta, *Pad, GasOilDelta,
        *Pad, WoodDelta, *Pad, MetalsDelta, *Pad, AssassinationRiskDelta, *Pad, InvasionRiskDelta,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyActiveEventState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString ChoicePad = Indent(IndentSpaces + 2);
    FString ChoiceJson = TEXT("[");
    for (int32 Index = 0; Index < Choices.Num(); ++Index)
    {
        ChoiceJson += FString::Printf(TEXT("\n%s%s"), *ChoicePad, *Choices[Index].ToJson(IndentSpaces + 4));
        if (Index < Choices.Num() - 1) { ChoiceJson += TEXT(","); }
    }
    ChoiceJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"eventId\": \"%s\",\n")
        TEXT("%s\"eventType\": \"%s\",\n")
        TEXT("%s\"title\": \"%s\",\n")
        TEXT("%s\"description\": \"%s\",\n")
        TEXT("%s\"triggerReason\": \"%s\",\n")
        TEXT("%s\"createdTurn\": %d,\n")
        TEXT("%s\"severity\": %d,\n")
        TEXT("%s\"triggered\": %s,\n")
        TEXT("%s\"resolved\": %s,\n")
        TEXT("%s\"selectedChoiceId\": \"%s\",\n")
        TEXT("%s\"resolutionSummary\": \"%s\",\n")
        TEXT("%s\"choices\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(EventId), *Pad, *JsonEscape(EventType), *Pad, *JsonEscape(Title), *Pad, *JsonEscape(Description), *Pad, *JsonEscape(TriggerReason),
        *Pad, CreatedTurn, *Pad, Severity, *Pad, bTriggered ? TEXT("true") : TEXT("false"), *Pad, bResolved ? TEXT("true") : TEXT("false"),
        *Pad, *JsonEscape(SelectedChoiceId), *Pad, *JsonEscape(ResolutionSummary), *Pad, *ChoiceJson,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyEventSystemState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString EventPad = Indent(IndentSpaces + 2);
    FString EventJson = TEXT("[");
    for (int32 Index = 0; Index < ActiveEvents.Num(); ++Index)
    {
        EventJson += FString::Printf(TEXT("\n%s%s"), *EventPad, *ActiveEvents[Index].ToJson(IndentSpaces + 4));
        if (Index < ActiveEvents.Num() - 1) { EventJson += TEXT(","); }
    }
    EventJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastEventTurn\": %d,\n")
        TEXT("%s\"eventCounter\": %d,\n")
        TEXT("%s\"activeEventLimit\": %d,\n")
        TEXT("%s\"activeEvents\": %s,\n")
        TEXT("%s\"eventHistory\": %s\n")
        TEXT("%s}"),
        *Pad, LastEventTurn, *Pad, EventCounter, *Pad, ActiveEventLimit, *Pad, *EventJson, *Pad, *StringArrayToJson(EventHistory),
        *Indent(IndentSpaces - 2));
}
FString FDemocracyCitizenGroupState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"groupName\": \"%s\",\n")
        TEXT("%s\"populationShare\": %d,\n")
        TEXT("%s\"approval\": %d,\n")
        TEXT("%s\"needFood\": %d,\n")
        TEXT("%s\"needWater\": %d,\n")
        TEXT("%s\"needJobs\": %d,\n")
        TEXT("%s\"needSecurity\": %d,\n")
        TEXT("%s\"needHealthcare\": %d,\n")
        TEXT("%s\"unrestPressure\": %d,\n")
        TEXT("%s\"unrestSources\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(GroupName),
        *Pad, PopulationShare,
        *Pad, Approval,
        *Pad, NeedFood,
        *Pad, NeedWater,
        *Pad, NeedJobs,
        *Pad, NeedSecurity,
        *Pad, NeedHealthcare,
        *Pad, UnrestPressure,
        *Pad, *StringArrayToJson(UnrestSources),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRegionState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"regionName\": \"%s\",\n")
        TEXT("%s\"climate\": \"%s\",\n")
        TEXT("%s\"populationShare\": %d,\n")
        TEXT("%s\"approval\": %d,\n")
        TEXT("%s\"stability\": %d,\n")
        TEXT("%s\"unrest\": %d,\n")
        TEXT("%s\"foodAccess\": %d,\n")
        TEXT("%s\"waterAccess\": %d,\n")
        TEXT("%s\"jobs\": %d,\n")
        TEXT("%s\"security\": %d,\n")
        TEXT("%s\"infrastructure\": %d,\n")
        TEXT("%s\"unrestSources\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(RegionName),
        *Pad, *JsonEscape(Climate),
        *Pad, PopulationShare,
        *Pad, Approval,
        *Pad, Stability,
        *Pad, Unrest,
        *Pad, FoodAccess,
        *Pad, WaterAccess,
        *Pad, Jobs,
        *Pad, Security,
        *Pad, Infrastructure,
        *Pad, *StringArrayToJson(UnrestSources),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyDemographicsState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString ChildPad = Indent(IndentSpaces + 2);
    FString GroupJson = TEXT("[");
    for (int32 Index = 0; Index < CitizenGroups.Num(); ++Index)
    {
        GroupJson += FString::Printf(TEXT("\n%s%s"), *ChildPad, *CitizenGroups[Index].ToJson(IndentSpaces + 4));
        if (Index < CitizenGroups.Num() - 1) { GroupJson += TEXT(","); }
    }
    GroupJson += FString::Printf(TEXT("\n%s]"), *Pad);

    FString RegionJson = TEXT("[");
    for (int32 Index = 0; Index < Regions.Num(); ++Index)
    {
        RegionJson += FString::Printf(TEXT("\n%s%s"), *ChildPad, *Regions[Index].ToJson(IndentSpaces + 4));
        if (Index < Regions.Num() - 1) { RegionJson += TEXT(","); }
    }
    RegionJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"totalPopulationThousands\": %d,\n")
        TEXT("%s\"averageGroupApproval\": %d,\n")
        TEXT("%s\"averageRegionalApproval\": %d,\n")
        TEXT("%s\"nationalNeedsPressure\": %d,\n")
        TEXT("%s\"demographicUnrestPressure\": %d,\n")
        TEXT("%s\"citizenGroups\": %s,\n")
        TEXT("%s\"regions\": %s,\n")
        TEXT("%s\"nationalUnrestSources\": %s\n")
        TEXT("%s}"),
        *Pad, TotalPopulationThousands,
        *Pad, AverageGroupApproval,
        *Pad, AverageRegionalApproval,
        *Pad, NationalNeedsPressure,
        *Pad, DemographicUnrestPressure,
        *Pad, *GroupJson,
        *Pad, *RegionJson,
        *Pad, *StringArrayToJson(NationalUnrestSources),
        *Indent(IndentSpaces - 2));
}
FString FDemocracyEconomyBudgetState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"taxRate\": %d,\n")
        TEXT("%s\"taxPolicy\": \"%s\",\n")
        TEXT("%s\"publicServicesSpending\": %d,\n")
        TEXT("%s\"infrastructureSpending\": %d,\n")
        TEXT("%s\"defenseSpending\": %d,\n")
        TEXT("%s\"debt\": %d,\n")
        TEXT("%s\"income\": %d,\n")
        TEXT("%s\"expenses\": %d,\n")
        TEXT("%s\"deficit\": %d,\n")
        TEXT("%s\"inflation\": %d,\n")
        TEXT("%s\"publicServices\": %d,\n")
        TEXT("%s\"productionEfficiency\": %d,\n")
        TEXT("%s\"spendingPosture\": \"%s\",\n")
        TEXT("%s\"lastBudgetSummary\": \"%s\"\n")
        TEXT("%s}"),
        *Pad, TaxRate,
        *Pad, *JsonEscape(TaxPolicy),
        *Pad, PublicServicesSpending,
        *Pad, InfrastructureSpending,
        *Pad, DefenseSpending,
        *Pad, Debt,
        *Pad, Income,
        *Pad, Expenses,
        *Pad, Deficit,
        *Pad, Inflation,
        *Pad, PublicServices,
        *Pad, ProductionEfficiency,
        *Pad, *JsonEscape(SpendingPosture),
        *Pad, *JsonEscape(LastBudgetSummary),
        *Indent(IndentSpaces - 2));
}
FString FDemocracyDepartmentState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"departmentName\": \"%s\",\n")
        TEXT("%s\"ministerTitle\": \"%s\",\n")
        TEXT("%s\"domain\": \"%s\",\n")
        TEXT("%s\"budgetShare\": %d,\n")
        TEXT("%s\"staffing\": %d,\n")
        TEXT("%s\"effectiveness\": %d,\n")
        TEXT("%s\"publicTrust\": %d,\n")
        TEXT("%s\"priority\": %d,\n")
        TEXT("%s\"currentAction\": \"%s\",\n")
        TEXT("%s\"policyInterface\": \"%s\",\n")
        TEXT("%s\"advisorySummary\": \"%s\",\n")
        TEXT("%s\"actionEffects\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(DepartmentName),
        *Pad, *JsonEscape(MinisterTitle),
        *Pad, *JsonEscape(Domain),
        *Pad, BudgetShare,
        *Pad, Staffing,
        *Pad, Effectiveness,
        *Pad, PublicTrust,
        *Pad, Priority,
        *Pad, *JsonEscape(CurrentAction),
        *Pad, *JsonEscape(PolicyInterface),
        *Pad, *JsonEscape(AdvisorySummary),
        *Pad, *StringArrayToJson(ActionEffects),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyDepartmentSystemState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString DepartmentPad = Indent(IndentSpaces + 2);
    FString DepartmentJson = TEXT("[");
    for (int32 Index = 0; Index < Departments.Num(); ++Index)
    {
        DepartmentJson += FString::Printf(TEXT("\n%s%s"), *DepartmentPad, *Departments[Index].ToJson(IndentSpaces + 4));
        if (Index < Departments.Num() - 1)
        {
            DepartmentJson += TEXT(",");
        }
    }
    DepartmentJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"coordination\": %d,\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"departments\": %s\n")
        TEXT("%s}"),
        *Pad, LastUpdatedTurn,
        *Pad, Coordination,
        *Pad, *JsonEscape(Summary),
        *Pad, *DepartmentJson,
        *Indent(IndentSpaces - 2));
}
FString FDemocracyApprovalCauseState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"causeName\": \"%s\",\n")
        TEXT("%s\"category\": \"%s\",\n")
        TEXT("%s\"approvalImpact\": %d,\n")
        TEXT("%s\"unrestImpact\": %d,\n")
        TEXT("%s\"stabilityImpact\": %d,\n")
        TEXT("%s\"severity\": %d,\n")
        TEXT("%s\"sourceMetric\": \"%s\",\n")
        TEXT("%s\"currentStatus\": \"%s\",\n")
        TEXT("%s\"suggestedResponses\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CauseName),
        *Pad, *JsonEscape(Category),
        *Pad, ApprovalImpact,
        *Pad, UnrestImpact,
        *Pad, StabilityImpact,
        *Pad, Severity,
        *Pad, *JsonEscape(SourceMetric),
        *Pad, *JsonEscape(CurrentStatus),
        *Pad, *StringArrayToJson(SuggestedResponses),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyApprovalStabilityModelState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString CausePad = Indent(IndentSpaces + 2);
    FString CauseJson = TEXT("[");
    for (int32 Index = 0; Index < Causes.Num(); ++Index)
    {
        CauseJson += FString::Printf(TEXT("\n%s%s"), *CausePad, *Causes[Index].ToJson(IndentSpaces + 4));
        if (Index < Causes.Num() - 1)
        {
            CauseJson += TEXT(",");
        }
    }
    CauseJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"netApprovalPressure\": %d,\n")
        TEXT("%s\"netUnrestPressure\": %d,\n")
        TEXT("%s\"netStabilityPressure\": %d,\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"causes\": %s\n")
        TEXT("%s}"),
        *Pad, NetApprovalPressure,
        *Pad, NetUnrestPressure,
        *Pad, NetStabilityPressure,
        *Pad, LastUpdatedTurn,
        *Pad, *JsonEscape(Summary),
        *Pad, *CauseJson,
        *Indent(IndentSpaces - 2));
}
FString FDemocracyPressReleaseRecordState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"turn\": %d,\n")
        TEXT("%s\"announcementType\": \"%s\",\n")
        TEXT("%s\"messageQuality\": \"%s\",\n")
        TEXT("%s\"truthful\": %s,\n")
        TEXT("%s\"approvalDelta\": %d,\n")
        TEXT("%s\"stabilityDelta\": %d,\n")
        TEXT("%s\"diplomacyDelta\": %d,\n")
        TEXT("%s\"unrestDelta\": %d,\n")
        TEXT("%s\"credibilityDelta\": %d,\n")
        TEXT("%s\"credibilityAfter\": %d,\n")
        TEXT("%s\"summary\": \"%s\"\n")
        TEXT("%s}"),
        *Pad, Turn,
        *Pad, *JsonEscape(AnnouncementType),
        *Pad, *JsonEscape(MessageQuality),
        *Pad, bTruthful ? TEXT("true") : TEXT("false"),
        *Pad, ApprovalDelta,
        *Pad, StabilityDelta,
        *Pad, DiplomacyDelta,
        *Pad, UnrestDelta,
        *Pad, CredibilityDelta,
        *Pad, CredibilityAfter,
        *Pad, *JsonEscape(Summary),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyPressOfficeState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString RecordPad = Indent(IndentSpaces + 2);
    FString RecordJson = TEXT("[");
    for (int32 Index = 0; Index < Records.Num(); ++Index)
    {
        RecordJson += FString::Printf(TEXT("\n%s%s"), *RecordPad, *Records[Index].ToJson(IndentSpaces + 4));
        if (Index < Records.Num() - 1)
        {
            RecordJson += TEXT(",");
        }
    }
    RecordJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"credibility\": %d,\n")
        TEXT("%s\"consecutiveEmptyAnnouncements\": %d,\n")
        TEXT("%s\"consecutiveFalseAnnouncements\": %d,\n")
        TEXT("%s\"totalAnnouncements\": %d,\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"maxRecords\": %d,\n")
        TEXT("%s\"lastAnnouncementSummary\": \"%s\",\n")
        TEXT("%s\"records\": %s\n")
        TEXT("%s}"),
        *Pad, Credibility,
        *Pad, ConsecutiveEmptyAnnouncements,
        *Pad, ConsecutiveFalseAnnouncements,
        *Pad, TotalAnnouncements,
        *Pad, LastUpdatedTurn,
        *Pad, MaxRecords,
        *Pad, *JsonEscape(LastAnnouncementSummary),
        *Pad, *RecordJson,
        *Indent(IndentSpaces - 2));
}
FString FDemocracyMeetingRecordState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"turn\": %d,\n")
        TEXT("%s\"meetingType\": \"%s\",\n")
        TEXT("%s\"participantName\": \"%s\",\n")
        TEXT("%s\"agendaItem\": \"%s\",\n")
        TEXT("%s\"outcomeSummary\": \"%s\",\n")
        TEXT("%s\"approvalDelta\": %d,\n")
        TEXT("%s\"stabilityDelta\": %d,\n")
        TEXT("%s\"unrestDelta\": %d,\n")
        TEXT("%s\"diplomacyDelta\": %d,\n")
        TEXT("%s\"treasuryDelta\": %d,\n")
        TEXT("%s\"economyDelta\": %d,\n")
        TEXT("%s\"militaryDelta\": %d,\n")
        TEXT("%s\"infrastructureDelta\": %d,\n")
        TEXT("%s\"advisorCoordinationDelta\": %d,\n")
        TEXT("%s\"foreignTrustDelta\": %d\n")
        TEXT("%s}"),
        *Pad, Turn,
        *Pad, *JsonEscape(MeetingType),
        *Pad, *JsonEscape(ParticipantName),
        *Pad, *JsonEscape(AgendaItem),
        *Pad, *JsonEscape(OutcomeSummary),
        *Pad, ApprovalDelta,
        *Pad, StabilityDelta,
        *Pad, UnrestDelta,
        *Pad, DiplomacyDelta,
        *Pad, TreasuryDelta,
        *Pad, EconomyDelta,
        *Pad, MilitaryDelta,
        *Pad, InfrastructureDelta,
        *Pad, AdvisorCoordinationDelta,
        *Pad, ForeignTrustDelta,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyMeetingSystemState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString RecordPad = Indent(IndentSpaces + 2);
    FString RecordJson = TEXT("[");
    for (int32 Index = 0; Index < Records.Num(); ++Index)
    {
        RecordJson += FString::Printf(TEXT("\n%s%s"), *RecordPad, *Records[Index].ToJson(IndentSpaces + 4));
        if (Index < Records.Num() - 1)
        {
            RecordJson += TEXT(",");
        }
    }
    RecordJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"totalMeetings\": %d,\n")
        TEXT("%s\"advisorCoordination\": %d,\n")
        TEXT("%s\"foreignTrust\": %d,\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"maxRecords\": %d,\n")
        TEXT("%s\"lastMeetingSummary\": \"%s\",\n")
        TEXT("%s\"records\": %s\n")
        TEXT("%s}"),
        *Pad, TotalMeetings,
        *Pad, AdvisorCoordination,
        *Pad, ForeignTrust,
        *Pad, LastUpdatedTurn,
        *Pad, MaxRecords,
        *Pad, *JsonEscape(LastMeetingSummary),
        *Pad, *RecordJson,
        *Indent(IndentSpaces - 2));
}
FString FDemocracyDevelopmentTrackState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"trackName\": \"%s\",\n")
        TEXT("%s\"focusArea\": \"%s\",\n")
        TEXT("%s\"level\": %d,\n")
        TEXT("%s\"progress\": %d,\n")
        TEXT("%s\"progressTarget\": %d,\n")
        TEXT("%s\"treasuryCost\": %d,\n")
        TEXT("%s\"woodCost\": %d,\n")
        TEXT("%s\"metalsCost\": %d,\n")
        TEXT("%s\"fuelCost\": %d,\n")
        TEXT("%s\"currentProject\": \"%s\",\n")
        TEXT("%s\"strategicBenefit\": \"%s\",\n")
        TEXT("%s\"unlocks\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(TrackName),
        *Pad, *JsonEscape(FocusArea),
        *Pad, Level,
        *Pad, Progress,
        *Pad, ProgressTarget,
        *Pad, TreasuryCost,
        *Pad, WoodCost,
        *Pad, MetalsCost,
        *Pad, FuelCost,
        *Pad, *JsonEscape(CurrentProject),
        *Pad, *JsonEscape(StrategicBenefit),
        *Pad, *StringArrayToJson(Unlocks),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyDevelopmentSystemState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString TrackPad = Indent(IndentSpaces + 2);
    FString TrackJson = TEXT("[");
    for (int32 Index = 0; Index < Tracks.Num(); ++Index)
    {
        TrackJson += FString::Printf(TEXT("\n%s%s"), *TrackPad, *Tracks[Index].ToJson(IndentSpaces + 4));
        if (Index < Tracks.Num() - 1)
        {
            TrackJson += TEXT(",");
        }
    }
    TrackJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"activeFocus\": \"%s\",\n")
        TEXT("%s\"developmentPoints\": %d,\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"tracks\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ActiveFocus),
        *Pad, DevelopmentPoints,
        *Pad, LastUpdatedTurn,
        *Pad, *JsonEscape(Summary),
        *Pad, *TrackJson,
        *Indent(IndentSpaces - 2));
}
FString FDemocracyDecisionRecordState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"turn\": %d,\n")
        TEXT("%s\"category\": \"%s\",\n")
        TEXT("%s\"decisionTitle\": \"%s\",\n")
        TEXT("%s\"decisionDetail\": \"%s\",\n")
        TEXT("%s\"consequenceSummary\": \"%s\",\n")
        TEXT("%s\"approvalAfter\": %d,\n")
        TEXT("%s\"stabilityAfter\": %d,\n")
        TEXT("%s\"unrestAfter\": %d,\n")
        TEXT("%s\"treasuryAfter\": %d,\n")
        TEXT("%s\"economyAfter\": %d,\n")
        TEXT("%s\"militaryAfter\": %d,\n")
        TEXT("%s\"severity\": %d,\n")
        TEXT("%s\"timestampUtc\": \"%s\",\n")
        TEXT("%s\"tags\": %s\n")
        TEXT("%s}"),
        *Pad, Turn,
        *Pad, *JsonEscape(Category),
        *Pad, *JsonEscape(DecisionTitle),
        *Pad, *JsonEscape(DecisionDetail),
        *Pad, *JsonEscape(ConsequenceSummary),
        *Pad, ApprovalAfter,
        *Pad, StabilityAfter,
        *Pad, UnrestAfter,
        *Pad, TreasuryAfter,
        *Pad, EconomyAfter,
        *Pad, MilitaryAfter,
        *Pad, Severity,
        *Pad, *JsonEscape(TimestampUtc),
        *Pad, *StringArrayToJson(Tags),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyDecisionHistoryState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString RecordPad = Indent(IndentSpaces + 2);
    FString RecordJson = TEXT("[");
    for (int32 Index = 0; Index < Records.Num(); ++Index)
    {
        RecordJson += FString::Printf(TEXT("\n%s%s"), *RecordPad, *Records[Index].ToJson(IndentSpaces + 4));
        if (Index < Records.Num() - 1)
        {
            RecordJson += TEXT(",");
        }
    }
    RecordJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"maxRecords\": %d,\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"records\": %s\n")
        TEXT("%s}"),
        *Pad, LastUpdatedTurn,
        *Pad, MaxRecords,
        *Pad, *JsonEscape(Summary),
        *Pad, *RecordJson,
        *Indent(IndentSpaces - 2));
}
FString FDemocracyGeneratedCountryState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"countryName\": \"%s\",\n")
        TEXT("%s\"continentName\": \"%s\",\n")
        TEXT("%s\"climate\": \"%s\",\n")
        TEXT("%s\"politicalType\": \"%s\",\n")
        TEXT("%s\"diplomaticAlignment\": \"%s\",\n")
        TEXT("%s\"powerScore\": %d,\n")
        TEXT("%s\"stability\": %d,\n")
        TEXT("%s\"borderPressure\": %d,\n")
        TEXT("%s\"alliedWithPlayer\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CountryName),
        *Pad, *JsonEscape(ContinentName),
        *Pad, *JsonEscape(Climate),
        *Pad, *JsonEscape(PoliticalType),
        *Pad, *JsonEscape(DiplomaticAlignment),
        *Pad, PowerScore,
        *Pad, Stability,
        *Pad, BorderPressure,
        *Pad, bAlliedWithPlayer ? TEXT("true") : TEXT("false"),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyContinentState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString CountryPad = Indent(IndentSpaces + 2);
    FString CountryJson = TEXT("[");
    for (int32 Index = 0; Index < Countries.Num(); ++Index)
    {
        CountryJson += FString::Printf(TEXT("\n%s%s"), *CountryPad, *Countries[Index].ToJson(IndentSpaces + 4));
        if (Index < Countries.Num() - 1)
        {
            CountryJson += TEXT(",");
        }
    }
    CountryJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"continentName\": \"%s\",\n")
        TEXT("%s\"climate\": \"%s\",\n")
        TEXT("%s\"countryCount\": %d,\n")
        TEXT("%s\"countries\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ContinentName),
        *Pad, *JsonEscape(Climate),
        *Pad, CountryCount,
        *Pad, *CountryJson,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyWorldMapState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString ContinentPad = Indent(IndentSpaces + 2);
    FString ContinentJson = TEXT("[");
    for (int32 Index = 0; Index < Continents.Num(); ++Index)
    {
        ContinentJson += FString::Printf(TEXT("\n%s%s"), *ContinentPad, *Continents[Index].ToJson(IndentSpaces + 4));
        if (Index < Continents.Num() - 1)
        {
            ContinentJson += TEXT(",");
        }
    }
    ContinentJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"continentCount\": %d,\n")
        TEXT("%s\"totalCountryCount\": %d,\n")
        TEXT("%s\"democraticAllyCount\": %d,\n")
        TEXT("%s\"nonDemocraticCountryCount\": %d,\n")
        TEXT("%s\"generationRule\": \"%s\",\n")
        TEXT("%s\"continents\": %s\n")
        TEXT("%s}"),
        *Pad, ContinentCount,
        *Pad, TotalCountryCount,
        *Pad, DemocraticAllyCount,
        *Pad, NonDemocraticCountryCount,
        *Pad, *JsonEscape(GenerationRule),
        *Pad, *ContinentJson,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRivalCountryState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"countryName\": \"%s\",\n")
        TEXT("%s\"temperament\": \"%s\",\n")
        TEXT("%s\"relationToPlayer\": \"%s\",\n")
        TEXT("%s\"powerScore\": %d,\n")
        TEXT("%s\"borderPressure\": %d,\n")
        TEXT("%s\"tradeValue\": %d\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CountryName),
        *Pad, *JsonEscape(Temperament),
        *Pad, *JsonEscape(RelationToPlayer),
        *Pad, PowerScore,
        *Pad, BorderPressure,
        *Pad, TradeValue,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsWorldState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString RivalPad = Indent(IndentSpaces + 2);
    FString RivalJson = TEXT("[");
    for (int32 Index = 0; Index < Rivals.Num(); ++Index)
    {
        RivalJson += FString::Printf(TEXT("\n%s%s"), *RivalPad, *Rivals[Index].ToJson(IndentSpaces + 4));
        if (Index < Rivals.Num() - 1)
        {
            RivalJson += TEXT(",");
        }
    }
    RivalJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"simulationSecond\": %d,\n")
        TEXT("%s\"controlledTerritories\": %d,\n")
        TEXT("%s\"borderTerritories\": %d,\n")
        TEXT("%s\"knownRivalCountries\": %d,\n")
        TEXT("%s\"activeStrategicLayers\": %s,\n")
        TEXT("%s\"rivals\": %s\n")
        TEXT("%s}"),
        *Pad, SimulationSecond,
        *Pad, ControlledTerritories,
        *Pad, BorderTerritories,
        *Pad, KnownRivalCountries,
        *Pad, *StringArrayToJson(ActiveStrategicLayers),
        *Pad, *RivalJson,
        *Indent(IndentSpaces - 2));
}

FString FDemocracySimulationState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"turn\": %d,\n")
        TEXT("%s\"phase\": \"%s\",\n")
        TEXT("%s\"realTimeTickSeconds\": %.2f,\n")
        TEXT("%s\"paused\": %s,\n")
        TEXT("%s\"playerCountry\": %s,\n")
        TEXT("%s\"failureRisk\": %s,\n")
        TEXT("%s\"invasionRisk\": %s,\n")
        TEXT("%s\"advisorSystem\": %s,\n")
        TEXT("%s\"eventSystem\": %s,\n")
        TEXT("%s\"demographics\": %s,\n")
        TEXT("%s\"economyBudget\": %s,\n")
        TEXT("%s\"resourceChains\": %s,\n")
        TEXT("%s\"departments\": %s,\n")
        TEXT("%s\"approvalStability\": %s,\n")
        TEXT("%s\"pressOffice\": %s,\n")
        TEXT("%s\"meetingSystem\": %s,\n")
        TEXT("%s\"developmentSystem\": %s,\n")
        TEXT("%s\"decisionHistory\": %s,\n")
        TEXT("%s\"worldMap\": %s,\n")
        TEXT("%s\"rtsWorld\": %s\n")
        TEXT("%s}"),
        *Pad, Turn,
        *Pad, *JsonEscape(Phase),
        *Pad, RealTimeTickSeconds,
        *Pad, bPaused ? TEXT("true") : TEXT("false"),
        *Pad, *PlayerCountry.ToJson(IndentSpaces + 2),
        *Pad, *FailureRisk.ToJson(IndentSpaces + 2),
        *Pad, *InvasionRisk.ToJson(IndentSpaces + 2),
        *Pad, *AdvisorSystem.ToJson(IndentSpaces + 2),
        *Pad, *EventSystem.ToJson(IndentSpaces + 2),
        *Pad, *Demographics.ToJson(IndentSpaces + 2),
        *Pad, *EconomyBudget.ToJson(IndentSpaces + 2),
        *Pad, *ResourceChains.ToJson(IndentSpaces + 2),
        *Pad, *Departments.ToJson(IndentSpaces + 2),
        *Pad, *ApprovalStability.ToJson(IndentSpaces + 2),
        *Pad, *PressOffice.ToJson(IndentSpaces + 2),
        *Pad, *MeetingSystem.ToJson(IndentSpaces + 2),
        *Pad, *DevelopmentSystem.ToJson(IndentSpaces + 2),
        *Pad, *DecisionHistory.ToJson(IndentSpaces + 2),
        *Pad, *WorldMap.ToJson(IndentSpaces + 2),
        *Pad, *RtsWorld.ToJson(IndentSpaces + 2),
        *Indent(IndentSpaces - 2));
}

FDemocracySimulationState FDemocracyGameStateFactory::CreateInitialState(
    const FString& StateName,
    const FString& LeaderGender,
    const FString& AddressTitle,
    const FString& Climate,
    const FDifficultyProfile& DifficultyProfile)
{
    FDemocracySimulationState State;
    State.Turn = 1;
    State.Phase = TEXT("Initial Office Briefing");
    State.RealTimeTickSeconds = 1.0f;
    State.bPaused = true;

    State.PlayerCountry.CountryName = StateName;
    State.PlayerCountry.LeaderGender = LeaderGender;
    State.PlayerCountry.AddressTitle = AddressTitle;
    State.PlayerCountry.Climate = Climate;
    State.PlayerCountry.Difficulty = DifficultyProfile.Name;
    State.PlayerCountry.CountrySize = DifficultyProfile.CountrySize;
    State.PlayerCountry.CountrySizeScore = DifficultyProfile.CountrySizeScore;
    State.PlayerCountry.PublicApproval = DifficultyProfile.StartingApproval;
    State.PlayerCountry.Stability = FMath::Clamp(DifficultyProfile.StartingApproval - (DifficultyProfile.CountrySizeScore * 4), 25, 85);
    State.PlayerCountry.Unrest = InitialUnrestForDifficulty(DifficultyProfile);
    State.PlayerCountry.Treasury = DifficultyProfile.StartingTreasury;
    State.PlayerCountry.EconomicHealth = FMath::Clamp(DifficultyProfile.StartingApproval + 2 - DifficultyProfile.CountrySizeScore * 3, 35, 75);
    State.PlayerCountry.DiplomaticStanding = FMath::Clamp(62 - DifficultyProfile.CountrySizeScore * 5, 35, 70);
    State.PlayerCountry.Policies.ActivePolicyEffects = {
        TEXT("Balanced Budget: modest treasury gain with low public impact."),
        TEXT("Managed Development: balanced resource output and environmental pressure."),
        TEXT("Defensive Readiness: maintains military readiness without provoking rivals."),
        TEXT("Neutral Engagement: stable diplomacy with no strong alliance push."),
        TEXT("Public Stability: small stability support with low civil backlash.")
    };
    State.PlayerCountry.Technology = FMath::Clamp(3 - DifficultyProfile.CountrySizeScore / 2, 1, 3);
    State.PlayerCountry.MilitaryReadiness = FMath::Clamp(30 + DifficultyProfile.CountrySizeScore * 8, 30, 70);
    State.PlayerCountry.Infrastructure = FMath::Clamp(65 - DifficultyProfile.CountrySizeScore * 8, 25, 70);
    State.PlayerCountry.EnvironmentalHealth = Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase) ? 62 : 55;
    State.PlayerCountry.Resources.Food = DifficultyProfile.StartingResources.Food;
    State.PlayerCountry.Resources.GasOil = DifficultyProfile.StartingResources.GasOil;
    State.PlayerCountry.Resources.Wood = DifficultyProfile.StartingResources.Wood;
    State.PlayerCountry.Resources.Metals = DifficultyProfile.StartingResources.Metals;
    State.PlayerCountry.Resources.Water = DifficultyProfile.StartingResources.Water;

    State.FailureRisk = BuildFailureRiskState(DifficultyProfile);
    State.InvasionRisk = BuildInvasionRiskState(DifficultyProfile);
    State.AdvisorSystem.GuidanceLevel = WarningLevelForDifficulty(DifficultyProfile);
    State.AdvisorSystem.AdvisorCount = DifficultyProfile.AdvisorCount;
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    State.AdvisorSystem.Reports = {
        { TEXT("Resource Manager"), TEXT("Resources"), TEXT("Initial resource stockpiles are ready for testing."), TEXT("Monitor food and water first; shortages raise unrest quickly."), TEXT("No immediate resource emergency."), TEXT("Stockpiling improves safety but can slow economic expansion later."), State.AdvisorSystem.GuidanceLevel, 1 },
        { TEXT("Economic Advisor"), TEXT("Economy"), TEXT("The initial treasury and economy are loaded from the selected difficulty."), TEXT("Use policy changes to test budget pressure against public approval."), TEXT("Treasury can fall quickly under high-support policies."), TEXT("Spending can raise approval and stability, but it reduces the reserve available for emergencies."), State.AdvisorSystem.GuidanceLevel, 1 },
        { TEXT("Social Advisor"), TEXT("Stability"), TEXT("Public approval, unrest, and stability are the main internal safety indicators."), TEXT("Keep stability above the warning threshold and unrest below the warning threshold."), TEXT("Internal failure risk rises when unrest or instability crosses difficulty thresholds."), TEXT("Emergency controls reduce unrest faster but can damage approval."), State.AdvisorSystem.GuidanceLevel, 1 },
        { TEXT("Military Advisor"), TEXT("Military"), TEXT("Military readiness and diplomatic standing control foreign takeover pressure."), TEXT("Maintain readiness before rival pressure reaches critical levels."), TEXT("Low readiness can trigger a foreign takeover fail state."), TEXT("Mobilization improves readiness but costs treasury and can raise unrest."), State.AdvisorSystem.GuidanceLevel, 1 }
    };
    State.EventSystem.LastEventTurn = 0;
    State.EventSystem.EventCounter = 0;
    State.EventSystem.ActiveEventLimit = 3;
    State.EconomyBudget.TaxRate = FMath::Clamp(20 + DifficultyProfile.CountrySizeScore * 2, 18, 32);
    State.EconomyBudget.TaxPolicy = TEXT("Balanced Taxation");
    State.EconomyBudget.PublicServicesSpending = FMath::Clamp(40 - DifficultyProfile.CountrySizeScore * 3, 24, 40);
    State.EconomyBudget.InfrastructureSpending = FMath::Clamp(28 - DifficultyProfile.CountrySizeScore, 20, 30);
    State.EconomyBudget.DefenseSpending = FMath::Clamp(22 + DifficultyProfile.CountrySizeScore * 3, 24, 38);
    State.EconomyBudget.Debt = FMath::Clamp((DifficultyProfile.CountrySizeScore - 1) * 120, 0, 500);
    State.EconomyBudget.Income = DifficultyProfile.StartingTreasury / 10;
    State.EconomyBudget.Expenses = State.EconomyBudget.PublicServicesSpending + State.EconomyBudget.InfrastructureSpending + State.EconomyBudget.DefenseSpending;
    State.EconomyBudget.Deficit = State.EconomyBudget.Expenses - State.EconomyBudget.Income;
    State.EconomyBudget.Inflation = FMath::Clamp(2 + DifficultyProfile.CountrySizeScore, 2, 7);
    State.EconomyBudget.PublicServices = FMath::Clamp(55 + DifficultyProfile.StartingApproval / 8 - DifficultyProfile.CountrySizeScore * 4, 35, 70);
    State.EconomyBudget.ProductionEfficiency = FMath::Clamp(45 + State.PlayerCountry.Infrastructure / 3 + State.PlayerCountry.EconomicHealth / 5, 35, 80);
    State.EconomyBudget.SpendingPosture = TEXT("Balanced Services");
    State.EconomyBudget.LastBudgetSummary = TEXT("Initial budget loaded from difficulty profile.");
    State.Demographics.TotalPopulationThousands = FMath::Clamp(1800 * DifficultyProfile.CountrySizeScore + DifficultyProfile.StartingApproval * 20, 1500, 12000);
    State.Demographics.AverageGroupApproval = State.PlayerCountry.PublicApproval;
    State.Demographics.AverageRegionalApproval = State.PlayerCountry.PublicApproval;
    State.Demographics.NationalNeedsPressure = 0;
    State.Demographics.DemographicUnrestPressure = State.PlayerCountry.Unrest / 3;
    State.Demographics.NationalUnrestSources = { TEXT("Initial political pressure"), TEXT("Cost of living concerns") };
    State.Demographics.CitizenGroups = {
        { TEXT("Urban Workers"), 28, State.PlayerCountry.PublicApproval, 48, 48, 58, 46, 50, 12, { TEXT("Housing costs"), TEXT("Wage pressure") } },
        { TEXT("Rural Communities"), 18, State.PlayerCountry.PublicApproval - 2, 55, 45, 48, 52, 46, 10, { TEXT("Transport access"), TEXT("Farm input costs") } },
        { TEXT("Business Owners"), 12, State.PlayerCountry.PublicApproval + 3, 42, 42, 62, 48, 42, 8, { TEXT("Tax uncertainty"), TEXT("Market confidence") } },
        { TEXT("Public Sector"), 14, State.PlayerCountry.PublicApproval + 1, 45, 45, 52, 56, 55, 8, { TEXT("Budget reliability") } },
        { TEXT("Youth and Students"), 16, State.PlayerCountry.PublicApproval - 3, 44, 44, 64, 42, 52, 14, { TEXT("Jobs outlook"), TEXT("Civil rights") } },
        { TEXT("Retirees"), 12, State.PlayerCountry.PublicApproval, 50, 50, 35, 55, 64, 9, { TEXT("Healthcare access"), TEXT("Savings security") } }
    };
    State.Demographics.Regions = {
        { TEXT("Capital District"), Climate, 22, State.PlayerCountry.PublicApproval + 2, State.PlayerCountry.Stability, State.PlayerCountry.Unrest, 58, 58, 62, 56, State.PlayerCountry.Infrastructure, { TEXT("Visibility of national politics") } },
        { TEXT("Northern Region"), TEXT("Northern Cold"), 18, State.PlayerCountry.PublicApproval, State.PlayerCountry.Stability, State.PlayerCountry.Unrest, 52, 50, 50, 54, State.PlayerCountry.Infrastructure - 4, { TEXT("Heating and transport costs") } },
        { TEXT("Central Region"), TEXT("Middle Moderate"), 28, State.PlayerCountry.PublicApproval + 1, State.PlayerCountry.Stability, State.PlayerCountry.Unrest, 58, 57, 56, 52, State.PlayerCountry.Infrastructure, { TEXT("Cost of living") } },
        { TEXT("Southern Region"), TEXT("Southern Tropical"), 20, State.PlayerCountry.PublicApproval - 1, State.PlayerCountry.Stability - 1, State.PlayerCountry.Unrest + 1, 55, 60, 52, 50, State.PlayerCountry.Infrastructure - 2, { TEXT("Storm readiness"), TEXT("Water management") } },
        { TEXT("Border Region"), Climate, 12, State.PlayerCountry.PublicApproval - 2, State.PlayerCountry.Stability - 2, State.PlayerCountry.Unrest + 2, 50, 50, 48, 45, State.PlayerCountry.Infrastructure - 5, { TEXT("Border security"), TEXT("Trade disruption") } }
    };
    State.ResourceChains.LastUpdatedTurn = State.Turn;
    State.ResourceChains.TotalShortagePressure = 0;
    State.ResourceChains.TradeBalance = 0;
    State.ResourceChains.Summary = TEXT("Initial production chain loaded. Step the simulation to calculate live production, consumption, trade, reserves, and shortages.");
    State.ResourceChains.Chains = {
        { TEXT("Food"), 0, 0, 0, 0, State.PlayerCountry.Resources.Food, 160, 0, FMath::Max(0, State.PlayerCountry.Resources.Food - 160), 90, TEXT("Feeds the population; shortages quickly raise unrest and lower approval."), TEXT("Pending first simulation tick."), { TEXT("Agriculture"), TEXT("climate"), TEXT("infrastructure") } },
        { TEXT("Water"), 0, 0, 0, 0, State.PlayerCountry.Resources.Water, 140, 0, FMath::Max(0, State.PlayerCountry.Resources.Water - 140), 95, TEXT("Supports health, services, and regional stability; shortages hurt demographics."), TEXT("Pending first simulation tick."), { TEXT("climate"), TEXT("public services"), TEXT("infrastructure") } },
        { TEXT("Fuel"), 0, 0, 0, 0, State.PlayerCountry.Resources.GasOil, 90, 0, FMath::Max(0, State.PlayerCountry.Resources.GasOil - 90), 100, TEXT("Powers logistics, industry, and military readiness; shortages increase inflation."), TEXT("Pending first simulation tick."), { TEXT("extraction"), TEXT("trade"), TEXT("defense demand") } },
        { TEXT("Wood"), 0, 0, 0, 0, State.PlayerCountry.Resources.Wood, 80, 0, FMath::Max(0, State.PlayerCountry.Resources.Wood - 80), 60, TEXT("Supports construction, repairs, and disaster recovery."), TEXT("Pending first simulation tick."), { TEXT("forestry"), TEXT("environment"), TEXT("infrastructure spending") } },
        { TEXT("Metals"), 0, 0, 0, 0, State.PlayerCountry.Resources.Metals, 85, 0, FMath::Max(0, State.PlayerCountry.Resources.Metals - 85), 85, TEXT("Supports industry, infrastructure, and military production."), TEXT("Pending first simulation tick."), { TEXT("mining"), TEXT("industrial policy"), TEXT("defense demand") } }
    };
    State.Departments.LastUpdatedTurn = State.Turn;
    State.Departments.Coordination = FMath::Clamp(58 - DifficultyProfile.CountrySizeScore * 3, 35, 70);
    State.Departments.Summary = TEXT("Initial ministries formed. Use the Departments Desk to assign actions and connect policies to execution.");
    State.Departments.Departments = {
        { TEXT("Defense"), TEXT("Minister of Defense"), TEXT("Military readiness, invasion risk, border security, and mobilization."), State.EconomyBudget.DefenseSpending, 55, State.PlayerCountry.MilitaryReadiness, 50, 55, TEXT("Maintain Readiness"), TEXT("Military policy"), TEXT("Keep readiness above takeover warning thresholds."), { TEXT("Improves military readiness when funded."), TEXT("High tempo can increase unrest and fuel demand.") } },
        { TEXT("Treasury"), TEXT("Treasury Secretary"), TEXT("Taxes, spending, debt, inflation, reserves, and emergency funding."), State.EconomyBudget.TaxRate, 60, State.PlayerCountry.EconomicHealth, 52, 55, TEXT("Stabilize Budget"), TEXT("Tax policy and spending posture"), TEXT("Keep deficit and inflation under control without breaking approval."), { TEXT("Improves treasury and budget clarity."), TEXT("Austerity can reduce public trust.") } },
        { TEXT("Agriculture"), TEXT("Minister of Agriculture"), TEXT("Food production, rural regions, farm inputs, and food shortage relief."), 12, 52, State.PlayerCountry.Resources.Food / 4, 50, 50, TEXT("Boost Food Supply"), TEXT("Environmental and resource policy"), TEXT("Food shortages drive unrest quickly."), { TEXT("Improves food production and rural approval."), TEXT("Requires water, infrastructure, and treasury support.") } },
        { TEXT("Energy"), TEXT("Minister of Energy"), TEXT("Fuel reserves, power grid, imports, exports, and industrial energy demand."), 12, 50, State.PlayerCountry.Resources.GasOil / 3, 48, 50, TEXT("Secure Fuel Supply"), TEXT("Extraction and trade policy"), TEXT("Fuel shortages weaken military readiness and raise inflation."), { TEXT("Improves fuel availability and production efficiency."), TEXT("Extraction can hurt environmental health.") } },
        { TEXT("Health"), TEXT("Minister of Health"), TEXT("Public health, crisis response, water safety, and public services."), State.EconomyBudget.PublicServicesSpending / 2, 55, State.EconomyBudget.PublicServices, 55, 50, TEXT("Maintain Public Health"), TEXT("Public services and welfare policy"), TEXT("Health capacity improves stability during shortages and disasters."), { TEXT("Improves public trust and lowers needs pressure."), TEXT("Requires steady spending.") } },
        { TEXT("Education"), TEXT("Minister of Education"), TEXT("Schools, workforce training, technology growth, and youth approval."), 10, 50, 50 + State.PlayerCountry.Technology * 5, 54, 45, TEXT("Workforce Training"), TEXT("Long-term technology and public services"), TEXT("Education is a slow-burn way to raise production and approval."), { TEXT("Improves technology and jobs over time."), TEXT("Short-term crisis impact is limited.") } },
        { TEXT("Infrastructure"), TEXT("Minister of Infrastructure"), TEXT("Roads, logistics, utilities, construction, repairs, and disaster resilience."), State.EconomyBudget.InfrastructureSpending, 54, State.PlayerCountry.Infrastructure, 50, 55, TEXT("Repair Critical Systems"), TEXT("Infrastructure spending and resource chains"), TEXT("Infrastructure supports every resource chain."), { TEXT("Improves production efficiency and regional stability."), TEXT("Consumes wood, metals, and treasury.") } }
    };
    State.ApprovalStability.LastUpdatedTurn = State.Turn;
    State.ApprovalStability.NetApprovalPressure = 0;
    State.ApprovalStability.NetUnrestPressure = State.PlayerCountry.Unrest / 4;
    State.ApprovalStability.NetStabilityPressure = FMath::Max(0, 60 - State.PlayerCountry.Stability) / 2;
    State.ApprovalStability.Summary = TEXT("Initial cause model seeded. Step the simulation to calculate live approval, unrest, and stability drivers.");
    State.ApprovalStability.Causes = {
        { TEXT("Food Shortage"), TEXT("Resources"), 0, 0, 0, 0, TEXT("Food reserves"), TEXT("Initial food pressure pending first simulation tick."), { TEXT("Boost Agriculture"), TEXT("Import Food"), TEXT("Use emergency relief") } },
        { TEXT("Water Access"), TEXT("Resources"), 0, 0, 0, 0, TEXT("Water reserves"), TEXT("Initial water access pending first simulation tick."), { TEXT("Improve public services"), TEXT("Repair utilities"), TEXT("Import water") } },
        { TEXT("Tax Burden"), TEXT("Economy"), 0, 0, 0, 0, TEXT("Tax rate"), TEXT("Initial tax burden loaded from difficulty."), { TEXT("Lower taxes"), TEXT("Balance services before raising rates") } },
        { TEXT("Inflation"), TEXT("Economy"), 0, 0, 0, 0, TEXT("Inflation"), TEXT("Initial cost of living pressure loaded."), { TEXT("Improve production efficiency"), TEXT("Reduce shortages"), TEXT("Stabilize the budget") } },
        { TEXT("Unemployment"), TEXT("Economy"), 0, 0, 0, 0, TEXT("Economic health and jobs"), TEXT("Initial job pressure pending demographics review."), { TEXT("Use workforce training"), TEXT("Support industry"), TEXT("Improve infrastructure") } },
        { TEXT("War Fatigue"), TEXT("Security"), 0, 0, 0, 0, TEXT("Military policy and invasion risk"), TEXT("Initial security fatigue pending threat review."), { TEXT("Lower mobilization tempo"), TEXT("Improve diplomacy"), TEXT("Use defense funding only when needed") } },
        { TEXT("Corruption"), TEXT("Legitimacy"), 0, 0, 0, 0, TEXT("Scandals and emergency authority"), TEXT("Initial legitimacy pressure pending events."), { TEXT("Resolve scandals transparently"), TEXT("Avoid repeated emergency powers"), TEXT("Reform departments") } },
        { TEXT("Public Services"), TEXT("Services"), 0, 0, 0, 0, TEXT("Public services"), TEXT("Initial service confidence loaded."), { TEXT("Increase public services spending"), TEXT("Strengthen Health ministry") } },
        { TEXT("Infrastructure Reliability"), TEXT("Infrastructure"), 0, 0, 0, 0, TEXT("Infrastructure"), TEXT("Initial infrastructure condition loaded."), { TEXT("Repair critical systems"), TEXT("Secure wood and metals"), TEXT("Fund infrastructure") } }
    };
    State.PressOffice.Credibility = FMath::Clamp(72 - DifficultyProfile.CountrySizeScore * 4, 52, 76);
    State.PressOffice.ConsecutiveEmptyAnnouncements = 0;
    State.PressOffice.ConsecutiveFalseAnnouncements = 0;
    State.PressOffice.TotalAnnouncements = 0;
    State.PressOffice.LastUpdatedTurn = State.Turn;
    State.PressOffice.MaxRecords = 40;
    State.PressOffice.LastAnnouncementSummary = TEXT("Press credibility initialized. Announcements can calm the public, support diplomacy, or damage trust if abused.");
    State.MeetingSystem.TotalMeetings = 0;
    State.MeetingSystem.AdvisorCoordination = FMath::Clamp(56 - DifficultyProfile.CountrySizeScore * 3, 40, 60);
    State.MeetingSystem.ForeignTrust = FMath::Clamp(54 - DifficultyProfile.CountrySizeScore * 4, 34, 58);
    State.MeetingSystem.LastUpdatedTurn = State.Turn;
    State.MeetingSystem.MaxRecords = 50;
    State.MeetingSystem.LastMeetingSummary = TEXT("Meeting system initialized. Advisor and foreign official agendas can now update the simulation state.");
    State.DevelopmentSystem.ActiveFocus = TEXT("Infrastructure");
    State.DevelopmentSystem.DevelopmentPoints = 0;
    State.DevelopmentSystem.LastUpdatedTurn = State.Turn;
    State.DevelopmentSystem.Summary = TEXT("Long-term development tracks initialized. Choose a focus from the Development Desk to pursue strategic upgrades.");
    State.DevelopmentSystem.Tracks = {
        { TEXT("Infrastructure"), TEXT("Infrastructure"), 1, 0, 100, 28, 4, 3, 1, TEXT("National Logistics Upgrade"), TEXT("Raises infrastructure, production efficiency, and resource-chain resilience."), { TEXT("Better roads"), TEXT("Grid reliability"), TEXT("Disaster response capacity") } },
        { TEXT("Military"), TEXT("Military"), 1, 0, 110, 32, 1, 5, 4, TEXT("Readiness Modernization"), TEXT("Raises military readiness and reduces foreign takeover pressure."), { TEXT("Modern logistics"), TEXT("Border surveillance"), TEXT("Rapid response doctrine") } },
        { TEXT("Agriculture"), TEXT("Agriculture"), 1, 0, 95, 24, 2, 2, 1, TEXT("Food Security Program"), TEXT("Improves food and water production, lowering shortage-driven unrest."), { TEXT("Irrigation"), TEXT("Storage networks"), TEXT("Climate-hardy crops") } },
        { TEXT("Industry"), TEXT("Industry"), 1, 0, 115, 34, 2, 5, 3, TEXT("Industrial Base Expansion"), TEXT("Raises economic health, production efficiency, and metals output."), { TEXT("Manufacturing clusters"), TEXT("Materials processing"), TEXT("Skilled workforce") } },
        { TEXT("Communications"), TEXT("Communications"), 1, 0, 90, 22, 1, 2, 1, TEXT("National Communications Network"), TEXT("Improves stability, press credibility, advisor coordination, and public trust."), { TEXT("Emergency alerts"), TEXT("Public information systems"), TEXT("Government coordination") } }
    };
    State.DecisionHistory.LastUpdatedTurn = State.Turn;
    State.DecisionHistory.MaxRecords = 80;
    State.DecisionHistory.Summary = TEXT("State founded. Major player decisions and consequences will be logged here for ongoing briefings.");
    State.DecisionHistory.Records = {
        { State.Turn, TEXT("State Creation"), TEXT("Initial State Created"), FString::Printf(TEXT("Difficulty %s, climate %s, address %s."), *DifficultyProfile.Name, *Climate, *AddressTitle), TEXT("Initial simulation state, world map, resource chains, departments, and advisory systems created."), State.PlayerCountry.PublicApproval, State.PlayerCountry.Stability, State.PlayerCountry.Unrest, State.PlayerCountry.Treasury, State.PlayerCountry.EconomicHealth, State.PlayerCountry.MilitaryReadiness, 10, TEXT("Initial Save"), { TEXT("creation"), TEXT("briefing") } }
    };
    State.WorldMap = BuildWorldMapState(DifficultyProfile, StateName, Climate);

    State.RtsWorld.SimulationSecond = 0;
    State.RtsWorld.ControlledTerritories = DifficultyProfile.CountrySizeScore * 3;
    State.RtsWorld.BorderTerritories = DifficultyProfile.CountrySizeScore + 1;
    State.RtsWorld.KnownRivalCountries = FMath::Clamp(DifficultyProfile.CountrySizeScore + 1, 2, 5);
    State.RtsWorld.ActiveStrategicLayers = { TEXT("Territory"), TEXT("Resources"), TEXT("Diplomacy"), TEXT("Military Pressure") };

    State.RtsWorld.Rivals = {
        { TEXT("Northmark"), TEXT("Pragmatic"), TEXT("Cautious"), 42 + DifficultyProfile.CountrySizeScore * 6, 12 + DifficultyProfile.CountrySizeScore * 4, 18 },
        { TEXT("Eastmere"), TEXT("Commercial"), TEXT("Neutral"), 38 + DifficultyProfile.CountrySizeScore * 5, 8 + DifficultyProfile.CountrySizeScore * 3, 30 },
        { TEXT("Southport Union"), TEXT("Assertive"), TEXT("Tense"), 48 + DifficultyProfile.CountrySizeScore * 7, 18 + DifficultyProfile.CountrySizeScore * 5, 12 }
    };

    return State;
}




