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
        WorldMap.PlanetName = TEXT("Dulia");
        WorldMap.MapDataVersion = TEXT("DuliaMapData.v1");
        WorldMap.DurableCountryTarget = 195;
        WorldMap.ActiveCountryCount = StartingCountryCountForDifficulty(DifficultyProfile);
        WorldMap.TotalCountryCount = WorldMap.DurableCountryTarget;
        WorldMap.TotalMapRegionCount = 8;
        WorldMap.GenerationRule = TEXT("Planet Dulia has a durable 195-country map across eight permanent continents. Difficulty controls active starting pressure, resources, advisor help, and warning lead time; it no longer changes the durable country count.");

        const int32 BaseCountriesPerContinent = WorldMap.DurableCountryTarget / 8;
        const int32 RemainderCountries = WorldMap.DurableCountryTarget % 8;
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
                Country.CountryId = FString::Printf(TEXT("DUL-C%03d"), GlobalCountryIndex + 1);
                Country.MapRegionId = FString::Printf(TEXT("DUL-R%02d"), ContinentIndex + 1);
                Country.MapCountryIndex = GlobalCountryIndex + 1;
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
                    Country.DesiredProvinceCount = FMath::Clamp(4 + DifficultyProfile.CountrySizeScore, 4, 10);
                    Country.PopulationWeight = FMath::Clamp(55 + DifficultyProfile.CountrySizeScore * 12, 40, 120);
                    Country.AreaWeight = FMath::Clamp(45 + DifficultyProfile.CountrySizeScore * 15, 35, 130);
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
                    Country.DesiredProvinceCount = FMath::Clamp(3 + Country.PowerScore / 28 + Country.BorderPressure / 45, 2, 10);
                    Country.PopulationWeight = FMath::Clamp(25 + ((GlobalCountryIndex * 11 + ContinentIndex * 7) % 85) + Country.PowerScore / 6, 20, 140);
                    Country.AreaWeight = FMath::Clamp(20 + ((GlobalCountryIndex * 17 + ContinentIndex * 5) % 95) + Country.PowerScore / 8, 20, 150);

                    if (Country.bAlliedWithPlayer)
                    {
                        ++WorldMap.DemocraticAllyCount;
                    }
                    else if (!Country.PoliticalType.Contains(TEXT("Democratic")))
                    {
                        ++WorldMap.NonDemocraticCountryCount;
                    }
                }

                WorldMap.TotalProvinceCount += Country.DesiredProvinceCount;
                Continent.Countries.Add(Country);
                ++GlobalCountryIndex;
            }

            WorldMap.Continents.Add(Continent);
        }

        WorldMap.MapDataSummary = FString::Printf(TEXT("Planet %s map data v1: %d durable countries, %d planned provinces, %d continent regions, %d active starting countries for %s difficulty."), *WorldMap.PlanetName, WorldMap.TotalCountryCount, WorldMap.TotalProvinceCount, WorldMap.TotalMapRegionCount, WorldMap.ActiveCountryCount, *DifficultyProfile.Name);
        return WorldMap;
    }


    FString RelationshipStatusForAlignment(const FString& Alignment)
    {
        if (Alignment.Equals(TEXT("Player"), ESearchCase::IgnoreCase) || Alignment.Equals(TEXT("Allied"), ESearchCase::IgnoreCase))
        {
            return TEXT("Ally");
        }
        if (Alignment.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase))
        {
            return TEXT("Hostile");
        }
        if (Alignment.Equals(TEXT("Tense"), ESearchCase::IgnoreCase))
        {
            return TEXT("Rival");
        }
        return TEXT("Neutral");
    }

    FDemocracyDiplomacyMatrixState BuildDiplomacyMatrixState(const FDemocracyWorldMapState& WorldMap, int32 CurrentTurn)
    {
        FDemocracyDiplomacyMatrixState Matrix;
        Matrix.LastUpdatedTurn = CurrentTurn;
        int32 BorderTensionTotal = 0;

        for (const FDemocracyContinentState& Continent : WorldMap.Continents)
        {
            for (const FDemocracyGeneratedCountryState& Country : Continent.Countries)
            {
                FDemocracyDiplomacyRelationshipState Relationship;
                Relationship.CountryName = Country.CountryName;
                Relationship.ContinentName = Country.ContinentName;
                Relationship.GovernmentType = Country.PoliticalType;
                Relationship.RelationshipStatus = RelationshipStatusForAlignment(Country.DiplomaticAlignment);
                Relationship.BorderTension = Country.BorderPressure;
                Relationship.LastChangedTurn = CurrentTurn;
                Relationship.Trust = FMath::Clamp(Country.Stability + (Country.bAlliedWithPlayer ? 20 : 0) - Country.BorderPressure / 2, 0, 100);
                Relationship.bTradePartner = Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase) ||
                    (Relationship.RelationshipStatus.Equals(TEXT("Neutral"), ESearchCase::IgnoreCase) && Relationship.Trust >= 45);
                Relationship.bSanctionsActive = Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase) && Country.BorderPressure >= 55;
                Relationship.TreatyStatus = Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase) ? TEXT("Mutual Recognition") : TEXT("None");
                Relationship.TradeValue = Relationship.bTradePartner ? FMath::Clamp(Country.PowerScore / 2 + Relationship.Trust / 3 - Country.BorderPressure / 4, 5, 80) : 0;
                if (!Relationship.TreatyStatus.Equals(TEXT("None"), ESearchCase::IgnoreCase))
                {
                    Relationship.ActiveTreaties.Add(Relationship.TreatyStatus);
                }
                if (Relationship.bSanctionsActive)
                {
                    Relationship.Notes.Add(TEXT("Sanctions active due to hostile posture and border pressure."));
                }
                if (Relationship.BorderTension >= 60)
                {
                    Relationship.Notes.Add(TEXT("High border tension; this country can drive invasion risk."));
                }
                if (Relationship.bTradePartner)
                {
                    Relationship.Notes.Add(TEXT("Trade route available for resource-chain imports and exports."));
                }

                if (Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase)) { ++Matrix.AllyCount; }
                else if (Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase)) { ++Matrix.RivalCount; }
                else if (Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase)) { ++Matrix.HostileCount; }
                else { ++Matrix.NeutralCount; }
                if (Relationship.bTradePartner) { ++Matrix.TradePartnerCount; }
                if (Relationship.bSanctionsActive) { ++Matrix.SanctionsCount; }
                if (Relationship.ActiveTreaties.Num() > 0) { ++Matrix.TreatyCount; }
                BorderTensionTotal += Relationship.BorderTension;
                Matrix.Relationships.Add(Relationship);
            }
        }

        Matrix.AverageBorderTension = Matrix.Relationships.Num() > 0 ? BorderTensionTotal / Matrix.Relationships.Num() : 0;
        Matrix.Summary = FString::Printf(TEXT("Diplomacy matrix initialized: %d allies, %d neutral, %d rivals, %d hostile, %d trade partners, %d sanctions, average border tension %d."),
            Matrix.AllyCount, Matrix.NeutralCount, Matrix.RivalCount, Matrix.HostileCount, Matrix.TradePartnerCount, Matrix.SanctionsCount, Matrix.AverageBorderTension);
        return Matrix;
    }



    bool IsGovernmentRuleDemocracy(const FString& GovernmentType)
    {
        return GovernmentType.Contains(TEXT("Democracy"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Democratic"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Republic"), ESearchCase::IgnoreCase);
    }

    bool IsGovernmentRuleDictatorship(const FString& GovernmentType)
    {
        return GovernmentType.Contains(TEXT("Dictatorship"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Authoritarian"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Autocracy"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Regime"), ESearchCase::IgnoreCase) ||
            GovernmentType.Contains(TEXT("Hostile Bloc"), ESearchCase::IgnoreCase);
    }

    FString GovernmentRuleAlignment(const FString& GovernmentType)
    {
        if (IsGovernmentRuleDemocracy(GovernmentType))
        {
            return TEXT("Democracy");
        }
        if (IsGovernmentRuleDictatorship(GovernmentType))
        {
            return TEXT("Dictatorship");
        }
        return TEXT("Non-Aligned");
    }

    bool IsSameGovernmentRuleAlignment(const FString& Left, const FString& Right)
    {
        const FString LeftAlignment = GovernmentRuleAlignment(Left);
        const FString RightAlignment = GovernmentRuleAlignment(Right);
        return !LeftAlignment.Equals(TEXT("Non-Aligned"), ESearchCase::IgnoreCase) && LeftAlignment.Equals(RightAlignment, ESearchCase::IgnoreCase);
    }
    FString ProvinceResourceFocus(int32 ProvinceIndex, const FString& Climate)
    {
        static const TCHAR* Focuses[] = { TEXT("Food"), TEXT("Fuel"), TEXT("Wood"), TEXT("Metals"), TEXT("Water") };
        if (Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase))
        {
            static const TCHAR* NorthernFocuses[] = { TEXT("Fuel"), TEXT("Metals"), TEXT("Wood"), TEXT("Water"), TEXT("Food") };
            return NorthernFocuses[ProvinceIndex % 5];
        }
        if (Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase))
        {
            static const TCHAR* SouthernFocuses[] = { TEXT("Food"), TEXT("Water"), TEXT("Wood"), TEXT("Fuel"), TEXT("Metals") };
            return SouthernFocuses[ProvinceIndex % 5];
        }
        return Focuses[ProvinceIndex % 5];
    }

    int32 DesiredProvinceCountForCountry(const FDemocracyGeneratedCountryState& Country, bool bPlayerCountry, int32 PlayerProvinceTarget)
    {
        if (Country.DesiredProvinceCount > 0)
        {
            return FMath::Clamp(Country.DesiredProvinceCount, 2, 10);
        }
        return FMath::Clamp(3 + Country.PowerScore / 28 + Country.BorderPressure / 45 + (bPlayerCountry ? PlayerProvinceTarget / 5 : 0), 2, 10);
    }

    FString TerrainTypeForProvince(int32 ProvinceIndex, const FString& Climate, const FString& ResourceFocus)
    {
        if (Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase))
        {
            return ProvinceIndex % 3 == 0 ? TEXT("Mountain") : TEXT("Tundra");
        }
        if (Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase))
        {
            return ResourceFocus.Equals(TEXT("Food"), ESearchCase::IgnoreCase) ? TEXT("Rainforest Farmland") : TEXT("Coastal Jungle");
        }
        if (ResourceFocus.Equals(TEXT("Metals"), ESearchCase::IgnoreCase))
        {
            return TEXT("Highlands");
        }
        if (ResourceFocus.Equals(TEXT("Fuel"), ESearchCase::IgnoreCase))
        {
            return TEXT("Basin");
        }
        return ProvinceIndex % 2 == 0 ? TEXT("Plains") : TEXT("Urban Corridor");
    }

    void RecalculateMapOwnership(FDemocracyMapOwnershipState& Ownership)
    {
        Ownership.TotalCountries = Ownership.Countries.Num();
        Ownership.TotalProvinces = Ownership.Provinces.Num();
        Ownership.PlayerControlledProvinces = 0;
        Ownership.ContestedProvinces = 0;
        Ownership.BorderProvinceCount = 0;
        Ownership.TotalMapRegionCount = Ownership.Continents.Num();
        Ownership.TotalPopulationWeight = 0;
        Ownership.TotalAreaWeight = 0;
        for (FDemocracyCountryOwnershipState& Country : Ownership.Countries)
        {
            Country.TotalProvinces = Country.ProvinceIds.Num();
            Country.ControlledProvinces = 0;
            Country.OccupiedProvinces = 0;
            Country.LostProvinces = 0;
            Country.BorderProvinces = 0;
            Country.ResourceBase = 0;
            Country.MilitaryValue = 0;
        }
        for (FDemocracyContinentOwnershipState& Continent : Ownership.Continents)
        {
            Continent.ProvinceCount = 0;
            Continent.PlayerControlledProvinces = 0;
            Continent.ContestedProvinces = 0;
        }
        for (const FDemocracyProvinceOwnershipState& Province : Ownership.Provinces)
        {
            if (Province.bPlayerControlled) ++Ownership.PlayerControlledProvinces;
            if (!Province.CurrentControllerCountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase)) ++Ownership.ContestedProvinces;
            if (Province.bBorderProvince) ++Ownership.BorderProvinceCount;
            Ownership.TotalPopulationWeight += Province.PopulationWeight;
            Ownership.TotalAreaWeight += Province.AreaWeight;
            for (FDemocracyCountryOwnershipState& Country : Ownership.Countries)
            {
                if (Country.CountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase))
                {
                    if (Province.CurrentControllerCountryName.Equals(Country.CountryName, ESearchCase::IgnoreCase)) ++Country.ControlledProvinces;
                    else ++Country.LostProvinces;
                    if (Province.bBorderProvince) ++Country.BorderProvinces;
                    Country.ResourceBase += Province.StrategicValue;
                    Country.MilitaryValue += Province.bBorderProvince ? Province.StrategicValue * 2 : Province.StrategicValue;
                    break;
                }
            }
            for (FDemocracyCountryOwnershipState& Country : Ownership.Countries)
            {
                if (!Province.CurrentControllerCountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase) && Country.CountryName.Equals(Province.CurrentControllerCountryName, ESearchCase::IgnoreCase))
                {
                    ++Country.OccupiedProvinces;
                    break;
                }
            }
            for (FDemocracyContinentOwnershipState& Continent : Ownership.Continents)
            {
                if (Continent.ContinentName.Equals(Province.ContinentName, ESearchCase::IgnoreCase))
                {
                    ++Continent.ProvinceCount;
                    if (Province.bPlayerControlled) ++Continent.PlayerControlledProvinces;
                    if (!Province.CurrentControllerCountryName.Equals(Province.OriginalCountryName, ESearchCase::IgnoreCase)) ++Continent.ContestedProvinces;
                    break;
                }
            }
        }
        Ownership.Summary = FString::Printf(TEXT("%s ownership: %d durable countries, %d provinces, %d regions, %d player controlled, %d contested, %d border provinces."), *Ownership.PlanetName, Ownership.TotalCountries, Ownership.TotalProvinces, Ownership.TotalMapRegionCount, Ownership.PlayerControlledProvinces, Ownership.ContestedProvinces, Ownership.BorderProvinceCount);
    }

    FDemocracyMapOwnershipState BuildMapOwnershipState(const FDemocracyWorldMapState& WorldMap, const FString& PlayerCountryName, int32 CurrentTurn, int32 PlayerProvinceTarget)
    {
        FDemocracyMapOwnershipState Ownership;
        Ownership.PlanetName = WorldMap.PlanetName;
        Ownership.MapDataVersion = WorldMap.MapDataVersion;
        Ownership.DurableCountryTarget = WorldMap.DurableCountryTarget;
        Ownership.LastUpdatedTurn = CurrentTurn;
        Ownership.PlayerCountryName = PlayerCountryName;
        int32 GlobalCountryIndex = 0;
        int32 PlayerProvincesRemaining = FMath::Max(1, PlayerProvinceTarget);
        for (const FDemocracyContinentState& Continent : WorldMap.Continents)
        {
            FDemocracyContinentOwnershipState ContinentOwnership;
            ContinentOwnership.ContinentName = Continent.ContinentName;
            ContinentOwnership.Climate = Continent.Climate;
            ContinentOwnership.CountryCount = Continent.Countries.Num();
            for (const FDemocracyGeneratedCountryState& Country : Continent.Countries)
            {
                const bool bPlayerCountry = Country.CountryName.Equals(PlayerCountryName, ESearchCase::IgnoreCase);
                const int32 ProvinceCount = DesiredProvinceCountForCountry(Country, bPlayerCountry, PlayerProvinceTarget);
                FDemocracyCountryOwnershipState CountryOwnership;
                CountryOwnership.CountryId = Country.CountryId.IsEmpty() ? FString::Printf(TEXT("DUL-C%03d"), GlobalCountryIndex + 1) : Country.CountryId;
                CountryOwnership.MapRegionId = Country.MapRegionId.IsEmpty() ? FString::Printf(TEXT("DUL-R%02d"), Ownership.Continents.Num() + 1) : Country.MapRegionId;
                CountryOwnership.MapCountryIndex = Country.MapCountryIndex > 0 ? Country.MapCountryIndex : GlobalCountryIndex + 1;
                CountryOwnership.PopulationWeight = Country.PopulationWeight;
                CountryOwnership.AreaWeight = Country.AreaWeight;
                CountryOwnership.CountryName = Country.CountryName;
                CountryOwnership.ContinentName = Country.ContinentName;
                CountryOwnership.GovernmentType = Country.PoliticalType;
                CountryOwnership.bPlayerCountry = bPlayerCountry;
                CountryOwnership.bCapitalControlled = true;
                ContinentOwnership.CountryNames.Add(Country.CountryName);
                for (int32 ProvinceIndex = 0; ProvinceIndex < ProvinceCount; ++ProvinceIndex)
                {
                    FDemocracyProvinceOwnershipState Province;
                    Province.CountryId = CountryOwnership.CountryId;
                    Province.MapRegionId = CountryOwnership.MapRegionId;
                    Province.ProvinceIndex = ProvinceIndex + 1;
                    Province.ProvinceId = FString::Printf(TEXT("%s-P%02d"), *CountryOwnership.CountryId, Province.ProvinceIndex);
                    Province.ProvinceName = ProvinceIndex == 0 ? FString::Printf(TEXT("%s Capital District"), *Country.CountryName) : FString::Printf(TEXT("%s Province %d"), *Country.CountryName, ProvinceIndex + 1);
                    Province.ContinentName = Country.ContinentName;
                    Province.OriginalCountryName = Country.CountryName;
                    Province.CurrentOwnerCountryName = Country.CountryName;
                    Province.CurrentControllerCountryName = Country.CountryName;
                    Province.GovernmentType = Country.PoliticalType;
                    Province.Climate = Country.Climate;
                    Province.ResourceFocus = ProvinceResourceFocus(ProvinceIndex + GlobalCountryIndex, Country.Climate);
                    Province.TerrainType = TerrainTypeForProvince(ProvinceIndex, Country.Climate, Province.ResourceFocus);
                    Province.PopulationWeight = FMath::Max(1, Country.PopulationWeight / ProvinceCount + (ProvinceIndex == 0 ? 4 : 0));
                    Province.AreaWeight = FMath::Max(1, Country.AreaWeight / ProvinceCount + (ProvinceIndex % 3));
                    Province.StrategicValue = FMath::Clamp(1 + Country.PowerScore / 24 + (ProvinceIndex == 0 ? 2 : 0), 1, 8);
                    Province.Stability = FMath::Clamp(Country.Stability - ProvinceIndex % 3, 0, 100);
                    Province.Unrest = FMath::Clamp(100 - Country.Stability + Country.BorderPressure / 3, 0, 100);
                    Province.bPlayerControlled = bPlayerCountry || PlayerProvincesRemaining > 0;
                    if (Province.bPlayerControlled)
                    {
                        Province.CurrentOwnerCountryName = PlayerCountryName;
                        Province.CurrentControllerCountryName = PlayerCountryName;
                        Province.GovernmentType = TEXT("Democracy");
                        --PlayerProvincesRemaining;
                    }
                    Province.bBorderProvince = ProvinceIndex == ProvinceCount - 1 || Country.BorderPressure >= 40 || Province.bPlayerControlled;
                    Province.LastChangedTurn = CurrentTurn;
                    Ownership.Provinces.Add(Province);
                    CountryOwnership.ProvinceIds.Add(Province.ProvinceId);
                }
                Ownership.Countries.Add(CountryOwnership);
                ++GlobalCountryIndex;
            }
            Ownership.Continents.Add(ContinentOwnership);
        }
        RecalculateMapOwnership(Ownership);
        return Ownership;
    }

    FDemocracyCommandAuthorityActionState MakeCommandAuthorityAction(const FString& Id, const FString& Label, const FString& Layer, const FString& Type, const FString& Surface, bool bOffice, bool bRts, int32 Cooldown, int32 Cost, int32 Approval, int32 Stability, int32 Unrest, int32 Diplomacy, int32 Military, int32 InvasionRisk, int32 Resources, const FString& Prereq, const FString& Preview)
    {
        FDemocracyCommandAuthorityActionState Action;
        Action.CommandId = Id;
        Action.Label = Label;
        Action.AuthorityLayer = Layer;
        Action.CommandType = Type;
        Action.ExecutionSurface = Surface;
        Action.bOfficeAllowed = bOffice;
        Action.bRtsViewAllowed = bRts;
        Action.CooldownTurns = Cooldown;
        Action.TreasuryCost = Cost;
        Action.ApprovalDelta = Approval;
        Action.StabilityDelta = Stability;
        Action.UnrestDelta = Unrest;
        Action.DiplomacyDelta = Diplomacy;
        Action.MilitaryDelta = Military;
        Action.InvasionRiskDelta = InvasionRisk;
        Action.ResourceDelta = Resources;
        Action.Prerequisite = Prereq;
        Action.EffectPreview = Preview;
        return Action;
    }

    FDemocracyCommandAuthorityState BuildCommandAuthorityState(const FDemocracySimulationState& State)
    {
        FDemocracyCommandAuthorityState Authority;
        Authority.LastUpdatedTurn = State.Turn;
        Authority.ActiveCommandPosture = State.PlayerCountry.Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase) ? TEXT("Emergency Administration") : TEXT("Civil Administration");
        Authority.OfficeAuthoritySummary = TEXT("Office commands can mobilize reserves, defend borders, negotiate, embargo, trade, send aid, and declare emergency measures through advisors, departments, diplomacy, and policy channels.");
        Authority.RtsAuthoritySummary = TEXT("RTS-view commands can deploy, defend, and contest territory only through the future RTS layer; simulation office receives the outcomes through RTS backflow.");
        Authority.Actions = {
            MakeCommandAuthorityAction(TEXT("office_mobilize_reserves"), TEXT("Mobilize Reserves"), TEXT("Office"), TEXT("Security"), TEXT("Computer/Defense"), true, false, 2, 90, -2, 1, 3, -1, 8, -8, -4, TEXT("Requires treasury and Defense ministry authority."), TEXT("Raises military readiness and lowers invasion risk, but costs treasury, resources, approval, and unrest.")),
            MakeCommandAuthorityAction(TEXT("office_defend_borders"), TEXT("Order Border Defense"), TEXT("Office"), TEXT("Security"), TEXT("Computer/Globe"), true, true, 1, 55, 0, 1, 1, 0, 5, -5, -2, TEXT("Requires active border provinces."), TEXT("Improves readiness and slows invasion pressure without direct troop control from the office.")),
            MakeCommandAuthorityAction(TEXT("office_negotiate"), TEXT("Negotiate With Rivals"), TEXT("Office"), TEXT("Diplomacy"), TEXT("Meeting Room/Globe"), true, false, 1, 35, 1, 1, -1, 6, -1, -6, 0, TEXT("Requires at least one rival or hostile relation."), TEXT("Improves diplomacy and lowers border tension; may look weak if overused during crises.")),
            MakeCommandAuthorityAction(TEXT("office_embargo"), TEXT("Authorize Embargo"), TEXT("Office"), TEXT("Diplomacy"), TEXT("Computer/Globe"), true, false, 3, 25, -1, 0, 1, -4, 0, 3, -2, TEXT("Requires hostile relation or high border tension."), TEXT("Pressures hostile states but damages trade, diplomacy, and resource access.")),
            MakeCommandAuthorityAction(TEXT("office_trade"), TEXT("Expand Trade"), TEXT("Office"), TEXT("Economy"), TEXT("Computer/Treasury"), true, false, 1, 40, 1, 0, -1, 4, 0, -1, 8, TEXT("Requires diplomacy above 30."), TEXT("Improves resources, trade value, and diplomacy at a treasury cost.")),
            MakeCommandAuthorityAction(TEXT("office_aid"), TEXT("Send Foreign Aid"), TEXT("Office"), TEXT("Diplomacy"), TEXT("Meeting Room/Press"), true, false, 2, 70, 1, 1, -1, 5, 0, -2, -3, TEXT("Requires treasury reserve."), TEXT("Builds diplomatic standing and stability but spends treasury and supplies.")),
            MakeCommandAuthorityAction(TEXT("office_emergency"), TEXT("Declare Emergency Measures"), TEXT("Office"), TEXT("Emergency"), TEXT("Computer/Phone"), true, false, 4, 120, -5, 5, -8, -3, 3, -6, -5, TEXT("Requires unrest or invasion pressure."), TEXT("Rapidly reduces unrest and risk while damaging approval, legitimacy, diplomacy, and treasury.")),
            MakeCommandAuthorityAction(TEXT("office_declare_war"), TEXT("Declare War"), TEXT("Office"), TEXT("War"), TEXT("Computer/Globe"), true, true, 8, 180, -8, -6, 10, -15, -4, 18, -8, TEXT("Requires hostile relation or severe border tension and at least 45 military readiness."), TEXT("Creates an active war state, raises escalation and invasion risk, damages diplomacy, and pushes future battles into RTS backflow.")),
            MakeCommandAuthorityAction(TEXT("office_request_alliance_aid"), TEXT("Request Alliance Aid"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Globe"), true, false, 4, 60, 1, 1, -1, 3, 5, -7, 4, TEXT("Requires at least one ally, treaty partner, or high-trust trade partner."), TEXT("Asks allies for support, improving readiness and supplies while logging diplomatic dependence.")),
            MakeCommandAuthorityAction(TEXT("office_negotiate_ceasefire"), TEXT("Negotiate Ceasefire"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Phone"), true, false, 3, 45, 2, 3, -3, 5, -2, -8, 0, TEXT("Requires an active war or severe border conflict."), TEXT("Lowers escalation, invasion risk, and unrest; may reduce military initiative.")),
            MakeCommandAuthorityAction(TEXT("office_surrender_territory"), TEXT("Surrender Territory"), TEXT("Office"), TEXT("War"), TEXT("Meeting Room/Computer"), true, false, 10, 20, -12, -8, 12, 2, -10, -15, -12, TEXT("Requires an active war with high defeat risk or takeover pressure."), TEXT("Reduces immediate invasion pressure but loses territory, resources, approval, and stability.")),
            MakeCommandAuthorityAction(TEXT("office_impose_sanctions"), TEXT("Impose Sanctions"), TEXT("Office"), TEXT("War"), TEXT("Computer/Globe"), true, false, 4, 35, -2, 0, 2, -6, 0, 4, -4, TEXT("Requires hostile relation, rival status, active war, or high border tension."), TEXT("Applies sanctions to a hostile state, increasing pressure but damaging trade and diplomacy.")),
            MakeCommandAuthorityAction(TEXT("rts_deploy_forces"), TEXT("Deploy Forces"), TEXT("RTS"), TEXT("Military"), TEXT("RTS View"), false, true, 1, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Future RTS layer only."), TEXT("Direct troop deployment is not ordered from the simulation office; RTS results flow back after battles.")),
            MakeCommandAuthorityAction(TEXT("rts_attack_operation"), TEXT("Launch Attack Operation"), TEXT("RTS"), TEXT("Military"), TEXT("RTS View"), false, true, 2, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Future RTS layer only."), TEXT("Attack orders belong to the RTS layer and will later create casualties, fatigue, diplomacy, and territory backflow."))
        };
        return Authority;
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
        TEXT("%s\"lastEconomicPolicyTurn\": %d,\n")
        TEXT("%s\"lastEnvironmentalPolicyTurn\": %d,\n")
        TEXT("%s\"lastMilitaryPolicyTurn\": %d,\n")
        TEXT("%s\"lastDiplomacyPolicyTurn\": %d,\n")
        TEXT("%s\"lastCivilPolicyTurn\": %d,\n")
        TEXT("%s\"policyCooldownTurns\": %d,\n")
        TEXT("%s\"lastPolicyChangeSummary\": \"%s\",\n")
        TEXT("%s\"activePolicyEffects\": %s,\n")
        TEXT("%s\"policyRuleStatus\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(EconomicPolicy),
        *Pad, *JsonEscape(EnvironmentalPolicy),
        *Pad, *JsonEscape(MilitaryPolicy),
        *Pad, *JsonEscape(DiplomacyPolicy),
        *Pad, *JsonEscape(CivilPolicy),
        *Pad, PolicyChangeCount,
        *Pad, LastEconomicPolicyTurn,
        *Pad, LastEnvironmentalPolicyTurn,
        *Pad, LastMilitaryPolicyTurn,
        *Pad, LastDiplomacyPolicyTurn,
        *Pad, LastCivilPolicyTurn,
        *Pad, PolicyCooldownTurns,
        *Pad, *JsonEscape(LastPolicyChangeSummary),
        *Pad, *StringArrayToJson(ActivePolicyEffects),
        *Pad, *StringArrayToJson(PolicyRuleStatus),
        *Indent(IndentSpaces - 2));
}
FString FDemocracyDiplomacyRelationshipState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"countryName\": \"%s\",\n")
        TEXT("%s\"continentName\": \"%s\",\n")
        TEXT("%s\"governmentType\": \"%s\",\n")
        TEXT("%s\"relationshipStatus\": \"%s\",\n")
        TEXT("%s\"tradePartner\": %s,\n")
        TEXT("%s\"sanctionsActive\": %s,\n")
        TEXT("%s\"treatyStatus\": \"%s\",\n")
        TEXT("%s\"borderTension\": %d,\n")
        TEXT("%s\"trust\": %d,\n")
        TEXT("%s\"tradeValue\": %d,\n")
        TEXT("%s\"lastChangedTurn\": %d,\n")
        TEXT("%s\"activeTreaties\": %s,\n")
        TEXT("%s\"notes\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CountryName),
        *Pad, *JsonEscape(ContinentName),
        *Pad, *JsonEscape(GovernmentType),
        *Pad, *JsonEscape(RelationshipStatus),
        *Pad, bTradePartner ? TEXT("true") : TEXT("false"),
        *Pad, bSanctionsActive ? TEXT("true") : TEXT("false"),
        *Pad, *JsonEscape(TreatyStatus),
        *Pad, BorderTension,
        *Pad, Trust,
        *Pad, TradeValue,
        *Pad, LastChangedTurn,
        *Pad, *StringArrayToJson(ActiveTreaties),
        *Pad, *StringArrayToJson(Notes),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyDiplomacyMatrixState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString RelationshipPad = Indent(IndentSpaces + 2);
    FString RelationshipJson = TEXT("[");
    for (int32 Index = 0; Index < Relationships.Num(); ++Index)
    {
        RelationshipJson += FString::Printf(TEXT("\n%s%s"), *RelationshipPad, *Relationships[Index].ToJson(IndentSpaces + 4));
        if (Index < Relationships.Num() - 1)
        {
            RelationshipJson += TEXT(",");
        }
    }
    RelationshipJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"allyCount\": %d,\n")
        TEXT("%s\"neutralCount\": %d,\n")
        TEXT("%s\"rivalCount\": %d,\n")
        TEXT("%s\"hostileCount\": %d,\n")
        TEXT("%s\"tradePartnerCount\": %d,\n")
        TEXT("%s\"sanctionsCount\": %d,\n")
        TEXT("%s\"treatyCount\": %d,\n")
        TEXT("%s\"averageBorderTension\": %d,\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"relationships\": %s\n")
        TEXT("%s}"),
        *Pad, LastUpdatedTurn,
        *Pad, AllyCount,
        *Pad, NeutralCount,
        *Pad, RivalCount,
        *Pad, HostileCount,
        *Pad, TradePartnerCount,
        *Pad, SanctionsCount,
        *Pad, TreatyCount,
        *Pad, AverageBorderTension,
        *Pad, *JsonEscape(Summary),
        *Pad, *RelationshipJson,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyGovernmentDiplomacyRuleState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"ruleId\": \"%s\",\n")
        TEXT("%s\"ruleName\": \"%s\",\n")
        TEXT("%s\"ruleType\": \"%s\",\n")
        TEXT("%s\"description\": \"%s\",\n")
        TEXT("%s\"enabled\": %s,\n")
        TEXT("%s\"trustThreshold\": %d,\n")
        TEXT("%s\"borderTensionThreshold\": %d,\n")
        TEXT("%s\"stabilityCost\": %d,\n")
        TEXT("%s\"unrestCost\": %d,\n")
        TEXT("%s\"diplomacyCost\": %d,\n")
        TEXT("%s\"turnsRequired\": %d,\n")
        TEXT("%s\"allowedGovernmentTypes\": %s,\n")
        TEXT("%s\"blockedGovernmentTypes\": %s,\n")
        TEXT("%s\"consequences\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(RuleId),
        *Pad, *JsonEscape(RuleName),
        *Pad, *JsonEscape(RuleType),
        *Pad, *JsonEscape(Description),
        *Pad, bEnabled ? TEXT("true") : TEXT("false"),
        *Pad, TrustThreshold,
        *Pad, BorderTensionThreshold,
        *Pad, StabilityCost,
        *Pad, UnrestCost,
        *Pad, DiplomacyCost,
        *Pad, TurnsRequired,
        *Pad, *StringArrayToJson(AllowedGovernmentTypes),
        *Pad, *StringArrayToJson(BlockedGovernmentTypes),
        *Pad, *StringArrayToJson(Consequences),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyGovernmentDiplomacyRulesState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString RulePad = Indent(IndentSpaces + 2);
    FString RuleJson = TEXT("[");
    for (int32 Index = 0; Index < Rules.Num(); ++Index)
    {
        RuleJson += FString::Printf(TEXT("\n%s%s"), *RulePad, *Rules[Index].ToJson(IndentSpaces + 4));
        if (Index < Rules.Num() - 1)
        {
            RuleJson += TEXT(",");
        }
    }
    RuleJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"playerGovernmentType\": \"%s\",\n")
        TEXT("%s\"targetGovernmentType\": \"%s\",\n")
        TEXT("%s\"transitionProgress\": %d,\n")
        TEXT("%s\"transitionTurnsRemaining\": %d,\n")
        TEXT("%s\"transitionStabilityCost\": %d,\n")
        TEXT("%s\"transitionUnrestCost\": %d,\n")
        TEXT("%s\"transitionDiplomacyCost\": %d,\n")
        TEXT("%s\"allowedAllianceCount\": %d,\n")
        TEXT("%s\"blockedAllianceCount\": %d,\n")
        TEXT("%s\"activeTreatyCount\": %d,\n")
        TEXT("%s\"activeSanctionsCount\": %d,\n")
        TEXT("%s\"highBorderTensionCount\": %d,\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"rules\": %s,\n")
        TEXT("%s\"activeRestrictions\": %s,\n")
        TEXT("%s\"sideSwitchConsequences\": %s\n")
        TEXT("%s}"),
        *Pad, LastUpdatedTurn,
        *Pad, *JsonEscape(PlayerGovernmentType),
        *Pad, *JsonEscape(TargetGovernmentType),
        *Pad, TransitionProgress,
        *Pad, TransitionTurnsRemaining,
        *Pad, TransitionStabilityCost,
        *Pad, TransitionUnrestCost,
        *Pad, TransitionDiplomacyCost,
        *Pad, AllowedAllianceCount,
        *Pad, BlockedAllianceCount,
        *Pad, ActiveTreatyCount,
        *Pad, ActiveSanctionsCount,
        *Pad, HighBorderTensionCount,
        *Pad, *JsonEscape(Summary),
        *Pad, *RuleJson,
        *Pad, *StringArrayToJson(ActiveRestrictions),
        *Pad, *StringArrayToJson(SideSwitchConsequences),
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
        TEXT("%s\"deadlineTurn\": %d,\n")
        TEXT("%s\"severity\": %d,\n")
        TEXT("%s\"triggered\": %s,\n")
        TEXT("%s\"resolved\": %s,\n")
        TEXT("%s\"completionState\": \"%s\",\n")
        TEXT("%s\"selectedChoiceId\": \"%s\",\n")
        TEXT("%s\"resolutionSummary\": \"%s\",\n")
        TEXT("%s\"unresolvedPenaltySummary\": \"%s\",\n")
        TEXT("%s\"followUpEventType\": \"%s\",\n")
        TEXT("%s\"followUpTitle\": \"%s\",\n")
        TEXT("%s\"followUpDescription\": \"%s\",\n")
        TEXT("%s\"followUpSeverityDelta\": %d,\n")
        TEXT("%s\"choices\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(EventId), *Pad, *JsonEscape(EventType), *Pad, *JsonEscape(Title), *Pad, *JsonEscape(Description), *Pad, *JsonEscape(TriggerReason),
        *Pad, CreatedTurn, *Pad, DeadlineTurn, *Pad, Severity, *Pad, bTriggered ? TEXT("true") : TEXT("false"), *Pad, bResolved ? TEXT("true") : TEXT("false"),
        *Pad, *JsonEscape(CompletionState), *Pad, *JsonEscape(SelectedChoiceId), *Pad, *JsonEscape(ResolutionSummary),
        *Pad, *JsonEscape(UnresolvedPenaltySummary), *Pad, *JsonEscape(FollowUpEventType), *Pad, *JsonEscape(FollowUpTitle), *Pad, *JsonEscape(FollowUpDescription), *Pad, FollowUpSeverityDelta,
        *Pad, *ChoiceJson,
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
        TEXT("%s\"debtCapacity\": %d,\n")
        TEXT("%s\"spendingLimit\": %d,\n")
        TEXT("%s\"creditStress\": %d,\n")
        TEXT("%s\"spendingLimited\": %s,\n")
        TEXT("%s\"spendingPosture\": \"%s\",\n")
        TEXT("%s\"budgetConstraintStatus\": \"%s\",\n")
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
        *Pad, DebtCapacity,
        *Pad, SpendingLimit,
        *Pad, CreditStress,
        *Pad, bSpendingLimited ? TEXT("true") : TEXT("false"),
        *Pad, *JsonEscape(SpendingPosture),
        *Pad, *JsonEscape(BudgetConstraintStatus),
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
        TEXT("%s\"countryId\": \"%s\",\n")
        TEXT("%s\"mapRegionId\": \"%s\",\n")
        TEXT("%s\"countryName\": \"%s\",\n")
        TEXT("%s\"continentName\": \"%s\",\n")
        TEXT("%s\"climate\": \"%s\",\n")
        TEXT("%s\"politicalType\": \"%s\",\n")
        TEXT("%s\"diplomaticAlignment\": \"%s\",\n")
        TEXT("%s\"mapCountryIndex\": %d,\n")
        TEXT("%s\"desiredProvinceCount\": %d,\n")
        TEXT("%s\"populationWeight\": %d,\n")
        TEXT("%s\"areaWeight\": %d,\n")
        TEXT("%s\"powerScore\": %d,\n")
        TEXT("%s\"stability\": %d,\n")
        TEXT("%s\"borderPressure\": %d,\n")
        TEXT("%s\"alliedWithPlayer\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CountryId),
        *Pad, *JsonEscape(MapRegionId),
        *Pad, *JsonEscape(CountryName),
        *Pad, *JsonEscape(ContinentName),
        *Pad, *JsonEscape(Climate),
        *Pad, *JsonEscape(PoliticalType),
        *Pad, *JsonEscape(DiplomaticAlignment),
        *Pad, MapCountryIndex,
        *Pad, DesiredProvinceCount,
        *Pad, PopulationWeight,
        *Pad, AreaWeight,
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
        TEXT("%s\"planetName\": \"%s\",\n")
        TEXT("%s\"mapDataVersion\": \"%s\",\n")
        TEXT("%s\"continentCount\": %d,\n")
        TEXT("%s\"durableCountryTarget\": %d,\n")
        TEXT("%s\"activeCountryCount\": %d,\n")
        TEXT("%s\"totalCountryCount\": %d,\n")
        TEXT("%s\"totalProvinceCount\": %d,\n")
        TEXT("%s\"totalMapRegionCount\": %d,\n")
        TEXT("%s\"democraticAllyCount\": %d,\n")
        TEXT("%s\"nonDemocraticCountryCount\": %d,\n")
        TEXT("%s\"generationRule\": \"%s\",\n")
        TEXT("%s\"mapDataSummary\": \"%s\",\n")
        TEXT("%s\"continents\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(PlanetName),
        *Pad, *JsonEscape(MapDataVersion),
        *Pad, ContinentCount,
        *Pad, DurableCountryTarget,
        *Pad, ActiveCountryCount,
        *Pad, TotalCountryCount,
        *Pad, TotalProvinceCount,
        *Pad, TotalMapRegionCount,
        *Pad, DemocraticAllyCount,
        *Pad, NonDemocraticCountryCount,
        *Pad, *JsonEscape(GenerationRule),
        *Pad, *JsonEscape(MapDataSummary),
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

FString FDemocracyRtsOutcomeState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"turn\": %d,\n")
        TEXT("%s\"outcomeId\": \"%s\",\n")
        TEXT("%s\"conflictName\": \"%s\",\n")
        TEXT("%s\"opponentCountry\": \"%s\",\n")
        TEXT("%s\"outcomeType\": \"%s\",\n")
        TEXT("%s\"importEventType\": \"%s\",\n")
        TEXT("%s\"attentionCategory\": \"%s\",\n")
        TEXT("%s\"affectedCountryName\": \"%s\",\n")
        TEXT("%s\"affectedProvinceId\": \"%s\",\n")
        TEXT("%s\"affectedProvinceName\": \"%s\",\n")
        TEXT("%s\"affectedResource\": \"%s\",\n")
        TEXT("%s\"attentionDeadlineTurn\": %d,\n")
        TEXT("%s\"attentionSeverity\": %d,\n")
        TEXT("%s\"territoryDelta\": %d,\n")
        TEXT("%s\"casualties\": %d,\n")
        TEXT("%s\"resourceDisruption\": %d,\n")
        TEXT("%s\"warFatigueDelta\": %d,\n")
        TEXT("%s\"diplomaticDamage\": %d,\n")
        TEXT("%s\"stabilityDelta\": %d,\n")
        TEXT("%s\"invasionRiskDelta\": %d,\n")
        TEXT("%s\"budgetStrain\": %d,\n")
        TEXT("%s\"requiresSimulationAttention\": %s,\n")
        TEXT("%s\"acknowledgedBySimulation\": %s,\n")
        TEXT("%s\"appliedToSimulation\": %s,\n")
        TEXT("%s\"simulationAttentionStatus\": \"%s\",\n")
        TEXT("%s\"attentionSummary\": \"%s\",\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"consequenceTags\": %s\n")
        TEXT("%s}"),
        *Pad, Turn,
        *Pad, *JsonEscape(OutcomeId),
        *Pad, *JsonEscape(ConflictName),
        *Pad, *JsonEscape(OpponentCountry),
        *Pad, *JsonEscape(OutcomeType),
        *Pad, *JsonEscape(ImportEventType),
        *Pad, *JsonEscape(AttentionCategory),
        *Pad, *JsonEscape(AffectedCountryName),
        *Pad, *JsonEscape(AffectedProvinceId),
        *Pad, *JsonEscape(AffectedProvinceName),
        *Pad, *JsonEscape(AffectedResource),
        *Pad, AttentionDeadlineTurn,
        *Pad, AttentionSeverity,
        *Pad, TerritoryDelta,
        *Pad, Casualties,
        *Pad, ResourceDisruption,
        *Pad, WarFatigueDelta,
        *Pad, DiplomaticDamage,
        *Pad, StabilityDelta,
        *Pad, InvasionRiskDelta,
        *Pad, BudgetStrain,
        *Pad, bRequiresSimulationAttention ? TEXT("true") : TEXT("false"),
        *Pad, bAcknowledgedBySimulation ? TEXT("true") : TEXT("false"),
        *Pad, bAppliedToSimulation ? TEXT("true") : TEXT("false"),
        *Pad, *JsonEscape(SimulationAttentionStatus),
        *Pad, *JsonEscape(AttentionSummary),
        *Pad, *JsonEscape(Summary),
        *Pad, *StringArrayToJson(ConsequenceTags),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsBackflowState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString OutcomePad = Indent(IndentSpaces + 2);
    FString PendingJson = TEXT("[");
    for (int32 Index = 0; Index < PendingOutcomes.Num(); ++Index)
    {
        PendingJson += FString::Printf(TEXT("\n%s%s"), *OutcomePad, *PendingOutcomes[Index].ToJson(IndentSpaces + 4));
        if (Index < PendingOutcomes.Num() - 1)
        {
            PendingJson += TEXT(",");
        }
    }
    PendingJson += FString::Printf(TEXT("\n%s]"), *Pad);

    FString HistoryJson = TEXT("[");
    for (int32 Index = 0; Index < OutcomeHistory.Num(); ++Index)
    {
        HistoryJson += FString::Printf(TEXT("\n%s%s"), *OutcomePad, *OutcomeHistory[Index].ToJson(IndentSpaces + 4));
        if (Index < OutcomeHistory.Num() - 1)
        {
            HistoryJson += TEXT(",");
        }
    }
    HistoryJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastAppliedTurn\": %d,\n")
        TEXT("%s\"pendingOutcomeCount\": %d,\n")
        TEXT("%s\"pendingAttentionCount\": %d,\n")
        TEXT("%s\"battleLossCount\": %d,\n")
        TEXT("%s\"provinceCaptureCount\": %d,\n")
        TEXT("%s\"capitalThreatCount\": %d,\n")
        TEXT("%s\"supplyRouteBreakCount\": %d,\n")
        TEXT("%s\"totalTerritoryDelta\": %d,\n")
        TEXT("%s\"totalCasualties\": %d,\n")
        TEXT("%s\"warFatigue\": %d,\n")
        TEXT("%s\"resourceDisruptionPressure\": %d,\n")
        TEXT("%s\"budgetStrainPressure\": %d,\n")
        TEXT("%s\"diplomaticDamagePressure\": %d,\n")
        TEXT("%s\"lastOutcomeSummary\": \"%s\",\n")
        TEXT("%s\"lastImportQueueSummary\": \"%s\",\n")
        TEXT("%s\"pendingOutcomes\": %s,\n")
        TEXT("%s\"outcomeHistory\": %s\n")
        TEXT("%s}"),
        *Pad, LastAppliedTurn,
        *Pad, PendingOutcomeCount,
        *Pad, PendingAttentionCount,
        *Pad, BattleLossCount,
        *Pad, ProvinceCaptureCount,
        *Pad, CapitalThreatCount,
        *Pad, SupplyRouteBreakCount,
        *Pad, TotalTerritoryDelta,
        *Pad, TotalCasualties,
        *Pad, WarFatigue,
        *Pad, ResourceDisruptionPressure,
        *Pad, BudgetStrainPressure,
        *Pad, DiplomaticDamagePressure,
        *Pad, *JsonEscape(LastOutcomeSummary),
        *Pad, *JsonEscape(LastImportQueueSummary),
        *Pad, *PendingJson,
        *Pad, *HistoryJson,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyProvinceOwnershipState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"provinceId\": \"%s\",\n")
        TEXT("%s\"countryId\": \"%s\",\n")
        TEXT("%s\"mapRegionId\": \"%s\",\n")
        TEXT("%s\"provinceName\": \"%s\",\n")
        TEXT("%s\"continentName\": \"%s\",\n")
        TEXT("%s\"originalCountryName\": \"%s\",\n")
        TEXT("%s\"currentOwnerCountryName\": \"%s\",\n")
        TEXT("%s\"currentControllerCountryName\": \"%s\",\n")
        TEXT("%s\"governmentType\": \"%s\",\n")
        TEXT("%s\"climate\": \"%s\",\n")
        TEXT("%s\"resourceFocus\": \"%s\",\n")
        TEXT("%s\"terrainType\": \"%s\",\n")
        TEXT("%s\"provinceIndex\": %d,\n")
        TEXT("%s\"populationWeight\": %d,\n")
        TEXT("%s\"areaWeight\": %d,\n")
        TEXT("%s\"strategicValue\": %d,\n")
        TEXT("%s\"stability\": %d,\n")
        TEXT("%s\"unrest\": %d,\n")
        TEXT("%s\"playerControlled\": %s,\n")
        TEXT("%s\"borderProvince\": %s,\n")
        TEXT("%s\"lastChangedTurn\": %d\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ProvinceId),
        *Pad, *JsonEscape(CountryId),
        *Pad, *JsonEscape(MapRegionId),
        *Pad, *JsonEscape(ProvinceName),
        *Pad, *JsonEscape(ContinentName),
        *Pad, *JsonEscape(OriginalCountryName),
        *Pad, *JsonEscape(CurrentOwnerCountryName),
        *Pad, *JsonEscape(CurrentControllerCountryName),
        *Pad, *JsonEscape(GovernmentType),
        *Pad, *JsonEscape(Climate),
        *Pad, *JsonEscape(ResourceFocus),
        *Pad, *JsonEscape(TerrainType),
        *Pad, ProvinceIndex,
        *Pad, PopulationWeight,
        *Pad, AreaWeight,
        *Pad, StrategicValue,
        *Pad, Stability,
        *Pad, Unrest,
        *Pad, bPlayerControlled ? TEXT("true") : TEXT("false"),
        *Pad, bBorderProvince ? TEXT("true") : TEXT("false"),
        *Pad, LastChangedTurn,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyCountryOwnershipState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"countryId\": \"%s\",\n")
        TEXT("%s\"mapRegionId\": \"%s\",\n")
        TEXT("%s\"countryName\": \"%s\",\n")
        TEXT("%s\"continentName\": \"%s\",\n")
        TEXT("%s\"governmentType\": \"%s\",\n")
        TEXT("%s\"mapCountryIndex\": %d,\n")
        TEXT("%s\"totalProvinces\": %d,\n")
        TEXT("%s\"controlledProvinces\": %d,\n")
        TEXT("%s\"occupiedProvinces\": %d,\n")
        TEXT("%s\"lostProvinces\": %d,\n")
        TEXT("%s\"borderProvinces\": %d,\n")
        TEXT("%s\"resourceBase\": %d,\n")
        TEXT("%s\"militaryValue\": %d,\n")
        TEXT("%s\"populationWeight\": %d,\n")
        TEXT("%s\"areaWeight\": %d,\n")
        TEXT("%s\"playerCountry\": %s,\n")
        TEXT("%s\"capitalControlled\": %s,\n")
        TEXT("%s\"provinceIds\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CountryId),
        *Pad, *JsonEscape(MapRegionId),
        *Pad, *JsonEscape(CountryName),
        *Pad, *JsonEscape(ContinentName),
        *Pad, *JsonEscape(GovernmentType),
        *Pad, MapCountryIndex,
        *Pad, TotalProvinces,
        *Pad, ControlledProvinces,
        *Pad, OccupiedProvinces,
        *Pad, LostProvinces,
        *Pad, BorderProvinces,
        *Pad, ResourceBase,
        *Pad, MilitaryValue,
        *Pad, PopulationWeight,
        *Pad, AreaWeight,
        *Pad, bPlayerCountry ? TEXT("true") : TEXT("false"),
        *Pad, bCapitalControlled ? TEXT("true") : TEXT("false"),
        *Pad, *StringArrayToJson(ProvinceIds),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyContinentOwnershipState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"continentName\": \"%s\",\n")
        TEXT("%s\"climate\": \"%s\",\n")
        TEXT("%s\"countryCount\": %d,\n")
        TEXT("%s\"provinceCount\": %d,\n")
        TEXT("%s\"playerControlledProvinces\": %d,\n")
        TEXT("%s\"contestedProvinces\": %d,\n")
        TEXT("%s\"countryNames\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ContinentName),
        *Pad, *JsonEscape(Climate),
        *Pad, CountryCount,
        *Pad, ProvinceCount,
        *Pad, PlayerControlledProvinces,
        *Pad, ContestedProvinces,
        *Pad, *StringArrayToJson(CountryNames),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyMapOwnershipState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString EntryPad = Indent(IndentSpaces + 2);
    FString ProvinceJson = TEXT("[");
    for (int32 Index = 0; Index < Provinces.Num(); ++Index)
    {
        ProvinceJson += FString::Printf(TEXT("\n%s%s"), *EntryPad, *Provinces[Index].ToJson(IndentSpaces + 4));
        if (Index < Provinces.Num() - 1) ProvinceJson += TEXT(",");
    }
    ProvinceJson += FString::Printf(TEXT("\n%s]"), *Pad);
    FString CountryJson = TEXT("[");
    for (int32 Index = 0; Index < Countries.Num(); ++Index)
    {
        CountryJson += FString::Printf(TEXT("\n%s%s"), *EntryPad, *Countries[Index].ToJson(IndentSpaces + 4));
        if (Index < Countries.Num() - 1) CountryJson += TEXT(",");
    }
    CountryJson += FString::Printf(TEXT("\n%s]"), *Pad);
    FString ContinentJson = TEXT("[");
    for (int32 Index = 0; Index < Continents.Num(); ++Index)
    {
        ContinentJson += FString::Printf(TEXT("\n%s%s"), *EntryPad, *Continents[Index].ToJson(IndentSpaces + 4));
        if (Index < Continents.Num() - 1) ContinentJson += TEXT(",");
    }
    ContinentJson += FString::Printf(TEXT("\n%s]"), *Pad);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"planetName\": \"%s\",\n")
        TEXT("%s\"mapDataVersion\": \"%s\",\n")
        TEXT("%s\"durableCountryTarget\": %d,\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"totalCountries\": %d,\n")
        TEXT("%s\"totalProvinces\": %d,\n")
        TEXT("%s\"totalMapRegionCount\": %d,\n")
        TEXT("%s\"totalPopulationWeight\": %d,\n")
        TEXT("%s\"totalAreaWeight\": %d,\n")
        TEXT("%s\"playerControlledProvinces\": %d,\n")
        TEXT("%s\"contestedProvinces\": %d,\n")
        TEXT("%s\"borderProvinceCount\": %d,\n")
        TEXT("%s\"playerCountryName\": \"%s\",\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"provinces\": %s,\n")
        TEXT("%s\"countries\": %s,\n")
        TEXT("%s\"continents\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(PlanetName),
        *Pad, *JsonEscape(MapDataVersion),
        *Pad, DurableCountryTarget,
        *Pad, LastUpdatedTurn,
        *Pad, TotalCountries,
        *Pad, TotalProvinces,
        *Pad, TotalMapRegionCount,
        *Pad, TotalPopulationWeight,
        *Pad, TotalAreaWeight,
        *Pad, PlayerControlledProvinces,
        *Pad, ContestedProvinces,
        *Pad, BorderProvinceCount,
        *Pad, *JsonEscape(PlayerCountryName),
        *Pad, *JsonEscape(Summary),
        *Pad, *ProvinceJson,
        *Pad, *CountryJson,
        *Pad, *ContinentJson,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsScopeBoundaryState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"scopeVersion\": \"%s\",\n")
        TEXT("%s\"scopeSummary\": \"%s\",\n")
        TEXT("%s\"rtsOwns\": %s,\n")
        TEXT("%s\"simulationOwns\": %s,\n")
        TEXT("%s\"blockedUntilRts\": %s,\n")
        TEXT("%s\"backflowRequired\": %s,\n")
        TEXT("%s\"candidateAssetPacks\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ScopeVersion),
        *Pad, *JsonEscape(ScopeSummary),
        *Pad, *StringArrayToJson(RtsOwns),
        *Pad, *StringArrayToJson(SimulationOwns),
        *Pad, *StringArrayToJson(BlockedUntilRts),
        *Pad, *StringArrayToJson(BackflowRequired),
        *Pad, *StringArrayToJson(CandidateAssetPacks),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsViewModeState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"viewId\": \"%s\",\n")
        TEXT("%s\"displayName\": \"%s\",\n")
        TEXT("%s\"purpose\": \"%s\",\n")
        TEXT("%s\"implementedPlaceholder\": %s,\n")
        TEXT("%s\"defaultView\": %s,\n")
        TEXT("%s\"interactions\": %s,\n")
        TEXT("%s\"visibleLayers\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ViewId),
        *Pad, *JsonEscape(DisplayName),
        *Pad, *JsonEscape(Purpose),
        *Pad, bImplementedPlaceholder ? TEXT("true") : TEXT("false"),
        *Pad, bDefaultView ? TEXT("true") : TEXT("false"),
        *Pad, *StringArrayToJson(Interactions),
        *Pad, *StringArrayToJson(VisibleLayers),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsBuildingState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"buildingId\": \"%s\",\n")
        TEXT("%s\"displayName\": \"%s\",\n")
        TEXT("%s\"buildingType\": \"%s\",\n")
        TEXT("%s\"resourceFocus\": \"%s\",\n")
        TEXT("%s\"candidateAssetHint\": \"%s\",\n")
        TEXT("%s\"level\": %d,\n")
        TEXT("%s\"buildCost\": %d,\n")
        TEXT("%s\"upgradeCost\": %d,\n")
        TEXT("%s\"buildTimeTurns\": %d,\n")
        TEXT("%s\"productionPerTick\": %d,\n")
        TEXT("%s\"defenseValue\": %d,\n")
        TEXT("%s\"constructed\": %s,\n")
        TEXT("%s\"upgradeQueued\": %s,\n")
        TEXT("%s\"status\": \"%s\",\n")
        TEXT("%s\"prerequisites\": %s,\n")
        TEXT("%s\"runtimeTags\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(BuildingId),
        *Pad, *JsonEscape(DisplayName),
        *Pad, *JsonEscape(BuildingType),
        *Pad, *JsonEscape(ResourceFocus),
        *Pad, *JsonEscape(CandidateAssetHint),
        *Pad, Level,
        *Pad, BuildCost,
        *Pad, UpgradeCost,
        *Pad, BuildTimeTurns,
        *Pad, ProductionPerTick,
        *Pad, DefenseValue,
        *Pad, bConstructed ? TEXT("true") : TEXT("false"),
        *Pad, bUpgradeQueued ? TEXT("true") : TEXT("false"),
        *Pad, *JsonEscape(Status),
        *Pad, *StringArrayToJson(Prerequisites),
        *Pad, *StringArrayToJson(RuntimeTags),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsCityBaseState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString EntryPad = Indent(IndentSpaces + 2);
    FString BuildingJson = TEXT("[");
    for (int32 Index = 0; Index < Buildings.Num(); ++Index)
    {
        BuildingJson += FString::Printf(TEXT("\n%s%s"), *EntryPad, *Buildings[Index].ToJson(IndentSpaces + 4));
        if (Index < Buildings.Num() - 1)
        {
            BuildingJson += TEXT(",");
        }
    }
    BuildingJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"baseId\": \"%s\",\n")
        TEXT("%s\"displayName\": \"%s\",\n")
        TEXT("%s\"linkedCountryName\": \"%s\",\n")
        TEXT("%s\"linkedProvinceId\": \"%s\",\n")
        TEXT("%s\"viewModeId\": \"%s\",\n")
        TEXT("%s\"gridWidth\": %d,\n")
        TEXT("%s\"gridHeight\": %d,\n")
        TEXT("%s\"buildQueueCount\": %d,\n")
        TEXT("%s\"upgradeQueueCount\": %d,\n")
        TEXT("%s\"baseSummary\": \"%s\",\n")
        TEXT("%s\"buildings\": %s,\n")
        TEXT("%s\"buildQueue\": %s,\n")
        TEXT("%s\"runtimeNotes\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(BaseId),
        *Pad, *JsonEscape(DisplayName),
        *Pad, *JsonEscape(LinkedCountryName),
        *Pad, *JsonEscape(LinkedProvinceId),
        *Pad, *JsonEscape(ViewModeId),
        *Pad, GridWidth,
        *Pad, GridHeight,
        *Pad, BuildQueueCount,
        *Pad, UpgradeQueueCount,
        *Pad, *JsonEscape(BaseSummary),
        *Pad, *BuildingJson,
        *Pad, *StringArrayToJson(BuildQueue),
        *Pad, *StringArrayToJson(RuntimeNotes),
        *Indent(IndentSpaces - 2));
}
FString FDemocracyRtsWorldState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString EntryPad = Indent(IndentSpaces + 2);
    FString ViewModeJson = TEXT("[");
    for (int32 Index = 0; Index < ViewModes.Num(); ++Index)
    {
        ViewModeJson += FString::Printf(TEXT("\n%s%s"), *EntryPad, *ViewModes[Index].ToJson(IndentSpaces + 4));
        if (Index < ViewModes.Num() - 1)
        {
            ViewModeJson += TEXT(",");
        }
    }
    ViewModeJson += FString::Printf(TEXT("\n%s]"), *Pad);

    FString RivalJson = TEXT("[");
    for (int32 Index = 0; Index < Rivals.Num(); ++Index)
    {
        RivalJson += FString::Printf(TEXT("\n%s%s"), *EntryPad, *Rivals[Index].ToJson(IndentSpaces + 4));
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
        TEXT("%s\"activeViewMode\": \"%s\",\n")
        TEXT("%s\"activeStrategicLayers\": %s,\n")
        TEXT("%s\"scopeBoundary\": %s,\n")
        TEXT("%s\"viewModes\": %s,\n")
        TEXT("%s\"cityBase\": %s,\n")
        TEXT("%s\"rivals\": %s,\n")
        TEXT("%s\"backflow\": %s,\n")
        TEXT("%s\"ownership\": %s\n")
        TEXT("%s}"),
        *Pad, SimulationSecond,
        *Pad, ControlledTerritories,
        *Pad, BorderTerritories,
        *Pad, KnownRivalCountries,
        *Pad, *JsonEscape(ActiveViewMode),
        *Pad, *StringArrayToJson(ActiveStrategicLayers),
        *Pad, *ScopeBoundary.ToJson(IndentSpaces + 2),
        *Pad, *ViewModeJson,
        *Pad, *CityBase.ToJson(IndentSpaces + 2),
        *Pad, *RivalJson,
        *Pad, *Backflow.ToJson(IndentSpaces + 2),
        *Pad, *Ownership.ToJson(IndentSpaces + 2),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsSaveBoundaryState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"boundaryVersion\": \"%s\",\n")
        TEXT("%s\"simulationAuthority\": \"%s\",\n")
        TEXT("%s\"rtsAuthority\": \"%s\",\n")
        TEXT("%s\"saveAuthority\": \"%s\",\n")
        TEXT("%s\"multiplayerAuthority\": \"%s\",\n")
        TEXT("%s\"simulationOwnedFields\": %s,\n")
        TEXT("%s\"rtsOwnedFields\": %s,\n")
        TEXT("%s\"sharedHandshakeFields\": %s,\n")
        TEXT("%s\"simulationExportsToRts\": %s,\n")
        TEXT("%s\"rtsImportsToSimulation\": %s,\n")
        TEXT("%s\"forbiddenSimulationWrites\": %s,\n")
        TEXT("%s\"saveRules\": %s,\n")
        TEXT("%s\"serverAuthoritativeFields\": %s,\n")
        TEXT("%s\"clientRequestOnlyFields\": %s,\n")
        TEXT("%s\"serverValidationNotes\": %s,\n")
        TEXT("%s\"boundaryValidationNotes\": %s,\n")
        TEXT("%s\"boundarySummary\": \"%s\"\n")
        TEXT("%s}"),
        *Pad, LastUpdatedTurn,
        *Pad, *JsonEscape(BoundaryVersion),
        *Pad, *JsonEscape(SimulationAuthority),
        *Pad, *JsonEscape(RtsAuthority),
        *Pad, *JsonEscape(SaveAuthority),
        *Pad, *JsonEscape(MultiplayerAuthority),
        *Pad, *StringArrayToJson(SimulationOwnedFields),
        *Pad, *StringArrayToJson(RtsOwnedFields),
        *Pad, *StringArrayToJson(SharedHandshakeFields),
        *Pad, *StringArrayToJson(SimulationExportsToRts),
        *Pad, *StringArrayToJson(RtsImportsToSimulation),
        *Pad, *StringArrayToJson(ForbiddenSimulationWrites),
        *Pad, *StringArrayToJson(SaveRules),
        *Pad, *StringArrayToJson(ServerAuthoritativeFields),
        *Pad, *StringArrayToJson(ClientRequestOnlyFields),
        *Pad, *StringArrayToJson(ServerValidationNotes),
        *Pad, *StringArrayToJson(BoundaryValidationNotes),
        *Pad, *JsonEscape(BoundarySummary),
        *Indent(IndentSpaces - 2));
}
FString FDemocracyWarParticipantState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"countryName\": \"%s\",\n")
        TEXT("%s\"role\": \"%s\",\n")
        TEXT("%s\"alignment\": \"%s\",\n")
        TEXT("%s\"commitment\": %d,\n")
        TEXT("%s\"warSupport\": %d,\n")
        TEXT("%s\"casualties\": %d\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CountryName),
        *Pad, *JsonEscape(Role),
        *Pad, *JsonEscape(Alignment),
        *Pad, Commitment,
        *Pad, WarSupport,
        *Pad, Casualties,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyWarFrontState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"frontName\": \"%s\",\n")
        TEXT("%s\"regionName\": \"%s\",\n")
        TEXT("%s\"contestedBorder\": \"%s\",\n")
        TEXT("%s\"pressure\": %d,\n")
        TEXT("%s\"playerControl\": %d,\n")
        TEXT("%s\"status\": \"%s\"\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(FrontName),
        *Pad, *JsonEscape(RegionName),
        *Pad, *JsonEscape(ContestedBorder),
        *Pad, Pressure,
        *Pad, PlayerControl,
        *Pad, *JsonEscape(Status),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyWarConflictState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString ParticipantPad = Indent(IndentSpaces + 2);
    FString ParticipantJson = TEXT("[");
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        ParticipantJson += FString::Printf(TEXT("\n%s%s"), *ParticipantPad, *Participants[Index].ToJson(IndentSpaces + 4));
        if (Index < Participants.Num() - 1) ParticipantJson += TEXT(",");
    }
    ParticipantJson += FString::Printf(TEXT("\n%s]"), *Pad);

    const FString FrontPad = Indent(IndentSpaces + 2);
    FString FrontJson = TEXT("[");
    for (int32 Index = 0; Index < Fronts.Num(); ++Index)
    {
        FrontJson += FString::Printf(TEXT("\n%s%s"), *FrontPad, *Fronts[Index].ToJson(IndentSpaces + 4));
        if (Index < Fronts.Num() - 1) FrontJson += TEXT(",");
    }
    FrontJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"conflictId\": \"%s\",\n")
        TEXT("%s\"conflictName\": \"%s\",\n")
        TEXT("%s\"conflictType\": \"%s\",\n")
        TEXT("%s\"status\": \"%s\",\n")
        TEXT("%s\"primaryObjective\": \"%s\",\n")
        TEXT("%s\"enemyObjective\": \"%s\",\n")
        TEXT("%s\"startedTurn\": %d,\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"escalationLevel\": %d,\n")
        TEXT("%s\"warScore\": %d,\n")
        TEXT("%s\"victoryProgress\": %d,\n")
        TEXT("%s\"defeatRisk\": %d,\n")
        TEXT("%s\"victoryCondition\": \"%s\",\n")
        TEXT("%s\"defeatCondition\": \"%s\",\n")
        TEXT("%s\"participants\": %s,\n")
        TEXT("%s\"fronts\": %s,\n")
        TEXT("%s\"activeModifiers\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(ConflictId),
        *Pad, *JsonEscape(ConflictName),
        *Pad, *JsonEscape(ConflictType),
        *Pad, *JsonEscape(Status),
        *Pad, *JsonEscape(PrimaryObjective),
        *Pad, *JsonEscape(EnemyObjective),
        *Pad, StartedTurn,
        *Pad, LastUpdatedTurn,
        *Pad, EscalationLevel,
        *Pad, WarScore,
        *Pad, VictoryProgress,
        *Pad, DefeatRisk,
        *Pad, *JsonEscape(VictoryCondition),
        *Pad, *JsonEscape(DefeatCondition),
        *Pad, *ParticipantJson,
        *Pad, *FrontJson,
        *Pad, *StringArrayToJson(ActiveModifiers),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyWarSystemState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString ActivePad = Indent(IndentSpaces + 2);
    FString ActiveJson = TEXT("[");
    for (int32 Index = 0; Index < ActiveConflicts.Num(); ++Index)
    {
        ActiveJson += FString::Printf(TEXT("\n%s%s"), *ActivePad, *ActiveConflicts[Index].ToJson(IndentSpaces + 4));
        if (Index < ActiveConflicts.Num() - 1) ActiveJson += TEXT(",");
    }
    ActiveJson += FString::Printf(TEXT("\n%s]"), *Pad);

    const FString HistoryPad = Indent(IndentSpaces + 2);
    FString HistoryJson = TEXT("[");
    for (int32 Index = 0; Index < ConflictHistory.Num(); ++Index)
    {
        HistoryJson += FString::Printf(TEXT("\n%s%s"), *HistoryPad, *ConflictHistory[Index].ToJson(IndentSpaces + 4));
        if (Index < ConflictHistory.Num() - 1) HistoryJson += TEXT(",");
    }
    HistoryJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"activeConflictCount\": %d,\n")
        TEXT("%s\"escalationPressure\": %d,\n")
        TEXT("%s\"warFatigue\": %d,\n")
        TEXT("%s\"totalCasualties\": %d,\n")
        TEXT("%s\"readinessStatus\": \"%s\",\n")
        TEXT("%s\"summary\": \"%s\",\n")
        TEXT("%s\"activeConflicts\": %s,\n")
        TEXT("%s\"conflictHistory\": %s\n")
        TEXT("%s}"),
        *Pad, LastUpdatedTurn,
        *Pad, ActiveConflictCount,
        *Pad, EscalationPressure,
        *Pad, WarFatigue,
        *Pad, TotalCasualties,
        *Pad, *JsonEscape(ReadinessStatus),
        *Pad, *JsonEscape(Summary),
        *Pad, *ActiveJson,
        *Pad, *HistoryJson,
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsRegionInputState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"regionName\": \"%s\",\n")
        TEXT("%s\"climate\": \"%s\",\n")
        TEXT("%s\"resourceFocus\": \"%s\",\n")
        TEXT("%s\"stability\": %d,\n")
        TEXT("%s\"unrest\": %d,\n")
        TEXT("%s\"strategicValue\": %d,\n")
        TEXT("%s\"playerControlled\": %s,\n")
        TEXT("%s\"borderRegion\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(RegionName),
        *Pad, *JsonEscape(Climate),
        *Pad, *JsonEscape(ResourceFocus),
        *Pad, Stability,
        *Pad, Unrest,
        *Pad, StrategicValue,
        *Pad, bPlayerControlled ? TEXT("true") : TEXT("false"),
        *Pad, bBorderRegion ? TEXT("true") : TEXT("false"),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyRtsDiplomacyInputState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"countryName\": \"%s\",\n")
        TEXT("%s\"relationshipStatus\": \"%s\",\n")
        TEXT("%s\"treatyStatus\": \"%s\",\n")
        TEXT("%s\"ally\": %s,\n")
        TEXT("%s\"enemy\": %s,\n")
        TEXT("%s\"tradePartner\": %s,\n")
        TEXT("%s\"sanctionsActive\": %s,\n")
        TEXT("%s\"borderTension\": %d,\n")
        TEXT("%s\"trust\": %d\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CountryName),
        *Pad, *JsonEscape(RelationshipStatus),
        *Pad, *JsonEscape(TreatyStatus),
        *Pad, bAlly ? TEXT("true") : TEXT("false"),
        *Pad, bEnemy ? TEXT("true") : TEXT("false"),
        *Pad, bTradePartner ? TEXT("true") : TEXT("false"),
        *Pad, bSanctionsActive ? TEXT("true") : TEXT("false"),
        *Pad, BorderTension,
        *Pad, Trust,
        *Indent(IndentSpaces - 2));
}

FString FDemocracySimulationToRtsContractState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString RegionPad = Indent(IndentSpaces + 2);
    FString RegionJson = TEXT("[");
    for (int32 Index = 0; Index < Regions.Num(); ++Index)
    {
        RegionJson += FString::Printf(TEXT("\n%s%s"), *RegionPad, *Regions[Index].ToJson(IndentSpaces + 4));
        if (Index < Regions.Num() - 1) RegionJson += TEXT(",");
    }
    RegionJson += FString::Printf(TEXT("\n%s]"), *Pad);

    const FString DiplomacyPad = Indent(IndentSpaces + 2);
    FString DiplomacyJson = TEXT("[");
    for (int32 Index = 0; Index < Diplomacy.Num(); ++Index)
    {
        DiplomacyJson += FString::Printf(TEXT("\n%s%s"), *DiplomacyPad, *Diplomacy[Index].ToJson(IndentSpaces + 4));
        if (Index < Diplomacy.Num() - 1) DiplomacyJson += TEXT(",");
    }
    DiplomacyJson += FString::Printf(TEXT("\n%s]"), *Pad);

    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"contractVersion\": \"%s\",\n")
        TEXT("%s\"playerCountryName\": \"%s\",\n")
        TEXT("%s\"governmentType\": \"%s\",\n")
        TEXT("%s\"treasury\": %d,\n")
        TEXT("%s\"militaryReadiness\": %d,\n")
        TEXT("%s\"technology\": %d,\n")
        TEXT("%s\"stability\": %d,\n")
        TEXT("%s\"unrest\": %d,\n")
        TEXT("%s\"publicApproval\": %d,\n")
        TEXT("%s\"invasionRisk\": %d,\n")
        TEXT("%s\"resources\": %s,\n")
        TEXT("%s\"activePolicies\": %s,\n")
        TEXT("%s\"technologyUnlocks\": %s,\n")
        TEXT("%s\"allies\": %s,\n")
        TEXT("%s\"enemies\": %s,\n")
        TEXT("%s\"activeWars\": %s,\n")
        TEXT("%s\"strategicPermissions\": %s,\n")
        TEXT("%s\"regions\": %s,\n")
        TEXT("%s\"diplomacy\": %s,\n")
        TEXT("%s\"exportSummary\": \"%s\"\n")
        TEXT("%s}"),
        *Pad, LastUpdatedTurn,
        *Pad, *JsonEscape(ContractVersion),
        *Pad, *JsonEscape(PlayerCountryName),
        *Pad, *JsonEscape(GovernmentType),
        *Pad, Treasury,
        *Pad, MilitaryReadiness,
        *Pad, Technology,
        *Pad, Stability,
        *Pad, Unrest,
        *Pad, PublicApproval,
        *Pad, InvasionRisk,
        *Pad, *Resources.ToJson(IndentSpaces + 2),
        *Pad, *StringArrayToJson(ActivePolicies),
        *Pad, *StringArrayToJson(TechnologyUnlocks),
        *Pad, *StringArrayToJson(Allies),
        *Pad, *StringArrayToJson(Enemies),
        *Pad, *StringArrayToJson(ActiveWars),
        *Pad, *StringArrayToJson(StrategicPermissions),
        *Pad, *RegionJson,
        *Pad, *DiplomacyJson,
        *Pad, *JsonEscape(ExportSummary),
        *Indent(IndentSpaces - 2));
}

FString FDemocracyCommandAuthorityActionState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"commandId\": \"%s\",\n")
        TEXT("%s\"label\": \"%s\",\n")
        TEXT("%s\"authorityLayer\": \"%s\",\n")
        TEXT("%s\"commandType\": \"%s\",\n")
        TEXT("%s\"executionSurface\": \"%s\",\n")
        TEXT("%s\"officeAllowed\": %s,\n")
        TEXT("%s\"rtsViewAllowed\": %s,\n")
        TEXT("%s\"enabled\": %s,\n")
        TEXT("%s\"cooldownTurns\": %d,\n")
        TEXT("%s\"lastExecutedTurn\": %d,\n")
        TEXT("%s\"treasuryCost\": %d,\n")
        TEXT("%s\"approvalDelta\": %d,\n")
        TEXT("%s\"stabilityDelta\": %d,\n")
        TEXT("%s\"unrestDelta\": %d,\n")
        TEXT("%s\"diplomacyDelta\": %d,\n")
        TEXT("%s\"militaryDelta\": %d,\n")
        TEXT("%s\"invasionRiskDelta\": %d,\n")
        TEXT("%s\"resourceDelta\": %d,\n")
        TEXT("%s\"prerequisite\": \"%s\",\n")
        TEXT("%s\"effectPreview\": \"%s\",\n")
        TEXT("%s\"disabledReason\": \"%s\"\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(CommandId), *Pad, *JsonEscape(Label), *Pad, *JsonEscape(AuthorityLayer), *Pad, *JsonEscape(CommandType), *Pad, *JsonEscape(ExecutionSurface),
        *Pad, bOfficeAllowed ? TEXT("true") : TEXT("false"), *Pad, bRtsViewAllowed ? TEXT("true") : TEXT("false"), *Pad, bEnabled ? TEXT("true") : TEXT("false"),
        *Pad, CooldownTurns, *Pad, LastExecutedTurn, *Pad, TreasuryCost, *Pad, ApprovalDelta, *Pad, StabilityDelta, *Pad, UnrestDelta, *Pad, DiplomacyDelta, *Pad, MilitaryDelta, *Pad, InvasionRiskDelta, *Pad, ResourceDelta,
        *Pad, *JsonEscape(Prerequisite), *Pad, *JsonEscape(EffectPreview), *Pad, *JsonEscape(DisabledReason), *Indent(IndentSpaces - 2));
}

FString FDemocracyCommandAuthorityState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    const FString ActionPad = Indent(IndentSpaces + 2);
    FString ActionJson = TEXT("[");
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
    {
        ActionJson += FString::Printf(TEXT("\n%s%s"), *ActionPad, *Actions[Index].ToJson(IndentSpaces + 4));
        if (Index < Actions.Num() - 1) ActionJson += TEXT(",");
    }
    ActionJson += FString::Printf(TEXT("\n%s]"), *Pad);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"lastUpdatedTurn\": %d,\n")
        TEXT("%s\"activeCommandPosture\": \"%s\",\n")
        TEXT("%s\"officeAuthoritySummary\": \"%s\",\n")
        TEXT("%s\"rtsAuthoritySummary\": \"%s\",\n")
        TEXT("%s\"lastCommandSummary\": \"%s\",\n")
        TEXT("%s\"actions\": %s\n")
        TEXT("%s}"),
        *Pad, LastUpdatedTurn, *Pad, *JsonEscape(ActiveCommandPosture), *Pad, *JsonEscape(OfficeAuthoritySummary), *Pad, *JsonEscape(RtsAuthoritySummary), *Pad, *JsonEscape(LastCommandSummary), *Pad, *ActionJson, *Indent(IndentSpaces - 2));
}

FString FDemocracyObjectiveState::ToJson(int32 IndentSpaces) const
{
    const FString Pad = Indent(IndentSpaces);
    return FString::Printf(
        TEXT("{\n")
        TEXT("%s\"mode\": \"%s\",\n")
        TEXT("%s\"playerGovernmentType\": \"%s\",\n")
        TEXT("%s\"governmentTransitionTarget\": \"%s\",\n")
        TEXT("%s\"governmentTransitionProgress\": %d,\n")
        TEXT("%s\"governmentTransitionTurnsRemaining\": %d,\n")
        TEXT("%s\"democraticCountryCount\": %d,\n")
        TEXT("%s\"dictatorshipCountryCount\": %d,\n")
        TEXT("%s\"otherGovernmentCount\": %d,\n")
        TEXT("%s\"totalTrackedCountryCount\": %d,\n")
        TEXT("%s\"democracyConversionProgress\": %d,\n")
        TEXT("%s\"dictatorshipsRemainingForVictory\": %d,\n")
        TEXT("%s\"softVictoryAchieved\": %s,\n")
        TEXT("%s\"softVictoryTurn\": %d,\n")
        TEXT("%s\"postVictoryTurnsElapsed\": %d,\n")
        TEXT("%s\"simulationContinuesAfterVictory\": %s,\n")
        TEXT("%s\"postVictoryContinuationActive\": %s,\n")
        TEXT("%s\"regressionMonitoringActive\": %s,\n")
        TEXT("%s\"regressionWarningActive\": %s,\n")
        TEXT("%s\"regressionRisk\": %d,\n")
        TEXT("%s\"victoryCondition\": \"%s\",\n")
        TEXT("%s\"postVictoryObjective\": \"%s\",\n")
        TEXT("%s\"multiplayerServerObjective\": \"%s\",\n")
        TEXT("%s\"multiplayerOngoingNoFinalWin\": %s,\n")
        TEXT("%s\"serverDemocracySlots\": %d,\n")
        TEXT("%s\"serverDictatorshipSlots\": %d,\n")
        TEXT("%s\"longTermObjective\": \"%s\",\n")
        TEXT("%s\"objectiveSummary\": \"%s\",\n")
        TEXT("%s\"activeObjectiveNotes\": %s,\n")
        TEXT("%s\"allianceRules\": %s,\n")
        TEXT("%s\"objectiveHooks\": %s\n")
        TEXT("%s}"),
        *Pad, *JsonEscape(Mode),
        *Pad, *JsonEscape(PlayerGovernmentType),
        *Pad, *JsonEscape(GovernmentTransitionTarget),
        *Pad, GovernmentTransitionProgress,
        *Pad, GovernmentTransitionTurnsRemaining,
        *Pad, DemocraticCountryCount,
        *Pad, DictatorshipCountryCount,
        *Pad, OtherGovernmentCount,
        *Pad, TotalTrackedCountryCount,
        *Pad, DemocracyConversionProgress,
        *Pad, DictatorshipsRemainingForVictory,
        *Pad, bSoftVictoryAchieved ? TEXT("true") : TEXT("false"),
        *Pad, SoftVictoryTurn,
        *Pad, PostVictoryTurnsElapsed,
        *Pad, bSimulationContinuesAfterVictory ? TEXT("true") : TEXT("false"),
        *Pad, bPostVictoryContinuationActive ? TEXT("true") : TEXT("false"),
        *Pad, bRegressionMonitoringActive ? TEXT("true") : TEXT("false"),
        *Pad, bRegressionWarningActive ? TEXT("true") : TEXT("false"),
        *Pad, RegressionRisk,
        *Pad, *JsonEscape(VictoryCondition),
        *Pad, *JsonEscape(PostVictoryObjective),
        *Pad, *JsonEscape(MultiplayerServerObjective),
        *Pad, bMultiplayerOngoingNoFinalWin ? TEXT("true") : TEXT("false"),
        *Pad, ServerDemocracySlots,
        *Pad, ServerDictatorshipSlots,
        *Pad, *JsonEscape(LongTermObjective),
        *Pad, *JsonEscape(ObjectiveSummary),
        *Pad, *StringArrayToJson(ActiveObjectiveNotes),
        *Pad, *StringArrayToJson(AllianceRules),
        *Pad, *StringArrayToJson(ObjectiveHooks),
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
        TEXT("%s\"diplomacyMatrix\": %s,\n")
        TEXT("%s\"governmentDiplomacyRules\": %s,\n")
        TEXT("%s\"rtsWorld\": %s,\n")
        TEXT("%s\"rtsSaveBoundary\": %s,\n")
        TEXT("%s\"warSystem\": %s,\n")
        TEXT("%s\"simulationToRtsContract\": %s,\n")
        TEXT("%s\"commandAuthority\": %s,\n")
        TEXT("%s\"objectiveState\": %s\n")
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
        *Pad, *DiplomacyMatrix.ToJson(IndentSpaces + 2),
        *Pad, *GovernmentDiplomacyRules.ToJson(IndentSpaces + 2),
        *Pad, *RtsWorld.ToJson(IndentSpaces + 2),
        *Pad, *RtsSaveBoundary.ToJson(IndentSpaces + 2),
        *Pad, *WarSystem.ToJson(IndentSpaces + 2),
        *Pad, *SimulationToRtsContract.ToJson(IndentSpaces + 2),
        *Pad, *CommandAuthority.ToJson(IndentSpaces + 2),
        *Pad, *ObjectiveState.ToJson(IndentSpaces + 2),
        *Indent(IndentSpaces - 2));
}


FDemocracyGovernmentDiplomacyRulesState FDemocracyGameStateFactory::BuildGovernmentDiplomacyRulesState(const FDemocracySimulationState& State)
{
    FDemocracyGovernmentDiplomacyRulesState RulesState = State.GovernmentDiplomacyRules;
    RulesState.LastUpdatedTurn = State.Turn;
    RulesState.PlayerGovernmentType = State.ObjectiveState.PlayerGovernmentType.IsEmpty() ? TEXT("Democracy") : State.ObjectiveState.PlayerGovernmentType;
    RulesState.TargetGovernmentType = State.ObjectiveState.GovernmentTransitionTarget;
    RulesState.TransitionProgress = State.ObjectiveState.GovernmentTransitionProgress;
    RulesState.TransitionTurnsRemaining = State.ObjectiveState.GovernmentTransitionTurnsRemaining;
    RulesState.TransitionStabilityCost = RulesState.TransitionTurnsRemaining > 0 ? FMath::Clamp(6 + State.PlayerCountry.CountrySizeScore * 2, 8, 18) : 0;
    RulesState.TransitionUnrestCost = RulesState.TransitionTurnsRemaining > 0 ? FMath::Clamp(4 + State.PlayerCountry.CountrySizeScore * 3, 7, 20) : 0;
    RulesState.TransitionDiplomacyCost = RulesState.TransitionTurnsRemaining > 0 ? FMath::Clamp(5 + State.PlayerCountry.CountrySizeScore * 2, 7, 16) : 0;
    RulesState.AllowedAllianceCount = 0;
    RulesState.BlockedAllianceCount = 0;
    RulesState.ActiveTreatyCount = 0;
    RulesState.ActiveSanctionsCount = 0;
    RulesState.HighBorderTensionCount = 0;
    RulesState.Rules.Reset();
    RulesState.ActiveRestrictions.Reset();
    RulesState.SideSwitchConsequences.Reset();

    FDemocracyGovernmentDiplomacyRuleState AllianceRule;
    AllianceRule.RuleId = TEXT("alliance_same_alignment");
    AllianceRule.RuleName = TEXT("Alliance Alignment Lock");
    AllianceRule.RuleType = TEXT("Alliance");
    AllianceRule.Description = TEXT("Democracies can ally only with democracies; dictatorships can ally only with dictatorships. Non-aligned governments may trade or negotiate but cannot become formal allies until aligned.");
    AllianceRule.AllowedGovernmentTypes = { TEXT("Democracy with Democracy"), TEXT("Dictatorship with Dictatorship") };
    AllianceRule.BlockedGovernmentTypes = { TEXT("Democracy with Dictatorship"), TEXT("Dictatorship with Democracy"), TEXT("Non-Aligned formal alliances") };
    AllianceRule.Consequences = { TEXT("Invalid alliances are downgraded to neutral/treaty contact."), TEXT("Alliance outreach must target the same government alignment."), TEXT("Multiplayer sides inherit this lock when players choose or switch governments.") };
    RulesState.Rules.Add(AllianceRule);

    FDemocracyGovernmentDiplomacyRuleState TreatyRule;
    TreatyRule.RuleId = TEXT("treaty_requires_trust");
    TreatyRule.RuleName = TEXT("Treaty Trust Gate");
    TreatyRule.RuleType = TEXT("Treaty");
    TreatyRule.Description = TEXT("Treaties require stable relations before signing; low trust or high border tension blocks new treaties even when governments match.");
    TreatyRule.TrustThreshold = 45;
    TreatyRule.BorderTensionThreshold = 65;
    TreatyRule.Consequences = { TEXT("Treaties improve trade and diplomatic standing."), TEXT("Broken treaties reduce trust and can create border tension."), TEXT("Treaties do not override the alliance alignment lock.") };
    RulesState.Rules.Add(TreatyRule);

    FDemocracyGovernmentDiplomacyRuleState SanctionsRule;
    SanctionsRule.RuleId = TEXT("sanctions_escalate_tension");
    SanctionsRule.RuleName = TEXT("Sanctions Escalation");
    SanctionsRule.RuleType = TEXT("Sanction");
    SanctionsRule.Description = TEXT("Sanctions are allowed against hostile, rival, or high-tension states, but they reduce trade and can increase border tension.");
    SanctionsRule.BorderTensionThreshold = 55;
    SanctionsRule.Consequences = { TEXT("Sanctions lower trade value and trust."), TEXT("Repeated sanctions can push rivals toward hostile status."), TEXT("Sanctions can be useful pressure against dictatorships but carry invasion-risk cost.") };
    RulesState.Rules.Add(SanctionsRule);

    FDemocracyGovernmentDiplomacyRuleState BorderRule;
    BorderRule.RuleId = TEXT("border_tension_warning");
    BorderRule.RuleName = TEXT("Border Tension Escalation");
    BorderRule.RuleType = TEXT("Border Tension");
    BorderRule.Description = TEXT("Border tension at 60 or higher is a strategic warning; 75 or higher feeds active war and invasion risk systems.");
    BorderRule.BorderTensionThreshold = 60;
    BorderRule.Consequences = { TEXT("High tension appears in advisor warnings and RTS contract enemies."), TEXT("Very high tension can create durable war/conflict state."), TEXT("Negotiation, trade, and aid can reduce tension over time.") };
    RulesState.Rules.Add(BorderRule);

    FDemocracyGovernmentDiplomacyRuleState SwitchRule;
    SwitchRule.RuleId = TEXT("government_side_switch");
    SwitchRule.RuleName = TEXT("Government Side Switch");
    SwitchRule.RuleType = TEXT("Government Transition");
    SwitchRule.Description = TEXT("Changing between democracy and dictatorship takes time and applies stability, unrest, diplomacy, alliance, treaty, and trust consequences.");
    SwitchRule.StabilityCost = FMath::Clamp(6 + State.PlayerCountry.CountrySizeScore * 2, 8, 18);
    SwitchRule.UnrestCost = FMath::Clamp(4 + State.PlayerCountry.CountrySizeScore * 3, 7, 20);
    SwitchRule.DiplomacyCost = FMath::Clamp(5 + State.PlayerCountry.CountrySizeScore * 2, 7, 16);
    SwitchRule.TurnsRequired = FMath::Clamp(3 + State.PlayerCountry.CountrySizeScore, 4, 8);
    SwitchRule.AllowedGovernmentTypes = { TEXT("Democracy"), TEXT("Dictatorship") };
    SwitchRule.Consequences = { TEXT("Existing incompatible alliances become blocked during transition."), TEXT("Treaty partners lose trust until the new government is stable."), TEXT("Domestic stability drops and unrest rises while institutions are rewritten."), TEXT("Multiplayer side changes occupy a timed server slot and cannot be instant.") };
    RulesState.Rules.Add(SwitchRule);

    const FString PlayerAlignment = GovernmentRuleAlignment(RulesState.PlayerGovernmentType);
    for (const FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
    {
        const bool bAlliance = Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase);
        if (bAlliance)
        {
            if (IsSameGovernmentRuleAlignment(RulesState.PlayerGovernmentType, Relationship.GovernmentType))
            {
                ++RulesState.AllowedAllianceCount;
            }
            else
            {
                ++RulesState.BlockedAllianceCount;
            }
        }
        if (Relationship.ActiveTreaties.Num() > 0 || !Relationship.TreatyStatus.Equals(TEXT("None"), ESearchCase::IgnoreCase))
        {
            ++RulesState.ActiveTreatyCount;
        }
        if (Relationship.bSanctionsActive)
        {
            ++RulesState.ActiveSanctionsCount;
        }
        if (Relationship.BorderTension >= 60)
        {
            ++RulesState.HighBorderTensionCount;
        }
    }

    RulesState.ActiveRestrictions = {
        FString::Printf(TEXT("Player alignment: %s. Formal alliances are limited to matching democracy/dictatorship alignment."), *PlayerAlignment),
        TEXT("Treaties require trust 45+ and border tension below 65 before they can be signed or upgraded."),
        TEXT("Sanctions require hostile/rival posture or border tension 55+, and they reduce trade/trust while raising escalation risk."),
        TEXT("Border tension 60+ is a warning; 75+ exports as active war/invasion pressure to RTS."),
        FString::Printf(TEXT("Current blocked alliances from alignment mismatch: %d."), RulesState.BlockedAllianceCount)
    };

    RulesState.SideSwitchConsequences = {
        FString::Printf(TEXT("Changing government side takes %d turns by default for this country size."), SwitchRule.TurnsRequired),
        FString::Printf(TEXT("During transition apply approximately -%d stability, +%d unrest, and -%d diplomatic standing unless mitigated."), SwitchRule.StabilityCost, SwitchRule.UnrestCost, SwitchRule.DiplomacyCost),
        TEXT("Allies with the old side become restricted until relationships are renegotiated."),
        TEXT("Treaties remain records but can lose trust or be suspended when the new alignment conflicts with the partner."),
        TEXT("Multiplayer side switching is server-timed and cannot be completed by editing local save data.")
    };
    if (RulesState.TransitionTurnsRemaining > 0)
    {
        RulesState.SideSwitchConsequences.Add(FString::Printf(TEXT("Active transition to %s: %d%% complete, %d turns remaining."), *RulesState.TargetGovernmentType, RulesState.TransitionProgress, RulesState.TransitionTurnsRemaining));
    }

    RulesState.Summary = FString::Printf(TEXT("Government/diplomacy rules turn %d: player %s (%s), alliances allowed %d, blocked %d, treaties %d, sanctions %d, high border tensions %d."),
        State.Turn,
        *RulesState.PlayerGovernmentType,
        *PlayerAlignment,
        RulesState.AllowedAllianceCount,
        RulesState.BlockedAllianceCount,
        RulesState.ActiveTreatyCount,
        RulesState.ActiveSanctionsCount,
        RulesState.HighBorderTensionCount);
    return RulesState;
}
FDemocracyRtsSaveBoundaryState FDemocracyGameStateFactory::BuildRtsSaveBoundaryState(const FDemocracySimulationState& State)
{
    FDemocracyRtsSaveBoundaryState Boundary = State.RtsSaveBoundary;
    Boundary.LastUpdatedTurn = State.Turn;
    Boundary.BoundaryVersion = TEXT("RTSSaveBoundary.v1");
    const bool bMultiplayer = State.ObjectiveState.Mode.Equals(TEXT("Multiplayer"), ESearchCase::IgnoreCase);
    Boundary.SimulationAuthority = TEXT("Simulation owns national policy, diplomacy, economy, approval, stability, unrest, advisors, events, objectives, press history, meetings, development, and failure risks.");
    Boundary.RtsAuthority = TEXT("RTS owns unit positions, unit orders, battle resolution, local tactical objectives, province control changes, battlefield construction, resource extraction sites, and army logistics.");
    Boundary.SaveAuthority = bMultiplayer
        ? TEXT("Multiplayer saves are not trusted locally; the server stores authoritative simulation, RTS, war, diplomacy, and ownership state.")
        : TEXT("Single-player saves store simulation state, RTS ownership/backflow, and this boundary locally with autosave and backup protection.");
    Boundary.MultiplayerAuthority = TEXT("Server authority owns player slots, government side, side-switch timers, country ownership, war declarations, RTS outcomes, resources, and anti-cheat validation.");
    Boundary.SimulationOwnedFields = {
        TEXT("playerCountry policies/resources/treasury/economy/approval/stability/unrest"),
        TEXT("diplomacyMatrix relationships/treaties/sanctions/trade"),
        TEXT("eventSystem/advisorSystem/meetingSystem/pressOffice/departments/developmentSystem"),
        TEXT("failureRisk/invasionRisk/warSystem strategic status"),
        TEXT("objectiveState and government transition progress")
    };
    Boundary.RtsOwnedFields = {
        TEXT("unit positions, unit health, formations, orders, and army groups"),
        TEXT("battle instances, local objectives, casualties, and tactical victory state"),
        TEXT("province controller deltas and contested borders"),
        TEXT("battlefield construction, farms, mines, cities, roads, and resource extraction nodes"),
        TEXT("local RTS fog-of-war, pathing, supply lines, and deployment state")
    };
    Boundary.SharedHandshakeFields = {
        TEXT("rtsWorld.ownership provides the durable map ownership snapshot"),
        TEXT("simulationToRtsContract exports simulation inputs to the RTS layer"),
        TEXT("rtsWorld.backflow imports tactical outcomes back into simulation"),
        TEXT("warSystem links strategic wars to RTS conflict identifiers"),
        TEXT("commandAuthority decides which office orders can request RTS actions")
    };
    Boundary.SimulationExportsToRts = {
        TEXT("country resources, treasury, technology, policies, readiness, stability, unrest, approval"),
        TEXT("allies, enemies, treaties, sanctions, trade partners, border tension"),
        TEXT("active wars, escalation, objectives, fronts, victory/defeat conditions"),
        TEXT("region stability, unrest, climate, resource focus, and strategic value"),
        TEXT("authorized strategic commands such as mobilize, defend, negotiate, embargo, trade, aid, emergency")
    };
    Boundary.RtsImportsToSimulation = {
        TEXT("territory gained/lost and province controller changes"),
        TEXT("battle casualties, war fatigue, and military readiness pressure"),
        TEXT("resource disruption, supply route damage, budget strain, and infrastructure damage"),
        TEXT("diplomatic damage, border escalation, invasion risk, and stability shifts"),
        TEXT("battle completion, ceasefire, surrender, occupation, or capital-threat results")
    };
    Boundary.ForbiddenSimulationWrites = {
        TEXT("Simulation office must not directly write unit positions or individual battle results."),
        TEXT("Simulation office must not directly build farms, mines, cities, roads, or battlefield structures."),
        TEXT("Simulation office must not directly teleport resources, troops, or assets on the RTS map."),
        TEXT("Local multiplayer clients must not write save state, country ownership, war outcome, government side, or resource totals.")
    };
    Boundary.SaveRules = {
        TEXT("Single-player manual save persists currentGameState including simulation, RTS ownership/backflow, war, command, contract, and boundary state."),
        TEXT("Single-player autosave rotates primary/autosave/bak1/bak2 so fail-state recovery can load a protected previous save."),
        TEXT("RTS transient render/pathing state should be rebuilt by RTS from durable ownership, unit, battle, and contract data."),
        TEXT("Multiplayer local files may cache UI preferences only; authoritative save state must come from server response."),
        TEXT("Every RTS outcome must enter rtsWorld.backflow before it mutates simulation-owned systems.")
    };
    Boundary.ServerAuthoritativeFields = {
        TEXT("saves: account-linked multiplayer save data, autosave snapshots, protected recovery points, and server revision ids"),
        TEXT("country ownership: country slots, province ownership/controller changes, capital control, occupations, and contested borders"),
        TEXT("wars: declarations, participants, objectives, escalation, fronts, ceasefires, surrender, casualties, and victory/defeat resolution"),
        TEXT("diplomacy: alliances, treaties, sanctions, trade partner status, border tension, hostile/rival status, and diplomatic cooldowns"),
        TEXT("government transitions: democracy/dictatorship selection, side-switch timers, transition progress, consequences, and slot limits"),
        TEXT("resources: authoritative resource totals, production, consumption, shortages, imports/exports, reserves, and anti-cheat reconciliation"),
        TEXT("RTS results: battle lost/won, province captured/lost, capital threatened, supply route broken, resource disruption, and backflow queue imports")
    };
    Boundary.ClientRequestOnlyFields = {
        TEXT("local client may request policy, diplomacy, trade, aid, embargo, mobilization, emergency, and meeting actions but server validates final state"),
        TEXT("local client may display cached country, diplomacy, resource, and RTS snapshots but must not treat them as authoritative in multiplayer"),
        TEXT("local client may send RTS commands, but server owns accepted commands, timestamps, outcomes, and rollback/reconciliation"),
        TEXT("local client may store UI settings, keybinds, accessibility, graphics, audio, login remember-me preference, and local debug settings only")
    };
    Boundary.ServerValidationNotes = {
        TEXT("Reject client-submitted multiplayer save mutations unless generated by a signed server transaction."),
        TEXT("Validate every country ownership, war, diplomacy, government transition, resource, and RTS-result mutation against server state and turn/revision."),
        TEXT("Run RTS outcome imports through rtsWorld.backflow so simulation attention, logs, warnings, and autosave recovery remain consistent."),
        TEXT("Never trust local resource totals, battle results, province captures, government side switches, diplomacy changes, or war resolution in multiplayer."),
        TEXT("Keep single-player local authority separate from multiplayer server authority to avoid accidentally enabling client-side cheating.")
    };
    Boundary.BoundaryValidationNotes = {
        FString::Printf(TEXT("Turn %d boundary refreshed after %d owned provinces and %d active war(s)."), State.Turn, State.RtsWorld.Ownership.PlayerControlledProvinces, State.WarSystem.ActiveConflicts.Num()),
        FString::Printf(TEXT("Contract exports %d regions, %d diplomacy records, and %d active war/risk records."), State.SimulationToRtsContract.Regions.Num(), State.SimulationToRtsContract.Diplomacy.Num(), State.SimulationToRtsContract.ActiveWars.Num()),
        bMultiplayer ? TEXT("Mode: multiplayer server-authoritative boundary active.") : TEXT("Mode: single-player local-save boundary active.")
    };
    Boundary.BoundarySummary = FString::Printf(TEXT("RTS save boundary %s: simulation owns national systems, RTS owns tactical state, %s save authority, %d export fields, %d import result types."),
        *Boundary.BoundaryVersion,
        bMultiplayer ? TEXT("server") : TEXT("local single-player"),
        Boundary.SimulationExportsToRts.Num(),
        Boundary.RtsImportsToSimulation.Num());
    return Boundary;
}
FDemocracyWarSystemState FDemocracyGameStateFactory::BuildWarConflictState(const FDemocracySimulationState& State)
{
    FDemocracyWarSystemState WarSystem = State.WarSystem;
    WarSystem.LastUpdatedTurn = State.Turn;
    WarSystem.ActiveConflicts.RemoveAll([](const FDemocracyWarConflictState& Conflict)
    {
        return Conflict.Status.Equals(TEXT("Resolved"), ESearchCase::IgnoreCase) || Conflict.Status.Equals(TEXT("Archived"), ESearchCase::IgnoreCase);
    });

    const int32 InvasionPressure = State.InvasionRisk.CurrentInvasionRisk;
    const int32 ReadinessGap = FMath::Max(0, State.InvasionRisk.MilitaryReadinessWarningThreshold - State.PlayerCountry.MilitaryReadiness);
    const int32 BorderPressure = State.RtsWorld.Ownership.BorderProvinceCount + InvasionPressure / 8;
    FString PrimaryOpponent;
    int32 HighestTension = 0;
    for (const FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
    {
        const bool bThreat = Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase) || Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase) || Relationship.BorderTension >= 60;
        if (bThreat && Relationship.BorderTension >= HighestTension)
        {
            HighestTension = Relationship.BorderTension;
            PrimaryOpponent = Relationship.CountryName;
        }
    }
    if (PrimaryOpponent.IsEmpty() && State.RtsWorld.Rivals.Num() > 0)
    {
        const FDemocracyRivalCountryState& Rival = State.RtsWorld.Rivals[0];
        PrimaryOpponent = Rival.CountryName;
        HighestTension = Rival.BorderPressure;
    }

    const bool bShouldCreateConflict = !PrimaryOpponent.IsEmpty() && (InvasionPressure >= 35 || HighestTension >= 55 || State.RtsWorld.Backflow.PendingOutcomes.Num() > 0 || State.RtsWorld.Backflow.OutcomeHistory.Num() > 0);
    if (bShouldCreateConflict)
    {
        const FString ConflictId = FString::Printf(TEXT("war-%s"), *PrimaryOpponent.Replace(TEXT(" "), TEXT("-")).ToLower());
        FDemocracyWarConflictState* Existing = nullptr;
        for (FDemocracyWarConflictState& Conflict : WarSystem.ActiveConflicts)
        {
            if (Conflict.ConflictId.Equals(ConflictId, ESearchCase::IgnoreCase))
            {
                Existing = &Conflict;
                break;
            }
        }
        if (!Existing)
        {
            FDemocracyWarConflictState NewConflict;
            NewConflict.ConflictId = ConflictId;
            NewConflict.ConflictName = FString::Printf(TEXT("%s Border Conflict"), *PrimaryOpponent);
            NewConflict.ConflictType = InvasionPressure >= 65 ? TEXT("Invasion War") : TEXT("Border Conflict");
            NewConflict.Status = TEXT("Active");
            NewConflict.PrimaryObjective = TEXT("Defend controlled provinces, preserve capital control, and prevent forced takeover.");
            NewConflict.EnemyObjective = TEXT("Pressure borders, disrupt resources, and force political concessions.");
            NewConflict.StartedTurn = State.Turn;
            NewConflict.VictoryCondition = TEXT("Hold capital, reduce invasion risk below warning, and push escalation below 25.");
            NewConflict.DefeatCondition = TEXT("Capital lost, takeover risk reaches trigger, or military readiness collapses while border pressure is critical.");
            WarSystem.ActiveConflicts.Add(NewConflict);
            Existing = &WarSystem.ActiveConflicts.Last();
        }
        if (Existing)
        {
            Existing->LastUpdatedTurn = State.Turn;
            Existing->ConflictType = InvasionPressure >= 65 ? TEXT("Invasion War") : TEXT("Border Conflict");
            Existing->EscalationLevel = FMath::Clamp(1 + InvasionPressure / 20 + HighestTension / 30 + ReadinessGap / 10, 1, 5);
            Existing->WarScore = FMath::Clamp(State.PlayerCountry.MilitaryReadiness + State.PlayerCountry.DiplomaticStanding / 2 - InvasionPressure - HighestTension / 2, -100, 100);
            Existing->VictoryProgress = FMath::Clamp(50 + Existing->WarScore / 2 - InvasionPressure / 3, 0, 100);
            Existing->DefeatRisk = FMath::Clamp(InvasionPressure + ReadinessGap * 2 + HighestTension / 3 - State.PlayerCountry.Stability / 4, 0, 100);
            Existing->Status = Existing->DefeatRisk >= 80 ? TEXT("Critical") : (Existing->EscalationLevel >= 4 ? TEXT("Escalating") : TEXT("Active"));
            Existing->Participants = {
                { State.PlayerCountry.CountryName, TEXT("Defender"), TEXT("Player"), FMath::Clamp(State.PlayerCountry.MilitaryReadiness, 0, 100), FMath::Clamp(100 - State.RtsWorld.Backflow.WarFatigue, 0, 100), State.RtsWorld.Backflow.TotalCasualties },
                { PrimaryOpponent, TEXT("Aggressor"), TEXT("Opponent"), FMath::Clamp(45 + HighestTension / 2, 0, 100), FMath::Clamp(45 + InvasionPressure / 2, 0, 100), 0 }
            };
            Existing->Fronts.Reset();
            Existing->Fronts.Add({ TEXT("Primary Border Front"), TEXT("Border Region"), FString::Printf(TEXT("%s border"), *PrimaryOpponent), FMath::Clamp(BorderPressure + InvasionPressure / 2, 0, 100), FMath::Clamp(100 - InvasionPressure + State.PlayerCountry.MilitaryReadiness / 3, 0, 100), Existing->Status });
            if (State.RtsWorld.Ownership.ContestedProvinces > 0)
            {
                Existing->Fronts.Add({ TEXT("Contested Province Front"), TEXT("Contested Provinces"), TEXT("Unresolved province control"), FMath::Clamp(State.RtsWorld.Ownership.ContestedProvinces * 12, 0, 100), FMath::Clamp(55 - State.RtsWorld.Ownership.ContestedProvinces * 5, 0, 100), TEXT("Contested") });
            }
            Existing->ActiveModifiers = {
                FString::Printf(TEXT("Invasion pressure %d"), InvasionPressure),
                FString::Printf(TEXT("Opponent tension %d"), HighestTension),
                FString::Printf(TEXT("Readiness %d"), State.PlayerCountry.MilitaryReadiness),
                FString::Printf(TEXT("War fatigue %d"), State.RtsWorld.Backflow.WarFatigue)
            };
        }
    }

    WarSystem.ActiveConflictCount = WarSystem.ActiveConflicts.Num();
    WarSystem.EscalationPressure = 0;
    WarSystem.WarFatigue = State.RtsWorld.Backflow.WarFatigue;
    WarSystem.TotalCasualties = State.RtsWorld.Backflow.TotalCasualties;
    for (const FDemocracyWarConflictState& Conflict : WarSystem.ActiveConflicts)
    {
        WarSystem.EscalationPressure += Conflict.EscalationLevel * 12 + Conflict.DefeatRisk / 5;
    }
    WarSystem.EscalationPressure = FMath::Clamp(WarSystem.EscalationPressure, 0, 100);
    WarSystem.ReadinessStatus = State.PlayerCountry.MilitaryReadiness >= State.InvasionRisk.MilitaryReadinessWarningThreshold ? TEXT("Ready") : TEXT("Readiness Warning");
    WarSystem.Summary = WarSystem.ActiveConflictCount > 0
        ? FString::Printf(TEXT("War state: %d active conflict(s), escalation pressure %d, fatigue %d, casualties %d."), WarSystem.ActiveConflictCount, WarSystem.EscalationPressure, WarSystem.WarFatigue, WarSystem.TotalCasualties)
        : FString::Printf(TEXT("War state: no active wars. Border pressure %d, invasion risk %d, readiness %d."), BorderPressure, InvasionPressure, State.PlayerCountry.MilitaryReadiness);
    return WarSystem;
}

FDemocracySimulationToRtsContractState FDemocracyGameStateFactory::BuildSimulationToRtsContractState(const FDemocracySimulationState& State)
{
    FDemocracySimulationToRtsContractState Contract;
    const FDemocracyCountryState& Country = State.PlayerCountry;
    Contract.LastUpdatedTurn = State.Turn;
    Contract.PlayerCountryName = Country.CountryName;
    Contract.GovernmentType = State.ObjectiveState.PlayerGovernmentType.IsEmpty() ? TEXT("Democracy") : State.ObjectiveState.PlayerGovernmentType;
    Contract.Treasury = Country.Treasury;
    Contract.MilitaryReadiness = Country.MilitaryReadiness;
    Contract.Technology = Country.Technology;
    Contract.Stability = Country.Stability;
    Contract.Unrest = Country.Unrest;
    Contract.PublicApproval = Country.PublicApproval;
    Contract.InvasionRisk = State.InvasionRisk.CurrentInvasionRisk;
    Contract.Resources = Country.Resources;
    Contract.ActivePolicies = {
        FString::Printf(TEXT("Economic:%s"), *Country.Policies.EconomicPolicy),
        FString::Printf(TEXT("Environmental:%s"), *Country.Policies.EnvironmentalPolicy),
        FString::Printf(TEXT("Military:%s"), *Country.Policies.MilitaryPolicy),
        FString::Printf(TEXT("Diplomacy:%s"), *Country.Policies.DiplomacyPolicy),
        FString::Printf(TEXT("Civil:%s"), *Country.Policies.CivilPolicy)
    };
    Contract.StrategicPermissions = {
        TEXT("Simulation office may mobilize, defend, negotiate, embargo, trade, send aid, and declare emergencies."),
        TEXT("RTS layer receives readiness, diplomacy, resources, regional stability, wars, technologies, and policy posture."),
        TEXT("Direct troop movement, battles, construction, farms, mines, city upgrades, and manual resource transport remain RTS-only."),
        State.RtsSaveBoundary.BoundarySummary
    };

    for (const FDemocracyDevelopmentTrackState& Track : State.DevelopmentSystem.Tracks)
    {
        if (Track.Level > 0 || Track.Progress > 0)
        {
            Contract.TechnologyUnlocks.Add(FString::Printf(TEXT("%s L%d: %s"), *Track.TrackName, Track.Level, *FString::Join(Track.Unlocks, TEXT(", "))));
        }
    }

    for (const FDemocracyDiplomacyRelationshipState& Relationship : State.DiplomacyMatrix.Relationships)
    {
        FDemocracyRtsDiplomacyInputState Input;
        Input.CountryName = Relationship.CountryName;
        Input.RelationshipStatus = Relationship.RelationshipStatus;
        Input.TreatyStatus = Relationship.TreatyStatus;
        Input.bAlly = Relationship.RelationshipStatus.Equals(TEXT("Ally"), ESearchCase::IgnoreCase);
        Input.bEnemy = Relationship.RelationshipStatus.Equals(TEXT("Hostile"), ESearchCase::IgnoreCase) || Relationship.RelationshipStatus.Equals(TEXT("Rival"), ESearchCase::IgnoreCase) || Relationship.BorderTension >= 70;
        Input.bTradePartner = Relationship.bTradePartner;
        Input.bSanctionsActive = Relationship.bSanctionsActive;
        Input.BorderTension = Relationship.BorderTension;
        Input.Trust = Relationship.Trust;
        Contract.Diplomacy.Add(Input);
        if (Input.bAlly) Contract.Allies.Add(Relationship.CountryName);
        if (Input.bEnemy) Contract.Enemies.Add(Relationship.CountryName);
        if (Input.bEnemy && Relationship.BorderTension >= 75)
        {
            Contract.ActiveWars.Add(FString::Printf(TEXT("%s border conflict risk %d"), *Relationship.CountryName, Relationship.BorderTension));
        }
    }

    for (const FDemocracyRegionState& Region : State.Demographics.Regions)
    {
        FDemocracyRtsRegionInputState Input;
        Input.RegionName = Region.RegionName;
        Input.Climate = Region.Climate;
        Input.ResourceFocus = Region.FoodAccess < 45 ? TEXT("Food") : (Region.Infrastructure < 45 ? TEXT("Infrastructure") : TEXT("Population"));
        Input.Stability = Region.Stability;
        Input.Unrest = Region.Unrest;
        Input.StrategicValue = FMath::Clamp((Region.PopulationShare / 8) + (Region.Infrastructure / 25) + (Region.Security / 25), 1, 10);
        Input.bPlayerControlled = true;
        Input.bBorderRegion = Region.Security < 45 || Region.Unrest > 50;
        Contract.Regions.Add(Input);
    }

    for (const FDemocracyProvinceOwnershipState& Province : State.RtsWorld.Ownership.Provinces)
    {
        if (!Province.bPlayerControlled || Contract.Regions.Num() >= 24)
        {
            continue;
        }
        FDemocracyRtsRegionInputState Input;
        Input.RegionName = Province.ProvinceName;
        Input.Climate = Province.Climate;
        Input.ResourceFocus = Province.ResourceFocus;
        Input.Stability = Province.Stability;
        Input.Unrest = Province.Unrest;
        Input.StrategicValue = Province.StrategicValue;
        Input.bPlayerControlled = Province.bPlayerControlled;
        Input.bBorderRegion = Province.bBorderProvince;
        Contract.Regions.Add(Input);
    }

    for (const FDemocracyWarConflictState& Conflict : State.WarSystem.ActiveConflicts)
    {
        Contract.ActiveWars.Add(FString::Printf(TEXT("%s | %s | escalation %d | victory %d | defeat risk %d"), *Conflict.ConflictName, *Conflict.Status, Conflict.EscalationLevel, Conflict.VictoryProgress, Conflict.DefeatRisk));
    }
    if (Contract.ActiveWars.Num() == 0 && State.InvasionRisk.CurrentInvasionRisk >= State.InvasionRisk.BorderPressureWarningThreshold)
    {
        Contract.ActiveWars.Add(FString::Printf(TEXT("Unassigned border pressure risk %d"), State.InvasionRisk.CurrentInvasionRisk));
    }

    Contract.ExportSummary = FString::Printf(TEXT("Sim-to-RTS v1 turn %d exports %s: resources F%d G%d W%d M%d, readiness %d, allies %d, enemies %d, active war risks %d, regions %d."),
        State.Turn,
        *Country.CountryName,
        Country.Resources.Food,
        Country.Resources.GasOil,
        Country.Resources.Wood,
        Country.Resources.Metals,
        Country.MilitaryReadiness,
        Contract.Allies.Num(),
        Contract.Enemies.Num(),
        Contract.ActiveWars.Num(),
        Contract.Regions.Num());
    return Contract;
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
    State.PlayerCountry.Policies.PolicyRuleStatus = {
        TEXT("Policy rules active: advanced choices may require treasury, resources, national conditions, and cooldown time.")
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
    State.EconomyBudget.DebtCapacity = FMath::Clamp(700 + State.PlayerCountry.EconomicHealth * 10 + State.PlayerCountry.DiplomaticStanding * 4 - State.PlayerCountry.Unrest * 4, 350, 3000);
    State.EconomyBudget.SpendingLimit = FMath::Clamp(State.EconomyBudget.Income + State.PlayerCountry.Treasury / 8 + FMath::Max(0, State.EconomyBudget.DebtCapacity - State.EconomyBudget.Debt) / 10, 45, 220);
    State.EconomyBudget.CreditStress = 0;
    State.EconomyBudget.bSpendingLimited = false;
    State.EconomyBudget.SpendingPosture = TEXT("Balanced Services");
    State.EconomyBudget.BudgetConstraintStatus = TEXT("Initial budget within debt capacity.");
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
    State.DiplomacyMatrix = BuildDiplomacyMatrixState(State.WorldMap, State.Turn);

    State.RtsWorld.SimulationSecond = 0;
    State.RtsWorld.ControlledTerritories = DifficultyProfile.CountrySizeScore * 3;
    State.RtsWorld.BorderTerritories = DifficultyProfile.CountrySizeScore + 1;
    State.RtsWorld.KnownRivalCountries = FMath::Clamp(DifficultyProfile.CountrySizeScore + 1, 2, 5);
    State.RtsWorld.ActiveStrategicLayers = { TEXT("Territory"), TEXT("Resources"), TEXT("Diplomacy"), TEXT("Military Pressure"), TEXT("City/Base"), TEXT("World Map") };
    State.RtsWorld.ActiveViewMode = TEXT("world_map");
    State.RtsWorld.ScopeBoundary.ScopeVersion = TEXT("RTSScope.v1");
    State.RtsWorld.ScopeBoundary.ScopeSummary = TEXT("Simulation office grants strategic authority; RTS resolves city/base work, world-map movement, battles, province control, and tactical resource disruption.");
    State.RtsWorld.ScopeBoundary.RtsOwns = { TEXT("City/base layout, building placement, upgrade queues, local production, unit positions, army movement, battles, scouting, supply routes, province capture, and tactical timers."), TEXT("World-map view state, tactical overlays, selected province/city/base targets, and resolved combat/build results."), TEXT("RTS outcome queue items that must be imported back into simulation before national consequences are final.") };
    State.RtsWorld.ScopeBoundary.SimulationOwns = { TEXT("Policies, advisors, diplomacy, treaties, sanctions, economy, budget, public approval, stability, unrest, demographics, events, objectives, autosaves, and fail-state protection."), TEXT("Strategic permissions such as mobilization, emergency powers, war declarations, alliance aid, sanctions, trade, and ceasefire negotiation."), TEXT("Player-facing briefings and dashboard summaries that explain why RTS actions are available or blocked.") };
    State.RtsWorld.ScopeBoundary.BlockedUntilRts = { TEXT("Direct troop movement from the office"), TEXT("Manual battle targeting"), TEXT("Manual farm/mine/logging/oil harvesting"), TEXT("Permanent city construction placement"), TEXT("Live unit pathfinding and combat") };
    State.RtsWorld.ScopeBoundary.BackflowRequired = { TEXT("Territory gained or lost"), TEXT("Province captured or contested"), TEXT("Casualties"), TEXT("Resource disruption"), TEXT("War fatigue"), TEXT("Diplomatic damage"), TEXT("Stability and unrest shifts"), TEXT("Invasion risk"), TEXT("Budget strain"), TEXT("Supply-route failure") };
    State.RtsWorld.ScopeBoundary.CandidateAssetPacks = { TEXT("FabLibrary/RTS_Modern_Combat_Vehicle_Pack_Free-f9509a39"), TEXT("FabLibrary/Tactical_Crowd_AI_Toolkit__GPU_Influence_Maps-d81cbf03"), TEXT("FabLibrary/Vehicle_Variety_Pack_Volume_2-591e3b3f"), TEXT("FabLibrary/Arctic_Military_Soldier___Cold_Weather_Combat_Character"), TEXT("FabLibrary/Naval_Tactical_Soldier___Maritime_Military_Character") };

    FDemocracyRtsViewModeState CityView;
    CityView.ViewId = TEXT("city_base");
    CityView.DisplayName = TEXT("City / Base View");
    CityView.Purpose = TEXT("Manage the capital/base layout, building state, production, storage, defenses, and upgrade queues.");
    CityView.bImplementedPlaceholder = true;
    CityView.bDefaultView = false;
    CityView.Interactions = { TEXT("Select building"), TEXT("Inspect production"), TEXT("Queue construction placeholder"), TEXT("Queue upgrade placeholder"), TEXT("Inspect city defense") };
    CityView.VisibleLayers = { TEXT("Buildings"), TEXT("Production"), TEXT("Storage"), TEXT("Defenses"), TEXT("Upgrade timers") };

    FDemocracyRtsViewModeState WorldView;
    WorldView.ViewId = TEXT("world_map");
    WorldView.DisplayName = TEXT("World / Map View");
    WorldView.Purpose = TEXT("Inspect Planet Dulia countries, provinces, alliances, rivals, fronts, armies, rally points, and strategic pressure.");
    WorldView.bImplementedPlaceholder = true;
    WorldView.bDefaultView = true;
    WorldView.Interactions = { TEXT("Select country"), TEXT("Select province"), TEXT("Inspect army placeholder"), TEXT("Set rally point placeholder"), TEXT("View border pressure") };
    WorldView.VisibleLayers = { TEXT("Country ownership"), TEXT("Province control"), TEXT("Government type"), TEXT("Alliances"), TEXT("War fronts"), TEXT("Supply routes placeholder") };
    State.RtsWorld.ViewModes = { CityView, WorldView };

    State.RtsWorld.CityBase.BaseId = TEXT("capital-base");
    State.RtsWorld.CityBase.DisplayName = TEXT("Capital Command District");
    State.RtsWorld.CityBase.LinkedCountryName = StateName;
    State.RtsWorld.CityBase.ViewModeId = TEXT("city_base");
    State.RtsWorld.CityBase.GridWidth = 12;
    State.RtsWorld.CityBase.GridHeight = 18;
    State.RtsWorld.CityBase.BaseSummary = TEXT("Initial placeholder city/base supports resource buildings, defense, storage, research, and command structures. Visual assets stay replaceable while mechanics are tested.");
    State.RtsWorld.CityBase.RuntimeNotes = { TEXT("Building layout is data-only for now."), TEXT("Permanent assets should replace placeholder meshes after RTS mechanics stabilize."), TEXT("Any build/upgrade completion must report through RTS backflow before simulation consequences apply.") };
    State.RtsWorld.CityBase.Buildings = {
        { TEXT("capital_command"), TEXT("Government Command Center"), TEXT("Command"), TEXT("Authority"), TEXT("Generic civic HQ placeholder; future government/capital building asset"), 1, 0, 140, 0, 0, 18, true, false, TEXT("Operational"), {}, { TEXT("capital"), TEXT("command"), TEXT("required") } },
        { TEXT("barracks"), TEXT("Defense Barracks"), TEXT("Military"), TEXT("Readiness"), TEXT("RTS_Modern_Combat_Vehicle_Pack_Free for later vehicle/army staging visuals"), 1, 75, 120, 2, 0, 28, true, false, TEXT("Operational"), { TEXT("Government Command Center") }, { TEXT("military"), TEXT("defense") } },
        { TEXT("farm_hub"), TEXT("Agriculture Hub"), TEXT("Resource"), TEXT("Food"), TEXT("Generic farm/field placeholder"), 1, 45, 80, 1, 8, 0, true, false, TEXT("Operational"), {}, { TEXT("food"), TEXT("production") } },
        { TEXT("fuel_depot"), TEXT("Fuel Depot"), TEXT("Resource"), TEXT("Fuel"), TEXT("Generic fuel/oil storage placeholder"), 1, 55, 95, 2, 5, 4, true, false, TEXT("Operational"), {}, { TEXT("fuel"), TEXT("logistics") } },
        { TEXT("lumber_yard"), TEXT("Lumber Yard"), TEXT("Resource"), TEXT("Wood"), TEXT("Generic lumber/warehouse placeholder"), 1, 40, 70, 1, 6, 0, true, false, TEXT("Operational"), {}, { TEXT("wood"), TEXT("construction") } },
        { TEXT("metal_refinery"), TEXT("Metal Refinery"), TEXT("Resource"), TEXT("Metals"), TEXT("Generic industrial refinery placeholder"), 1, 70, 110, 2, 5, 2, true, false, TEXT("Operational"), {}, { TEXT("metals"), TEXT("industry") } },
        { TEXT("warehouse"), TEXT("Strategic Warehouse"), TEXT("Storage"), TEXT("Reserve"), TEXT("Warehouse/freight placeholder"), 1, 50, 90, 1, 0, 6, true, false, TEXT("Operational"), {}, { TEXT("storage"), TEXT("supplies") } },
        { TEXT("research_center"), TEXT("Development Center"), TEXT("Technology"), TEXT("Research"), TEXT("Generic lab/communications placeholder"), 1, 80, 140, 3, 2, 0, true, false, TEXT("Operational"), { TEXT("Government Command Center") }, { TEXT("technology"), TEXT("upgrades") } },
        { TEXT("defense_post"), TEXT("Perimeter Defense Post"), TEXT("Defense"), TEXT("Security"), TEXT("RTS_Modern_Combat_Vehicle_Pack_Free for later defensive vehicle visuals"), 1, 65, 115, 2, 0, 32, true, false, TEXT("Operational"), { TEXT("Defense Barracks") }, { TEXT("defense"), TEXT("border") } }
    };
    State.RtsWorld.CityBase.BuildQueueCount = State.RtsWorld.CityBase.BuildQueue.Num();
    State.RtsWorld.CityBase.UpgradeQueueCount = 0;
    State.RtsWorld.Rivals = {
        { TEXT("Northmark"), TEXT("Pragmatic"), TEXT("Cautious"), 42 + DifficultyProfile.CountrySizeScore * 6, 12 + DifficultyProfile.CountrySizeScore * 4, 18 },
        { TEXT("Eastmere"), TEXT("Commercial"), TEXT("Neutral"), 38 + DifficultyProfile.CountrySizeScore * 5, 8 + DifficultyProfile.CountrySizeScore * 3, 30 },
        { TEXT("Southport Union"), TEXT("Assertive"), TEXT("Tense"), 48 + DifficultyProfile.CountrySizeScore * 7, 18 + DifficultyProfile.CountrySizeScore * 5, 12 }
    };
    State.RtsWorld.Backflow.LastAppliedTurn = 0;
    State.RtsWorld.Backflow.PendingOutcomeCount = 0;
    State.RtsWorld.Backflow.WarFatigue = 0;
    State.RtsWorld.Backflow.ResourceDisruptionPressure = 0;
    State.RtsWorld.Backflow.BudgetStrainPressure = 0;
    State.RtsWorld.Backflow.DiplomaticDamagePressure = 0;
    State.RtsWorld.Backflow.LastOutcomeSummary = TEXT("RTS backflow is ready. Future tactical results will feed territory, casualties, resources, fatigue, diplomacy, stability, invasion risk, and budget strain back into simulation.");
    State.RtsWorld.Ownership = BuildMapOwnershipState(State.WorldMap, StateName, State.Turn, State.RtsWorld.ControlledTerritories);
    if (State.RtsWorld.Ownership.Provinces.Num() > 0)
    {
        State.RtsWorld.CityBase.LinkedProvinceId = State.RtsWorld.Ownership.Provinces[0].ProvinceId;
    }
    State.RtsWorld.ControlledTerritories = State.RtsWorld.Ownership.PlayerControlledProvinces;
    State.RtsWorld.BorderTerritories = State.RtsWorld.Ownership.BorderProvinceCount;
    State.GovernmentDiplomacyRules = FDemocracyGameStateFactory::BuildGovernmentDiplomacyRulesState(State);
    State.RtsSaveBoundary = FDemocracyGameStateFactory::BuildRtsSaveBoundaryState(State);
    State.WarSystem = FDemocracyGameStateFactory::BuildWarConflictState(State);
    State.SimulationToRtsContract = FDemocracyGameStateFactory::BuildSimulationToRtsContractState(State);
    State.RtsSaveBoundary = FDemocracyGameStateFactory::BuildRtsSaveBoundaryState(State);
    State.SimulationToRtsContract = FDemocracyGameStateFactory::BuildSimulationToRtsContractState(State);
    State.CommandAuthority = BuildCommandAuthorityState(State);

    return State;
}







