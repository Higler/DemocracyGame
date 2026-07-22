#include "LoginHUD.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Engine/PointLight.h"
#include "DifficultyProfile.h"
#include "DemocracyGameState.h"
#include "DemocracySaveGameRuntime.h"
#include "Framework/Application/SlateApplication.h"
#include "OfficePlayerPawn.h"
#include "OfficeLevelBuilder.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "SlateOptMacros.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/PointLightComponent.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    struct FPlaceholderServer
    {
        FString Name;
        FString Region;
        FString Ruleset;
        int32 PingMs;
    };

    const TArray<FPlaceholderServer>& GetPlaceholderServers()
    {
        static const TArray<FPlaceholderServer> Servers = {
            { TEXT("Civic-East-01"), TEXT("US East"), TEXT("Standard"), 28 },
            { TEXT("Civic-Central-02"), TEXT("US Central"), TEXT("Standard"), 47 },
            { TEXT("Civic-West-01"), TEXT("US West"), TEXT("Standard"), 71 },
            { TEXT("Coalition-EU-01"), TEXT("Europe"), TEXT("Standard"), 118 },
            { TEXT("Assembly-Asia-01"), TEXT("Asia Pacific"), TEXT("Standard"), 184 }
        };

        return Servers;
    }

    FText BodyText(const FString& Text)
    {
        return FText::FromString(Text);
    }

    FString PercentText(float Value)
    {
        return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.0f));
    }

    FString JsonEscape(FString Text)
    {
        Text.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        Text.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Text.ReplaceInline(TEXT("\r"), TEXT("\\r"));
        Text.ReplaceInline(TEXT("\n"), TEXT("\\n"));
        return Text;
    }

    FString SaveTimestamp()
    {
        return FDateTime::UtcNow().ToIso8601();
    }
}


    struct FPolicyTickModifiers
    {
        int32 ApprovalDelta = 0;
        int32 StabilityDelta = 0;
        int32 UnrestDelta = 0;
        int32 TreasuryDelta = 0;
        int32 EconomicDelta = 0;
        int32 DiplomacyDelta = 0;
        int32 MilitaryDelta = 0;
        int32 InfrastructureDelta = 0;
        int32 EnvironmentDelta = 0;
        int32 FoodDelta = 0;
        int32 GasOilDelta = 0;
        int32 WoodDelta = 0;
        int32 MetalsDelta = 0;
        int32 WaterDelta = 0;
        int32 InvasionRiskDelta = 0;
    };

    void AddPolicyEffect(FPolicyTickModifiers& Modifiers, TArray<FString>& Effects, const FString& EffectText,
        int32 Approval, int32 Stability, int32 Unrest, int32 Treasury, int32 Economy, int32 Diplomacy,
        int32 Military, int32 Infrastructure, int32 Environment, int32 Food, int32 GasOil, int32 Wood, int32 Metals, int32 Water, int32 InvasionRisk)
    {
        Effects.Add(EffectText);
        Modifiers.ApprovalDelta += Approval;
        Modifiers.StabilityDelta += Stability;
        Modifiers.UnrestDelta += Unrest;
        Modifiers.TreasuryDelta += Treasury;
        Modifiers.EconomicDelta += Economy;
        Modifiers.DiplomacyDelta += Diplomacy;
        Modifiers.MilitaryDelta += Military;
        Modifiers.InfrastructureDelta += Infrastructure;
        Modifiers.EnvironmentDelta += Environment;
        Modifiers.FoodDelta += Food;
        Modifiers.GasOilDelta += GasOil;
        Modifiers.WoodDelta += Wood;
        Modifiers.MetalsDelta += Metals;
        Modifiers.WaterDelta += Water;
        Modifiers.InvasionRiskDelta += InvasionRisk;
    }

    FPolicyTickModifiers BuildPolicyModifiers(const FDemocracyPolicyState& Policies, TArray<FString>* OutEffects = nullptr)
    {
        FPolicyTickModifiers Modifiers;
        TArray<FString> Effects;

        if (Policies.EconomicPolicy.Equals(TEXT("Stimulus Spending"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Stimulus Spending: approval/economy/infrastructure rise, treasury burns faster."), 2, 1, -1, -12, 3, 0, 0, 1, 0, 2, 0, 0, 0, 1, 0);
        }
        else if (Policies.EconomicPolicy.Equals(TEXT("Austerity Program"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Austerity Program: treasury improves, approval and stability suffer."), -3, -1, 2, 16, -2, 0, 0, -1, 0, -1, 0, 0, 0, 0, 0);
        }
        else if (Policies.EconomicPolicy.Equals(TEXT("Industrial Subsidies"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Industrial Subsidies: economy and materials improve, treasury and environment decline."), 0, 0, 0, -8, 3, 0, 0, 1, -2, 0, 1, 2, 3, 0, 0);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Balanced Budget: modest treasury gain with low public impact."), 0, 0, 0, 6, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }

        if (Policies.EnvironmentalPolicy.Equals(TEXT("Conservation Mandate"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Conservation Mandate: environmental health and stability improve, extraction slows."), 1, 1, -1, -4, 0, 1, 0, 0, 3, 1, -1, -2, -2, 2, 0);
        }
        else if (Policies.EnvironmentalPolicy.Equals(TEXT("Extraction Expansion"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Extraction Expansion: resources and treasury improve, environment and unrest worsen."), -1, -1, 2, 8, 2, -1, 0, 0, -4, 0, 3, 4, 4, -1, 0);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Managed Development: balanced resource output and environmental pressure."), 0, 0, 0, 2, 1, 0, 0, 0, -1, 1, 1, 1, 1, 0, 0);
        }

        if (Policies.MilitaryPolicy.Equals(TEXT("National Mobilization"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("National Mobilization: readiness rises, but treasury, approval, and unrest worsen."), -2, 0, 2, -10, -1, -1, 5, 0, 0, -1, -2, 0, -2, 0, -1);
        }
        else if (Policies.MilitaryPolicy.Equals(TEXT("Demilitarization"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Demilitarization: approval, treasury, and diplomacy rise, military risk increases."), 2, 0, -1, 8, 0, 3, -4, 0, 0, 0, 1, 0, 1, 0, 3);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Defensive Readiness: maintains military readiness without provoking rivals."), 0, 1, 0, -3, 0, 1, 2, 0, 0, 0, -1, 0, -1, 0, -1);
        }

        if (Policies.DiplomacyPolicy.Equals(TEXT("Alliance Outreach"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Alliance Outreach: diplomacy and invasion safety improve, treasury cost rises."), 1, 1, -1, -6, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0, -3);
        }
        else if (Policies.DiplomacyPolicy.Equals(TEXT("Hardline Sovereignty"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Hardline Sovereignty: readiness and domestic approval rise, diplomacy and invasion risk worsen."), 1, 0, 1, 0, 0, -4, 2, 0, 0, 0, -1, 0, -1, 0, 3);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Neutral Engagement: stable diplomacy with no strong alliance push."), 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }

        if (Policies.CivilPolicy.Equals(TEXT("Civil Liberties"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Civil Liberties: approval and stability improve while unrest cools slowly."), 3, 1, -2, -3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }
        else if (Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase))
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Emergency Powers: unrest drops immediately, but approval, diplomacy, and legitimacy suffer."), -4, 2, -4, -4, -1, -3, 1, 0, 0, 0, 0, 0, 0, 0, 1);
        }
        else
        {
            AddPolicyEffect(Modifiers, Effects, TEXT("Public Stability: small stability support with low civil backlash."), 0, 1, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        }

        if (OutEffects)
        {
            *OutEffects = Effects;
        }
        return Modifiers;
    }

    FString JoinPolicyEffects(const FDemocracyPolicyState& Policies)
    {
        TArray<FString> Effects;
        BuildPolicyModifiers(Policies, &Effects);
        return FString::Join(Effects, TEXT("\n"));
    }


    FDemocracyResourceChainEntry MakeResourceChainEntry(const FString& Name, int32 Reserve, int32 ReserveTarget, int32 Production, int32 Consumption, int32 Imports, int32 Exports, int32 StrategicValue, const FString& Role, const TArray<FString>& Drivers)
    {
        FDemocracyResourceChainEntry Entry;
        Entry.ResourceName = Name;
        Entry.Production = FMath::Max(0, Production);
        Entry.Consumption = FMath::Max(0, Consumption);
        Entry.Imports = FMath::Max(0, Imports);
        Entry.Exports = FMath::Max(0, Exports);
        Entry.Reserve = FMath::Max(0, Reserve);
        Entry.ReserveTarget = FMath::Max(1, ReserveTarget);
        Entry.Shortage = FMath::Max(0, Entry.ReserveTarget - Entry.Reserve);
        Entry.Surplus = FMath::Max(0, Entry.Reserve - Entry.ReserveTarget);
        Entry.StrategicValue = FMath::Clamp(StrategicValue, 0, 100);
        Entry.Role = Role;
        Entry.Drivers = Drivers;
        Entry.Status = Entry.Shortage > 0
            ? FString::Printf(TEXT("Shortage %d below reserve target %d."), Entry.Shortage, Entry.ReserveTarget)
            : FString::Printf(TEXT("Stable reserve %d with surplus %d over target."), Entry.Reserve, Entry.Surplus);
        return Entry;
    }

    int32 GetResourceChainShortage(const FDemocracyResourceProductionChainState& ResourceChains, const FString& ResourceName)
    {
        for (const FDemocracyResourceChainEntry& Entry : ResourceChains.Chains)
        {
            if (Entry.ResourceName.Equals(ResourceName, ESearchCase::IgnoreCase))
            {
                return Entry.Shortage;
            }
        }
        return 0;
    }

    FString BuildResourceChainSummaryText(const FDemocracyResourceProductionChainState& ResourceChains)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Shortage pressure %d | trade balance %+d | updated turn %d"), ResourceChains.TotalShortagePressure, ResourceChains.TradeBalance, ResourceChains.LastUpdatedTurn));
        Lines.Add(ResourceChains.Summary);
        for (const FDemocracyResourceChainEntry& Entry : ResourceChains.Chains)
        {
            Lines.Add(FString::Printf(TEXT("%s: reserve %d/%d | production %d | consumption %d | imports %d | exports %d | shortage %d | %s"),
                *Entry.ResourceName,
                Entry.Reserve,
                Entry.ReserveTarget,
                Entry.Production,
                Entry.Consumption,
                Entry.Imports,
                Entry.Exports,
                Entry.Shortage,
                *Entry.Role));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void RecalculateResourceProductionChains(FDemocracySimulationState& State, const FPolicyTickModifiers& PolicyModifiers, int32 FoodUse, int32 WaterUse, int32 GasUse, int32 WoodUse, int32 MetalsUse)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyResourceInventory& Resources = Country.Resources;
        FDemocracyEconomyBudgetState& Budget = State.EconomyBudget;
        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const int32 ProductionBonus = FMath::Clamp(Budget.ProductionEfficiency / 16, 0, 7);
        const int32 ServicesBonus = FMath::Clamp(Budget.PublicServices / 30, 0, 4);
        const int32 ClimateFoodBonus = Country.Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase) ? 3 : (Country.Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase) ? -1 : 1);
        const int32 ClimateWaterBonus = Country.Climate.Equals(TEXT("Northern Cold"), ESearchCase::IgnoreCase) ? 2 : (Country.Climate.Equals(TEXT("Southern Tropical"), ESearchCase::IgnoreCase) ? 1 : 0);
        const int32 TradeCapacity = FMath::Clamp(Country.DiplomaticStanding / 18 + Country.Treasury / 450, 0, 10);
        const int32 ExportCapacity = FMath::Clamp(Country.DiplomaticStanding / 28 + Budget.ProductionEfficiency / 35, 0, 6);

        const int32 FoodTarget = 150 + DifficultyScore * 18;
        const int32 WaterTarget = 130 + DifficultyScore * 15;
        const int32 FuelTarget = 80 + DifficultyScore * 14;
        const int32 WoodTarget = 70 + DifficultyScore * 10;
        const int32 MetalsTarget = 75 + DifficultyScore * 12;

        const int32 FoodProduction = FMath::Max(0, Country.Infrastructure / 12 + Country.EnvironmentalHealth / 25 + ProductionBonus + ClimateFoodBonus + PolicyModifiers.FoodDelta);
        const int32 WaterProduction = FMath::Max(0, Country.Infrastructure / 18 + ServicesBonus + ClimateWaterBonus + PolicyModifiers.WaterDelta);
        const int32 FuelProduction = FMath::Max(0, Budget.ProductionEfficiency / 18 + PolicyModifiers.GasOilDelta + (Country.Policies.EnvironmentalPolicy.Equals(TEXT("Extraction Expansion"), ESearchCase::IgnoreCase) ? 3 : 0));
        const int32 WoodProduction = FMath::Max(0, Country.EnvironmentalHealth / 20 + Budget.InfrastructureSpending / 20 + PolicyModifiers.WoodDelta);
        const int32 MetalsProduction = FMath::Max(0, Budget.ProductionEfficiency / 20 + Country.Infrastructure / 30 + PolicyModifiers.MetalsDelta + (Country.Policies.EconomicPolicy.Equals(TEXT("Industrial Subsidies"), ESearchCase::IgnoreCase) ? 2 : 0));

        const int32 FuelConsumption = GasUse + Budget.DefenseSpending / 18 + Budget.InfrastructureSpending / 35;
        const int32 WoodConsumption = WoodUse + Budget.InfrastructureSpending / 16;
        const int32 MetalsConsumption = MetalsUse + Budget.DefenseSpending / 16 + Budget.InfrastructureSpending / 22;

        const int32 FoodImports = Resources.Food < FoodTarget ? FMath::Clamp(TradeCapacity, 0, 8) : 0;
        const int32 WaterImports = Resources.Water < WaterTarget ? FMath::Clamp(TradeCapacity / 2, 0, 5) : 0;
        const int32 FuelImports = Resources.GasOil < FuelTarget ? FMath::Clamp(TradeCapacity, 0, 9) : 0;
        const int32 WoodImports = Resources.Wood < WoodTarget ? FMath::Clamp(TradeCapacity / 2, 0, 5) : 0;
        const int32 MetalsImports = Resources.Metals < MetalsTarget ? FMath::Clamp(TradeCapacity / 2 + 1, 0, 6) : 0;

        const int32 FoodExports = Resources.Food > FoodTarget + 90 ? FMath::Clamp(ExportCapacity, 0, 5) : 0;
        const int32 WaterExports = Resources.Water > WaterTarget + 80 ? FMath::Clamp(ExportCapacity / 2, 0, 3) : 0;
        const int32 FuelExports = Resources.GasOil > FuelTarget + 70 ? FMath::Clamp(ExportCapacity, 0, 5) : 0;
        const int32 WoodExports = Resources.Wood > WoodTarget + 60 ? FMath::Clamp(ExportCapacity / 2, 0, 4) : 0;
        const int32 MetalsExports = Resources.Metals > MetalsTarget + 60 ? FMath::Clamp(ExportCapacity / 2, 0, 4) : 0;

        Resources.Food = FMath::Max(0, Resources.Food + FoodProduction + FoodImports - FoodUse - FoodExports);
        Resources.Water = FMath::Max(0, Resources.Water + WaterProduction + WaterImports - WaterUse - WaterExports);
        Resources.GasOil = FMath::Max(0, Resources.GasOil + FuelProduction + FuelImports - FuelConsumption - FuelExports);
        Resources.Wood = FMath::Max(0, Resources.Wood + WoodProduction + WoodImports - WoodConsumption - WoodExports);
        Resources.Metals = FMath::Max(0, Resources.Metals + MetalsProduction + MetalsImports - MetalsConsumption - MetalsExports);

        State.ResourceChains.Chains.Reset();
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Food"), Resources.Food, FoodTarget, FoodProduction, FoodUse, FoodImports, FoodExports, 90, TEXT("Population food access; shortages raise unrest fastest."), { TEXT("agriculture"), TEXT("climate"), TEXT("infrastructure"), TEXT("food imports") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Water"), Resources.Water, WaterTarget, WaterProduction, WaterUse, WaterImports, WaterExports, 95, TEXT("Public health and regional stability; shortages damage demographics."), { TEXT("climate"), TEXT("public services"), TEXT("water access") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Fuel"), Resources.GasOil, FuelTarget, FuelProduction, FuelConsumption, FuelImports, FuelExports, 100, TEXT("Logistics, industry, military readiness, and inflation pressure."), { TEXT("extraction"), TEXT("trade"), TEXT("defense demand") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Wood"), Resources.Wood, WoodTarget, WoodProduction, WoodConsumption, WoodImports, WoodExports, 60, TEXT("Construction, infrastructure repairs, and disaster recovery."), { TEXT("forestry"), TEXT("infrastructure spending"), TEXT("environment") }));
        State.ResourceChains.Chains.Add(MakeResourceChainEntry(TEXT("Metals"), Resources.Metals, MetalsTarget, MetalsProduction, MetalsConsumption, MetalsImports, MetalsExports, 85, TEXT("Industry, infrastructure projects, and military production."), { TEXT("mining"), TEXT("industrial policy"), TEXT("defense spending") }));

        int32 WeightedShortage = 0;
        for (const FDemocracyResourceChainEntry& Entry : State.ResourceChains.Chains)
        {
            WeightedShortage += (Entry.Shortage * Entry.StrategicValue) / FMath::Max(1, Entry.ReserveTarget);
        }
        State.ResourceChains.TotalShortagePressure = FMath::Clamp(WeightedShortage, 0, 100);

        const int32 ImportCost = FoodImports * 5 + WaterImports * 4 + FuelImports * 8 + WoodImports * 4 + MetalsImports * 7;
        const int32 ExportIncome = FoodExports * 4 + WaterExports * 2 + FuelExports * 7 + WoodExports * 3 + MetalsExports * 6;
        State.ResourceChains.TradeBalance = ExportIncome - ImportCost;
        Country.Treasury = FMath::Max(0, Country.Treasury + State.ResourceChains.TradeBalance);
        State.ResourceChains.LastUpdatedTurn = State.Turn;
        State.ResourceChains.Summary = FString::Printf(TEXT("Production chains updated. Imports cost %d, exports earned %d, net trade %+d."), ImportCost, ExportIncome, State.ResourceChains.TradeBalance);
    }
    FDemocracyDepartmentState MakeDepartment(const FString& DepartmentName, const FString& MinisterTitle, const FString& Domain, int32 BudgetShare, int32 Staffing, int32 Effectiveness, int32 PublicTrust, int32 Priority, const FString& CurrentAction, const FString& PolicyInterface, const FString& AdvisorySummary, const TArray<FString>& ActionEffects)
    {
        FDemocracyDepartmentState Department;
        Department.DepartmentName = DepartmentName;
        Department.MinisterTitle = MinisterTitle;
        Department.Domain = Domain;
        Department.BudgetShare = FMath::Clamp(BudgetShare, 0, 100);
        Department.Staffing = FMath::Clamp(Staffing, 0, 100);
        Department.Effectiveness = FMath::Clamp(Effectiveness, 0, 100);
        Department.PublicTrust = FMath::Clamp(PublicTrust, 0, 100);
        Department.Priority = FMath::Clamp(Priority, 0, 100);
        Department.CurrentAction = CurrentAction;
        Department.PolicyInterface = PolicyInterface;
        Department.AdvisorySummary = AdvisorySummary;
        Department.ActionEffects = ActionEffects;
        return Department;
    }

    void InitializeDefaultDepartments(FDemocracySimulationState& State)
    {
        if (State.Departments.Departments.Num() > 0)
        {
            return;
        }

        const FDemocracyCountryState& Country = State.PlayerCountry;
        State.Departments.LastUpdatedTurn = State.Turn;
        State.Departments.Coordination = FMath::Clamp(58 - Country.CountrySizeScore * 3, 35, 70);
        State.Departments.Summary = TEXT("Departments initialized from loaded save. Use the Departments Desk to assign actions.");
        State.Departments.Departments = {
            MakeDepartment(TEXT("Defense"), TEXT("Minister of Defense"), TEXT("Military readiness, invasion risk, border security, and mobilization."), State.EconomyBudget.DefenseSpending, 55, Country.MilitaryReadiness, 50, 55, TEXT("Maintain Readiness"), TEXT("Military policy"), TEXT("Keep readiness above takeover warning thresholds."), { TEXT("Improves readiness when funded."), TEXT("High tempo can increase unrest and fuel demand.") }),
            MakeDepartment(TEXT("Treasury"), TEXT("Treasury Secretary"), TEXT("Taxes, spending, debt, inflation, reserves, and emergency funding."), State.EconomyBudget.TaxRate, 60, Country.EconomicHealth, 52, 55, TEXT("Stabilize Budget"), TEXT("Tax policy and spending posture"), TEXT("Keep deficit and inflation under control."), { TEXT("Improves treasury and budget clarity."), TEXT("Austerity can reduce public trust.") }),
            MakeDepartment(TEXT("Agriculture"), TEXT("Minister of Agriculture"), TEXT("Food production, rural regions, farm inputs, and shortage relief."), 12, 52, Country.Resources.Food / 4, 50, 50, TEXT("Boost Food Supply"), TEXT("Environmental and resource policy"), TEXT("Food shortages drive unrest quickly."), { TEXT("Improves food production and rural approval."), TEXT("Requires water and infrastructure support.") }),
            MakeDepartment(TEXT("Energy"), TEXT("Minister of Energy"), TEXT("Fuel reserves, power grid, imports, exports, and industrial energy demand."), 12, 50, Country.Resources.GasOil / 3, 48, 50, TEXT("Secure Fuel Supply"), TEXT("Extraction and trade policy"), TEXT("Fuel shortages weaken readiness and raise inflation."), { TEXT("Improves fuel availability and production efficiency."), TEXT("Extraction can hurt environmental health.") }),
            MakeDepartment(TEXT("Health"), TEXT("Minister of Health"), TEXT("Public health, crisis response, water safety, and public services."), State.EconomyBudget.PublicServicesSpending / 2, 55, State.EconomyBudget.PublicServices, 55, 50, TEXT("Maintain Public Health"), TEXT("Public services and welfare policy"), TEXT("Health capacity improves stability during shortages."), { TEXT("Improves public trust and lowers needs pressure."), TEXT("Requires steady spending.") }),
            MakeDepartment(TEXT("Education"), TEXT("Minister of Education"), TEXT("Schools, workforce training, technology growth, and youth approval."), 10, 50, 50 + Country.Technology * 5, 54, 45, TEXT("Workforce Training"), TEXT("Long-term technology and public services"), TEXT("Education raises production over time."), { TEXT("Improves technology and jobs over time."), TEXT("Short-term crisis impact is limited.") }),
            MakeDepartment(TEXT("Infrastructure"), TEXT("Minister of Infrastructure"), TEXT("Roads, logistics, utilities, construction, repairs, and disaster resilience."), State.EconomyBudget.InfrastructureSpending, 54, Country.Infrastructure, 50, 55, TEXT("Repair Critical Systems"), TEXT("Infrastructure spending and resource chains"), TEXT("Infrastructure supports every resource chain."), { TEXT("Improves production efficiency and regional stability."), TEXT("Consumes wood, metals, and treasury.") })
        };
    }

    FDemocracyDepartmentState* FindDepartment(FDemocracyDepartmentSystemState& Departments, const FString& DepartmentName)
    {
        for (FDemocracyDepartmentState& Department : Departments.Departments)
        {
            if (Department.DepartmentName.Equals(DepartmentName, ESearchCase::IgnoreCase))
            {
                return &Department;
            }
        }
        return nullptr;
    }

    const FDemocracyDepartmentState* FindDepartmentConst(const FDemocracyDepartmentSystemState& Departments, const FString& DepartmentName)
    {
        for (const FDemocracyDepartmentState& Department : Departments.Departments)
        {
            if (Department.DepartmentName.Equals(DepartmentName, ESearchCase::IgnoreCase))
            {
                return &Department;
            }
        }
        return nullptr;
    }

    int32 GetDepartmentEffectiveness(const FDemocracyDepartmentSystemState& Departments, const FString& DepartmentName)
    {
        const FDemocracyDepartmentState* Department = FindDepartmentConst(Departments, DepartmentName);
        return Department ? Department->Effectiveness : 50;
    }

    void RecalculateDepartments(FDemocracySimulationState& State)
    {
        InitializeDefaultDepartments(State);
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyDepartmentSystemState& Departments = State.Departments;
        int32 TotalEffectiveness = 0;
        int32 CriticalCount = 0;

        for (FDemocracyDepartmentState& Department : Departments.Departments)
        {
            int32 Stress = 0;
            int32 Funding = Department.BudgetShare;
            if (Department.DepartmentName.Equals(TEXT("Defense"), ESearchCase::IgnoreCase))
            {
                Funding = State.EconomyBudget.DefenseSpending;
                Stress = FMath::Max(0, 65 - Country.MilitaryReadiness) + State.InvasionRisk.CurrentInvasionRisk / 4;
                Department.AdvisorySummary = TEXT("Controls mobilization, readiness, and takeover response options.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Treasury"), ESearchCase::IgnoreCase))
            {
                Funding = State.EconomyBudget.TaxRate;
                Stress = FMath::Max(0, State.EconomyBudget.Deficit) / 12 + State.EconomyBudget.Inflation;
                Department.AdvisorySummary = TEXT("Controls budget, debt, tax, and emergency funding actions.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Agriculture"), ESearchCase::IgnoreCase))
            {
                Stress = GetResourceChainShortage(State.ResourceChains, TEXT("Food")) / 4;
                Department.AdvisorySummary = TEXT("Controls food supply actions and rural production support.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Energy"), ESearchCase::IgnoreCase))
            {
                Stress = GetResourceChainShortage(State.ResourceChains, TEXT("Fuel")) / 4 + FMath::Max(0, State.EconomyBudget.Inflation - 8);
                Department.AdvisorySummary = TEXT("Controls fuel security, power reliability, and extraction/trade response.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Health"), ESearchCase::IgnoreCase))
            {
                Funding = State.EconomyBudget.PublicServicesSpending / 2;
                Stress = State.Demographics.NationalNeedsPressure / 5 + GetResourceChainShortage(State.ResourceChains, TEXT("Water")) / 5;
                Department.AdvisorySummary = TEXT("Controls public health, shortage response, and welfare support.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Education"), ESearchCase::IgnoreCase))
            {
                Stress = FMath::Max(0, 55 - Country.Technology * 12) + FMath::Max(0, 55 - Country.EconomicHealth) / 3;
                Department.AdvisorySummary = TEXT("Controls long-term technology, workforce, and youth approval actions.");
            }
            else if (Department.DepartmentName.Equals(TEXT("Infrastructure"), ESearchCase::IgnoreCase))
            {
                Funding = State.EconomyBudget.InfrastructureSpending;
                Stress = FMath::Max(0, 70 - Country.Infrastructure) + GetResourceChainShortage(State.ResourceChains, TEXT("Wood")) / 6 + GetResourceChainShortage(State.ResourceChains, TEXT("Metals")) / 6;
                Department.AdvisorySummary = TEXT("Controls repairs, logistics, utilities, and construction capacity.");
            }

            const int32 PrioritySupport = Department.Priority / 8;
            const int32 FundingSupport = Funding / 4;
            Department.BudgetShare = FMath::Clamp(Funding, 0, 100);
            Department.Effectiveness = FMath::Clamp((Department.Effectiveness + Department.Staffing + Department.PublicTrust + PrioritySupport + FundingSupport - Stress / 2) / 3, 0, 100);
            Department.PublicTrust = FMath::Clamp(Department.PublicTrust + (Department.Effectiveness > 62 ? 1 : 0) - (Stress > 25 ? 1 : 0) - (Department.CurrentAction.Contains(TEXT("Emergency")) ? 1 : 0), 0, 100);
            Department.ActionEffects.Reset();
            Department.ActionEffects.Add(FString::Printf(TEXT("Effectiveness %d, trust %d, priority %d, stress %d."), Department.Effectiveness, Department.PublicTrust, Department.Priority, Stress));
            Department.ActionEffects.Add(FString::Printf(TEXT("Current action: %s."), *Department.CurrentAction));
            TotalEffectiveness += Department.Effectiveness;
            CriticalCount += Department.Effectiveness < 35 ? 1 : 0;
        }

        const int32 AverageEffectiveness = Departments.Departments.Num() > 0 ? TotalEffectiveness / Departments.Departments.Num() : 50;
        Departments.Coordination = FMath::Clamp((Departments.Coordination + AverageEffectiveness + Country.Stability / 2 - CriticalCount * 5) / 2, 0, 100);
        Departments.LastUpdatedTurn = State.Turn;
        Departments.Summary = FString::Printf(TEXT("%d ministries active | average effectiveness %d | coordination %d | critical ministries %d."), Departments.Departments.Num(), AverageEffectiveness, Departments.Coordination, CriticalCount);
    }

    FString BuildDepartmentSummaryText(const FDemocracyDepartmentSystemState& Departments)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Coordination %d | updated turn %d"), Departments.Coordination, Departments.LastUpdatedTurn));
        Lines.Add(Departments.Summary);
        for (const FDemocracyDepartmentState& Department : Departments.Departments)
        {
            Lines.Add(FString::Printf(TEXT("%s: action %s | effectiveness %d | trust %d | priority %d | interface: %s"),
                *Department.DepartmentName,
                *Department.CurrentAction,
                Department.Effectiveness,
                Department.PublicTrust,
                Department.Priority,
                *Department.PolicyInterface));
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    FString BuildPressOfficeSummaryText(const FDemocracyPressOfficeState& PressOffice)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Credibility %d | announcements %d | empty streak %d | false streak %d | updated turn %d"), PressOffice.Credibility, PressOffice.TotalAnnouncements, PressOffice.ConsecutiveEmptyAnnouncements, PressOffice.ConsecutiveFalseAnnouncements, PressOffice.LastUpdatedTurn));
        Lines.Add(PressOffice.LastAnnouncementSummary);
        const int32 StartIndex = FMath::Max(0, PressOffice.Records.Num() - 5);
        for (int32 Index = PressOffice.Records.Num() - 1; Index >= StartIndex; --Index)
        {
            const FDemocracyPressReleaseRecordState& Record = PressOffice.Records[Index];
            Lines.Add(FString::Printf(TEXT("Turn %d | %s | %s | truthful %s | approval %+d stability %+d diplomacy %+d unrest %+d credibility %+d | %s"),
                Record.Turn,
                *Record.AnnouncementType,
                *Record.MessageQuality,
                Record.bTruthful ? TEXT("yes") : TEXT("no"),
                Record.ApprovalDelta,
                Record.StabilityDelta,
                Record.DiplomacyDelta,
                Record.UnrestDelta,
                Record.CredibilityDelta,
                *Record.Summary));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void InitializePressOfficeIfMissing(FDemocracySimulationState& State)
    {
        if (State.PressOffice.MaxRecords <= 0)
        {
            State.PressOffice.MaxRecords = 40;
        }
        if (State.PressOffice.Credibility <= 0)
        {
            State.PressOffice.Credibility = FMath::Clamp(72 - State.PlayerCountry.CountrySizeScore * 4, 52, 76);
        }
        if (State.PressOffice.LastAnnouncementSummary.IsEmpty())
        {
            State.PressOffice.LastAnnouncementSummary = TEXT("Press office initialized for this save. Announcements now affect public trust and diplomacy.");
        }
    }
    FString BuildMeetingSystemSummaryText(const FDemocracyMeetingSystemState& MeetingSystem)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Meetings %d | advisor coordination %d | foreign trust %d | updated turn %d"), MeetingSystem.TotalMeetings, MeetingSystem.AdvisorCoordination, MeetingSystem.ForeignTrust, MeetingSystem.LastUpdatedTurn));
        Lines.Add(MeetingSystem.LastMeetingSummary);
        const int32 StartIndex = FMath::Max(0, MeetingSystem.Records.Num() - 6);
        for (int32 Index = MeetingSystem.Records.Num() - 1; Index >= StartIndex; --Index)
        {
            const FDemocracyMeetingRecordState& Record = MeetingSystem.Records[Index];
            Lines.Add(FString::Printf(TEXT("Turn %d | %s with %s | %s | approval %+d stability %+d unrest %+d diplomacy %+d treasury %+d economy %+d military %+d infrastructure %+d | %s"),
                Record.Turn,
                *Record.MeetingType,
                *Record.ParticipantName,
                *Record.AgendaItem,
                Record.ApprovalDelta,
                Record.StabilityDelta,
                Record.UnrestDelta,
                Record.DiplomacyDelta,
                Record.TreasuryDelta,
                Record.EconomyDelta,
                Record.MilitaryDelta,
                Record.InfrastructureDelta,
                *Record.OutcomeSummary));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void InitializeMeetingSystemIfMissing(FDemocracySimulationState& State)
    {
        if (State.MeetingSystem.MaxRecords <= 0)
        {
            State.MeetingSystem.MaxRecords = 50;
        }
        if (State.MeetingSystem.AdvisorCoordination <= 0)
        {
            State.MeetingSystem.AdvisorCoordination = FMath::Clamp(56 - State.PlayerCountry.CountrySizeScore * 3, 40, 60);
        }
        if (State.MeetingSystem.ForeignTrust <= 0)
        {
            State.MeetingSystem.ForeignTrust = FMath::Clamp(54 - State.PlayerCountry.CountrySizeScore * 4, 34, 58);
        }
        if (State.MeetingSystem.LastMeetingSummary.IsEmpty())
        {
            State.MeetingSystem.LastMeetingSummary = TEXT("Meeting system initialized for this save.");
        }
    }
    FDemocracyDevelopmentTrackState MakeDevelopmentTrack(const FString& TrackName, int32 Level, int32 Progress, int32 ProgressTarget, int32 TreasuryCost, int32 WoodCost, int32 MetalsCost, int32 FuelCost, const FString& Project, const FString& Benefit, const TArray<FString>& Unlocks)
    {
        FDemocracyDevelopmentTrackState Track;
        Track.TrackName = TrackName;
        Track.FocusArea = TrackName;
        Track.Level = FMath::Clamp(Level, 1, 10);
        Track.Progress = FMath::Clamp(Progress, 0, FMath::Max(1, ProgressTarget));
        Track.ProgressTarget = FMath::Max(1, ProgressTarget);
        Track.TreasuryCost = FMath::Max(0, TreasuryCost);
        Track.WoodCost = FMath::Max(0, WoodCost);
        Track.MetalsCost = FMath::Max(0, MetalsCost);
        Track.FuelCost = FMath::Max(0, FuelCost);
        Track.CurrentProject = Project;
        Track.StrategicBenefit = Benefit;
        Track.Unlocks = Unlocks;
        return Track;
    }

    void InitializeDevelopmentSystemIfMissing(FDemocracySimulationState& State)
    {
        if (State.DevelopmentSystem.Tracks.Num() == 0)
        {
            State.DevelopmentSystem.ActiveFocus = TEXT("Infrastructure");
            State.DevelopmentSystem.DevelopmentPoints = 0;
            State.DevelopmentSystem.LastUpdatedTurn = State.Turn;
            State.DevelopmentSystem.Summary = TEXT("Development system initialized from existing save. Choose a focus and step the simulation to gain progress.");
            State.DevelopmentSystem.Tracks = {
                MakeDevelopmentTrack(TEXT("Infrastructure"), 1, 0, 100, 28, 4, 3, 1, TEXT("National Logistics Upgrade"), TEXT("Raises infrastructure, production efficiency, and resource-chain resilience."), { TEXT("Better roads"), TEXT("Grid reliability"), TEXT("Disaster response capacity") }),
                MakeDevelopmentTrack(TEXT("Military"), 1, 0, 110, 32, 1, 5, 4, TEXT("Readiness Modernization"), TEXT("Raises military readiness and reduces foreign takeover pressure."), { TEXT("Modern logistics"), TEXT("Border surveillance"), TEXT("Rapid response doctrine") }),
                MakeDevelopmentTrack(TEXT("Agriculture"), 1, 0, 95, 24, 2, 2, 1, TEXT("Food Security Program"), TEXT("Improves food and water production, lowering shortage-driven unrest."), { TEXT("Irrigation"), TEXT("Storage networks"), TEXT("Climate-hardy crops") }),
                MakeDevelopmentTrack(TEXT("Industry"), 1, 0, 115, 34, 2, 5, 3, TEXT("Industrial Base Expansion"), TEXT("Raises economic health, production efficiency, and metals output."), { TEXT("Manufacturing clusters"), TEXT("Materials processing"), TEXT("Skilled workforce") }),
                MakeDevelopmentTrack(TEXT("Communications"), 1, 0, 90, 22, 1, 2, 1, TEXT("National Communications Network"), TEXT("Improves stability, press credibility, advisor coordination, and public trust."), { TEXT("Emergency alerts"), TEXT("Public information systems"), TEXT("Government coordination") })
            };
        }
        if (State.DevelopmentSystem.ActiveFocus.IsEmpty())
        {
            State.DevelopmentSystem.ActiveFocus = State.DevelopmentSystem.Tracks.Num() > 0 ? State.DevelopmentSystem.Tracks[0].TrackName : TEXT("Infrastructure");
        }
    }

    FDemocracyDevelopmentTrackState* FindDevelopmentTrack(FDemocracyDevelopmentSystemState& DevelopmentSystem, const FString& TrackName)
    {
        for (FDemocracyDevelopmentTrackState& Track : DevelopmentSystem.Tracks)
        {
            if (Track.TrackName.Equals(TrackName, ESearchCase::IgnoreCase))
            {
                return &Track;
            }
        }
        return nullptr;
    }

    FString BuildDevelopmentSummaryText(const FDemocracyDevelopmentSystemState& DevelopmentSystem)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Active focus: %s | development points %d | updated turn %d"), *DevelopmentSystem.ActiveFocus, DevelopmentSystem.DevelopmentPoints, DevelopmentSystem.LastUpdatedTurn));
        Lines.Add(DevelopmentSystem.Summary);
        for (const FDemocracyDevelopmentTrackState& Track : DevelopmentSystem.Tracks)
        {
            Lines.Add(FString::Printf(TEXT("%s: level %d | progress %d/%d | cost T%d W%d M%d F%d | project: %s | %s"),
                *Track.TrackName,
                Track.Level,
                Track.Progress,
                Track.ProgressTarget,
                Track.TreasuryCost,
                Track.WoodCost,
                Track.MetalsCost,
                Track.FuelCost,
                *Track.CurrentProject,
                *Track.StrategicBenefit));
        }
        return FString::Join(Lines, TEXT("\n"));
    }

    void ApplyDevelopmentLevelBonus(FDemocracySimulationState& State, const FDemocracyDevelopmentTrackState& Track)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        if (Track.TrackName.Equals(TEXT("Infrastructure"), ESearchCase::IgnoreCase))
        {
            Country.Infrastructure = FMath::Clamp(Country.Infrastructure + 4, 0, 100);
            State.EconomyBudget.ProductionEfficiency = FMath::Clamp(State.EconomyBudget.ProductionEfficiency + 2, 0, 100);
        }
        else if (Track.TrackName.Equals(TEXT("Military"), ESearchCase::IgnoreCase))
        {
            Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + 5, 0, 100);
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 5, 0, State.InvasionRisk.InvasionRiskTrigger);
        }
        else if (Track.TrackName.Equals(TEXT("Agriculture"), ESearchCase::IgnoreCase))
        {
            Country.Resources.Food = FMath::Max(0, Country.Resources.Food + 18);
            Country.Resources.Water = FMath::Max(0, Country.Resources.Water + 10);
            Country.Unrest = FMath::Clamp(Country.Unrest - 1, 0, 100);
        }
        else if (Track.TrackName.Equals(TEXT("Industry"), ESearchCase::IgnoreCase))
        {
            Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + 4, 0, 100);
            State.EconomyBudget.ProductionEfficiency = FMath::Clamp(State.EconomyBudget.ProductionEfficiency + 3, 0, 100);
            Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals + 8);
        }
        else if (Track.TrackName.Equals(TEXT("Communications"), ESearchCase::IgnoreCase))
        {
            Country.Stability = FMath::Clamp(Country.Stability + 3, 0, 100);
            State.PressOffice.Credibility = FMath::Clamp(State.PressOffice.Credibility + 3, 0, 100);
            State.MeetingSystem.AdvisorCoordination = FMath::Clamp(State.MeetingSystem.AdvisorCoordination + 3, 0, 100);
        }
        Country.Technology = FMath::Clamp(Country.Technology + 1, 1, 25);
    }

    void TickDevelopmentSystem(FDemocracySimulationState& State)
    {
        InitializeDevelopmentSystemIfMissing(State);
        FDemocracyDevelopmentTrackState* ActiveTrack = FindDevelopmentTrack(State.DevelopmentSystem, State.DevelopmentSystem.ActiveFocus);
        if (!ActiveTrack)
        {
            return;
        }

        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyResourceInventory& Resources = Country.Resources;
        const int32 EducationEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Education"));
        const int32 InfrastructureEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Infrastructure"));
        const int32 ProgressGain = FMath::Clamp(4 + Country.Technology / 2 + EducationEffectiveness / 25 + InfrastructureEffectiveness / 35, 4, 18);
        const bool bCanFund = Country.Treasury >= ActiveTrack->TreasuryCost && Resources.Wood >= ActiveTrack->WoodCost && Resources.Metals >= ActiveTrack->MetalsCost && Resources.GasOil >= ActiveTrack->FuelCost;

        if (!bCanFund)
        {
            State.DevelopmentSystem.Summary = FString::Printf(TEXT("%s stalled: needs treasury %d, wood %d, metals %d, fuel %d."), *ActiveTrack->TrackName, ActiveTrack->TreasuryCost, ActiveTrack->WoodCost, ActiveTrack->MetalsCost, ActiveTrack->FuelCost);
            State.DevelopmentSystem.LastUpdatedTurn = State.Turn;
            return;
        }

        Country.Treasury = FMath::Max(0, Country.Treasury - ActiveTrack->TreasuryCost);
        Resources.Wood = FMath::Max(0, Resources.Wood - ActiveTrack->WoodCost);
        Resources.Metals = FMath::Max(0, Resources.Metals - ActiveTrack->MetalsCost);
        Resources.GasOil = FMath::Max(0, Resources.GasOil - ActiveTrack->FuelCost);
        ActiveTrack->Progress += ProgressGain;
        ++State.DevelopmentSystem.DevelopmentPoints;
        State.DevelopmentSystem.LastUpdatedTurn = State.Turn;

        if (ActiveTrack->Progress >= ActiveTrack->ProgressTarget)
        {
            ActiveTrack->Progress -= ActiveTrack->ProgressTarget;
            ++ActiveTrack->Level;
            ActiveTrack->ProgressTarget += 35;
            ActiveTrack->TreasuryCost += 6;
            ActiveTrack->WoodCost += ActiveTrack->TrackName.Equals(TEXT("Infrastructure"), ESearchCase::IgnoreCase) ? 2 : 1;
            ActiveTrack->MetalsCost += ActiveTrack->TrackName.Equals(TEXT("Military"), ESearchCase::IgnoreCase) || ActiveTrack->TrackName.Equals(TEXT("Industry"), ESearchCase::IgnoreCase) ? 2 : 1;
            ApplyDevelopmentLevelBonus(State, *ActiveTrack);
            State.DevelopmentSystem.Summary = FString::Printf(TEXT("%s advanced to level %d. Strategic benefit applied: %s"), *ActiveTrack->TrackName, ActiveTrack->Level, *ActiveTrack->StrategicBenefit);
        }
        else
        {
            State.DevelopmentSystem.Summary = FString::Printf(TEXT("%s gained %d progress toward %s."), *ActiveTrack->TrackName, ProgressGain, *ActiveTrack->CurrentProject);
        }
    }
    void LogDecision(FDemocracySimulationState& State, const FString& Category, const FString& Title, const FString& Detail, const FString& Consequence, int32 Severity, const TArray<FString>& Tags)
    {
        FDemocracyDecisionHistoryState& History = State.DecisionHistory;
        if (History.MaxRecords <= 0)
        {
            History.MaxRecords = 80;
        }

        const FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyDecisionRecordState Record;
        Record.Turn = State.Turn;
        Record.Category = Category;
        Record.DecisionTitle = Title;
        Record.DecisionDetail = Detail;
        Record.ConsequenceSummary = Consequence;
        Record.ApprovalAfter = Country.PublicApproval;
        Record.StabilityAfter = Country.Stability;
        Record.UnrestAfter = Country.Unrest;
        Record.TreasuryAfter = Country.Treasury;
        Record.EconomyAfter = Country.EconomicHealth;
        Record.MilitaryAfter = Country.MilitaryReadiness;
        Record.Severity = FMath::Clamp(Severity, 0, 100);
        Record.TimestampUtc = SaveTimestamp();
        Record.Tags = Tags;

        History.Records.Add(Record);
        while (History.Records.Num() > History.MaxRecords)
        {
            History.Records.RemoveAt(0);
        }
        History.LastUpdatedTurn = State.Turn;
        History.Summary = FString::Printf(TEXT("%d major decisions logged. Latest: %s - %s."), History.Records.Num(), *Category, *Title);
    }

    void InitializeDecisionHistoryIfMissing(FDemocracySimulationState& State)
    {
        if (State.DecisionHistory.MaxRecords <= 0)
        {
            State.DecisionHistory.MaxRecords = 80;
        }
        if (State.DecisionHistory.Records.Num() == 0)
        {
            LogDecision(State, TEXT("Loaded State"), TEXT("Decision History Started"), TEXT("Existing save loaded before decision history existed."), TEXT("Future policies, events, budget changes, department actions, and save actions will be recorded."), 5, { TEXT("compatibility"), TEXT("save") });
        }
    }

    FString BuildDecisionHistorySummaryText(const FDemocracyDecisionHistoryState& History)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Records %d/%d | updated turn %d"), History.Records.Num(), History.MaxRecords, History.LastUpdatedTurn));
        Lines.Add(History.Summary);
        const int32 StartIndex = FMath::Max(0, History.Records.Num() - 6);
        for (int32 Index = History.Records.Num() - 1; Index >= StartIndex; --Index)
        {
            const FDemocracyDecisionRecordState& Record = History.Records[Index];
            Lines.Add(FString::Printf(TEXT("Turn %d | %s | %s | severity %d | %s"),
                Record.Turn,
                *Record.Category,
                *Record.DecisionTitle,
                Record.Severity,
                *Record.ConsequenceSummary));
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    FDemocracyApprovalCauseState MakeApprovalCause(const FString& CauseName, const FString& Category, int32 Severity, int32 ApprovalImpact, int32 UnrestImpact, int32 StabilityImpact, const FString& SourceMetric, const FString& CurrentStatus, const TArray<FString>& SuggestedResponses)
    {
        FDemocracyApprovalCauseState Cause;
        Cause.CauseName = CauseName;
        Cause.Category = Category;
        Cause.Severity = FMath::Clamp(Severity, 0, 100);
        Cause.ApprovalImpact = FMath::Clamp(ApprovalImpact, -100, 100);
        Cause.UnrestImpact = FMath::Clamp(UnrestImpact, -100, 100);
        Cause.StabilityImpact = FMath::Clamp(StabilityImpact, -100, 100);
        Cause.SourceMetric = SourceMetric;
        Cause.CurrentStatus = CurrentStatus;
        Cause.SuggestedResponses = SuggestedResponses;
        return Cause;
    }

    int32 GetActiveEventSeverityByType(const FDemocracyEventSystemState& EventSystem, const FString& EventType)
    {
        int32 Severity = 0;
        for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
        {
            if (!Event.bResolved && Event.EventType.Equals(EventType, ESearchCase::IgnoreCase))
            {
                Severity = FMath::Max(Severity, Event.Severity);
            }
        }
        return Severity;
    }

    void RecalculateApprovalStability(FDemocracySimulationState& State)
    {
        FDemocracyApprovalStabilityModelState& Model = State.ApprovalStability;
        const FDemocracyCountryState& Country = State.PlayerCountry;
        const FDemocracyEconomyBudgetState& Budget = State.EconomyBudget;
        const FDemocracyDemographicsState& Demographics = State.Demographics;
        const FDemocracyPolicyState& Policies = Country.Policies;

        const int32 FoodSeverity = FMath::Clamp(GetResourceChainShortage(State.ResourceChains, TEXT("Food")) / 2 + FMath::Max(0, 120 - Country.Resources.Food) / 5, 0, 100);
        const int32 WaterSeverity = FMath::Clamp(GetResourceChainShortage(State.ResourceChains, TEXT("Water")) / 2 + FMath::Max(0, 105 - Country.Resources.Water) / 5, 0, 100);
        const int32 TaxSeverity = FMath::Clamp(FMath::Max(0, Budget.TaxRate - 22) * 4 + (Budget.TaxPolicy.Equals(TEXT("High Taxes"), ESearchCase::IgnoreCase) ? 12 : 0), 0, 100);
        const int32 InflationSeverity = FMath::Clamp(FMath::Max(0, Budget.Inflation - 4) * 7 + FMath::Max(0, State.ResourceChains.TotalShortagePressure - 20) / 2, 0, 100);
        const int32 UnemploymentSeverity = FMath::Clamp(FMath::Max(0, 62 - Country.EconomicHealth) + FMath::Max(0, Demographics.NationalNeedsPressure - 45) / 2, 0, 100);
        const int32 WarFatigueSeverity = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk / 2 + (Policies.MilitaryPolicy.Equals(TEXT("National Mobilization"), ESearchCase::IgnoreCase) ? 24 : 0) + FMath::Max(0, 45 - Country.MilitaryReadiness) / 2, 0, 100);
        const int32 ScandalSeverity = GetActiveEventSeverityByType(State.EventSystem, TEXT("Scandal"));
        const int32 CorruptionSeverity = FMath::Clamp(ScandalSeverity + (Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase) ? 18 : 0) + FMath::Max(0, Policies.PolicyChangeCount - 5) * 3, 0, 100);
        const int32 ServicesSeverity = FMath::Clamp(FMath::Max(0, 62 - Budget.PublicServices) + FMath::Max(0, 55 - GetDepartmentEffectiveness(State.Departments, TEXT("Health"))) / 2, 0, 100);
        const int32 InfrastructureSeverity = FMath::Clamp(FMath::Max(0, 68 - Country.Infrastructure) + GetResourceChainShortage(State.ResourceChains, TEXT("Wood")) / 6 + GetResourceChainShortage(State.ResourceChains, TEXT("Metals")) / 6, 0, 100);
        const int32 CivilRightsRelief = Policies.CivilPolicy.Equals(TEXT("Civil Liberties"), ESearchCase::IgnoreCase) ? 12 : 0;

        Model.Causes.Reset();
        Model.Causes.Add(MakeApprovalCause(TEXT("Food Shortage"), TEXT("Resources"), FoodSeverity, -FoodSeverity / 3, FoodSeverity / 3, -FoodSeverity / 4, TEXT("Food reserves and production chain shortage"), FString::Printf(TEXT("Food shortage severity %d; reserve %d."), FoodSeverity, Country.Resources.Food), { TEXT("Boost Agriculture"), TEXT("Import Food"), TEXT("Use emergency food relief") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Water Access"), TEXT("Resources"), WaterSeverity, -WaterSeverity / 4, WaterSeverity / 3, -WaterSeverity / 3, TEXT("Water reserves and regional access"), FString::Printf(TEXT("Water access severity %d; reserve %d."), WaterSeverity, Country.Resources.Water), { TEXT("Improve public services"), TEXT("Repair utilities"), TEXT("Import water") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Tax Burden"), TEXT("Economy"), TaxSeverity, -TaxSeverity / 4, TaxSeverity / 5, -TaxSeverity / 6, TEXT("Tax rate and tax policy"), FString::Printf(TEXT("Tax burden severity %d; rate %d%%."), TaxSeverity, Budget.TaxRate), { TEXT("Lower taxes"), TEXT("Offset high taxes with services"), TEXT("Stabilize spending before raising rates") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Inflation"), TEXT("Economy"), InflationSeverity, -InflationSeverity / 4, InflationSeverity / 4, -InflationSeverity / 5, TEXT("Inflation and shortage pressure"), FString::Printf(TEXT("Cost-of-living severity %d; inflation %d."), InflationSeverity, Budget.Inflation), { TEXT("Reduce shortages"), TEXT("Improve production efficiency"), TEXT("Balance deficit pressure") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Unemployment"), TEXT("Economy"), UnemploymentSeverity, -UnemploymentSeverity / 5, UnemploymentSeverity / 4, -UnemploymentSeverity / 5, TEXT("Economic health and jobs needs"), FString::Printf(TEXT("Jobs severity %d; economic health %d."), UnemploymentSeverity, Country.EconomicHealth), { TEXT("Use workforce training"), TEXT("Support industry"), TEXT("Improve infrastructure") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("War Fatigue"), TEXT("Security"), WarFatigueSeverity, -WarFatigueSeverity / 4, WarFatigueSeverity / 5, -WarFatigueSeverity / 6, TEXT("Mobilization, readiness, and invasion risk"), FString::Printf(TEXT("War fatigue severity %d; invasion risk %d."), WarFatigueSeverity, State.InvasionRisk.CurrentInvasionRisk), { TEXT("Improve diplomacy"), TEXT("Avoid sustained mobilization"), TEXT("Fund readiness before crisis") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Corruption"), TEXT("Legitimacy"), CorruptionSeverity, -CorruptionSeverity / 4, CorruptionSeverity / 4, -CorruptionSeverity / 3, TEXT("Scandals, emergency powers, and rapid policy churn"), FString::Printf(TEXT("Legitimacy severity %d; active scandal severity %d."), CorruptionSeverity, ScandalSeverity), { TEXT("Resolve scandals transparently"), TEXT("Avoid repeated emergency powers"), TEXT("Use administrative reform") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Public Services"), TEXT("Services"), ServicesSeverity, -ServicesSeverity / 5, ServicesSeverity / 5, -ServicesSeverity / 4, TEXT("Public services and Health ministry"), FString::Printf(TEXT("Services severity %d; public services %d."), ServicesSeverity, Budget.PublicServices), { TEXT("Increase public services spending"), TEXT("Strengthen Health ministry"), TEXT("Protect water access") }));
        Model.Causes.Add(MakeApprovalCause(TEXT("Infrastructure Reliability"), TEXT("Infrastructure"), InfrastructureSeverity, -InfrastructureSeverity / 6, InfrastructureSeverity / 5, -InfrastructureSeverity / 4, TEXT("Infrastructure, wood, and metals"), FString::Printf(TEXT("Infrastructure severity %d; infrastructure %d."), InfrastructureSeverity, Country.Infrastructure), { TEXT("Repair critical systems"), TEXT("Secure wood and metals"), TEXT("Fund infrastructure push") }));

        if (CivilRightsRelief > 0)
        {
            Model.Causes.Add(MakeApprovalCause(TEXT("Civil Rights Confidence"), TEXT("Legitimacy"), CivilRightsRelief, CivilRightsRelief, -CivilRightsRelief / 2, CivilRightsRelief / 2, TEXT("Civil policy"), TEXT("Civil liberties are lowering legitimacy pressure."), { TEXT("Keep civil protections stable"), TEXT("Avoid emergency powers unless necessary") }));
        }

        int32 ApprovalPressure = 0;
        int32 UnrestPressure = 0;
        int32 StabilityPressure = 0;
        int32 CriticalCauseCount = 0;
        FString TopCause = TEXT("No major pressure source");
        int32 TopSeverity = -1;
        for (const FDemocracyApprovalCauseState& Cause : Model.Causes)
        {
            ApprovalPressure += Cause.ApprovalImpact;
            UnrestPressure += Cause.UnrestImpact;
            StabilityPressure += Cause.StabilityImpact;
            if (Cause.Severity >= 45)
            {
                ++CriticalCauseCount;
            }
            if (Cause.Severity > TopSeverity)
            {
                TopSeverity = Cause.Severity;
                TopCause = Cause.CauseName;
            }
        }

        Model.NetApprovalPressure = FMath::Clamp(ApprovalPressure, -100, 100);
        Model.NetUnrestPressure = FMath::Clamp(UnrestPressure, -100, 100);
        Model.NetStabilityPressure = FMath::Clamp(StabilityPressure, -100, 100);
        Model.LastUpdatedTurn = State.Turn;
        Model.Summary = FString::Printf(TEXT("Top cause: %s severity %d | critical causes %d | approval pressure %+d | unrest pressure %+d | stability pressure %+d."), *TopCause, FMath::Max(0, TopSeverity), CriticalCauseCount, Model.NetApprovalPressure, Model.NetUnrestPressure, Model.NetStabilityPressure);
    }

    FString BuildApprovalStabilitySummaryText(const FDemocracyApprovalStabilityModelState& Model)
    {
        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("Approval pressure %+d | unrest pressure %+d | stability pressure %+d | updated turn %d"), Model.NetApprovalPressure, Model.NetUnrestPressure, Model.NetStabilityPressure, Model.LastUpdatedTurn));
        Lines.Add(Model.Summary);
        for (const FDemocracyApprovalCauseState& Cause : Model.Causes)
        {
            Lines.Add(FString::Printf(TEXT("%s (%s): severity %d | approval %+d | unrest %+d | stability %+d | %s"), *Cause.CauseName, *Cause.Category, Cause.Severity, Cause.ApprovalImpact, Cause.UnrestImpact, Cause.StabilityImpact, *Cause.CurrentStatus));
        }
        return FString::Join(Lines, TEXT("\n"));
    }
    FString AdvisorGuidanceForDifficultyScore(int32 DifficultyScore)
    {
        if (DifficultyScore <= 1)
        {
            return TEXT("Early Detailed");
        }
        if (DifficultyScore == 2)
        {
            return TEXT("Standard");
        }
        if (DifficultyScore == 3)
        {
            return TEXT("Late Limited");
        }
        return TEXT("Minimal");
    }

    FString GuidanceText(const FString& GuidanceLevel, const FString& Detailed, const FString& Standard, const FString& Limited, const FString& Minimal)
    {
        if (GuidanceLevel.Contains(TEXT("Detailed")))
        {
            return Detailed;
        }
        if (GuidanceLevel.Contains(TEXT("Limited")))
        {
            return Limited;
        }
        if (GuidanceLevel.Contains(TEXT("Minimal")))
        {
            return Minimal;
        }
        return Standard;
    }

    bool IsDetailedGuidance(const FString& GuidanceLevel)
    {
        return GuidanceLevel.Contains(TEXT("Detailed"));
    }

    bool IsLimitedGuidance(const FString& GuidanceLevel)
    {
        return GuidanceLevel.Contains(TEXT("Limited"));
    }

    bool IsMinimalGuidance(const FString& GuidanceLevel)
    {
        return GuidanceLevel.Contains(TEXT("Minimal"));
    }

    FString DifficultyGuidancePreview(const FDifficultyProfile& Profile)
    {
        if (Profile.PlayerHelpLevel.Equals(TEXT("High"), ESearchCase::IgnoreCase))
        {
            return TEXT("Tutorial guidance: detailed advisor explanations, direct action steps, extra recovery tips, and full UI cause/effect text.");
        }
        if (Profile.PlayerHelpLevel.Equals(TEXT("Standard"), ESearchCase::IgnoreCase))
        {
            return TEXT("Tutorial guidance: standard advisor explanations, visible warnings, and concise UI cause/effect summaries.");
        }
        if (Profile.PlayerHelpLevel.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
        {
            return TEXT("Tutorial guidance: limited explanations. Advisors identify problems but provide fewer direct answers.");
        }
        return TEXT("Tutorial guidance: minimal. UI exposes signals and consequences, but advisors give almost no step-by-step direction.");
    }

    FString RuntimeGuidanceSummary(const FDemocracySimulationState& State)
    {
        const FString GuidanceLevel = AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore);
        return GuidanceText(GuidanceLevel,
            TEXT("Detailed guidance active: advisor reports explain what changed, why it matters, what to do first, and the tradeoff."),
            TEXT("Standard guidance active: advisor reports show the issue, recommended response, and main warning."),
            TEXT("Limited guidance active: advisor reports give short warnings and broad direction."),
            TEXT("Minimal guidance active: advisor reports show only high-level risk signals."));
    }

    int32 VisibleAdvisorReportCount(const FDemocracyAdvisorSystemState& AdvisorSystem, const FDemocracyCountryState& Country)
    {
        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const int32 AdvisorLimit = FMath::Max(1, AdvisorSystem.AdvisorCount);
        if (DifficultyScore <= 1)
        {
            return FMath::Max(AdvisorLimit, 5);
        }
        if (DifficultyScore == 2)
        {
            return FMath::Max(AdvisorLimit, 4);
        }
        if (DifficultyScore == 3)
        {
            return FMath::Min(AdvisorLimit, 2);
        }
        return 1;
    }

    void AppendGuidedAdvisorReport(TArray<FString>& Lines, const FDemocracyAdvisorReport& Report, const FString& GuidanceLevel)
    {
        Lines.Add(FString::Printf(TEXT("\n%s (%s) | Severity %d | Guidance %s"), *Report.AdvisorName, *Report.Category, Report.Severity, *GuidanceLevel));
        if (IsMinimalGuidance(GuidanceLevel))
        {
            Lines.Add(FString::Printf(TEXT("Signal: %s"), *Report.Warning));
            return;
        }
        if (IsLimitedGuidance(GuidanceLevel))
        {
            Lines.Add(FString::Printf(TEXT("Warning: %s"), *Report.Warning));
            Lines.Add(FString::Printf(TEXT("Direction: %s"), *Report.Recommendation));
            return;
        }

        Lines.Add(FString::Printf(TEXT("Issue: %s"), *Report.IssueReport));
        Lines.Add(FString::Printf(TEXT("Recommendation: %s"), *Report.Recommendation));
        Lines.Add(FString::Printf(TEXT("Warning: %s"), *Report.Warning));
        if (IsDetailedGuidance(GuidanceLevel))
        {
            Lines.Add(FString::Printf(TEXT("Tradeoff: %s"), *Report.TradeoffExplanation));
            Lines.Add(TEXT("Tutorial cue: compare this report against the matching desk screen before committing a policy, budget, meeting, or press decision."));
        }
    }

    FDemocracyAdvisorReport MakeAdvisorReport(const FString& AdvisorName, const FString& Category, int32 Severity, const FString& GuidanceLevel, const FString& Issue, const FString& Recommendation, const FString& Warning, const FString& Tradeoff)
    {
        FDemocracyAdvisorReport Report;
        Report.AdvisorName = AdvisorName;
        Report.Category = Category;
        Report.Severity = FMath::Clamp(Severity, 0, 100);
        Report.GuidanceLevel = GuidanceLevel;
        Report.IssueReport = Issue;
        Report.Recommendation = Recommendation;
        Report.Warning = Warning;
        Report.TradeoffExplanation = Tradeoff;
        return Report;
    }

    TArray<FDemocracyAdvisorReport> GenerateAdvisorReports(const FDemocracySimulationState& State)
    {
        const FDemocracyCountryState& Country = State.PlayerCountry;
        const FDemocracyResourceInventory& Resources = Country.Resources;
        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const FString GuidanceLevel = AdvisorGuidanceForDifficultyScore(DifficultyScore);
        const int32 ShortageCount = State.ResourceChains.Chains.Num() > 0 ? State.ResourceChains.TotalShortagePressure / 18 : (Resources.Food < 120 ? 1 : 0) + (Resources.Water < 100 ? 1 : 0) + (Resources.GasOil < 60 ? 1 : 0) + (Resources.Wood < 55 ? 1 : 0) + (Resources.Metals < 55 ? 1 : 0);
        const int32 InternalRiskPct = State.FailureRisk.AssassinationRiskTrigger > 0 ? (State.FailureRisk.CurrentAssassinationRisk * 100) / State.FailureRisk.AssassinationRiskTrigger : 0;
        const int32 InvasionRiskPct = State.InvasionRisk.InvasionRiskTrigger > 0 ? (State.InvasionRisk.CurrentInvasionRisk * 100) / State.InvasionRisk.InvasionRiskTrigger : 0;

        TArray<FDemocracyAdvisorReport> Reports;
        Reports.Add(MakeAdvisorReport(
            TEXT("Resource Manager"),
            TEXT("Resources"),
            ShortageCount * 20,
            GuidanceLevel,
            State.ResourceChains.Chains.Num() > 0 ? State.ResourceChains.Summary : FString::Printf(TEXT("Food %d, water %d, gas/oil %d, wood %d, metals %d."), Resources.Food, Resources.Water, Resources.GasOil, Resources.Wood, Resources.Metals),
            GuidanceText(GuidanceLevel,
                TEXT("Prioritize food above 120 and water above 100 first; those two shortages directly increase unrest during each tick."),
                TEXT("Protect food and water reserves before expanding production."),
                TEXT("Resource pressure is visible. Stabilize core stockpiles."),
                TEXT("Watch shortages.")),
            ShortageCount > 0 ? TEXT("Shortages are already adding unrest and stability pressure.") : TEXT("No immediate shortage warning."),
            TEXT("Stockpiling improves stability but can delay industrial or military spending.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Economic Advisor"),
            TEXT("Economy"),
            FMath::Max(0, 70 - Country.EconomicHealth) + (Country.Treasury < 250 ? 20 : 0),
            GuidanceLevel,
            FString::Printf(TEXT("Treasury %d, economic health %d, active economic policy: %s."), Country.Treasury, Country.EconomicHealth, *Country.Policies.EconomicPolicy),
            GuidanceText(GuidanceLevel,
                TEXT("If approval is stable, Balanced Budget or Industrial Subsidies can rebuild treasury and production. If approval is weak, avoid austerity until unrest is controlled."),
                TEXT("Balance treasury recovery against approval loss."),
                TEXT("Budget pressure is rising. Review economic policy."),
                TEXT("Watch treasury.")),
            Country.Treasury < 200 ? TEXT("Treasury reserve is low for crisis response.") : TEXT("Treasury reserve is currently usable."),
            TEXT("Spending can raise approval and stability now, but it reduces emergency reserves.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Social Advisor"),
            TEXT("Stability"),
            FMath::Max(Country.Unrest, 100 - Country.Stability),
            GuidanceLevel,
            FString::Printf(TEXT("Approval %d, stability %d, unrest %d."), Country.PublicApproval, Country.Stability, Country.Unrest),
            GuidanceText(GuidanceLevel,
                TEXT("Keep unrest below the warning threshold and stability above the warning threshold. Civil Liberties helps approval; Emergency Powers lowers unrest but damages trust."),
                TEXT("Reduce unrest before it crosses the warning threshold."),
                TEXT("Public order is weakening. Act soon."),
                TEXT("Watch unrest.")),
            Country.Unrest >= State.FailureRisk.UnrestWarningThreshold || Country.Stability <= State.FailureRisk.StabilityWarningThreshold ? TEXT("Internal failure warnings are active.") : TEXT("Internal stability is above warning thresholds."),
            TEXT("Public freedoms improve legitimacy over time; emergency authority buys control at the cost of approval and diplomacy.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Military Advisor"),
            TEXT("Military"),
            FMath::Max(0, 75 - Country.MilitaryReadiness) + InvasionRiskPct / 2,
            GuidanceLevel,
            FString::Printf(TEXT("Military readiness %d, invasion risk %d/%d."), Country.MilitaryReadiness, State.InvasionRisk.CurrentInvasionRisk, State.InvasionRisk.InvasionRiskTrigger),
            GuidanceText(GuidanceLevel,
                TEXT("Readiness below the warning threshold should be corrected before rival pressure rises. National Mobilization is fast but expensive and unpopular."),
                TEXT("Raise readiness or lower border pressure through diplomacy."),
                TEXT("Defense posture is weak. Review military policy."),
                TEXT("Watch readiness.")),
            Country.MilitaryReadiness <= State.InvasionRisk.MilitaryReadinessWarningThreshold ? TEXT("Military readiness warning is active.") : TEXT("Military readiness is above warning level."),
            TEXT("Mobilization lowers takeover risk but drains treasury and can increase unrest.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Diplomacy Advisor"),
            TEXT("Diplomacy"),
            FMath::Max(0, 75 - Country.DiplomaticStanding) + InvasionRiskPct / 3,
            GuidanceLevel,
            FString::Printf(TEXT("Diplomatic standing %d, democratic allies %d, non-democratic countries %d."), Country.DiplomaticStanding, State.WorldMap.DemocraticAllyCount, State.WorldMap.NonDemocraticCountryCount),
            GuidanceText(GuidanceLevel,
                TEXT("Alliance Outreach improves diplomacy and reduces takeover risk, especially when military readiness is low."),
                TEXT("Use diplomacy to reduce foreign pressure."),
                TEXT("Foreign pressure is increasing. Review posture."),
                TEXT("Watch diplomacy.")),
            Country.DiplomaticStanding < 40 ? TEXT("Diplomatic isolation is becoming dangerous.") : TEXT("Diplomatic standing is currently manageable."),
            TEXT("Concessions can buy safety but may slow domestic spending or appear weak to hardline blocs.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Infrastructure Advisor"),
            TEXT("Infrastructure"),
            FMath::Max(0, 80 - Country.Infrastructure),
            GuidanceLevel,
            FString::Printf(TEXT("Infrastructure %d, technology %d."), Country.Infrastructure, Country.Technology),
            GuidanceText(GuidanceLevel,
                TEXT("Infrastructure improves resource resilience. Stimulus Spending or Industrial Subsidies can help, but each carries treasury or environmental costs."),
                TEXT("Improve infrastructure to soften shortages."),
                TEXT("Infrastructure is limiting resilience."),
                TEXT("Watch infrastructure.")),
            Country.Infrastructure < 35 ? TEXT("Infrastructure is near crisis levels.") : TEXT("Infrastructure is functional."),
            TEXT("Construction improves long-term stability but competes with defense and public relief funding.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Security Advisor"),
            TEXT("Security"),
            FMath::Max(InternalRiskPct, InvasionRiskPct),
            GuidanceLevel,
            FString::Printf(TEXT("Assassination risk %d/%d, foreign takeover risk %d/%d."), State.FailureRisk.CurrentAssassinationRisk, State.FailureRisk.AssassinationRiskTrigger, State.InvasionRisk.CurrentInvasionRisk, State.InvasionRisk.InvasionRiskTrigger),
            GuidanceText(GuidanceLevel,
                TEXT("Internal threats follow unrest and instability; foreign threats follow readiness and diplomacy. Treat whichever risk percentage is higher first."),
                TEXT("Address the highest active fail-state risk first."),
                TEXT("Failure risk is rising. Identify the pressure source."),
                TEXT("Watch risk.")),
            (InternalRiskPct >= 50 || InvasionRiskPct >= 50) ? TEXT("A fail-state risk is above half of its trigger value.") : TEXT("No immediate fail-state trigger warning."),
            TEXT("Security crackdowns may reduce immediate risk while worsening public trust or diplomacy.")));

        Reports.Add(MakeAdvisorReport(
            TEXT("Public Welfare Advisor"),
            TEXT("Welfare"),
            FMath::Max(0, 70 - Country.PublicApproval) + FMath::Max(0, 60 - Country.EnvironmentalHealth),
            GuidanceLevel,
            FString::Printf(TEXT("Approval %d, environmental health %d, climate %s."), Country.PublicApproval, Country.EnvironmentalHealth, *Country.Climate),
            GuidanceText(GuidanceLevel,
                TEXT("Public welfare improves when shortages, pollution, and approval pressure are controlled together. Conservation helps environment but can slow extraction."),
                TEXT("Balance welfare, environment, and resource output."),
                TEXT("Welfare pressure is visible. Review shortages and environment."),
                TEXT("Watch approval.")),
            Country.PublicApproval < 40 ? TEXT("Low approval can accelerate unrest.") : TEXT("Approval is not in immediate collapse."),
            TEXT("Relief programs improve quality of life but cost treasury and may slow military or industrial priorities.")));

        Reports.Sort([](const FDemocracyAdvisorReport& Left, const FDemocracyAdvisorReport& Right)
        {
            return Left.Severity > Right.Severity;
        });

        return Reports;
    }

    FDemocracyEventChoiceState MakeEventChoice(const FString& Id, const FString& Label, const FString& Description, const FString& Preview,
        int32 Approval, int32 Stability, int32 Unrest, int32 Treasury, int32 Economy, int32 Diplomacy, int32 Military, int32 Infrastructure,
        int32 Environment, int32 Food, int32 Water, int32 GasOil, int32 Wood, int32 Metals, int32 AssassinationRisk, int32 InvasionRisk)
    {
        FDemocracyEventChoiceState Choice;
        Choice.ChoiceId = Id;
        Choice.Label = Label;
        Choice.Description = Description;
        Choice.ConsequencePreview = Preview;
        Choice.ApprovalDelta = Approval;
        Choice.StabilityDelta = Stability;
        Choice.UnrestDelta = Unrest;
        Choice.TreasuryDelta = Treasury;
        Choice.EconomicDelta = Economy;
        Choice.DiplomacyDelta = Diplomacy;
        Choice.MilitaryDelta = Military;
        Choice.InfrastructureDelta = Infrastructure;
        Choice.EnvironmentDelta = Environment;
        Choice.FoodDelta = Food;
        Choice.WaterDelta = Water;
        Choice.GasOilDelta = GasOil;
        Choice.WoodDelta = Wood;
        Choice.MetalsDelta = Metals;
        Choice.AssassinationRiskDelta = AssassinationRisk;
        Choice.InvasionRiskDelta = InvasionRisk;
        return Choice;
    }

    FDemocracyActiveEventState MakeEvent(FDemocracySimulationState& State, const FString& Type, const FString& Title, const FString& Description, const FString& TriggerReason, int32 Severity, bool bTriggered, const TArray<FDemocracyEventChoiceState>& Choices)
    {
        ++State.EventSystem.EventCounter;
        FDemocracyActiveEventState Event;
        Event.EventId = FString::Printf(TEXT("EVT-%04d-T%d"), State.EventSystem.EventCounter, State.Turn);
        Event.EventType = Type;
        Event.Title = Title;
        Event.Description = Description;
        Event.TriggerReason = TriggerReason;
        Event.CreatedTurn = State.Turn;
        Event.Severity = FMath::Clamp(Severity, 1, 100);
        Event.bTriggered = bTriggered;
        Event.Choices = Choices;
        return Event;
    }

    bool HasActiveEventType(const FDemocracyEventSystemState& EventSystem, const FString& EventType)
    {
        for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
        {
            if (!Event.bResolved && Event.EventType.Equals(EventType, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    void PruneResolvedEvents(FDemocracyEventSystemState& EventSystem)
    {
        EventSystem.ActiveEvents.RemoveAll([](const FDemocracyActiveEventState& Event)
        {
            return Event.bResolved;
        });
    }

    void GenerateSimulationEvents(FDemocracySimulationState& State)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyResourceInventory& Resources = Country.Resources;
        FDemocracyEventSystemState& EventSystem = State.EventSystem;
        PruneResolvedEvents(EventSystem);

        if (EventSystem.ActiveEvents.Num() >= EventSystem.ActiveEventLimit)
        {
            return;
        }

        const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
        const int32 EventRoll = (State.Turn * 37 + State.RtsWorld.SimulationSecond * 11 + DifficultyScore * 17 + EventSystem.EventCounter * 23) % 100;
        const bool bCooldownReady = State.Turn > EventSystem.LastEventTurn;

        auto AddEvent = [&](FDemocracyActiveEventState&& Event)
        {
            if (EventSystem.ActiveEvents.Num() < EventSystem.ActiveEventLimit && !HasActiveEventType(EventSystem, Event.EventType))
            {
                EventSystem.ActiveEvents.Add(Event);
                EventSystem.LastEventTurn = State.Turn;
                State.Phase = TEXT("Event Decision Pending");
            }
        };

        if ((Resources.Food < 90 || Resources.Water < 75) && !HasActiveEventType(EventSystem, TEXT("Shortage")))
        {
            AddEvent(MakeEvent(State, TEXT("Shortage"), TEXT("Household Shortage Reports"), TEXT("Regional administrators report that families are struggling to access essential supplies."), TEXT("Triggered by low food or water reserves."), 70, true, {
                MakeEventChoice(TEXT("relief"), TEXT("Release emergency reserves"), TEXT("Spend treasury to distribute food and water immediately."), TEXT("Food/water improve, unrest drops, treasury falls."), 4, 3, -8, -140, 0, 0, 0, 0, 0, 80, 70, 0, 0, 0, -4, 0),
                MakeEventChoice(TEXT("ration"), TEXT("Order rationing"), TEXT("Stretch supplies while accepting public frustration."), TEXT("Resources stabilize modestly, approval falls, unrest rises."), -6, -2, 4, -20, 0, 0, 0, 0, 0, 35, 30, 0, 0, 0, 2, 0),
                MakeEventChoice(TEXT("market"), TEXT("Let markets adjust"), TEXT("Avoid direct spending and let prices signal scarcity."), TEXT("Treasury protected, economy improves slightly, unrest rises."), -8, -4, 8, 40, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0)
            }));
            return;
        }

        if ((Country.Unrest >= State.FailureRisk.UnrestWarningThreshold || Country.PublicApproval < 42) && !HasActiveEventType(EventSystem, TEXT("Protest")))
        {
            AddEvent(MakeEvent(State, TEXT("Protest"), TEXT("Capital Protest Movement"), TEXT("Organized demonstrations are forming near government offices and transportation hubs."), TEXT("Triggered by unrest or weak public approval."), 65, true, {
                MakeEventChoice(TEXT("address"), TEXT("Address the public"), TEXT("Promise reforms and open talks with organizers."), TEXT("Approval and stability improve, treasury cost is moderate."), 7, 4, -6, -60, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0),
                MakeEventChoice(TEXT("security"), TEXT("Deploy security cordons"), TEXT("Contain protests with police and military support."), TEXT("Unrest drops now, approval and diplomacy suffer."), -8, 2, -7, -30, 0, -3, 2, 0, 0, 0, 0, 0, 0, 0, 4, 1),
                MakeEventChoice(TEXT("ignore"), TEXT("Wait it out"), TEXT("Avoid giving the movement legitimacy."), TEXT("Treasury protected, unrest and assassination risk rise."), -5, -5, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0)
            }));
            return;
        }

        if ((Country.DiplomaticStanding < 45 || State.InvasionRisk.CurrentInvasionRisk > State.InvasionRisk.InvasionRiskTrigger / 3) && !HasActiveEventType(EventSystem, TEXT("Border Tension")))
        {
            AddEvent(MakeEvent(State, TEXT("Border Tension"), TEXT("Border Tension Escalates"), TEXT("A rival state has moved forces close to a disputed crossing."), TEXT("Triggered by low diplomacy or rising invasion risk."), 75, true, {
                MakeEventChoice(TEXT("deescalate"), TEXT("Send envoys"), TEXT("Open a crisis channel and offer inspection guarantees."), TEXT("Diplomacy improves and invasion risk falls, treasury cost rises."), 1, 2, -2, -80, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, -10),
                MakeEventChoice(TEXT("reinforce"), TEXT("Reinforce the border"), TEXT("Move units to visible defensive positions."), TEXT("Readiness rises, invasion risk can fall, diplomacy suffers."), 0, 1, 1, -90, 0, -5, 8, 0, 0, 0, 0, -20, 0, -20, 0, -4),
                MakeEventChoice(TEXT("concede"), TEXT("Offer concessions"), TEXT("Trade a limited concession for calm."), TEXT("Diplomacy improves but approval and stability fall."), -6, -3, 3, -20, 0, 10, -1, 0, 0, 0, 0, 0, 0, 0, 2, -8)
            }));
            return;
        }

        if (bCooldownReady && EventRoll < 12 + DifficultyScore * 3 && !HasActiveEventType(EventSystem, TEXT("Economic Shock")))
        {
            AddEvent(MakeEvent(State, TEXT("Economic Shock"), TEXT("Sudden Market Shock"), TEXT("Credit markets tighten and major employers pause investment."), TEXT("Random economic shock."), 45 + DifficultyScore * 8, false, {
                MakeEventChoice(TEXT("stimulus"), TEXT("Emergency stimulus"), TEXT("Inject public funds to protect jobs."), TEXT("Economy and approval improve, treasury drops."), 5, 2, -3, -180, 8, 0, 0, 2, 0, 0, 0, 0, 0, 0, -2, 0),
                MakeEventChoice(TEXT("austerity"), TEXT("Protect the treasury"), TEXT("Cut spending and preserve reserves."), TEXT("Treasury improves, approval and economy suffer."), -7, -3, 5, 120, -6, 0, 0, -2, 0, 0, 0, 0, 0, 0, 5, 0),
                MakeEventChoice(TEXT("targeted"), TEXT("Target key industries"), TEXT("Support logistics, agriculture, and materials producers."), TEXT("Economy and resources improve with moderate cost."), 2, 1, -2, -100, 5, 0, 0, 1, -1, 25, 10, 10, 15, 15, -1, 0)
            }));
            return;
        }

        if (bCooldownReady && EventRoll >= 12 && EventRoll < 20 + DifficultyScore * 3 && !HasActiveEventType(EventSystem, TEXT("Disaster")))
        {
            AddEvent(MakeEvent(State, TEXT("Disaster"), TEXT("Severe Weather Disaster"), TEXT("A destructive weather system damages infrastructure and disrupts supply routes."), TEXT("Random disaster event."), 55 + DifficultyScore * 7, false, {
                MakeEventChoice(TEXT("mobilize"), TEXT("Mobilize relief agencies"), TEXT("Fund a fast national response."), TEXT("Stability improves, infrastructure damage is reduced, treasury falls."), 4, 5, -6, -160, 0, 1, 0, 5, 0, 30, 25, 0, 10, 0, -3, 0),
                MakeEventChoice(TEXT("local"), TEXT("Leave it to local officials"), TEXT("Let regional offices manage the response."), TEXT("Treasury cost is low, infrastructure and approval suffer."), -5, -4, 6, -30, -2, 0, 0, -8, -2, -20, -15, 0, -10, 0, 4, 0),
                MakeEventChoice(TEXT("military"), TEXT("Use military logistics"), TEXT("Deploy military transport and engineers."), TEXT("Readiness and supplies fall, stability improves."), 2, 4, -5, -90, 0, 0, -5, 4, 0, 20, 20, -15, 0, -10, -2, 1)
            }));
            return;
        }

        if (bCooldownReady && EventRoll >= 20 && EventRoll < 28 + DifficultyScore * 3 && !HasActiveEventType(EventSystem, TEXT("Scandal")))
        {
            AddEvent(MakeEvent(State, TEXT("Scandal"), TEXT("Administration Scandal"), TEXT("Reports allege misconduct inside a senior ministry."), TEXT("Random scandal event."), 50 + DifficultyScore * 7, false, {
                MakeEventChoice(TEXT("investigate"), TEXT("Open investigation"), TEXT("Authorize a visible independent review."), TEXT("Approval recovers over time, stability holds, short-term economy cost."), 4, 3, -3, -70, -1, 2, 0, 0, 0, 0, 0, 0, 0, 0, -4, 0),
                MakeEventChoice(TEXT("deny"), TEXT("Deny and attack critics"), TEXT("Reject the accusations as political sabotage."), TEXT("Treasury protected, unrest and assassination risk rise."), -8, -5, 8, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0),
                MakeEventChoice(TEXT("dismiss"), TEXT("Dismiss the minister"), TEXT("Remove the official and promise reform."), TEXT("Approval improves but infrastructure and economy lose momentum."), 6, 1, -4, -40, -2, 1, 0, -2, 0, 0, 0, 0, 0, 0, -3, 0)
            }));
        }
    }

    FString BuildEventSummaryText(const FDemocracyEventSystemState& EventSystem)
    {
        int32 PendingCount = 0;
        for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
        {
            if (!Event.bResolved) { ++PendingCount; }
        }
        return FString::Printf(TEXT("Pending events: %d | last event turn: %d | history entries: %d"), PendingCount, EventSystem.LastEventTurn, EventSystem.EventHistory.Num());
    }

    void AddUniqueSource(TArray<FString>& Sources, const FString& Source)
    {
        if (!Sources.Contains(Source))
        {
            Sources.Add(Source);
        }
    }

    void RecalculateDemographics(FDemocracySimulationState& State)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyDemographicsState& Demographics = State.Demographics;
        FDemocracyResourceInventory& Resources = Country.Resources;
        if (Demographics.CitizenGroups.Num() == 0 || Demographics.Regions.Num() == 0)
        {
            return;
        }

        const bool bFoodShortage = Resources.Food < 120;
        const bool bWaterShortage = Resources.Water < 100;
        const bool bFuelShortage = Resources.GasOil < 60;
        const bool bWeakJobs = Country.EconomicHealth < 50;
        const bool bSecurityConcern = Country.Unrest > 45 || State.InvasionRisk.CurrentInvasionRisk > State.InvasionRisk.InvasionRiskTrigger / 3;
        int32 WeightedGroupApproval = 0;
        int32 WeightedRegionApproval = 0;
        int32 NeedsPressure = 0;
        int32 DemographicUnrest = 0;
        Demographics.NationalUnrestSources.Reset();

        for (FDemocracyCitizenGroupState& Group : Demographics.CitizenGroups)
        {
            Group.UnrestSources.Reset();
            Group.NeedFood = FMath::Clamp(Group.NeedFood + (bFoodShortage ? 4 : -1), 0, 100);
            Group.NeedWater = FMath::Clamp(Group.NeedWater + (bWaterShortage ? 4 : -1), 0, 100);
            Group.NeedJobs = FMath::Clamp(Group.NeedJobs + (bWeakJobs ? 3 : -1), 0, 100);
            Group.NeedSecurity = FMath::Clamp(Group.NeedSecurity + (bSecurityConcern ? 3 : -1), 0, 100);
            Group.NeedHealthcare = FMath::Clamp(Group.NeedHealthcare + (Country.EnvironmentalHealth < 45 ? 2 : 0), 0, 100);

            int32 ApprovalDelta = 0;
            ApprovalDelta += Country.PublicApproval > 60 ? 1 : 0;
            ApprovalDelta -= bFoodShortage ? 2 : 0;
            ApprovalDelta -= bWaterShortage ? 2 : 0;
            ApprovalDelta -= bWeakJobs ? 1 : 0;
            ApprovalDelta -= bSecurityConcern ? 1 : 0;
            if (Group.GroupName.Contains(TEXT("Business")))
            {
                ApprovalDelta += Country.EconomicHealth > 60 ? 2 : -1;
                ApprovalDelta += Country.Policies.EconomicPolicy.Equals(TEXT("Industrial Subsidies"), ESearchCase::IgnoreCase) ? 2 : 0;
                ApprovalDelta += Country.Policies.EconomicPolicy.Equals(TEXT("Austerity Program"), ESearchCase::IgnoreCase) ? 1 : 0;
            }
            if (Group.GroupName.Contains(TEXT("Urban")) || Group.GroupName.Contains(TEXT("Youth")))
            {
                ApprovalDelta += Country.Policies.CivilPolicy.Equals(TEXT("Civil Liberties"), ESearchCase::IgnoreCase) ? 2 : 0;
                ApprovalDelta -= Country.Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase) ? 3 : 0;
            }
            if (Group.GroupName.Contains(TEXT("Rural")))
            {
                ApprovalDelta += Resources.Food > 160 ? 1 : 0;
                ApprovalDelta -= Country.Infrastructure < 40 ? 2 : 0;
            }
            if (Group.GroupName.Contains(TEXT("Retirees")))
            {
                ApprovalDelta -= Group.NeedHealthcare > 65 ? 2 : 0;
                ApprovalDelta += Country.Stability > 60 ? 1 : 0;
            }

            Group.Approval = FMath::Clamp(Group.Approval + ApprovalDelta, 0, 100);
            Group.UnrestPressure = FMath::Clamp((100 - Group.Approval) / 4 + FMath::Max(0, Group.NeedFood - 60) / 5 + FMath::Max(0, Group.NeedWater - 60) / 5 + FMath::Max(0, Group.NeedJobs - 60) / 6 + FMath::Max(0, Group.NeedSecurity - 60) / 6, 0, 100);
            if (bFoodShortage) { AddUniqueSource(Group.UnrestSources, TEXT("Food access")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Food access")); }
            if (bWaterShortage) { AddUniqueSource(Group.UnrestSources, TEXT("Water access")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Water access")); }
            if (bWeakJobs) { AddUniqueSource(Group.UnrestSources, TEXT("Jobs and wages")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Jobs and wages")); }
            if (bSecurityConcern) { AddUniqueSource(Group.UnrestSources, TEXT("Security concerns")); AddUniqueSource(Demographics.NationalUnrestSources, TEXT("Security concerns")); }
            WeightedGroupApproval += Group.Approval * FMath::Max(1, Group.PopulationShare);
            NeedsPressure += Group.UnrestPressure * FMath::Max(1, Group.PopulationShare);
        }

        for (FDemocracyRegionState& Region : Demographics.Regions)
        {
            Region.UnrestSources.Reset();
            Region.FoodAccess = FMath::Clamp(Region.FoodAccess + (bFoodShortage ? -4 : 1), 0, 100);
            Region.WaterAccess = FMath::Clamp(Region.WaterAccess + (bWaterShortage ? -4 : 1), 0, 100);
            Region.Jobs = FMath::Clamp(Region.Jobs + (bWeakJobs ? -3 : 1), 0, 100);
            Region.Security = FMath::Clamp(Region.Security + (bSecurityConcern ? -3 : 1), 0, 100);
            Region.Infrastructure = FMath::Clamp((Region.Infrastructure + Country.Infrastructure) / 2 + (bFuelShortage ? -1 : 0), 0, 100);
            Region.Approval = FMath::Clamp((Region.Approval + Country.PublicApproval) / 2 + (Region.Jobs > 55 ? 1 : -1) + (Region.Security > 55 ? 1 : -1), 0, 100);
            Region.Stability = FMath::Clamp((Region.Stability + Country.Stability + Region.Security / 2 + Region.Infrastructure / 2) / 3, 0, 100);
            Region.Unrest = FMath::Clamp((100 - Region.Approval) / 3 + FMath::Max(0, 55 - Region.FoodAccess) / 3 + FMath::Max(0, 55 - Region.WaterAccess) / 3 + FMath::Max(0, 55 - Region.Jobs) / 4 + FMath::Max(0, 55 - Region.Security) / 4, 0, 100);
            if (Region.FoodAccess < 50) { AddUniqueSource(Region.UnrestSources, TEXT("Regional food access")); }
            if (Region.WaterAccess < 50) { AddUniqueSource(Region.UnrestSources, TEXT("Regional water access")); }
            if (Region.Jobs < 50) { AddUniqueSource(Region.UnrestSources, TEXT("Regional jobs")); }
            if (Region.Security < 50) { AddUniqueSource(Region.UnrestSources, TEXT("Regional security")); }
            WeightedRegionApproval += Region.Approval * FMath::Max(1, Region.PopulationShare);
            DemographicUnrest += Region.Unrest * FMath::Max(1, Region.PopulationShare);
        }

        Demographics.AverageGroupApproval = FMath::Clamp(WeightedGroupApproval / 100, 0, 100);
        Demographics.AverageRegionalApproval = FMath::Clamp(WeightedRegionApproval / 100, 0, 100);
        Demographics.NationalNeedsPressure = FMath::Clamp(NeedsPressure / 100, 0, 100);
        Demographics.DemographicUnrestPressure = FMath::Clamp(DemographicUnrest / 100, 0, 100);
        if (Demographics.NationalUnrestSources.Num() == 0)
        {
            Demographics.NationalUnrestSources.Add(TEXT("No dominant demographic unrest source"));
        }
    }

    FString BuildDemographicsSummaryText(const FDemocracyDemographicsState& Demographics)
    {
        return FString::Printf(TEXT("Population %.1fM | group approval %d | regional approval %d | needs pressure %d | demographic unrest %d\nUnrest sources: %s"),
            Demographics.TotalPopulationThousands / 1000.0f,
            Demographics.AverageGroupApproval,
            Demographics.AverageRegionalApproval,
            Demographics.NationalNeedsPressure,
            Demographics.DemographicUnrestPressure,
            *FString::Join(Demographics.NationalUnrestSources, TEXT(", ")));
    }

    void ApplyBudgetPreset(FDemocracyEconomyBudgetState& Budget, const FString& PresetName)
    {
        if (PresetName.Equals(TEXT("Low Taxes"), ESearchCase::IgnoreCase))
        {
            Budget.TaxPolicy = TEXT("Low Taxes");
            Budget.TaxRate = 16;
        }
        else if (PresetName.Equals(TEXT("High Taxes"), ESearchCase::IgnoreCase))
        {
            Budget.TaxPolicy = TEXT("High Taxes");
            Budget.TaxRate = 34;
        }
        else
        {
            Budget.TaxPolicy = TEXT("Balanced Taxation");
            Budget.TaxRate = 24;
        }
    }

    void ApplySpendingPreset(FDemocracyEconomyBudgetState& Budget, const FString& PresetName)
    {
        if (PresetName.Equals(TEXT("Austerity"), ESearchCase::IgnoreCase))
        {
            Budget.SpendingPosture = TEXT("Austerity");
            Budget.PublicServicesSpending = 22;
            Budget.InfrastructureSpending = 18;
            Budget.DefenseSpending = 22;
        }
        else if (PresetName.Equals(TEXT("Public Services"), ESearchCase::IgnoreCase))
        {
            Budget.SpendingPosture = TEXT("Public Services");
            Budget.PublicServicesSpending = 48;
            Budget.InfrastructureSpending = 24;
            Budget.DefenseSpending = 24;
        }
        else if (PresetName.Equals(TEXT("Infrastructure Push"), ESearchCase::IgnoreCase))
        {
            Budget.SpendingPosture = TEXT("Infrastructure Push");
            Budget.PublicServicesSpending = 34;
            Budget.InfrastructureSpending = 42;
            Budget.DefenseSpending = 24;
        }
        else if (PresetName.Equals(TEXT("Defense Funding"), ESearchCase::IgnoreCase))
        {
            Budget.SpendingPosture = TEXT("Defense Funding");
            Budget.PublicServicesSpending = 30;
            Budget.InfrastructureSpending = 24;
            Budget.DefenseSpending = 44;
        }
        else
        {
            Budget.SpendingPosture = TEXT("Balanced Services");
            Budget.PublicServicesSpending = 35;
            Budget.InfrastructureSpending = 28;
            Budget.DefenseSpending = 28;
        }
    }

    void RecalculateEconomyBudget(FDemocracySimulationState& State)
    {
        FDemocracyCountryState& Country = State.PlayerCountry;
        FDemocracyEconomyBudgetState& Budget = State.EconomyBudget;
        const int32 PopulationScale = FMath::Max(1, State.Demographics.TotalPopulationThousands / 1000);
        const int32 TaxStress = FMath::Max(0, Budget.TaxRate - 26) / 2;
        const int32 LowTaxStimulus = Budget.TaxRate < 20 ? 2 : 0;
        const int32 ProductionBase = Country.EconomicHealth + Country.Infrastructure / 2 + FMath::Max(0, Country.Technology * 4) + LowTaxStimulus * 4;
        Budget.ProductionEfficiency = FMath::Clamp(ProductionBase / 2 + Budget.InfrastructureSpending / 3 - FMath::Max(0, Budget.Inflation - 5), 0, 100);
        Budget.Income = FMath::Max(0, (Budget.ProductionEfficiency * Budget.TaxRate * PopulationScale) / 35 + Country.Resources.GasOil / 12 + Country.Resources.Metals / 14 + Country.Resources.Food / 18);
        Budget.Expenses = Budget.PublicServicesSpending + Budget.InfrastructureSpending + Budget.DefenseSpending + FMath::Max(0, Budget.Debt / 35) + PopulationScale * 4;
        Budget.Deficit = Budget.Expenses - Budget.Income;
        if (Budget.Deficit > 0)
        {
            Budget.Debt = FMath::Clamp(Budget.Debt + Budget.Deficit, 0, 20000);
        }
        else
        {
            Budget.Debt = FMath::Max(0, Budget.Debt + Budget.Deficit / 2);
        }

        Budget.Inflation = FMath::Clamp(2 + FMath::Max(0, Budget.Deficit) / 45 + FMath::Max(0, Budget.Debt - 800) / 350 + (Budget.TaxRate < 18 ? 1 : 0), 0, 35);
        Budget.PublicServices = FMath::Clamp(Budget.PublicServices + (Budget.PublicServicesSpending - 30) / 4 - FMath::Max(0, Budget.Inflation - 8) / 2 - FMath::Max(0, State.Demographics.NationalNeedsPressure - 50) / 8, 0, 100);
        Country.Treasury = FMath::Max(0, Country.Treasury - Budget.Deficit);
        Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + Budget.ProductionEfficiency / 30 - TaxStress - FMath::Max(0, Budget.Inflation - 8) / 3 - (Budget.Deficit > 80 ? 1 : 0), 0, 100);
        Country.PublicApproval = FMath::Clamp(Country.PublicApproval + Budget.PublicServices / 35 - TaxStress - FMath::Max(0, Budget.Inflation - 6) / 3 + (Budget.TaxRate < 20 ? 1 : 0), 0, 100);
        Country.Infrastructure = FMath::Clamp(Country.Infrastructure + Budget.InfrastructureSpending / 28 - 1, 0, 100);
        Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + Budget.DefenseSpending / 32 - 1, 0, 100);
        Budget.LastBudgetSummary = FString::Printf(TEXT("Income %d, expenses %d, %s %d, debt %d, inflation %d, services %d, production %d."),
            Budget.Income,
            Budget.Expenses,
            Budget.Deficit >= 0 ? TEXT("deficit") : TEXT("surplus"),
            FMath::Abs(Budget.Deficit),
            Budget.Debt,
            Budget.Inflation,
            Budget.PublicServices,
            Budget.ProductionEfficiency);
    }

    FString BuildEconomyBudgetSummaryText(const FDemocracyEconomyBudgetState& Budget)
    {
        return FString::Printf(TEXT("%s | tax %d%% | %s | income %d | expenses %d | %s %d | debt %d | inflation %d | services %d | production %d\n%s"),
            *Budget.TaxPolicy,
            Budget.TaxRate,
            *Budget.SpendingPosture,
            Budget.Income,
            Budget.Expenses,
            Budget.Deficit >= 0 ? TEXT("deficit") : TEXT("surplus"),
            FMath::Abs(Budget.Deficit),
            Budget.Debt,
            Budget.Inflation,
            Budget.PublicServices,
            Budget.ProductionEfficiency,
            *Budget.LastBudgetSummary);
    }
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void ALoginHUD::BeginPlay()
{
    Super::BeginPlay();

    BackgroundBrush = MakeShared<FSlateImageBrush>(
        FPaths::ProjectContentDir() / TEXT("UI/Login/Office_Login_Background.png"),
        FVector2D(1680.0f, 945.0f));
    OverlayBrush = MakeShared<FSlateColorBrush>(FLinearColor(0.0f, 0.0f, 0.0f, 0.24f));
    PanelBrush = MakeShared<FSlateColorBrush>(FLinearColor(0.025f, 0.028f, 0.032f, 0.88f));
    RowBrush = MakeShared<FSlateColorBrush>(FLinearColor(0.06f, 0.065f, 0.075f, 0.80f));

    LoginButtonStyle = MakeShared<FButtonStyle>();
    *LoginButtonStyle = FButtonStyle()
        .SetNormal(FSlateColorBrush(FLinearColor(0.05f, 0.06f, 0.07f, 0.88f)))
        .SetHovered(FSlateColorBrush(FLinearColor(0.14f, 0.16f, 0.19f, 0.94f)))
        .SetPressed(FSlateColorBrush(FLinearColor(0.02f, 0.03f, 0.04f, 0.98f)))
        .SetDisabled(FSlateColorBrush(FLinearColor(0.025f, 0.028f, 0.032f, 0.54f)))
        .SetNormalPadding(FMargin(16.0f, 10.0f))
        .SetPressedPadding(FMargin(16.0f, 12.0f, 16.0f, 8.0f));

    LoadRememberedLoginDetails();

    SettingsMenuWidgetClass = LoadClass<UUserWidget>(
        nullptr,
        TEXT("/Game/SpireSystems/SettingsMenuKit/Widgets/WBP_SettingsMenu.WBP_SettingsMenu_C"));
    if (!SettingsMenuWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("Settings Menu Kit widget was not found. Falling back to Slate settings screen."));
    }

    RefreshLoginWidget();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void ALoginHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    TearDownLoginWidget();
    StopSimulationTimer();

    LoginButtonStyle.Reset();
    RowBrush.Reset();
    PanelBrush.Reset();
    OverlayBrush.Reset();
    BackgroundBrush.Reset();

    Super::EndPlay(EndPlayReason);
}

void ALoginHUD::ShowScreen(ELoginFlowScreen Screen)
{
    CurrentScreen = Screen;
    RefreshLoginWidget();
}

void ALoginHUD::RefreshLoginWidget()
{
    if (!GEngine || !GEngine->GameViewport)
    {
        return;
    }

    TearDownLoginWidget();

    if (bInOfficeMode && (CurrentScreen == ELoginFlowScreen::OfficeOpeningBriefing || CurrentScreen == ELoginFlowScreen::OfficeDashboard || CurrentScreen == ELoginFlowScreen::OfficeComputerMenu || CurrentScreen == ELoginFlowScreen::OfficePolicies || CurrentScreen == ELoginFlowScreen::OfficeEvents || CurrentScreen == ELoginFlowScreen::OfficeDemographics || CurrentScreen == ELoginFlowScreen::OfficeBudget || CurrentScreen == ELoginFlowScreen::OfficeResourceChains || CurrentScreen == ELoginFlowScreen::OfficeDepartments || CurrentScreen == ELoginFlowScreen::OfficeDevelopment || CurrentScreen == ELoginFlowScreen::OfficeApprovalStability || CurrentScreen == ELoginFlowScreen::OfficeDecisionHistory || CurrentScreen == ELoginFlowScreen::OfficeWorldRts || CurrentScreen == ELoginFlowScreen::OfficeAdvisorWarnings || CurrentScreen == ELoginFlowScreen::OfficeMeetingAdvisor || CurrentScreen == ELoginFlowScreen::OfficePressRelease))
    {
        LoginScreenWidget =
            SNew(SOverlay)
            + SOverlay::Slot()
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, 36.0f, 0.0f))
            [
                BuildCurrentScreen()
            ];
    }
    else
    {
        LoginScreenWidget =
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SNew(SImage)
                .Image(BackgroundBrush.Get())
            ]
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderImage(OverlayBrush.Get())
            ]
            + SOverlay::Slot()
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                BuildCurrentScreen()
            ];
    }

    GEngine->GameViewport->AddViewportWidgetContent(LoginScreenWidget.ToSharedRef(), 100);
}

void ALoginHUD::TearDownLoginWidget()
{
    if (GEngine && GEngine->GameViewport && LoginScreenWidget.IsValid())
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(LoginScreenWidget.ToSharedRef());
    }

    LoginScreenWidget.Reset();
}

void ALoginHUD::HandleOfficeInteractable(const FString& InteractionName)
{
    APlayerController* PlayerController = GetOwningPlayerController();

    if (InteractionName.Equals(TEXT("Computer"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        ShowScreen(ELoginFlowScreen::OfficeComputerMenu);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("Globe"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        ShowScreen(ELoginFlowScreen::OfficeWorldRts);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("Phone"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        ShowScreen(ELoginFlowScreen::OfficeAdvisorWarnings);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("BriefingFolder"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        bShowFirstLoginBriefing = false;
        ShowScreen(ELoginFlowScreen::OfficeOpeningBriefing);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }
    if (InteractionName.Equals(TEXT("Lamp"), ESearchCase::IgnoreCase))
    {
        if (UWorld* World = GetWorld())
        {
            for (TActorIterator<APointLight> It(World); It; ++It)
            {
                APointLight* PointLight = *It;
                if (PointLight && PointLight->GetActorLabel().Equals(TEXT("Desk Lamp Toggle Light"), ESearchCase::IgnoreCase) && PointLight->PointLightComponent)
                {
                    const bool bTurnOn = PointLight->PointLightComponent->Intensity <= 1.0f;
                    PointLight->PointLightComponent->SetIntensity(bTurnOn ? 4200.0f : 0.0f);
                    if (GEngine)
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 2.0f, bTurnOn ? FColor::Yellow : FColor::Silver, bTurnOn ? TEXT("Desk lamp on.") : TEXT("Desk lamp off."));
                    }
                    return;
                }
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("Desk lamp toggle light was not found."));
        return;
    }

    if (InteractionName.Equals(TEXT("Door"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(0.0f, -1030.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("HallwayReturn"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(0.0f, -455.0f, 95.0f), FRotator(0.0f, 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("MeetingRoomDoor"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(-520.0f, -2760.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.StartsWith(TEXT("MeetingAdvisor_")))
    {
        bInOfficeMode = true;
        if (InteractionName.Equals(TEXT("MeetingAdvisor_Resources"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Resource Manager");
            SelectedMeetingAdvisorFocus = TEXT("Food, gas/oil, wood, metals, stockpile pressure, shortage relief, and trade dependency.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Military"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Military Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Military readiness, invasion risk, border defense, force posture, and emergency mobilization.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Social"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Social Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Approval, unrest, stability, public trust, social policy impact, and assassination risk warnings.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Economy"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Economic Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Budget health, production, inflation placeholders, taxation, public spending, and economic shocks.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Diplomacy"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Diplomacy Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Alliances, rival countries, negotiations, foreign official meetings, treaties, and diplomatic pressure.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Infrastructure"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Infrastructure Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Roads, power, communications, logistics, construction capacity, and national resilience.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Intelligence"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Security Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Internal threats, coup risk, assassination plots, counterintelligence, and hostile foreign action.");
        }
        else if (InteractionName.Equals(TEXT("MeetingAdvisor_Welfare"), ESearchCase::IgnoreCase))
        {
            SelectedMeetingAdvisorName = TEXT("Public Welfare Advisor");
            SelectedMeetingAdvisorFocus = TEXT("Health, education, housing placeholders, disaster response, and quality-of-life effects.");
        }
        else
        {
            SelectedMeetingAdvisorName = TEXT("Meeting Advisor");
            SelectedMeetingAdvisorFocus = TEXT("General advisor placeholder for future meeting logic.");
        }
        ShowScreen(ELoginFlowScreen::OfficeMeetingAdvisor);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }
    if (InteractionName.Equals(TEXT("MeetingRoomReturn"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(-520.0f, -2185.0f, 95.0f), FRotator(0.0f, 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }


    if (InteractionName.Equals(TEXT("PressRoomDoor"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(1680.0f, -2745.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("PressRoomReturn"), ESearchCase::IgnoreCase))
    {
        TearDownLoginWidget();
        bInOfficeMode = true;
        if (PlayerController)
        {
            APawn* ControlledPawn = PlayerController->GetPawn();
            if (ControlledPawn)
            {
                ControlledPawn->SetActorLocationAndRotation(FVector(520.0f, -2185.0f, 95.0f), FRotator(0.0f, 90.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
                PlayerController->SetViewTarget(ControlledPawn);
            }
            PlayerController->bShowMouseCursor = false;
            FInputModeGameOnly InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }

    if (InteractionName.Equals(TEXT("PressPodium"), ESearchCase::IgnoreCase))
    {
        bInOfficeMode = true;
        ShowScreen(ELoginFlowScreen::OfficePressRelease);
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            FInputModeGameAndUI InputMode;
            PlayerController->SetInputMode(InputMode);
        }
        return;
    }
    if (InteractionName.Equals(TEXT("HallwaySideDoor"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Log, TEXT("Hallway side door is visible but not connected to a destination yet."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Unhandled office interactable: %s"), *InteractionName);
}

TSharedRef<SWidget> ALoginHUD::BuildCurrentScreen()
{
    switch (CurrentScreen)
    {
    case ELoginFlowScreen::Settings:
        return BuildSettingsScreen();
    case ELoginFlowScreen::GameModeSelection:
        return BuildGameModeSelectionScreen();
    case ELoginFlowScreen::LocalSaveSelection:
        return BuildLocalSaveSelectionScreen();
    case ELoginFlowScreen::DifficultySelection:
        return BuildDifficultySelectionScreen();
    case ELoginFlowScreen::NewStateSetup:
        return BuildNewStateSetupScreen();
    case ELoginFlowScreen::LoadedGame:
        return BuildLoadedGameScreen();
    case ELoginFlowScreen::MultiplayerStateSelection:
        return BuildMultiplayerStateSelectionScreen();
    case ELoginFlowScreen::ServerSelection:
        return BuildServerSelectionScreen();
    case ELoginFlowScreen::OfficeOpeningBriefing:
        return BuildOfficeOpeningBriefingScreen();
    case ELoginFlowScreen::OfficeDashboard:
        return BuildOfficeDashboardScreen();
    case ELoginFlowScreen::OfficeComputerMenu:
        return BuildOfficeComputerMenuScreen();
    case ELoginFlowScreen::OfficePolicies:
        return BuildOfficePoliciesScreen();
    case ELoginFlowScreen::OfficeEvents:
        return BuildOfficeEventsScreen();
    case ELoginFlowScreen::OfficeDemographics:
        return BuildOfficeDemographicsScreen();
    case ELoginFlowScreen::OfficeBudget:
        return BuildOfficeBudgetScreen();
    case ELoginFlowScreen::OfficeResourceChains:
        return BuildOfficeResourceChainsScreen();
    case ELoginFlowScreen::OfficeDepartments:
        return BuildOfficeDepartmentsScreen();
    case ELoginFlowScreen::OfficeDevelopment:
        return BuildOfficeDevelopmentScreen();
    case ELoginFlowScreen::OfficeApprovalStability:
        return BuildOfficeApprovalStabilityScreen();
    case ELoginFlowScreen::OfficeDecisionHistory:
        return BuildOfficeDecisionHistoryScreen();
    case ELoginFlowScreen::OfficeWorldRts:
        return BuildOfficeWorldRtsScreen();
    case ELoginFlowScreen::OfficeAdvisorWarnings:
        return BuildOfficeAdvisorWarningsScreen();
    case ELoginFlowScreen::OfficeMeetingAdvisor:
        return BuildOfficeMeetingAdvisorScreen();
    case ELoginFlowScreen::OfficePressRelease:
        return BuildOfficePressReleaseScreen();
    case ELoginFlowScreen::GameOver:
        return BuildGameOverScreen();
    case ELoginFlowScreen::Login:
    default:
        return BuildLoginScreen();
    }
}

TSharedRef<SWidget> ALoginHUD::BuildLoginScreen()
{
    return BuildPanel(TEXT("Democracy"), TEXT("Mock account form. Authentication is not connected yet."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [
            SNew(SEditableTextBox)
            .HintText(BodyText(TEXT("Username or email")))
            .Text(BodyText(MockUserName))
            .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleMockUserNameChanged))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [
            SNew(SEditableTextBox)
            .HintText(BodyText(TEXT("Password")))
            .Text(BodyText(MockPassword))
            .IsPassword(true)
            .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleMockPasswordChanged))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [
            SNew(SCheckBox)
            .IsChecked(bRememberLoginDetails ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleRememberLoginDetailsChanged))
            [
                SNew(STextBlock)
                .Text(BodyText(TEXT("Remember login details")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
        [BuildButton(TEXT("Sign In"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSignInClicked))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Sign Up"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSignUpClicked))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Settings"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSettingsClicked))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Exit"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleExitClicked))]);
}

TSharedRef<SWidget> ALoginHUD::BuildSettingsScreen()
{
    if (SettingsMenuWidgetClass)
    {
        return BuildSettingsMenuProScreen();
    }
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Visual"), TEXT("Common display settings. Values are UI placeholders and are not persisted yet."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildCheckRow(TEXT("Fullscreen"), TEXT("Toggle fullscreen/windowed preference."), bFullscreen, FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleFullscreenChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildCheckRow(TEXT("VSync"), TEXT("Toggle vertical sync preference."), bVSync, FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleVSyncChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Brightness"), Brightness, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleBrightnessChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("UI Scale"), UiScale, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleUiScaleChanged))];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Audio"), TEXT("Common audio mix settings. Hook these into game user settings/audio bus later."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Master Volume"), MasterVolume, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleMasterVolumeChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Music Volume"), MusicVolume, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleMusicVolumeChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Effects Volume"), EffectsVolume, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleEffectsVolumeChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildSliderRow(TEXT("Voice Volume"), VoiceVolume, FOnFloatValueChanged::CreateUObject(this, &ALoginHUD::HandleVoiceVolumeChanged))];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Controls"), TEXT("Keybind rows are placeholders until input mapping capture is connected."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Move Camera Forward"), TEXT("W"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Move Camera Back"), TEXT("S"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Move Camera Left"), TEXT("A"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Move Camera Right"), TEXT("D"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildCheckRow(TEXT("Invert Look Y"), TEXT("Reverse vertical look direction for office camera controls."), bInvertLookY, FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleInvertLookYChanged))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Select / Confirm"), TEXT("Left Mouse"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildKeybindRow(TEXT("Cancel / Back"), TEXT("Right Mouse"))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildBackButton()];

    return BuildPanel(TEXT("Settings"), TEXT("Audio, visual, and control options."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 760.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildSettingsMenuProScreen()
{
    if (!SettingsMenuWidget && SettingsMenuWidgetClass)
    {
        SettingsMenuWidget = CreateWidget<UUserWidget>(GetOwningPlayerController(), SettingsMenuWidgetClass);
    }

    if (!SettingsMenuWidget)
    {
        return BuildPanel(TEXT("Settings"), TEXT("Settings Menu Kit widget could not be created."),
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Widget Missing"), TEXT("The SettingsMenuKit UMG asset is present, but Unreal could not instantiate it."))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
            [BuildBackButton()], 760.0f);
    }

    return BuildPanel(TEXT("Settings"), TEXT("Settings Menu Kit placeholder wired from imported UI assets."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            SNew(SBox)
            .MinDesiredWidth(920.0f)
            .MinDesiredHeight(560.0f)
            [
                SettingsMenuWidget->TakeWidget()
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [BuildBackButton()], 1040.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildGameModeSelectionScreen()
{
    return BuildPanel(TEXT("Select Game Mode"), TEXT("Temporary testing route keeps multiplayer locked."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Single Player"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSinglePlayerClicked))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Multiplayer - Disabled"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMultiplayerClicked), 300.0f, 52.0f, false)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [BuildInfoRow(TEXT("Multiplayer Locked"), TEXT("Visible for layout testing. It will activate when backend login, account states, and server services are ready."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildBackButton()]);
}

TSharedRef<SWidget> ALoginHUD::BuildLocalSaveSelectionScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
    TArray<FString> Saves = GetLocalSaveNames();

    if (Saves.IsEmpty())
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("No Local Saves Found"), TEXT("Choose a difficulty to create a new single-player state."))];
        Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Choose Difficulty"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCreateNewStateClicked), 320.0f, 52.0f)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLoginClicked), 180.0f, 44.0f)];

        return BuildPanel(TEXT("New Single Player State"), TEXT("No local saves are present in the game Saves folder."), Body);
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
    [BuildButton(TEXT("Create New State"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCreateNewStateClicked), 320.0f, 50.0f)];

    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SEditableTextBox)
        .HintText(BodyText(TEXT("Search saved states")))
        .Text(BodyText(LocalSaveSearchText))
        .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleLocalSaveSearchChanged))
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
    [
        SNew(SCheckBox)
        .IsChecked(bShowRecentLocalSavesOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleRecentLocalSavesChanged))
        [
            SNew(STextBlock)
            .Text(BodyText(TEXT("Recent saves only")))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
            .ColorAndOpacity(FLinearColor::White)
        ]
    ];

    TSharedRef<SScrollBox> SaveList = SNew(SScrollBox);
    const FString SearchLower = LocalSaveSearchText.ToLower();
    int32 VisibleSaves = 0;

    for (int32 SaveIndex = 0; SaveIndex < Saves.Num(); ++SaveIndex)
    {
        const FString& SaveName = Saves[SaveIndex];
        const bool bMatchesSearch = SearchLower.IsEmpty() || SaveName.ToLower().Contains(SearchLower);
        const bool bMatchesRecent = !bShowRecentLocalSavesOnly || SaveIndex < 5;

        if (!bMatchesSearch || !bMatchesRecent)
        {
            continue;
        }

        ++VisibleSaves;
        const FString Detail = FString::Printf(TEXT("Local file in %s. Placeholder list uses search and filter behavior like server selection."), *(FPaths::ProjectDir() / TEXT("Saves")));

        SaveList->AddSlot().Padding(0.0f, 4.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [BuildButton(SaveName, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectLocalSave, SaveName), 400.0f, 46.0f)]
                + SHorizontalBox::Slot().AutoWidth()
                [BuildButton(TEXT("Delete"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleDeleteLocalSave, SaveName), 110.0f, 46.0f)]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [BuildInfoRow(TEXT("Saved State"), Detail)]
        ];
    }

    if (VisibleSaves == 0)
    {
        SaveList->AddSlot().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("No Saves Found"), TEXT("Adjust search or filters, or create a new state."))];
    }

    Body->AddSlot().MaxHeight(310.0f).Padding(0.0f, 8.0f)
    [SaveList];
    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildInfoRow(TEXT("Local Save Rule"), TEXT("Single-player saves are read from the game directory Saves folder."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLoginClicked), 180.0f, 44.0f)];

    return BuildPanel(TEXT("Single Player Saves"), TEXT("Select a saved state or create a new one."), Body);
}

TSharedRef<SWidget> ALoginHUD::BuildDifficultySelectionScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    for (const FString& DifficultyName : FDifficultyProfileLibrary::GetDifficultyNames())
    {
        const FDifficultyProfile Profile = FDifficultyProfileLibrary::GetProfile(DifficultyName);
        Body->AddSlot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(DifficultyName, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectDifficulty, DifficultyName), 320.0f, 50.0f)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
        [BuildInfoRow(DifficultyName + TEXT(" Profile"), Profile.ToSummaryText())];
        Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 8.0f)
        [BuildInfoRow(TEXT("Guidance"), DifficultyGuidancePreview(Profile))];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Testing"), TEXT("Difficulty values are loaded from Config/DifficultyProfiles.ini."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLocalSavesClicked), 180.0f, 44.0f)];

    return BuildPanel(TEXT("Select Difficulty"), TEXT("Harder profiles increase country size, reduce support, and broaden but reduce starting resources."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 780.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildNewStateSetupScreen()
{
    const bool bHasRequiredSelections = !PendingDifficulty.IsEmpty() && !PendingClimate.IsEmpty() && !PendingLeaderGender.IsEmpty();

    return BuildPanel(TEXT("New State Setup"), TEXT("Name the state and choose its starting climate."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Difficulty"), PendingDifficulty.IsEmpty() ? TEXT("None selected") : PendingDifficulty)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Difficulty Profile"), PendingDifficulty.IsEmpty() ? TEXT("Select a difficulty first.") : FDifficultyProfileLibrary::GetProfile(PendingDifficulty).ToSummaryText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Tutorial Guidance"), PendingDifficulty.IsEmpty() ? TEXT("Select a difficulty first.") : DifficultyGuidancePreview(FDifficultyProfileLibrary::GetProfile(PendingDifficulty)))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [
            SNew(SEditableTextBox)
            .HintText(BodyText(TEXT("State name")))
            .Text(BodyText(PendingStateName))
            .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandlePendingStateNameChanged))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
        [BuildInfoRow(TEXT("Climate"), PendingClimate.IsEmpty() ? TEXT("Choose one climate below.") : PendingClimate)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingClimate == TEXT("Northern Cold") ? TEXT("Northern Cold - Selected") : TEXT("Northern Cold"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectClimate, FString(TEXT("Northern Cold"))), 360.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingClimate == TEXT("Middle Moderate") ? TEXT("Middle Moderate - Selected") : TEXT("Middle Moderate"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectClimate, FString(TEXT("Middle Moderate"))), 360.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingClimate == TEXT("Southern Tropical") ? TEXT("Southern Tropical - Selected") : TEXT("Southern Tropical"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectClimate, FString(TEXT("Southern Tropical"))), 360.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
        [BuildInfoRow(TEXT("President Address"), PendingAddressTitle.IsEmpty() ? TEXT("Choose male or female for dialogue address.") : PendingAddressTitle)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Create Status"), LastSaveStatus.IsEmpty() ? TEXT("Ready") : LastSaveStatus)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingLeaderGender == TEXT("Male") ? TEXT("Male - Mr. President") : TEXT("Male"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectLeaderGender, FString(TEXT("Male"))), 360.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(PendingLeaderGender == TEXT("Female") ? TEXT("Female - Miss President") : TEXT("Female"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectLeaderGender, FString(TEXT("Female"))), 360.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Create State and Load Game"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCreateInitialSaveClicked), 360.0f, 52.0f, bHasRequiredSelections)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToDifficultyClicked), 180.0f, 44.0f)]);
}

TSharedRef<SWidget> ALoginHUD::BuildLoadedGameScreen()
{
    return BuildPanel(TEXT("Loaded State"), TEXT("Runtime state is loaded. Enter the office to continue playing."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Loaded State"), LoadedStateName.IsEmpty() ? TEXT("Unnamed") : LoadedStateName)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Save File"), LoadedSavePath.IsEmpty() ? TEXT("No file path recorded.") : LoadedSavePath)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Runtime Load"), bHasLoadedRuntimeState ? LoadedSaveSummary : (LoadedSaveError.IsEmpty() ? TEXT("Runtime state has not been loaded.") : LoadedSaveError))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Simulation"), BuildSimulationStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Resources"), BuildResourceStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Policy"), BuildPolicyStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Events"), BuildEventStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Demographics"), BuildDemographicsStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Budget"), BuildEconomyBudgetStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Departments"), BuildDepartmentStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Development"), BuildDevelopmentStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Approval Causes"), BuildApprovalStabilityStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Decision History"), BuildDecisionHistoryStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Resume"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleResumeSimulationClicked), 150.0f, 42.0f, bHasLoadedRuntimeState && LoadedSaveState.RuntimeState.bPaused)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Pause"), FOnClicked::CreateUObject(this, &ALoginHUD::HandlePauseSimulationClicked), 150.0f, 42.0f, bHasLoadedRuntimeState && !LoadedSaveState.RuntimeState.bPaused)]
            + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [BuildButton(TEXT("Step Tick"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleStepSimulationClicked), 150.0f, 42.0f, bHasLoadedRuntimeState)]
            + SHorizontalBox::Slot().AutoWidth()
            [BuildButton(TEXT("Save"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSaveRuntimeStateClicked), 120.0f, 42.0f, bHasLoadedRuntimeState && !LoadedSavePath.IsEmpty())]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Save Status"), LastSaveStatus.IsEmpty() ? TEXT("Runtime changes are in memory until Save is pressed.") : LastSaveStatus)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildInfoRow(TEXT("Briefing"), TEXT("Ongoing briefings are available from the folder on the office desk."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildButton(TEXT("Enter Office"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleEnterOfficeClicked), 220.0f, 44.0f)], 860.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeOpeningBriefingScreen()
{
    const FString ScriptToShow = bShowFirstLoginBriefing
        ? (OpeningScriptText.IsEmpty() ? GetOpeningScriptText() : OpeningScriptText)
        : FString::Printf(TEXT("Ongoing briefing for %s.\n\n%s\n\n%s\n\nAdvisor notes:\n%s"),
            *(LoadedStateName.IsEmpty() ? FString(TEXT("Unnamed")) : LoadedStateName),
            *BuildSimulationStatusText(),
            *BuildResourceStatusText(),
            *BuildAdvisorWarningText());
    const FString BriefingTitle = bShowFirstLoginBriefing ? TEXT("Assistant Briefing") : TEXT("Ongoing Briefing");
    const FString BriefingSubtitle = bShowFirstLoginBriefing
        ? TEXT("The office is now loaded. The assistant begins the first briefing.")
        : TEXT("Updated state briefing from the desk folder.");

    return BuildPanel(BriefingTitle, BriefingSubtitle,
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Loaded State"), LoadedStateName.IsEmpty() ? TEXT("Unnamed") : LoadedStateName)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Assistant"), bShowFirstLoginBriefing ? TEXT("Opening dialogue uses the current script file. Bracketed player-choice placeholders are preserved for the next dialogue pass.") : TEXT("Current briefing is generated from the loaded runtime state."))]
        + SVerticalBox::Slot().MaxHeight(420.0f).Padding(0.0f, 12.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SBorder)
                .BorderImage(RowBrush.Get())
                .Padding(FMargin(14.0f, 12.0f))
                [
                    SNew(STextBlock)
                    .Text(BodyText(ScriptToShow))
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                    .ColorAndOpacity(FLinearColor(0.88f, 0.90f, 0.92f, 1.0f))
                ]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildButton(bShowFirstLoginBriefing ? TEXT("Begin In Office") : TEXT("Close Briefing"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBeginOfficeFromBriefingClicked), 220.0f, 42.0f)], 760.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildOfficeDashboardScreen()
{
    return BuildOfficeComputerMenuScreen();
}

TSharedRef<SWidget> ALoginHUD::BuildOfficeComputerMenuScreen()
{
    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyCountryState& Country = State.PlayerCountry;
    const FString GovernmentSummary = bHasLoadedRuntimeState
        ? FString::Printf(TEXT("%s | %s | %s | guidance %s"), *Country.CountryName, *Country.Difficulty, *Country.Climate, *State.AdvisorSystem.GuidanceLevel)
        : FString(TEXT("No runtime state loaded."));
    const FString RiskSummary = bHasLoadedRuntimeState
        ? FString::Printf(TEXT("Assassination %d/%d | Takeover %d/%d | top cause: %s"),
            State.FailureRisk.CurrentAssassinationRisk,
            State.FailureRisk.AssassinationRiskTrigger,
            State.InvasionRisk.CurrentInvasionRisk,
            State.InvasionRisk.InvasionRiskTrigger,
            *State.ApprovalStability.Summary)
        : FString(TEXT("Unavailable"));
    const FString ReportsSummary = bHasLoadedRuntimeState
        ? FString::Printf(TEXT("Advisor reports %d | decisions %d/%d | press records %d | meetings %d"),
            State.AdvisorSystem.Reports.Num(),
            State.DecisionHistory.Records.Num(),
            State.DecisionHistory.MaxRecords,
            State.PressOffice.Records.Num(),
            State.MeetingSystem.TotalMeetings)
        : FString(TEXT("Unavailable"));

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Government"), GovernmentSummary)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Simulation"), BuildSimulationStatusText())];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Risk / Causes"), RiskSummary)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [BuildInfoRow(TEXT("Save"), LastSaveStatus.IsEmpty() ? TEXT("Manual save and autosave protection are available from this computer.") : LastSaveStatus)];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Simulation Controls"), TEXT("Run, pause, step, and protect the current local runtime state."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Resume"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleResumeSimulationClicked), 130.0f, 38.0f, bHasLoadedRuntimeState && LoadedSaveState.RuntimeState.bPaused)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Pause"), FOnClicked::CreateUObject(this, &ALoginHUD::HandlePauseSimulationClicked), 130.0f, 38.0f, bHasLoadedRuntimeState && !LoadedSaveState.RuntimeState.bPaused)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Step Tick"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleStepSimulationClicked), 140.0f, 38.0f, bHasLoadedRuntimeState)]
        + SHorizontalBox::Slot().AutoWidth()
        [BuildButton(TEXT("Save"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSaveRuntimeStateClicked), 120.0f, 38.0f, bHasLoadedRuntimeState && !LoadedSavePath.IsEmpty())]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Control Desks"), TEXT("Open the main simulation systems from the computer."))];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Policies"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenPoliciesClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Budget / Economy"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenBudgetClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Resources"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenResourceChainsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Events"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenEventsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
        ]
        + SHorizontalBox::Slot().FillWidth(0.5f).Padding(6.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Advisors / Warnings"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenAdvisorWarningsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Departments"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenDepartmentsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Development"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenDevelopmentClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Demographics"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenDemographicsClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Reports"), ReportsSummary)];
    Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.5f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Approval Causes"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenApprovalStabilityClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Decision History"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenDecisionHistoryClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
        ]
        + SHorizontalBox::Slot().FillWidth(0.5f).Padding(6.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Meeting Reports"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenMeetingAdvisorClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Press Office"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleOpenPressReleaseClicked), 300.0f, 40.0f, bHasLoadedRuntimeState)]
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Current Reports"), TEXT("Snapshot of every major simulation channel."))];
    Body->AddSlot().MaxHeight(300.0f).Padding(0.0f, 4.0f)
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Policies"), BuildPolicyStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Budget"), BuildEconomyBudgetStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Resources"), BuildResourceChainStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Events"), BuildEventStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Advisors"), BuildAdvisorWarningText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Departments"), BuildDepartmentStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Reports"), BuildDecisionHistoryStatusText())]
        ]
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Open Settings"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSettingsClicked), 180.0f, 40.0f)]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [BuildButton(TEXT("Return To Menu"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleExitOfficeClicked), 190.0f, 40.0f)]
        + SHorizontalBox::Slot().AutoWidth()
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 140.0f, 40.0f)]
    ];

    return BuildPanel(TEXT("Simulation Dashboard"), TEXT("Computer control surface for policies, budget, resources, events, advisors, departments, and reports."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}


TSharedRef<SWidget> ALoginHUD::BuildOfficePoliciesScreen()
{
    const FDemocracyPolicyState& Policies = LoadedSaveState.RuntimeState.PlayerCountry.Policies;

    return BuildPanel(TEXT("Policy Desk"), TEXT("Choose active laws and national policies. Effects apply on every simulation tick."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().MaxHeight(470.0f).Padding(0.0f, 4.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildInfoRow(TEXT("Current Platform"), BuildPolicyStatusText())]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
                [BuildInfoRow(TEXT("Economic Policy"), TEXT("Controls treasury, approval, economy, infrastructure, and production pressure."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Balanced Budget"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Economic")), FString(TEXT("Balanced Budget"))), 420.0f, 38.0f, !Policies.EconomicPolicy.Equals(TEXT("Balanced Budget"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Stimulus Spending"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Economic")), FString(TEXT("Stimulus Spending"))), 420.0f, 38.0f, !Policies.EconomicPolicy.Equals(TEXT("Stimulus Spending"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Austerity Program"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Economic")), FString(TEXT("Austerity Program"))), 420.0f, 38.0f, !Policies.EconomicPolicy.Equals(TEXT("Austerity Program"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Industrial Subsidies"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Economic")), FString(TEXT("Industrial Subsidies"))), 420.0f, 38.0f, !Policies.EconomicPolicy.Equals(TEXT("Industrial Subsidies"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Environmental Policy"), TEXT("Controls extraction, environment, water/food pressure, and stability."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Managed Development"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Environmental")), FString(TEXT("Managed Development"))), 420.0f, 38.0f, !Policies.EnvironmentalPolicy.Equals(TEXT("Managed Development"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Conservation Mandate"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Environmental")), FString(TEXT("Conservation Mandate"))), 420.0f, 38.0f, !Policies.EnvironmentalPolicy.Equals(TEXT("Conservation Mandate"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Extraction Expansion"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Environmental")), FString(TEXT("Extraction Expansion"))), 420.0f, 38.0f, !Policies.EnvironmentalPolicy.Equals(TEXT("Extraction Expansion"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Military Policy"), TEXT("Controls readiness, invasion risk, treasury cost, and domestic pressure."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Defensive Readiness"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Military")), FString(TEXT("Defensive Readiness"))), 420.0f, 38.0f, !Policies.MilitaryPolicy.Equals(TEXT("Defensive Readiness"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("National Mobilization"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Military")), FString(TEXT("National Mobilization"))), 420.0f, 38.0f, !Policies.MilitaryPolicy.Equals(TEXT("National Mobilization"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Demilitarization"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Military")), FString(TEXT("Demilitarization"))), 420.0f, 38.0f, !Policies.MilitaryPolicy.Equals(TEXT("Demilitarization"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Diplomacy Policy"), TEXT("Controls diplomatic standing, alliances, border risk, and foreign pressure."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Neutral Engagement"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Diplomacy")), FString(TEXT("Neutral Engagement"))), 420.0f, 38.0f, !Policies.DiplomacyPolicy.Equals(TEXT("Neutral Engagement"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Alliance Outreach"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Diplomacy")), FString(TEXT("Alliance Outreach"))), 420.0f, 38.0f, !Policies.DiplomacyPolicy.Equals(TEXT("Alliance Outreach"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Hardline Sovereignty"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Diplomacy")), FString(TEXT("Hardline Sovereignty"))), 420.0f, 38.0f, !Policies.DiplomacyPolicy.Equals(TEXT("Hardline Sovereignty"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [BuildInfoRow(TEXT("Civil Policy"), TEXT("Controls approval, unrest, stability, legitimacy, and emergency authority."))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Public Stability"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Civil")), FString(TEXT("Public Stability"))), 420.0f, 38.0f, !Policies.CivilPolicy.Equals(TEXT("Public Stability"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Civil Liberties"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Civil")), FString(TEXT("Civil Liberties"))), 420.0f, 38.0f, !Policies.CivilPolicy.Equals(TEXT("Civil Liberties"), ESearchCase::IgnoreCase))]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [BuildButton(TEXT("Emergency Powers"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetPolicy, FString(TEXT("Civil")), FString(TEXT("Emergency Powers"))), 420.0f, 38.0f, !Policies.CivilPolicy.Equals(TEXT("Emergency Powers"), ESearchCase::IgnoreCase))]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)], 760.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeEventsScreen()
{
    FDemocracyEventSystemState& EventSystem = LoadedSaveState.RuntimeState.EventSystem;
    TSharedRef<SVerticalBox> EventList = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Event Status"), BuildEventStatusText())];

    bool bHasPendingEvent = false;
    for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
    {
        if (Event.bResolved)
        {
            continue;
        }

        bHasPendingEvent = true;
        EventList->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
        [BuildInfoRow(Event.Title, FString::Printf(TEXT("%s | Severity %d | %s\n%s"), *Event.EventType, Event.Severity, *Event.TriggerReason, *Event.Description))];

        for (const FDemocracyEventChoiceState& Choice : Event.Choices)
        {
            EventList->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(Choice.Label, FOnClicked::CreateUObject(this, &ALoginHUD::HandleResolveEventChoice, Event.EventId, Choice.ChoiceId), 520.0f, 38.0f)];
            EventList->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 6.0f)
            [BuildInfoRow(Choice.Description, Choice.ConsequencePreview)];
        }
    }

    if (!bHasPendingEvent)
    {
        EventList->AddSlot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("No Pending Events"), TEXT("Events can be triggered by shortages, protests, border pressure, or random shocks during simulation ticks."))];
    }

    EventList->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Event Desk"), TEXT("Random and triggered events require choices with direct simulation consequences."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [EventList], 780.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeBudgetScreen()
{
    const FDemocracyEconomyBudgetState& Budget = LoadedSaveState.RuntimeState.EconomyBudget;
    return BuildPanel(TEXT("Budget Desk"), TEXT("Taxes, spending, debt, inflation placeholder, public services, and production."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildInfoRow(TEXT("Current Budget"), BuildEconomyBudgetStatusText())]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
            [BuildInfoRow(TEXT("Tax Policy"), TEXT("Taxes raise income but can reduce approval and economic activity when too high."))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Low Taxes"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetTaxPolicy, FString(TEXT("Low Taxes"))), 360.0f, 38.0f, !Budget.TaxPolicy.Equals(TEXT("Low Taxes"), ESearchCase::IgnoreCase))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Balanced Taxation"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetTaxPolicy, FString(TEXT("Balanced Taxation"))), 360.0f, 38.0f, !Budget.TaxPolicy.Equals(TEXT("Balanced Taxation"), ESearchCase::IgnoreCase))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("High Taxes"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetTaxPolicy, FString(TEXT("High Taxes"))), 360.0f, 38.0f, !Budget.TaxPolicy.Equals(TEXT("High Taxes"), ESearchCase::IgnoreCase))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
            [BuildInfoRow(TEXT("Spending Posture"), TEXT("Spending choices shift services, infrastructure, readiness, deficits, and debt."))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Austerity"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetSpendingPosture, FString(TEXT("Austerity"))), 360.0f, 38.0f, !Budget.SpendingPosture.Equals(TEXT("Austerity"), ESearchCase::IgnoreCase))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Balanced Services"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetSpendingPosture, FString(TEXT("Balanced Services"))), 360.0f, 38.0f, !Budget.SpendingPosture.Equals(TEXT("Balanced Services"), ESearchCase::IgnoreCase))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Public Services"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetSpendingPosture, FString(TEXT("Public Services"))), 360.0f, 38.0f, !Budget.SpendingPosture.Equals(TEXT("Public Services"), ESearchCase::IgnoreCase))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Infrastructure Push"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetSpendingPosture, FString(TEXT("Infrastructure Push"))), 360.0f, 38.0f, !Budget.SpendingPosture.Equals(TEXT("Infrastructure Push"), ESearchCase::IgnoreCase))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(TEXT("Defense Funding"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetSpendingPosture, FString(TEXT("Defense Funding"))), 360.0f, 38.0f, !Budget.SpendingPosture.Equals(TEXT("Defense Funding"), ESearchCase::IgnoreCase))]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
            [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)]
        ], 780.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeDepartmentsScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializeDefaultDepartments(LoadedSaveState.RuntimeState);
        RecalculateDepartments(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Department Summary"), BuildDepartmentStatusText())];

    if (bHasLoadedRuntimeState)
    {
        for (const FDemocracyDepartmentState& Department : LoadedSaveState.RuntimeState.Departments.Departments)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
            [BuildInfoRow(Department.DepartmentName, FString::Printf(TEXT("%s\n%s\nBudget %d | staffing %d | effectiveness %d | trust %d | priority %d\nAction: %s\nPolicy interface: %s\n%s"),
                *Department.MinisterTitle,
                *Department.Domain,
                Department.BudgetShare,
                Department.Staffing,
                Department.Effectiveness,
                Department.PublicTrust,
                Department.Priority,
                *Department.CurrentAction,
                *Department.PolicyInterface,
                *Department.AdvisorySummary))];

            Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [BuildButton(TEXT("Routine"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDepartmentAction, Department.DepartmentName, FString(TEXT("Routine Operations"))), 112.0f, 34.0f)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [BuildButton(TEXT("Focus"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDepartmentAction, Department.DepartmentName, FString(TEXT("Focused Initiative"))), 112.0f, 34.0f)]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [BuildButton(TEXT("Emergency"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDepartmentAction, Department.DepartmentName, FString(TEXT("Emergency Response"))), 132.0f, 34.0f)]
                + SHorizontalBox::Slot().AutoWidth()
                [BuildButton(TEXT("Reform"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDepartmentAction, Department.DepartmentName, FString(TEXT("Administrative Reform"))), 118.0f, 34.0f)]
            ];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Departments Desk"), TEXT("Domestic ministries are the action interface for policies, resources, services, and crisis response."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeDevelopmentScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializeDevelopmentSystemIfMissing(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Development Summary"), BuildDevelopmentStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Focus"), TEXT("Choose one long-term development track. The active focus progresses during simulation ticks if treasury and resources are available."))];

    if (bHasLoadedRuntimeState)
    {
        for (const FDemocracyDevelopmentTrackState& Track : LoadedSaveState.RuntimeState.DevelopmentSystem.Tracks)
        {
            const bool bIsActive = Track.TrackName.Equals(LoadedSaveState.RuntimeState.DevelopmentSystem.ActiveFocus, ESearchCase::IgnoreCase);
            Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [BuildInfoRow(Track.TrackName, FString::Printf(TEXT("%s\nLevel %d | progress %d/%d | project %s\nCost per tick: treasury %d, wood %d, metals %d, fuel %d\nUnlocks: %s"),
                *Track.StrategicBenefit,
                Track.Level,
                Track.Progress,
                Track.ProgressTarget,
                *Track.CurrentProject,
                Track.TreasuryCost,
                Track.WoodCost,
                Track.MetalsCost,
                Track.FuelCost,
                *FString::Join(Track.Unlocks, TEXT(", "))))];
            Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
            [BuildButton(bIsActive ? FString::Printf(TEXT("Active: %s"), *Track.TrackName) : FString::Printf(TEXT("Set Focus: %s"), *Track.TrackName), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSetDevelopmentFocus, Track.TrackName), 360.0f, 36.0f, !bIsActive)];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Development Desk"), TEXT("Long-term technology and national development goals."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeApprovalStabilityScreen()
{
    if (bHasLoadedRuntimeState)
    {
        RecalculateApprovalStability(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Cause Summary"), BuildApprovalStabilityStatusText())];

    if (bHasLoadedRuntimeState)
    {
        for (const FDemocracyApprovalCauseState& Cause : LoadedSaveState.RuntimeState.ApprovalStability.Causes)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [BuildInfoRow(Cause.CauseName, FString::Printf(TEXT("%s | severity %d | approval %+d | unrest %+d | stability %+d\nMetric: %s\nStatus: %s\nResponses: %s"),
                *Cause.Category,
                Cause.Severity,
                Cause.ApprovalImpact,
                Cause.UnrestImpact,
                Cause.StabilityImpact,
                *Cause.SourceMetric,
                *Cause.CurrentStatus,
                *FString::Join(Cause.SuggestedResponses, TEXT(", "))))];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Approval Causes"), TEXT("Breakdown of why approval, unrest, and stability are moving."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeDecisionHistoryScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializeDecisionHistoryIfMissing(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Decision Continuity"), BuildDecisionHistoryStatusText())];

    if (bHasLoadedRuntimeState)
    {
        const TArray<FDemocracyDecisionRecordState>& Records = LoadedSaveState.RuntimeState.DecisionHistory.Records;
        for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
        {
            const FDemocracyDecisionRecordState& Record = Records[Index];
            Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [BuildInfoRow(FString::Printf(TEXT("Turn %d - %s"), Record.Turn, *Record.DecisionTitle), FString::Printf(TEXT("%s | severity %d | %s\nDecision: %s\nConsequence: %s\nAfter: approval %d, stability %d, unrest %d, treasury %d, economy %d, military %d\nTags: %s"),
                *Record.Category,
                Record.Severity,
                *Record.TimestampUtc,
                *Record.DecisionDetail,
                *Record.ConsequenceSummary,
                Record.ApprovalAfter,
                Record.StabilityAfter,
                Record.UnrestAfter,
                Record.TreasuryAfter,
                Record.EconomyAfter,
                Record.MilitaryAfter,
                *FString::Join(Record.Tags, TEXT(", "))))];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Decision History"), TEXT("Major choices and consequences for ongoing briefings, advisors, and save continuity."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 900.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeResourceChainsScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Resource Chain Summary"), BuildResourceChainStatusText())];

    if (bHasLoadedRuntimeState)
    {
        for (const FDemocracyResourceChainEntry& Entry : LoadedSaveState.RuntimeState.ResourceChains.Chains)
        {
            Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
            [BuildInfoRow(Entry.ResourceName, FString::Printf(TEXT("%s\nReserve %d/%d | production %d | consumption %d | imports %d | exports %d | shortage %d | surplus %d | strategic value %d\nDrivers: %s"),
                *Entry.Status,
                Entry.Reserve,
                Entry.ReserveTarget,
                Entry.Production,
                Entry.Consumption,
                Entry.Imports,
                Entry.Exports,
                Entry.Shortage,
                Entry.Surplus,
                Entry.StrategicValue,
                *FString::Join(Entry.Drivers, TEXT(", "))))];
        }
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Resource Chains"), TEXT("Production, consumption, shortages, imports, exports, reserves, and strategic role."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 820.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeDemographicsScreen()
{
    const FDemocracyDemographicsState& Demographics = LoadedSaveState.RuntimeState.Demographics;
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("National Demographics"), BuildDemographicsStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Citizen Groups"), TEXT("Approval, needs, and unrest sources by group."))];

    for (const FDemocracyCitizenGroupState& Group : Demographics.CitizenGroups)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(Group.GroupName, FString::Printf(TEXT("share %d%% | approval %d | unrest pressure %d | needs F%d W%d J%d S%d H%d\nSources: %s"),
            Group.PopulationShare,
            Group.Approval,
            Group.UnrestPressure,
            Group.NeedFood,
            Group.NeedWater,
            Group.NeedJobs,
            Group.NeedSecurity,
            Group.NeedHealthcare,
            *FString::Join(Group.UnrestSources, TEXT(", "))))];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
    [BuildInfoRow(TEXT("Regions"), TEXT("Regional approval, access, stability, and unrest."))];

    for (const FDemocracyRegionState& Region : Demographics.Regions)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(Region.RegionName, FString::Printf(TEXT("%s | share %d%% | approval %d | stability %d | unrest %d | access F%d W%d jobs %d security %d infra %d\nSources: %s"),
            *Region.Climate,
            Region.PopulationShare,
            Region.Approval,
            Region.Stability,
            Region.Unrest,
            Region.FoodAccess,
            Region.WaterAccess,
            Region.Jobs,
            Region.Security,
            Region.Infrastructure,
            *FString::Join(Region.UnrestSources, TEXT(", "))))];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Demographics Desk"), TEXT("Citizen groups, regions, needs, approval, and unrest sources."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 860.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeWorldRtsScreen()
{
    return BuildPanel(TEXT("World RTS Command"), TEXT("Prototype globe entry point for country-vs-country strategy."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("World"), bHasLoadedRuntimeState ? FString::Printf(TEXT("%d continents | %d countries"), LoadedSaveState.RuntimeState.WorldMap.Continents.Num(), LoadedSaveState.RuntimeState.WorldMap.TotalCountryCount) : TEXT("Unavailable"))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("RTS Layer"), TEXT("Placeholder for map camera, army control, country borders, diplomatic factions, and invasion state."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Readiness"), BuildSimulationStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)], 620.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficeAdvisorWarningsScreen()
{
    return BuildPanel(TEXT("Secure Phone"), TEXT("Prototype advisor warning feed from the loaded runtime state."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Advisor Warnings"), BuildAdvisorWarningText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Simulation"), BuildSimulationStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)], 620.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildOfficeMeetingAdvisorScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializeMeetingSystemIfMissing(LoadedSaveState.RuntimeState);
    }

    const FString AdvisorName = SelectedMeetingAdvisorName.IsEmpty() ? TEXT("Meeting Advisor") : SelectedMeetingAdvisorName;
    const FString AdvisorFocus = SelectedMeetingAdvisorFocus.IsEmpty() ? TEXT("General advisor meeting logic.") : SelectedMeetingAdvisorFocus;
    const FString CurrentState = bHasLoadedRuntimeState ? LoadedSaveSummary : TEXT("No runtime state loaded.");
    const bool bDiplomacyAdvisor = AdvisorName.Equals(TEXT("Diplomacy Advisor"), ESearchCase::IgnoreCase);

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("State"), CurrentState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Focus"), AdvisorFocus)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Meeting System"), BuildMeetingSystemStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Current Signals"), BuildSimulationStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Advisor Agenda"), TEXT("Choose an agenda. Meetings immediately update the simulation and are logged into decision history."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Situation Briefing"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Advisor")), AdvisorName, FString(TEXT("Situation Briefing"))), 380.0f, 38.0f, bHasLoadedRuntimeState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Focused Action Plan"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Advisor")), AdvisorName, FString(TEXT("Focused Action Plan"))), 380.0f, 38.0f, bHasLoadedRuntimeState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Emergency Response"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Advisor")), AdvisorName, FString(TEXT("Emergency Response"))), 380.0f, 38.0f, bHasLoadedRuntimeState)];

    if (bDiplomacyAdvisor)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 4.0f)
        [BuildInfoRow(TEXT("Foreign Officials"), TEXT("Use diplomatic agendas to affect foreign trust, diplomacy, trade, and invasion pressure."))];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Trade Delegation"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Foreign Official")), FString(TEXT("Trade Delegation")), FString(TEXT("Trade Delegation"))), 380.0f, 38.0f, bHasLoadedRuntimeState)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("De-escalation Talks"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Foreign Official")), FString(TEXT("Border Envoy")), FString(TEXT("De-escalation Talks"))), 380.0f, 38.0f, bHasLoadedRuntimeState)];
        Body->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Alliance Outreach"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleHoldMeeting, FString(TEXT("Foreign Official")), FString(TEXT("Allied Delegate")), FString(TEXT("Alliance Outreach"))), 380.0f, 38.0f, bHasLoadedRuntimeState)];
    }

    Body->AddSlot().AutoHeight().Padding(0.0f, 12.0f)
    [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(AdvisorName, TEXT("Advisor and foreign official meeting agendas."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 760.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildOfficePressReleaseScreen()
{
    if (bHasLoadedRuntimeState)
    {
        InitializePressOfficeIfMissing(LoadedSaveState.RuntimeState);
    }

    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Audience"), TEXT("State citizens, foreign officials, press corps, and global observers."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("State"), bHasLoadedRuntimeState ? LoadedSaveSummary : TEXT("No runtime state loaded."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Press Office"), BuildPressOfficeStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Current Signals"), BuildSimulationStatusText())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
        [BuildInfoRow(TEXT("Announcements"), TEXT("Truthful, relevant announcements can calm unrest and improve standing. Empty or false statements reduce credibility, especially when repeated."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Crisis Reassurance"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMakePressRelease, FString(TEXT("Crisis Reassurance"))), 360.0f, 38.0f, bHasLoadedRuntimeState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Policy Explanation"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMakePressRelease, FString(TEXT("Policy Explanation"))), 360.0f, 38.0f, bHasLoadedRuntimeState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Diplomatic Address"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMakePressRelease, FString(TEXT("Diplomatic Address"))), 360.0f, 38.0f, bHasLoadedRuntimeState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Victory Claim"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMakePressRelease, FString(TEXT("Victory Claim"))), 360.0f, 38.0f, bHasLoadedRuntimeState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("Empty Statement"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMakePressRelease, FString(TEXT("Empty Statement"))), 360.0f, 38.0f, bHasLoadedRuntimeState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildButton(TEXT("False Claim"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleMakePressRelease, FString(TEXT("False Claim"))), 360.0f, 38.0f, bHasLoadedRuntimeState)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Close"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleCloseOfficeOverlayClicked), 160.0f, 40.0f)];

    return BuildPanel(TEXT("Press Release Podium"), TEXT("Public announcements with credibility, diplomacy, approval, and unrest effects."),
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [Body], 760.0f);
}
TSharedRef<SWidget> ALoginHUD::BuildGameOverScreen()
{
    return BuildPanel(TEXT("Game Over"), GameOverReason.IsEmpty() ? TEXT("The administration has fallen.") : GameOverReason,
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Reason"), GameOverDetails.IsEmpty() ? TEXT("No details recorded.") : GameOverDetails)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Last Save"), LoadedSavePath.IsEmpty() ? TEXT("No local save path is loaded.") : LoadedSavePath)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
        [BuildButton(TEXT("Reload Previous Save"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleReloadPreviousSaveClicked), 280.0f, 46.0f, !LoadedSavePath.IsEmpty())]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [BuildButton(TEXT("Back To Saves"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLocalSavesClicked), 220.0f, 42.0f)], 680.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildMultiplayerStateSelectionScreen()
{
    return BuildPanel(TEXT("Online States"), TEXT("Server-owned game states tied to the signed-in account."),
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("Security Rule"), TEXT("Multiplayer saves are not stored locally; game servers provide account-linked state data."))]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
        [BuildButton(TEXT("Account State Alpha"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectOnlineState, FString(TEXT("Account State Alpha"))), 420.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Account State Beta"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectOnlineState, FString(TEXT("Account State Beta"))), 420.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
        [BuildButton(TEXT("Refresh From Game Server"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleRefreshOnlineStatesClicked), 420.0f, 46.0f)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f)
        [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToModeSelectionClicked), 180.0f, 44.0f)]);
}

TSharedRef<SWidget> ALoginHUD::BuildServerSelectionScreen()
{
    TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

    Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
    [BuildInfoRow(TEXT("Selected State"), SelectedOnlineState.IsEmpty() ? TEXT("None") : SelectedOnlineState)];

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
    [
        SNew(SEditableTextBox)
        .HintText(BodyText(TEXT("Search servers, regions, or rulesets")))
        .Text(BodyText(ServerSearchText))
        .OnTextChanged(FOnTextChanged::CreateUObject(this, &ALoginHUD::HandleServerSearchChanged))
    ];

    Body->AddSlot().AutoHeight().Padding(0.0f, 8.0f)
    [
        SNew(SCheckBox)
        .IsChecked(bShowRecommendedServersOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
        .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(this, &ALoginHUD::HandleRecommendedServersChanged))
        [
            SNew(STextBlock)
            .Text(BodyText(TEXT("Recommended ping only")))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
            .ColorAndOpacity(FLinearColor::White)
        ]
    ];

    TSharedRef<SScrollBox> ServerList = SNew(SScrollBox);
    const FString SearchLower = ServerSearchText.ToLower();
    int32 VisibleServers = 0;

    for (const FPlaceholderServer& Server : GetPlaceholderServers())
    {
        const FString SearchTarget = FString::Printf(TEXT("%s %s %s"), *Server.Name, *Server.Region, *Server.Ruleset).ToLower();
        const bool bMatchesSearch = SearchLower.IsEmpty() || SearchTarget.Contains(SearchLower);
        const bool bMatchesRecommended = !bShowRecommendedServersOnly || Server.PingMs <= 80;

        if (!bMatchesSearch || !bMatchesRecommended)
        {
            continue;
        }

        ++VisibleServers;
        const FString Label = FString::Printf(TEXT("%s  |  %s  |  %d ms"), *Server.Name, *Server.Region, Server.PingMs);
        const FString Detail = FString::Printf(TEXT("Ruleset: %s. Placeholder row sorted by best ping."), *Server.Ruleset);

        ServerList->AddSlot().Padding(0.0f, 4.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [BuildButton(Label, FOnClicked::CreateUObject(this, &ALoginHUD::HandleSelectServer, Server.Name), 520.0f, 46.0f)]
            + SVerticalBox::Slot().AutoHeight()
            [BuildInfoRow(TEXT("Server Details"), Detail)]
        ];
    }

    if (VisibleServers == 0)
    {
        ServerList->AddSlot().Padding(0.0f, 4.0f)
        [BuildInfoRow(TEXT("No Servers Found"), TEXT("Adjust search or filters. Live data will replace placeholders later."))];
    }

    Body->AddSlot().MaxHeight(310.0f).Padding(0.0f, 8.0f)
    [ServerList];
    Body->AddSlot().AutoHeight().Padding(0.0f, 14.0f)
    [BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToOnlineStatesClicked), 180.0f, 44.0f)];

    return BuildPanel(TEXT("Server Selection"), TEXT("Servers are displayed from best ping to worst ping."), Body);
}

TSharedRef<SWidget> ALoginHUD::BuildPanel(const FString& Title, const FString& Subtitle, const TSharedRef<SWidget>& Body, float Width)
{
    return SNew(SBox)
        .WidthOverride(Width)
        .MaxDesiredHeight(860.0f)
        [
            SNew(SBorder)
            .BorderImage(PanelBrush.Get())
            .Padding(28.0f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(Title))
                    .Justification(ETextJustify::Center)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 42))
                    .ColorAndOpacity(FLinearColor::White)
                    .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f))
                    .ShadowOffset(FVector2D(1.0f, 1.0f))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 20.0f)
                [
                    SNew(STextBlock)
                    .Text(BodyText(Subtitle))
                    .Justification(ETextJustify::Center)
                    .AutoWrapText(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                    .ColorAndOpacity(FLinearColor(0.84f, 0.86f, 0.88f, 1.0f))
                ]
                + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [Body]
                ]
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildButton(const FString& Label, FOnClicked ClickHandler, float Width, float Height, bool bEnabled) const
{
    return SNew(SBox)
        .WidthOverride(Width)
        .HeightOverride(Height)
        .HAlign(HAlign_Center)
        [
            SNew(SButton)
            .ButtonStyle(LoginButtonStyle.Get())
            .IsEnabled(bEnabled)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            .OnClicked(ClickHandler)
            [
                SNew(STextBlock)
                .Text(BodyText(Label))
                .Justification(ETextJustify::Center)
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
                .ColorAndOpacity(bEnabled ? FLinearColor::White : FLinearColor(0.48f, 0.50f, 0.53f, 1.0f))
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildBackButton()
{
    return BuildButton(TEXT("Back"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleBackToLoginClicked), 180.0f, 44.0f);
}

TSharedRef<SWidget> ALoginHUD::BuildInfoRow(const FString& Primary, const FString& Secondary) const
{
    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .Padding(FMargin(14.0f, 10.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(BodyText(Primary))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(BodyText(Secondary))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                .ColorAndOpacity(FLinearColor(0.78f, 0.81f, 0.84f, 1.0f))
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildSliderRow(const FString& Label, float Value, FOnFloatValueChanged ValueChanged) const
{
    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .Padding(FMargin(14.0f, 9.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s: %s"), *Label, *PercentText(Value))))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().FillWidth(0.58f).VAlign(VAlign_Center)
            [
                SNew(SSlider)
                .Value(Value)
                .OnValueChanged(ValueChanged)
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildCheckRow(const FString& Label, const FString& Detail, bool bChecked, FOnCheckStateChanged CheckChanged) const
{
    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .Padding(FMargin(14.0f, 9.0f))
        [
            SNew(SCheckBox)
            .IsChecked(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
            .OnCheckStateChanged(CheckChanged)
            [
                SNew(STextBlock)
                .Text(BodyText(FString::Printf(TEXT("%s - %s"), *Label, *Detail)))
                .AutoWrapText(true)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ];
}

TSharedRef<SWidget> ALoginHUD::BuildKeybindRow(const FString& ActionName, const FString& CurrentKey)
{
    return SNew(SBorder)
        .BorderImage(RowBrush.Get())
        .Padding(FMargin(14.0f, 9.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.50f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(BodyText(ActionName))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(BodyText(CurrentKey))
                .Justification(ETextJustify::Center)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
                .ColorAndOpacity(FLinearColor(0.86f, 0.89f, 0.92f, 1.0f))
            ]
            + SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center).HAlign(HAlign_Right)
            [BuildButton(TEXT("Change"), FOnClicked::CreateUObject(this, &ALoginHUD::HandleChangeKeybind, ActionName), 130.0f, 38.0f)]
        ];
}

TArray<FString> ALoginHUD::GetLocalSaveNames() const
{
    TArray<FString> SaveFiles;
    const FString SaveDirectory = FPaths::ProjectDir() / TEXT("Saves");

    IFileManager::Get().FindFiles(SaveFiles, *(SaveDirectory / TEXT("*.sav")), true, false);
    IFileManager::Get().FindFiles(SaveFiles, *(SaveDirectory / TEXT("*.json")), true, false);
    IFileManager::Get().FindFiles(SaveFiles, *(SaveDirectory / TEXT("*.democracy")), true, false);

    SaveFiles.Sort();
    return SaveFiles;
}

FString ALoginHUD::BuildSafeSaveFileName(const FString& StateName) const
{
    FString SafeName = StateName.TrimStartAndEnd();

    const TCHAR InvalidReplacement = TEXT('_');
    for (TCHAR& Character : SafeName)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
        {
            Character = InvalidReplacement;
        }
    }

    while (SafeName.Contains(TEXT("__")))
    {
        SafeName.ReplaceInline(TEXT("__"), TEXT("_"));
    }

    SafeName.TrimStartAndEndInline();
    if (SafeName.IsEmpty())
    {
        SafeName = TEXT("New_State");
    }

    return SafeName;
}

bool ALoginHUD::CreateInitialSinglePlayerSave(FString& OutSavePath)
{
    const FString CleanStateName = PendingStateName.TrimStartAndEnd();
    if (CleanStateName.IsEmpty() || PendingDifficulty.IsEmpty() || PendingClimate.IsEmpty() || PendingLeaderGender.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Single-player save creation failed: missing state name, difficulty, or climate."));
        return false;
    }

    const FString SaveDirectory = FPaths::ProjectDir() / TEXT("Saves");
    if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*SaveDirectory))
    {
        UE_LOG(LogTemp, Error, TEXT("Single-player save creation failed: could not create Saves directory at %s."), *SaveDirectory);
        return false;
    }

    const FString BaseFileName = BuildSafeSaveFileName(CleanStateName);
    FString SavePath = SaveDirectory / FString::Printf(TEXT("%s.democracy"), *BaseFileName);
    int32 DuplicateIndex = 2;

    while (FPaths::FileExists(SavePath))
    {
        SavePath = SaveDirectory / FString::Printf(TEXT("%s_%d.democracy"), *BaseFileName, DuplicateIndex++);
    }

    const FDifficultyProfile DifficultyProfile = FDifficultyProfileLibrary::GetProfile(PendingDifficulty);
    const FDemocracySimulationState InitialGameState = FDemocracyGameStateFactory::CreateInitialState(
        CleanStateName,
        PendingLeaderGender,
        PendingAddressTitle,
        PendingClimate,
        DifficultyProfile);
    const FString SaveId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    const FString CreatedAt = SaveTimestamp();
    const FString SaveContents = FString::Printf(
        TEXT("{\n")
        TEXT("  \"formatVersion\": 1,\n")
        TEXT("  \"saveId\": \"%s\",\n")
        TEXT("  \"stateName\": \"%s\",\n")
        TEXT("  \"mode\": \"SinglePlayer\",\n")
        TEXT("  \"difficulty\": \"%s\",\n")
        TEXT("  \"difficultyProfile\": %s,\n")
        TEXT("  \"climate\": \"%s\",\n")
        TEXT("  \"leaderGender\": \"%s\",\n")
        TEXT("  \"addressTitle\": \"%s\",\n")
        TEXT("  \"createdAtUtc\": \"%s\",\n")
        TEXT("  \"lastPlayedAtUtc\": \"%s\",\n")
        TEXT("  \"initialGameState\": %s,\n")
        TEXT("  \"progress\": {\n")
        TEXT("    \"turn\": 1,\n")
        TEXT("    \"phase\": \"Initial Setup\"\n")
        TEXT("  }\n")
        TEXT("}\n"),
        *JsonEscape(SaveId),
        *JsonEscape(CleanStateName),
        *JsonEscape(DifficultyProfile.Name),
        *DifficultyProfile.ToJson(2),
        *JsonEscape(PendingClimate),
        *JsonEscape(PendingLeaderGender),
        *JsonEscape(PendingAddressTitle),
        *JsonEscape(CreatedAt),
        *JsonEscape(CreatedAt),
        *InitialGameState.ToJson(2));

    if (!FFileHelper::SaveStringToFile(SaveContents, *SavePath))
    {
        UE_LOG(LogTemp, Error, TEXT("Single-player save creation failed: could not write %s."), *SavePath);
        return false;
    }

    OutSavePath = SavePath;
    LocalSaveSearchText.Empty();

    if (!LoadSinglePlayerSaveIntoRuntime(SavePath))
    {
        return false;
    }

    FString ProtectionError;
    if (FDemocracySaveGameRuntime::SaveSinglePlayerRuntimeState(LoadedSaveState, ProtectionError))
    {
        FDemocracySaveGameRuntime::SaveSinglePlayerAutosave(LoadedSaveState, ProtectionError);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Initial save protection setup failed: %s"), *ProtectionError);
    }

    UE_LOG(LogTemp, Log, TEXT("Single-player save created and loaded: %s"), *SavePath);
    return true;
}

bool ALoginHUD::LoadSinglePlayerSaveIntoRuntime(const FString& SavePath)
{
    FDemocracyLoadedSaveState LoadedSave;
    FString LoadError;
    if (!FDemocracySaveGameRuntime::LoadSinglePlayerSaveWithFallback(SavePath, LoadedSave, LoadError))
    {
        LoadedSaveError = LoadError;
        bHasLoadedRuntimeState = false;
        UE_LOG(LogTemp, Error, TEXT("Single-player save load failed: %s"), *LoadError);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 7.0f, FColor::Red, LoadError);
        }
        return false;
    }

    StopSimulationTimer();
    LoadedSaveState = LoadedSave;
    SimulationTickCount = 0;
    LoadedStateName = LoadedSave.StateName;
    LoadedSavePath = LoadedSave.SavePath;
    LoadedSaveSummary = LoadedSave.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();
    LoadedSaveError.Empty();
    LastSaveStatus = LoadError.IsEmpty() ? TEXT("Loaded local save into runtime.") : LoadError;
    GameOverReason.Empty();
    GameOverDetails.Empty();
    bHasLoadedRuntimeState = true;
    OpeningScriptText.Empty();
    bShowFirstLoginBriefing = false;
    LoadedSaveState.RuntimeState.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(LoadedSaveState.RuntimeState.PlayerCountry.CountrySizeScore);
    LoadedSaveState.RuntimeState.AdvisorSystem.LastUpdatedTurn = LoadedSaveState.RuntimeState.Turn;
    if (LoadedSaveState.RuntimeState.AdvisorSystem.AdvisorCount <= 0)
    {
        LoadedSaveState.RuntimeState.AdvisorSystem.AdvisorCount = FMath::Clamp(6 - LoadedSaveState.RuntimeState.PlayerCountry.CountrySizeScore, 1, 5);
    }
    if (LoadedSaveState.RuntimeState.ResourceChains.Chains.Num() == 0)
    {
        FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
        const FDemocracyResourceInventory& Resources = State.PlayerCountry.Resources;
        const int32 DifficultyScore = FMath::Clamp(State.PlayerCountry.CountrySizeScore, 1, 4);
        State.ResourceChains.LastUpdatedTurn = State.Turn;
        State.ResourceChains.TotalShortagePressure = 0;
        State.ResourceChains.TradeBalance = 0;
        State.ResourceChains.Summary = TEXT("Resource chain initialized from existing save reserves. Step simulation to calculate live production, consumption, imports, and exports.");
        State.ResourceChains.Chains = {
            MakeResourceChainEntry(TEXT("Food"), Resources.Food, 150 + DifficultyScore * 18, 0, 0, 0, 0, 90, TEXT("Population food access; shortages raise unrest fastest."), { TEXT("loaded reserve"), TEXT("agriculture") }),
            MakeResourceChainEntry(TEXT("Water"), Resources.Water, 130 + DifficultyScore * 15, 0, 0, 0, 0, 95, TEXT("Public health and regional stability; shortages damage demographics."), { TEXT("loaded reserve"), TEXT("public services") }),
            MakeResourceChainEntry(TEXT("Fuel"), Resources.GasOil, 80 + DifficultyScore * 14, 0, 0, 0, 0, 100, TEXT("Logistics, industry, military readiness, and inflation pressure."), { TEXT("loaded reserve"), TEXT("trade") }),
            MakeResourceChainEntry(TEXT("Wood"), Resources.Wood, 70 + DifficultyScore * 10, 0, 0, 0, 0, 60, TEXT("Construction, infrastructure repairs, and disaster recovery."), { TEXT("loaded reserve"), TEXT("forestry") }),
            MakeResourceChainEntry(TEXT("Metals"), Resources.Metals, 75 + DifficultyScore * 12, 0, 0, 0, 0, 85, TEXT("Industry, infrastructure projects, and military production."), { TEXT("loaded reserve"), TEXT("mining") })
        };
    }
    InitializeDefaultDepartments(LoadedSaveState.RuntimeState);
    RecalculateDepartments(LoadedSaveState.RuntimeState);
    RecalculateApprovalStability(LoadedSaveState.RuntimeState);
    InitializePressOfficeIfMissing(LoadedSaveState.RuntimeState);
    InitializeMeetingSystemIfMissing(LoadedSaveState.RuntimeState);
    LoadedSaveState.RuntimeState.AdvisorSystem.Reports = GenerateAdvisorReports(LoadedSaveState.RuntimeState);
    RecalculateDemographics(LoadedSaveState.RuntimeState);
    if (LoadedSaveState.RuntimeState.EventSystem.ActiveEventLimit <= 0)
    {
        LoadedSaveState.RuntimeState.EventSystem.ActiveEventLimit = 3;
    }

    UE_LOG(LogTemp, Log, TEXT("Single-player runtime state ready: %s"), *LoadedSaveSummary);
    return true;
}

FString ALoginHUD::BuildSimulationStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return LoadedSaveError.IsEmpty() ? TEXT("No runtime state is loaded.") : LoadedSaveError;
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyCountryState& Country = State.PlayerCountry;
    return FString::Printf(
        TEXT("Tick %d | Turn %d | %s | Approval %d | Stability %d | Unrest %d | Treasury %d | Economy %d | Diplomacy %d | Military %d | %s"),
        SimulationTickCount,
        State.Turn,
        State.bPaused ? TEXT("Paused") : TEXT("Running"),
        Country.PublicApproval,
        Country.Stability,
        Country.Unrest,
        Country.Treasury,
        Country.EconomicHealth,
        Country.DiplomaticStanding,
        Country.MilitaryReadiness,
        *State.Phase);
}

FString ALoginHUD::BuildResourceStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No resources loaded.");
    }

    const FDemocracyResourceInventory& Resources = LoadedSaveState.RuntimeState.PlayerCountry.Resources;
    return FString::Printf(
        TEXT("Food %d | Gas/Oil %d | Wood %d | Metals %d | Water %d"),
        Resources.Food,
        Resources.GasOil,
        Resources.Wood,
        Resources.Metals,
        Resources.Water);
}


FString ALoginHUD::BuildResourceChainStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No resource chain loaded.");
    }
    return BuildResourceChainSummaryText(LoadedSaveState.RuntimeState.ResourceChains);
}

FString ALoginHUD::BuildPolicyStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No policies loaded.");
    }

    const FDemocracyPolicyState& Policies = LoadedSaveState.RuntimeState.PlayerCountry.Policies;
    const FString EffectsText = Policies.ActivePolicyEffects.Num() > 0
        ? FString::Join(Policies.ActivePolicyEffects, TEXT("\n"))
        : JoinPolicyEffects(Policies);

    return FString::Printf(
        TEXT("Economic: %s\nEnvironment: %s\nMilitary: %s\nDiplomacy: %s\nCivil: %s\nChanges: %d\nLast: %s\n\nActive effects:\n%s"),
        *Policies.EconomicPolicy,
        *Policies.EnvironmentalPolicy,
        *Policies.MilitaryPolicy,
        *Policies.DiplomacyPolicy,
        *Policies.CivilPolicy,
        Policies.PolicyChangeCount,
        *Policies.LastPolicyChangeSummary,
        *EffectsText);
}
FString ALoginHUD::BuildEconomyBudgetStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No economy budget loaded.");
    }
    return BuildEconomyBudgetSummaryText(LoadedSaveState.RuntimeState.EconomyBudget);
}
FString ALoginHUD::BuildDemographicsStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No demographics loaded.");
    }
    return BuildDemographicsSummaryText(LoadedSaveState.RuntimeState.Demographics);
}
FString ALoginHUD::BuildEventStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No event system loaded.");
    }

    TArray<FString> Lines;
    const FDemocracyEventSystemState& EventSystem = LoadedSaveState.RuntimeState.EventSystem;
    Lines.Add(BuildEventSummaryText(EventSystem));
    for (const FDemocracyActiveEventState& Event : EventSystem.ActiveEvents)
    {
        if (!Event.bResolved)
        {
            Lines.Add(FString::Printf(TEXT("%s: %s | Severity %d"), *Event.EventType, *Event.Title, Event.Severity));
        }
    }
    return FString::Join(Lines, TEXT("\n"));
}
FString ALoginHUD::BuildDepartmentStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No departments loaded.");
    }
    return BuildDepartmentSummaryText(LoadedSaveState.RuntimeState.Departments);
}
FString ALoginHUD::BuildDevelopmentStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No development system loaded.");
    }
    return BuildDevelopmentSummaryText(LoadedSaveState.RuntimeState.DevelopmentSystem);
}
FString ALoginHUD::BuildApprovalStabilityStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No approval/stability cause model loaded.");
    }
    return BuildApprovalStabilitySummaryText(LoadedSaveState.RuntimeState.ApprovalStability);
}
FString ALoginHUD::BuildPressOfficeStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No press office loaded.");
    }
    return BuildPressOfficeSummaryText(LoadedSaveState.RuntimeState.PressOffice);
}
FString ALoginHUD::BuildMeetingSystemStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No meeting system loaded.");
    }
    return BuildMeetingSystemSummaryText(LoadedSaveState.RuntimeState.MeetingSystem);
}
FString ALoginHUD::BuildDecisionHistoryStatusText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No decision history loaded.");
    }
    return BuildDecisionHistorySummaryText(LoadedSaveState.RuntimeState.DecisionHistory);
}
FString ALoginHUD::BuildAdvisorWarningText() const
{
    if (!bHasLoadedRuntimeState)
    {
        return TEXT("No runtime state loaded.");
    }

    const FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyAdvisorSystemState& AdvisorSystem = State.AdvisorSystem;
    const FString GuidanceLevel = AdvisorSystem.GuidanceLevel.IsEmpty() ? AdvisorGuidanceForDifficultyScore(State.PlayerCountry.CountrySizeScore) : AdvisorSystem.GuidanceLevel;
    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Advisor guidance: %s | active advisors: %d | updated turn: %d"), *GuidanceLevel, AdvisorSystem.AdvisorCount, AdvisorSystem.LastUpdatedTurn));
    Lines.Add(RuntimeGuidanceSummary(State));

    if (AdvisorSystem.Reports.Num() > 0)
    {
        const int32 VisibleReports = FMath::Min(AdvisorSystem.Reports.Num(), VisibleAdvisorReportCount(AdvisorSystem, State.PlayerCountry));
        for (int32 Index = 0; Index < VisibleReports; ++Index)
        {
            AppendGuidedAdvisorReport(Lines, AdvisorSystem.Reports[Index], GuidanceLevel);
        }
        if (VisibleReports < AdvisorSystem.Reports.Num())
        {
            Lines.Add(FString::Printf(TEXT("\n%d lower-priority reports withheld by difficulty guidance."), AdvisorSystem.Reports.Num() - VisibleReports));
        }
    }
    else
    {
        Lines.Add(IsMinimalGuidance(GuidanceLevel) ? TEXT("No advisor signal available.") : TEXT("No structured advisor reports are available yet. Step the simulation once or create a new state."));
    }

    Lines.Add(TEXT("\nApproval / stability causes:"));
    Lines.Add(State.ApprovalStability.Summary);
    if (IsDetailedGuidance(GuidanceLevel))
    {
        Lines.Add(TEXT("Tutorial cue: top causes explain why approval, unrest, and stability moved. Fix high-severity causes first."));
    }

    Lines.Add(TEXT("\nRisk warnings:"));
    Lines.Add(TEXT("Internal stability risk:"));
    if (IsMinimalGuidance(GuidanceLevel))
    {
        Lines.Add(FString::Printf(TEXT("Assassination risk %d/%d."), State.FailureRisk.CurrentAssassinationRisk, State.FailureRisk.AssassinationRiskTrigger));
    }
    else
    {
        Lines.Append(State.FailureRisk.AdvisorWarnings);
        if (!IsLimitedGuidance(GuidanceLevel))
        {
            Lines.Append(State.FailureRisk.RecoveryTips);
        }
    }

    Lines.Add(TEXT("Foreign takeover risk:"));
    if (IsMinimalGuidance(GuidanceLevel))
    {
        Lines.Add(FString::Printf(TEXT("Takeover risk %d/%d."), State.InvasionRisk.CurrentInvasionRisk, State.InvasionRisk.InvasionRiskTrigger));
    }
    else
    {
        Lines.Append(State.InvasionRisk.AdvisorWarnings);
        if (!IsLimitedGuidance(GuidanceLevel))
        {
            Lines.Append(State.InvasionRisk.RecoveryTips);
        }
    }
    return FString::Join(Lines, TEXT("\n"));
}
void ALoginHUD::StartSimulationTimer()
{
    UWorld* World = GetWorld();
    if (!World || !bHasLoadedRuntimeState)
    {
        return;
    }

    LoadedSaveState.RuntimeState.bPaused = false;
    LoadedSaveState.RuntimeState.Phase = TEXT("Prototype Simulation Running");
    SimulationTickSummary = BuildSimulationStatusText();

    World->GetTimerManager().ClearTimer(SimulationTickTimerHandle);
    World->GetTimerManager().SetTimer(
        SimulationTickTimerHandle,
        this,
        &ALoginHUD::RunSimulationTick,
        5.0f,
        true);

    RefreshLoginWidget();
}

void ALoginHUD::StopSimulationTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SimulationTickTimerHandle);
    }

    if (bHasLoadedRuntimeState)
    {
        LoadedSaveState.RuntimeState.bPaused = true;
        LoadedSaveState.RuntimeState.Phase = TEXT("Prototype Simulation Paused");
        SimulationTickSummary = BuildSimulationStatusText();
    }
}

void ALoginHUD::RunSimulationTick()
{
    if (!bHasLoadedRuntimeState || LoadedSaveState.RuntimeState.bPaused)
    {
        return;
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyResourceInventory& Resources = Country.Resources;

    ++SimulationTickCount;
    ++State.RtsWorld.SimulationSecond;
    if (SimulationTickCount % 3 == 0)
    {
        ++State.Turn;
    }

    const int32 DifficultyScore = FMath::Clamp(Country.CountrySizeScore, 1, 4);
    const int32 FoodUse = 6 + DifficultyScore * 3;
    const int32 WaterUse = 5 + DifficultyScore * 2;
    const int32 GasUse = 1 + DifficultyScore;
    const int32 WoodUse = 2 + DifficultyScore;
    const int32 MetalsUse = 1 + DifficultyScore;

    const FPolicyTickModifiers PolicyModifiers = BuildPolicyModifiers(Country.Policies);
    RecalculateEconomyBudget(State);
    RecalculateResourceProductionChains(State, PolicyModifiers, FoodUse, WaterUse, GasUse, WoodUse, MetalsUse);

    const bool bFoodShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Food")) > 0;
    const bool bWaterShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Water")) > 0;
    const bool bFuelShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Fuel")) > 0 && DifficultyScore >= 3;
    const bool bWoodShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Wood")) > 0;
    const bool bMetalsShortage = GetResourceChainShortage(State.ResourceChains, TEXT("Metals")) > 0;
    RecalculateDepartments(State);
    TickDevelopmentSystem(State);
    const int32 DefenseEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Defense"));
    const int32 TreasuryEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Treasury"));
    const int32 AgricultureEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Agriculture"));
    const int32 EnergyEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Energy"));
    const int32 HealthEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Health"));
    const int32 EducationEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Education"));
    const int32 InfrastructureEffectiveness = GetDepartmentEffectiveness(State.Departments, TEXT("Infrastructure"));
    const int32 DepartmentSupport = FMath::Clamp(State.Departments.Coordination / 30, 0, 3);
    const int32 ShortagePressure = FMath::Clamp(State.ResourceChains.TotalShortagePressure / 18 + (bFoodShortage ? 1 : 0) + (bWaterShortage ? 1 : 0) - DepartmentSupport, 0, 12);
    Country.Treasury = FMath::Max(0, Country.Treasury + 8 - DifficultyScore * 3 - ShortagePressure + PolicyModifiers.TreasuryDelta + TreasuryEffectiveness / 35);
    Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + PolicyModifiers.EconomicDelta - ShortagePressure - (bFuelShortage ? 1 : 0) - (bMetalsShortage ? 1 : 0) + (Country.Treasury > 250 ? 1 : 0) + (TreasuryEffectiveness + EducationEffectiveness) / 70, 0, 100);
    Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + PolicyModifiers.DiplomacyDelta + (Country.PublicApproval > 65 ? 1 : 0) - (Country.Unrest > 60 ? 1 : 0), 0, 100);
    Country.Infrastructure = FMath::Clamp(Country.Infrastructure + PolicyModifiers.InfrastructureDelta - (bWoodShortage ? 1 : 0) + InfrastructureEffectiveness / 45, 0, 100);
    Country.EnvironmentalHealth = FMath::Clamp(Country.EnvironmentalHealth + PolicyModifiers.EnvironmentDelta, 0, 100);
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + 1 - ShortagePressure + PolicyModifiers.ApprovalDelta + (Country.EconomicHealth > 65 ? 1 : 0) + HealthEffectiveness / 60, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + (Country.PublicApproval >= 55 ? 1 : 0) - ShortagePressure + PolicyModifiers.StabilityDelta + (Country.DiplomaticStanding > 70 ? 1 : 0), 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + ShortagePressure + (Country.PublicApproval < 40 ? 1 : 0) - (Country.Stability > 65 ? 1 : 0) + PolicyModifiers.UnrestDelta, 0, 100);
    Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + (Resources.Metals > 80 ? 1 : 0) - (bFuelShortage ? 2 : 0) - (bMetalsShortage ? 1 : 0) + PolicyModifiers.MilitaryDelta + DefenseEffectiveness / 45, 0, 100);
    Resources.Food = FMath::Max(0, Resources.Food + AgricultureEffectiveness / 45);
    Resources.GasOil = FMath::Max(0, Resources.GasOil + EnergyEffectiveness / 50);
    RecalculateDemographics(State);
    Country.PublicApproval = FMath::Clamp((Country.PublicApproval + State.Demographics.AverageGroupApproval + State.Demographics.AverageRegionalApproval) / 3, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + FMath::Max(0, State.Demographics.DemographicUnrestPressure - 35) / 10 + FMath::Max(0, State.Demographics.NationalNeedsPressure - 35) / 12, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability - FMath::Max(0, State.Demographics.DemographicUnrestPressure - 45) / 12, 0, 100);
    RecalculateApprovalStability(State);
    const int32 CauseApprovalDelta = FMath::Clamp(State.ApprovalStability.NetApprovalPressure / 25, -4, 3);
    const int32 CauseUnrestDelta = FMath::Clamp(State.ApprovalStability.NetUnrestPressure / 22, -2, 5);
    const int32 CauseStabilityDelta = FMath::Clamp(State.ApprovalStability.NetStabilityPressure / 25, -4, 3);
    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + CauseApprovalDelta, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + CauseUnrestDelta, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + CauseStabilityDelta, 0, 100);
    RecalculateApprovalStability(State);

    State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(
        State.FailureRisk.CurrentAssassinationRisk + FMath::Max(0, Country.Unrest - 45) / 12 + FMath::Max(0, 45 - Country.Stability) / 12,
        0,
        State.FailureRisk.AssassinationRiskTrigger);
    State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(
        State.InvasionRisk.CurrentInvasionRisk + FMath::Max(0, 45 - Country.MilitaryReadiness) / 10 + FMath::Max(0, 45 - Country.DiplomaticStanding) / 12 + DifficultyScore - 1 + PolicyModifiers.InvasionRiskDelta,
        0,
        State.InvasionRisk.InvasionRiskTrigger);

    GenerateSimulationEvents(State);

    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(DifficultyScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    if (State.AdvisorSystem.AdvisorCount <= 0)
    {
        State.AdvisorSystem.AdvisorCount = FMath::Clamp(6 - DifficultyScore, 1, 5);
    }
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);

    State.Phase = TEXT("Prototype Simulation Running");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    SimulationTickSummary = BuildSimulationStatusText();

    UE_LOG(LogTemp, Log, TEXT("Simulation tick: %s | %s"), *SimulationTickSummary, *BuildResourceStatusText());

    if (EvaluateFailState())
    {
        return;
    }

    if (SimulationTickCount % 3 == 0 && !LoadedSavePath.IsEmpty())
    {
        FString AutosaveError;
        if (FDemocracySaveGameRuntime::SaveSinglePlayerAutosave(LoadedSaveState, AutosaveError))
        {
            LoadedSaveSummary = LoadedSaveState.ToSummaryText();
            LastSaveStatus = FString::Printf(TEXT("Autosaved protected runtime state at %s."), *LoadedSaveState.LastPlayedAtUtc);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Autosave failed: %s"), *AutosaveError);
        }
    }

    if (CurrentScreen == ELoginFlowScreen::LoadedGame || CurrentScreen == ELoginFlowScreen::OfficeOpeningBriefing || CurrentScreen == ELoginFlowScreen::OfficeDashboard || CurrentScreen == ELoginFlowScreen::OfficeComputerMenu || CurrentScreen == ELoginFlowScreen::OfficePolicies || CurrentScreen == ELoginFlowScreen::OfficeEvents || CurrentScreen == ELoginFlowScreen::OfficeDemographics || CurrentScreen == ELoginFlowScreen::OfficeBudget || CurrentScreen == ELoginFlowScreen::OfficeResourceChains || CurrentScreen == ELoginFlowScreen::OfficeDepartments || CurrentScreen == ELoginFlowScreen::OfficeDevelopment || CurrentScreen == ELoginFlowScreen::OfficeApprovalStability || CurrentScreen == ELoginFlowScreen::OfficeDecisionHistory || CurrentScreen == ELoginFlowScreen::OfficeWorldRts || CurrentScreen == ELoginFlowScreen::OfficeAdvisorWarnings || CurrentScreen == ELoginFlowScreen::OfficeMeetingAdvisor || CurrentScreen == ELoginFlowScreen::OfficePressRelease)
    {
        RefreshLoginWidget();
    }
}

bool ALoginHUD::EvaluateFailState()
{
    if (!bHasLoadedRuntimeState)
    {
        return false;
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    const FDemocracyCountryState& Country = State.PlayerCountry;

    const bool bAssassinationTriggered =
        State.FailureRisk.bGameOverOnAssassination &&
        (State.FailureRisk.CurrentAssassinationRisk >= State.FailureRisk.AssassinationRiskTrigger ||
            Country.Stability <= State.FailureRisk.StabilityCriticalThreshold ||
            Country.Unrest >= State.FailureRisk.UnrestCriticalThreshold);

    const bool bForeignTakeoverTriggered =
        State.InvasionRisk.bGameOverOnTakeover &&
        (State.InvasionRisk.CurrentInvasionRisk >= State.InvasionRisk.InvasionRiskTrigger ||
            Country.MilitaryReadiness <= State.InvasionRisk.MilitaryReadinessCriticalThreshold);

    if (!bAssassinationTriggered && !bForeignTakeoverTriggered)
    {
        return false;
    }

    StopSimulationTimer();
    State.bPaused = true;
    bInOfficeMode = false;

    if (bAssassinationTriggered)
    {
        GameOverReason = State.FailureRisk.GameOverReason;
        GameOverDetails = FString::Printf(
            TEXT("Stability %d, unrest %d, assassination risk %d/%d. Reload from the previous save to continue testing."),
            Country.Stability,
            Country.Unrest,
            State.FailureRisk.CurrentAssassinationRisk,
            State.FailureRisk.AssassinationRiskTrigger);
    }
    else
    {
        GameOverReason = State.InvasionRisk.GameOverReason;
        GameOverDetails = FString::Printf(
            TEXT("Military readiness %d, invasion risk %d/%d. Reload from the previous save to continue testing."),
            Country.MilitaryReadiness,
            State.InvasionRisk.CurrentInvasionRisk,
            State.InvasionRisk.InvasionRiskTrigger);
    }

    UE_LOG(LogTemp, Warning, TEXT("Game over triggered: %s - %s"), *GameOverReason, *GameOverDetails);
    ShowScreen(ELoginFlowScreen::GameOver);
    return true;
}

bool ALoginHUD::EnterOfficePrototype(bool bShowOpeningBriefing)
{
    UWorld* World = GetWorld();
    APlayerController* PlayerController = GetOwningPlayerController();
    if (!World || !PlayerController)
    {
        return false;
    }

    bInOfficeMode = true;
    TearDownLoginWidget();

    if (!OfficeLevelBuilder.IsValid())
    {
        OfficeLevelBuilder = World->SpawnActor<AOfficeLevelBuilder>(AOfficeLevelBuilder::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
    }

    if (OfficeLevelBuilder.IsValid())
    {
        OfficeLevelBuilder->BuildOffice();
    }

    if (!OfficePlayerPawn.IsValid())
    {
        OfficePlayerPawn = World->SpawnActor<AOfficePlayerPawn>(AOfficePlayerPawn::StaticClass(), FVector(0.0f, -420.0f, 90.0f), FRotator(0.0f, 90.0f, 0.0f));
    }

    if (!OfficePlayerPawn.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Office prototype failed: could not spawn office player pawn."));
        return false;
    }

    OfficePlayerPawn->SetActorLocation(FVector(0.0f, -420.0f, 90.0f));
    OfficePlayerPawn->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
    OfficePlayerPawn->SetInvertLookY(bInvertLookY);
    PlayerController->Possess(OfficePlayerPawn.Get());
    PlayerController->SetViewTarget(OfficePlayerPawn.Get());
    PlayerController->ClientSetViewTarget(OfficePlayerPawn.Get());
    PlayerController->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
    bOfficePrototypeSpawned = OfficeLevelBuilder.IsValid();

    PlayerController->bShowMouseCursor = bShowOpeningBriefing;
    if (bShowOpeningBriefing)
    {
        bShowFirstLoginBriefing = true;
    }
    if (bShowOpeningBriefing)
    {
        FInputModeGameAndUI InputMode;
        PlayerController->SetInputMode(InputMode);
        ShowScreen(ELoginFlowScreen::OfficeOpeningBriefing);
    }
    else
    {
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Green, TEXT("Office prototype loaded. WASD move, arrow keys look, E interact."));
    }

    return true;
}

FString ALoginHUD::GetAddressTitleForGender(const FString& GenderName) const
{
    if (GenderName.Equals(TEXT("Male"), ESearchCase::IgnoreCase))
    {
        return TEXT("Mr. President");
    }

    if (GenderName.Equals(TEXT("Female"), ESearchCase::IgnoreCase))
    {
        return TEXT("Miss President");
    }

    return TEXT("President");
}

FString ALoginHUD::GetOpeningScriptText() const
{
    FString ScriptText;
    const FString ScriptPath = FPaths::ProjectContentDir() / TEXT("Story/OpeningScript_FirstLogin.txt");

    if (FFileHelper::LoadFileToString(ScriptText, *ScriptPath))
    {
        return ScriptText;
    }

    return TEXT("Opening script missing. Expected Content/Story/OpeningScript_FirstLogin.txt.");
}

FReply ALoginHUD::HandleSignInClicked()
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: mock Sign In for '%s'. Routing directly to single-player saves for testing."), *MockUserName);
    ShowScreen(ELoginFlowScreen::LocalSaveSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSignUpClicked()
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: mock Sign Up for '%s'. Routing directly to single-player saves for testing."), *MockUserName);
    ShowScreen(ELoginFlowScreen::LocalSaveSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSettingsClicked()
{
    ShowScreen(ELoginFlowScreen::Settings);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToLoginClicked()
{
    ShowScreen(ELoginFlowScreen::Login);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSinglePlayerClicked()
{
    ShowScreen(ELoginFlowScreen::LocalSaveSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleMultiplayerClicked()
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: Multiplayer is intentionally disabled until backend services are ready."));
    return FReply::Handled();
}

FReply ALoginHUD::HandleCreateNewStateClicked()
{
    ShowScreen(ELoginFlowScreen::DifficultySelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectDifficulty(FString DifficultyName)
{
    PendingDifficulty = DifficultyName;
    PendingClimate.Empty();
    PendingLeaderGender.Empty();
    PendingAddressTitle.Empty();
    ShowScreen(ELoginFlowScreen::NewStateSetup);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectClimate(FString ClimateName)
{
    PendingClimate = ClimateName;
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectLeaderGender(FString GenderName)
{
    PendingLeaderGender = GenderName;
    PendingAddressTitle = GetAddressTitleForGender(GenderName);
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleCreateInitialSaveClicked()
{
    if (PendingStateName.TrimStartAndEnd().IsEmpty())
    {
        LastSaveStatus = TEXT("Enter a state name before creating the save.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    if (PendingDifficulty.IsEmpty() || PendingClimate.IsEmpty() || PendingLeaderGender.IsEmpty())
    {
        LastSaveStatus = TEXT("Choose difficulty, climate, and president address before creating the save.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FString SavePath;
    if (CreateInitialSinglePlayerSave(SavePath))
    {
        OpeningScriptText = GetOpeningScriptText();
        bShowFirstLoginBriefing = true;
        EnterOfficePrototype(true);
    }
    else
    {
        RefreshLoginWidget();
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleResumeSimulationClicked()
{
    StartSimulationTimer();
    return FReply::Handled();
}

FReply ALoginHUD::HandlePauseSimulationClicked()
{
    StopSimulationTimer();
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleStepSimulationClicked()
{
    if (bHasLoadedRuntimeState)
    {
        const bool bWasPaused = LoadedSaveState.RuntimeState.bPaused;
        LoadedSaveState.RuntimeState.bPaused = false;
        RunSimulationTick();
        LoadedSaveState.RuntimeState.bPaused = bWasPaused;
        if (bWasPaused)
        {
            LoadedSaveState.RuntimeState.Phase = TEXT("Prototype Simulation Paused");
            SimulationTickSummary = BuildSimulationStatusText();
        }
        RefreshLoginWidget();
    }

    return FReply::Handled();
}


FReply ALoginHUD::HandleOpenPoliciesClicked()
{
    ShowScreen(ELoginFlowScreen::OfficePolicies);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenBudgetClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeBudget);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenResourceChainsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeResourceChains);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenAdvisorWarningsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeAdvisorWarnings);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenDepartmentsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeDepartments);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenDevelopmentClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeDevelopment);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSetDevelopmentFocus(FString TrackName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    InitializeDevelopmentSystemIfMissing(LoadedSaveState.RuntimeState);
    FDemocracyDevelopmentTrackState* Track = FindDevelopmentTrack(LoadedSaveState.RuntimeState.DevelopmentSystem, TrackName);
    if (!Track)
    {
        LastSaveStatus = FString::Printf(TEXT("Development focus not found: %s"), *TrackName);
        return FReply::Handled();
    }

    LoadedSaveState.RuntimeState.DevelopmentSystem.ActiveFocus = Track->TrackName;
    LoadedSaveState.RuntimeState.DevelopmentSystem.LastUpdatedTurn = LoadedSaveState.RuntimeState.Turn;
    LoadedSaveState.RuntimeState.DevelopmentSystem.Summary = FString::Printf(TEXT("%s selected as the national development focus. Progress will advance during simulation ticks when treasury and resource costs are available."), *Track->TrackName);

    LogDecision(LoadedSaveState.RuntimeState, TEXT("Development Focus"), Track->TrackName, FString::Printf(TEXT("Development focus changed to %s."), *Track->TrackName), LoadedSaveState.RuntimeState.DevelopmentSystem.Summary, 20, { TEXT("development"), Track->TrackName });
    LastSaveStatus = FString::Printf(TEXT("Development focus set to %s."), *Track->TrackName);
    ShowScreen(ELoginFlowScreen::OfficeDevelopment);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenApprovalStabilityClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeApprovalStability);
    return FReply::Handled();
}
FReply ALoginHUD::HandleOpenDecisionHistoryClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeDecisionHistory);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenMeetingAdvisorClicked()
{
    if (SelectedMeetingAdvisorName.IsEmpty())
    {
        SelectedMeetingAdvisorName = TEXT("Meeting Advisor");
        SelectedMeetingAdvisorFocus = TEXT("General meeting report and agenda surface.");
    }
    ShowScreen(ELoginFlowScreen::OfficeMeetingAdvisor);
    return FReply::Handled();
}

FReply ALoginHUD::HandleOpenPressReleaseClicked()
{
    ShowScreen(ELoginFlowScreen::OfficePressRelease);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSetDepartmentAction(FString DepartmentName, FString ActionName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    InitializeDefaultDepartments(LoadedSaveState.RuntimeState);
    FDemocracyDepartmentState* Department = FindDepartment(LoadedSaveState.RuntimeState.Departments, DepartmentName);
    if (!Department)
    {
        return FReply::Handled();
    }

    Department->CurrentAction = ActionName;
    if (ActionName.Equals(TEXT("Focused Initiative"), ESearchCase::IgnoreCase))
    {
        Department->Priority = FMath::Clamp(Department->Priority + 12, 0, 100);
        Department->Staffing = FMath::Clamp(Department->Staffing + 4, 0, 100);
        LoadedSaveState.RuntimeState.PlayerCountry.Treasury = FMath::Max(0, LoadedSaveState.RuntimeState.PlayerCountry.Treasury - 35);
    }
    else if (ActionName.Equals(TEXT("Emergency Response"), ESearchCase::IgnoreCase))
    {
        Department->Priority = FMath::Clamp(Department->Priority + 22, 0, 100);
        Department->Staffing = FMath::Clamp(Department->Staffing + 8, 0, 100);
        Department->PublicTrust = FMath::Clamp(Department->PublicTrust - 2, 0, 100);
        LoadedSaveState.RuntimeState.PlayerCountry.Treasury = FMath::Max(0, LoadedSaveState.RuntimeState.PlayerCountry.Treasury - 90);
        LoadedSaveState.RuntimeState.PlayerCountry.Unrest = FMath::Clamp(LoadedSaveState.RuntimeState.PlayerCountry.Unrest - 1, 0, 100);
    }
    else if (ActionName.Equals(TEXT("Administrative Reform"), ESearchCase::IgnoreCase))
    {
        Department->Priority = FMath::Clamp(Department->Priority + 6, 0, 100);
        Department->PublicTrust = FMath::Clamp(Department->PublicTrust + 4, 0, 100);
        Department->Effectiveness = FMath::Clamp(Department->Effectiveness + 3, 0, 100);
        LoadedSaveState.RuntimeState.PlayerCountry.Treasury = FMath::Max(0, LoadedSaveState.RuntimeState.PlayerCountry.Treasury - 45);
    }
    else
    {
        Department->Priority = FMath::Clamp(Department->Priority - 6, 0, 100);
        Department->Staffing = FMath::Clamp(Department->Staffing - 1, 0, 100);
    }

    RecalculateDepartments(LoadedSaveState.RuntimeState);
    LogDecision(LoadedSaveState.RuntimeState, TEXT("Department Action"), DepartmentName, FString::Printf(TEXT("Assigned %s to %s."), *ActionName, *DepartmentName), BuildDepartmentSummaryText(LoadedSaveState.RuntimeState.Departments), ActionName.Equals(TEXT("Emergency Response"), ESearchCase::IgnoreCase) ? 45 : 25, { TEXT("department"), DepartmentName, ActionName });
    LoadedSaveState.RuntimeState.AdvisorSystem.Reports = GenerateAdvisorReports(LoadedSaveState.RuntimeState);
    LoadedSaveState.RuntimeState.Phase = FString::Printf(TEXT("%s Department Action Updated"), *DepartmentName);
    LastSaveStatus = TEXT("Department action changed. Save current state to persist it to disk.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleSetTaxPolicy(FString TaxPolicyName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    ApplyBudgetPreset(LoadedSaveState.RuntimeState.EconomyBudget, TaxPolicyName);
    RecalculateEconomyBudget(LoadedSaveState.RuntimeState);
    RecalculateDemographics(LoadedSaveState.RuntimeState);
    LoadedSaveState.RuntimeState.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(LoadedSaveState.RuntimeState.PlayerCountry.CountrySizeScore);
    LoadedSaveState.RuntimeState.AdvisorSystem.LastUpdatedTurn = LoadedSaveState.RuntimeState.Turn;
    InitializeDefaultDepartments(LoadedSaveState.RuntimeState);
    RecalculateDepartments(LoadedSaveState.RuntimeState);
    LogDecision(LoadedSaveState.RuntimeState, TEXT("Tax Policy"), TaxPolicyName, FString::Printf(TEXT("Tax policy changed to %s."), *TaxPolicyName), BuildEconomyBudgetSummaryText(LoadedSaveState.RuntimeState.EconomyBudget), 30, { TEXT("budget"), TEXT("tax"), TaxPolicyName });
    LoadedSaveState.RuntimeState.AdvisorSystem.Reports = GenerateAdvisorReports(LoadedSaveState.RuntimeState);
    LoadedSaveState.RuntimeState.Phase = TEXT("Budget Updated");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = TEXT("Tax policy changed. Save current state to persist it to disk.");
    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleSetSpendingPosture(FString SpendingPostureName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    ApplySpendingPreset(LoadedSaveState.RuntimeState.EconomyBudget, SpendingPostureName);
    RecalculateEconomyBudget(LoadedSaveState.RuntimeState);
    RecalculateDemographics(LoadedSaveState.RuntimeState);
    LoadedSaveState.RuntimeState.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(LoadedSaveState.RuntimeState.PlayerCountry.CountrySizeScore);
    LoadedSaveState.RuntimeState.AdvisorSystem.LastUpdatedTurn = LoadedSaveState.RuntimeState.Turn;
    InitializeDefaultDepartments(LoadedSaveState.RuntimeState);
    RecalculateDepartments(LoadedSaveState.RuntimeState);
    LogDecision(LoadedSaveState.RuntimeState, TEXT("Spending Posture"), SpendingPostureName, FString::Printf(TEXT("Spending posture changed to %s."), *SpendingPostureName), BuildEconomyBudgetSummaryText(LoadedSaveState.RuntimeState.EconomyBudget), 30, { TEXT("budget"), TEXT("spending"), SpendingPostureName });
    LoadedSaveState.RuntimeState.AdvisorSystem.Reports = GenerateAdvisorReports(LoadedSaveState.RuntimeState);
    LoadedSaveState.RuntimeState.Phase = TEXT("Budget Updated");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = TEXT("Spending posture changed. Save current state to persist it to disk.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleOpenDemographicsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeDemographics);
    return FReply::Handled();
}
FReply ALoginHUD::HandleOpenEventsClicked()
{
    ShowScreen(ELoginFlowScreen::OfficeEvents);
    return FReply::Handled();
}

FReply ALoginHUD::HandleResolveEventChoice(FString EventId, FString ChoiceId)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyResourceInventory& Resources = Country.Resources;

    for (FDemocracyActiveEventState& Event : State.EventSystem.ActiveEvents)
    {
        if (Event.bResolved || !Event.EventId.Equals(EventId, ESearchCase::IgnoreCase))
        {
            continue;
        }

        for (const FDemocracyEventChoiceState& Choice : Event.Choices)
        {
            if (!Choice.ChoiceId.Equals(ChoiceId, ESearchCase::IgnoreCase))
            {
                continue;
            }

            Country.PublicApproval = FMath::Clamp(Country.PublicApproval + Choice.ApprovalDelta, 0, 100);
            Country.Stability = FMath::Clamp(Country.Stability + Choice.StabilityDelta, 0, 100);
            Country.Unrest = FMath::Clamp(Country.Unrest + Choice.UnrestDelta, 0, 100);
            Country.Treasury = FMath::Max(0, Country.Treasury + Choice.TreasuryDelta);
            Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + Choice.EconomicDelta, 0, 100);
            Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + Choice.DiplomacyDelta, 0, 100);
            Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + Choice.MilitaryDelta, 0, 100);
            Country.Infrastructure = FMath::Clamp(Country.Infrastructure + Choice.InfrastructureDelta, 0, 100);
            Country.EnvironmentalHealth = FMath::Clamp(Country.EnvironmentalHealth + Choice.EnvironmentDelta, 0, 100);
            Resources.Food = FMath::Max(0, Resources.Food + Choice.FoodDelta);
            Resources.Water = FMath::Max(0, Resources.Water + Choice.WaterDelta);
            Resources.GasOil = FMath::Max(0, Resources.GasOil + Choice.GasOilDelta);
            Resources.Wood = FMath::Max(0, Resources.Wood + Choice.WoodDelta);
            Resources.Metals = FMath::Max(0, Resources.Metals + Choice.MetalsDelta);
            State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk + Choice.AssassinationRiskDelta, 0, State.FailureRisk.AssassinationRiskTrigger);
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk + Choice.InvasionRiskDelta, 0, State.InvasionRisk.InvasionRiskTrigger);

            Event.bResolved = true;
            Event.SelectedChoiceId = Choice.ChoiceId;
            Event.ResolutionSummary = FString::Printf(TEXT("%s resolved with choice: %s. %s"), *Event.Title, *Choice.Label, *Choice.ConsequencePreview);
            State.EventSystem.EventHistory.Add(FString::Printf(TEXT("Turn %d: %s"), State.Turn, *Event.ResolutionSummary));
            LogDecision(State, TEXT("Event Choice"), Event.Title, FString::Printf(TEXT("Selected %s."), *Choice.Label), Choice.ConsequencePreview, Event.Severity, { TEXT("event"), Event.EventType, Choice.ChoiceId });
            State.Phase = TEXT("Event Resolved");
            LoadedSaveSummary = LoadedSaveState.ToSummaryText();
            SimulationTickSummary = BuildSimulationStatusText();
            RecalculateEconomyBudget(State);
            RecalculateDemographics(State);
            State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(Country.CountrySizeScore);
            State.AdvisorSystem.LastUpdatedTurn = State.Turn;
            State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
            LastSaveStatus = TEXT("Event choice applied. Save current state to persist it to disk.");
            PruneResolvedEvents(State.EventSystem);
            RefreshLoginWidget();
            EvaluateFailState();
            return FReply::Handled();
        }
    }

    LastSaveStatus = TEXT("Event choice was not found or was already resolved.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleSetPolicy(FString PolicyCategory, FString PolicyName)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracyPolicyState& Policies = LoadedSaveState.RuntimeState.PlayerCountry.Policies;
    if (PolicyCategory.Equals(TEXT("Economic"), ESearchCase::IgnoreCase))
    {
        Policies.EconomicPolicy = PolicyName;
    }
    else if (PolicyCategory.Equals(TEXT("Environmental"), ESearchCase::IgnoreCase))
    {
        Policies.EnvironmentalPolicy = PolicyName;
    }
    else if (PolicyCategory.Equals(TEXT("Military"), ESearchCase::IgnoreCase))
    {
        Policies.MilitaryPolicy = PolicyName;
    }
    else if (PolicyCategory.Equals(TEXT("Diplomacy"), ESearchCase::IgnoreCase))
    {
        Policies.DiplomacyPolicy = PolicyName;
    }
    else if (PolicyCategory.Equals(TEXT("Civil"), ESearchCase::IgnoreCase))
    {
        Policies.CivilPolicy = PolicyName;
    }

    ++Policies.PolicyChangeCount;
    Policies.LastPolicyChangeSummary = FString::Printf(TEXT("%s policy changed to %s on turn %d."), *PolicyCategory, *PolicyName, LoadedSaveState.RuntimeState.Turn);
    BuildPolicyModifiers(Policies, &Policies.ActivePolicyEffects);
    RecalculateEconomyBudget(LoadedSaveState.RuntimeState);
    LoadedSaveState.RuntimeState.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(LoadedSaveState.RuntimeState.PlayerCountry.CountrySizeScore);
    LoadedSaveState.RuntimeState.AdvisorSystem.LastUpdatedTurn = LoadedSaveState.RuntimeState.Turn;
    InitializeDefaultDepartments(LoadedSaveState.RuntimeState);
    RecalculateDepartments(LoadedSaveState.RuntimeState);
    LogDecision(LoadedSaveState.RuntimeState, TEXT("Policy Change"), PolicyName, FString::Printf(TEXT("%s policy changed to %s."), *PolicyCategory, *PolicyName), BuildPolicyStatusText(), 35, { TEXT("policy"), PolicyCategory, PolicyName });
    LoadedSaveState.RuntimeState.AdvisorSystem.Reports = GenerateAdvisorReports(LoadedSaveState.RuntimeState);
    RecalculateDemographics(LoadedSaveState.RuntimeState);
    if (LoadedSaveState.RuntimeState.EventSystem.ActiveEventLimit <= 0)
    {
        LoadedSaveState.RuntimeState.EventSystem.ActiveEventLimit = 3;
    }
    LoadedSaveState.RuntimeState.Phase = TEXT("Policy Platform Updated");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = TEXT("Policy changed. Save current state to persist it to disk.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleSaveRuntimeStateClicked()
{
    if (!bHasLoadedRuntimeState)
    {
        LastSaveStatus = TEXT("No runtime state is loaded.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FString SaveError;
    if (FDemocracySaveGameRuntime::SaveSinglePlayerRuntimeState(LoadedSaveState, SaveError))
    {
        LoadedSaveSummary = LoadedSaveState.ToSummaryText();
        LastSaveStatus = FString::Printf(TEXT("Saved current runtime state at %s."), *LoadedSaveState.LastPlayedAtUtc);
    }
    else
    {
        LastSaveStatus = SaveError;
        UE_LOG(LogTemp, Error, TEXT("Save current runtime state failed: %s"), *SaveError);
    }

    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleHoldMeeting(FString MeetingType, FString ParticipantName, FString AgendaItem)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyMeetingSystemState& MeetingSystem = State.MeetingSystem;
    InitializeMeetingSystemIfMissing(State);

    FDemocracyMeetingRecordState Record;
    Record.Turn = State.Turn;
    Record.MeetingType = MeetingType;
    Record.ParticipantName = ParticipantName;
    Record.AgendaItem = AgendaItem;

    const bool bEmergency = AgendaItem.Equals(TEXT("Emergency Response"), ESearchCase::IgnoreCase);
    const bool bFocused = AgendaItem.Equals(TEXT("Focused Action Plan"), ESearchCase::IgnoreCase);
    const bool bBriefing = AgendaItem.Equals(TEXT("Situation Briefing"), ESearchCase::IgnoreCase);
    const int32 CoordinationBonus = FMath::Clamp((MeetingSystem.AdvisorCoordination - 45) / 15, -1, 2);
    const int32 ForeignTrustBonus = FMath::Clamp((MeetingSystem.ForeignTrust - 45) / 15, -1, 2);

    if (MeetingType.Equals(TEXT("Foreign Official"), ESearchCase::IgnoreCase))
    {
        if (AgendaItem.Equals(TEXT("Trade Delegation"), ESearchCase::IgnoreCase))
        {
            Record.DiplomacyDelta = 2 + ForeignTrustBonus;
            Record.TreasuryDelta = 45 + MeetingSystem.ForeignTrust / 3;
            Record.EconomyDelta = 1;
            Record.ForeignTrustDelta = 2;
            Country.Resources.Food = FMath::Max(0, Country.Resources.Food + 8);
            Country.Resources.GasOil = FMath::Max(0, Country.Resources.GasOil + 5);
            Record.OutcomeSummary = TEXT("Trade delegates agreed to limited imports and commercial access.");
        }
        else if (AgendaItem.Equals(TEXT("De-escalation Talks"), ESearchCase::IgnoreCase))
        {
            Record.DiplomacyDelta = 3 + ForeignTrustBonus;
            Record.StabilityDelta = 1;
            Record.UnrestDelta = -1;
            Record.TreasuryDelta = -25;
            Record.ForeignTrustDelta = 3;
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 8 - FMath::Max(0, ForeignTrustBonus), 0, State.InvasionRisk.InvasionRiskTrigger);
            Record.OutcomeSummary = TEXT("Border envoys lowered immediate foreign pressure at a modest treasury cost.");
        }
        else
        {
            Record.DiplomacyDelta = 5 + ForeignTrustBonus;
            Record.StabilityDelta = 1;
            Record.TreasuryDelta = -40;
            Record.ForeignTrustDelta = 4;
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - 4, 0, State.InvasionRisk.InvasionRiskTrigger);
            Record.OutcomeSummary = TEXT("Alliance outreach improved diplomatic standing and long-term foreign trust.");
        }
    }
    else
    {
        Record.AdvisorCoordinationDelta = bBriefing ? 1 : (bFocused ? 2 : 3);
        Record.TreasuryDelta = bEmergency ? -70 : (bFocused ? -30 : 0);
        Record.OutcomeSummary = FString::Printf(TEXT("%s completed a %s agenda."), *ParticipantName, *AgendaItem);

        if (ParticipantName.Equals(TEXT("Resource Manager"), ESearchCase::IgnoreCase))
        {
            Country.Resources.Food = FMath::Max(0, Country.Resources.Food + (bEmergency ? 18 : (bFocused ? 10 : 3)) + CoordinationBonus);
            Country.Resources.Water = FMath::Max(0, Country.Resources.Water + (bEmergency ? 14 : (bFocused ? 8 : 2)) + CoordinationBonus);
            Record.UnrestDelta = bEmergency ? -2 : (bFocused ? -1 : 0);
            Record.StabilityDelta = bEmergency ? 1 : 0;
        }
        else if (ParticipantName.Equals(TEXT("Military Advisor"), ESearchCase::IgnoreCase))
        {
            Record.MilitaryDelta = bEmergency ? 6 : (bFocused ? 4 : 1);
            Record.StabilityDelta = bEmergency ? 1 : 0;
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - (bEmergency ? 5 : (bFocused ? 3 : 1)), 0, State.InvasionRisk.InvasionRiskTrigger);
        }
        else if (ParticipantName.Equals(TEXT("Social Advisor"), ESearchCase::IgnoreCase))
        {
            Record.ApprovalDelta = bEmergency ? 2 : (bFocused ? 1 : 0);
            Record.StabilityDelta = bEmergency ? 3 : (bFocused ? 2 : 1);
            Record.UnrestDelta = bEmergency ? -4 : (bFocused ? -2 : -1);
        }
        else if (ParticipantName.Equals(TEXT("Economic Advisor"), ESearchCase::IgnoreCase))
        {
            Record.TreasuryDelta += bEmergency ? 20 : (bFocused ? 45 : 15);
            Record.EconomyDelta = bEmergency ? 1 : (bFocused ? 3 : 1);
            State.EconomyBudget.Inflation = FMath::Clamp(State.EconomyBudget.Inflation - (bFocused ? 1 : 0), 0, 100);
        }
        else if (ParticipantName.Equals(TEXT("Diplomacy Advisor"), ESearchCase::IgnoreCase))
        {
            Record.DiplomacyDelta = bEmergency ? 4 : (bFocused ? 3 : 1);
            Record.ForeignTrustDelta = bEmergency ? 1 : (bFocused ? 2 : 1);
            State.InvasionRisk.CurrentInvasionRisk = FMath::Clamp(State.InvasionRisk.CurrentInvasionRisk - (bEmergency ? 3 : 1), 0, State.InvasionRisk.InvasionRiskTrigger);
        }
        else if (ParticipantName.Equals(TEXT("Infrastructure Advisor"), ESearchCase::IgnoreCase))
        {
            Record.InfrastructureDelta = bEmergency ? 5 : (bFocused ? 3 : 1);
            Country.Resources.Wood = FMath::Max(0, Country.Resources.Wood - (bEmergency ? 4 : 1));
            Country.Resources.Metals = FMath::Max(0, Country.Resources.Metals - (bEmergency ? 4 : 1));
        }
        else if (ParticipantName.Equals(TEXT("Security Advisor"), ESearchCase::IgnoreCase))
        {
            Record.StabilityDelta = bEmergency ? 2 : (bFocused ? 1 : 0);
            Record.UnrestDelta = bEmergency ? -2 : (bFocused ? -1 : 0);
            State.FailureRisk.CurrentAssassinationRisk = FMath::Clamp(State.FailureRisk.CurrentAssassinationRisk - (bEmergency ? 8 : (bFocused ? 5 : 2)), 0, State.FailureRisk.AssassinationRiskTrigger);
        }
        else if (ParticipantName.Equals(TEXT("Public Welfare Advisor"), ESearchCase::IgnoreCase))
        {
            Record.ApprovalDelta = bEmergency ? 2 : (bFocused ? 1 : 0);
            Record.UnrestDelta = bEmergency ? -2 : (bFocused ? -1 : 0);
            State.EconomyBudget.PublicServices = FMath::Clamp(State.EconomyBudget.PublicServices + (bEmergency ? 4 : (bFocused ? 2 : 1)), 0, 100);
        }
    }

    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + Record.ApprovalDelta, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + Record.StabilityDelta, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + Record.UnrestDelta, 0, 100);
    Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + Record.DiplomacyDelta, 0, 100);
    Country.Treasury = FMath::Max(0, Country.Treasury + Record.TreasuryDelta);
    Country.EconomicHealth = FMath::Clamp(Country.EconomicHealth + Record.EconomyDelta, 0, 100);
    Country.MilitaryReadiness = FMath::Clamp(Country.MilitaryReadiness + Record.MilitaryDelta, 0, 100);
    Country.Infrastructure = FMath::Clamp(Country.Infrastructure + Record.InfrastructureDelta, 0, 100);
    MeetingSystem.AdvisorCoordination = FMath::Clamp(MeetingSystem.AdvisorCoordination + Record.AdvisorCoordinationDelta, 0, 100);
    MeetingSystem.ForeignTrust = FMath::Clamp(MeetingSystem.ForeignTrust + Record.ForeignTrustDelta, 0, 100);
    ++MeetingSystem.TotalMeetings;
    MeetingSystem.LastUpdatedTurn = State.Turn;

    MeetingSystem.LastMeetingSummary = FString::Printf(TEXT("%s with %s: %s"), *MeetingType, *ParticipantName, *Record.OutcomeSummary);
    MeetingSystem.Records.Add(Record);
    while (MeetingSystem.Records.Num() > MeetingSystem.MaxRecords)
    {
        MeetingSystem.Records.RemoveAt(0);
    }

    RecalculateEconomyBudget(State);
    RecalculateDepartments(State);
    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(Country.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    LogDecision(State, TEXT("Meeting"), FString::Printf(TEXT("%s - %s"), *ParticipantName, *AgendaItem), Record.OutcomeSummary, MeetingSystem.LastMeetingSummary, MeetingType.Equals(TEXT("Foreign Official"), ESearchCase::IgnoreCase) ? 32 : 22, { TEXT("meeting"), MeetingType, ParticipantName, AgendaItem });

    State.Phase = TEXT("Meeting Held");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = TEXT("Meeting held. Save current state to persist meeting history and outcomes.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleMakePressRelease(FString AnnouncementType)
{
    if (!bHasLoadedRuntimeState)
    {
        return FReply::Handled();
    }

    FDemocracySimulationState& State = LoadedSaveState.RuntimeState;
    FDemocracyCountryState& Country = State.PlayerCountry;
    FDemocracyPressOfficeState& PressOffice = State.PressOffice;
    InitializePressOfficeIfMissing(State);

    int32 ApprovalDelta = 0;
    int32 StabilityDelta = 0;
    int32 DiplomacyDelta = 0;
    int32 UnrestDelta = 0;
    int32 CredibilityDelta = 0;
    bool bTruthful = true;
    FString MessageQuality = TEXT("Substantive");
    FString Summary;

    const int32 CredibilityBonus = FMath::Clamp((PressOffice.Credibility - 50) / 15, -2, 2);
    if (AnnouncementType.Equals(TEXT("Crisis Reassurance"), ESearchCase::IgnoreCase))
    {
        ApprovalDelta = 1 + CredibilityBonus;
        StabilityDelta = 2 + CredibilityBonus;
        UnrestDelta = -2 - FMath::Max(0, CredibilityBonus);
        CredibilityDelta = 1;
        Summary = TEXT("The administration gave a focused reassurance speech tied to current risks.");
    }
    else if (AnnouncementType.Equals(TEXT("Policy Explanation"), ESearchCase::IgnoreCase))
    {
        ApprovalDelta = 2 + CredibilityBonus;
        StabilityDelta = 1;
        UnrestDelta = -1;
        CredibilityDelta = 2;
        Summary = TEXT("The administration explained current policies and tradeoffs to the public.");
    }
    else if (AnnouncementType.Equals(TEXT("Diplomatic Address"), ESearchCase::IgnoreCase))
    {
        ApprovalDelta = CredibilityBonus;
        StabilityDelta = 1;
        DiplomacyDelta = 3 + CredibilityBonus;
        UnrestDelta = -1;
        CredibilityDelta = 1;
        Summary = TEXT("The administration made a world-facing diplomatic address.");
    }
    else if (AnnouncementType.Equals(TEXT("Victory Claim"), ESearchCase::IgnoreCase))
    {
        const bool bSupportedBySignals = Country.PublicApproval >= 58 || Country.Stability >= 62 || Country.MilitaryReadiness >= 62 || Country.DiplomaticStanding >= 62;
        bTruthful = bSupportedBySignals;
        if (bSupportedBySignals)
        {
            ApprovalDelta = 3 + CredibilityBonus;
            StabilityDelta = 1;
            DiplomacyDelta = 1;
            UnrestDelta = -1;
            CredibilityDelta = 1;
            Summary = TEXT("The administration highlighted a credible recent success.");
        }
        else
        {
            ApprovalDelta = -2;
            StabilityDelta = -1;
            DiplomacyDelta = -2;
            UnrestDelta = 2;
            CredibilityDelta = -8 - PressOffice.ConsecutiveFalseAnnouncements * 4;
            MessageQuality = TEXT("Unsupported");
            Summary = TEXT("The victory claim was not supported by current national signals and damaged trust.");
        }
    }
    else if (AnnouncementType.Equals(TEXT("False Claim"), ESearchCase::IgnoreCase))
    {
        bTruthful = false;
        MessageQuality = TEXT("False");
        ApprovalDelta = -3;
        StabilityDelta = -2;
        DiplomacyDelta = -3;
        UnrestDelta = 3 + PressOffice.ConsecutiveFalseAnnouncements;
        CredibilityDelta = -12 - PressOffice.ConsecutiveFalseAnnouncements * 5;
        Summary = TEXT("A false announcement was challenged by observers and reduced institutional credibility.");
    }
    else
    {
        MessageQuality = TEXT("Empty");
        ApprovalDelta = -1 - PressOffice.ConsecutiveEmptyAnnouncements;
        StabilityDelta = -1;
        UnrestDelta = 1 + PressOffice.ConsecutiveEmptyAnnouncements;
        CredibilityDelta = -6 - PressOffice.ConsecutiveEmptyAnnouncements * 3;
        Summary = TEXT("The announcement contained no useful information and made the press office look evasive.");
    }

    if (MessageQuality.Equals(TEXT("Empty"), ESearchCase::IgnoreCase))
    {
        ++PressOffice.ConsecutiveEmptyAnnouncements;
    }
    else
    {
        PressOffice.ConsecutiveEmptyAnnouncements = 0;
    }

    if (!bTruthful)
    {
        ++PressOffice.ConsecutiveFalseAnnouncements;
    }
    else
    {
        PressOffice.ConsecutiveFalseAnnouncements = 0;
    }

    Country.PublicApproval = FMath::Clamp(Country.PublicApproval + ApprovalDelta, 0, 100);
    Country.Stability = FMath::Clamp(Country.Stability + StabilityDelta, 0, 100);
    Country.DiplomaticStanding = FMath::Clamp(Country.DiplomaticStanding + DiplomacyDelta, 0, 100);
    Country.Unrest = FMath::Clamp(Country.Unrest + UnrestDelta, 0, 100);
    PressOffice.Credibility = FMath::Clamp(PressOffice.Credibility + CredibilityDelta, 0, 100);
    ++PressOffice.TotalAnnouncements;
    PressOffice.LastUpdatedTurn = State.Turn;

    FDemocracyPressReleaseRecordState Record;
    Record.Turn = State.Turn;
    Record.AnnouncementType = AnnouncementType;
    Record.MessageQuality = MessageQuality;
    Record.bTruthful = bTruthful;
    Record.ApprovalDelta = ApprovalDelta;
    Record.StabilityDelta = StabilityDelta;
    Record.DiplomacyDelta = DiplomacyDelta;
    Record.UnrestDelta = UnrestDelta;
    Record.CredibilityDelta = CredibilityDelta;
    Record.CredibilityAfter = PressOffice.Credibility;
    Record.Summary = Summary;
    PressOffice.Records.Add(Record);
    while (PressOffice.Records.Num() > PressOffice.MaxRecords)
    {
        PressOffice.Records.RemoveAt(0);
    }

    PressOffice.LastAnnouncementSummary = FString::Printf(TEXT("%s | approval %+d, stability %+d, diplomacy %+d, unrest %+d, credibility %+d. Credibility now %d."),
        *Summary,
        ApprovalDelta,
        StabilityDelta,
        DiplomacyDelta,
        UnrestDelta,
        CredibilityDelta,
        PressOffice.Credibility);

    RecalculateDemographics(State);
    RecalculateApprovalStability(State);
    State.AdvisorSystem.GuidanceLevel = AdvisorGuidanceForDifficultyScore(Country.CountrySizeScore);
    State.AdvisorSystem.LastUpdatedTurn = State.Turn;
    State.AdvisorSystem.Reports = GenerateAdvisorReports(State);
    LogDecision(State, TEXT("Press Release"), AnnouncementType, Summary, PressOffice.LastAnnouncementSummary, bTruthful ? 18 : 55, { TEXT("press"), AnnouncementType, MessageQuality });

    State.Phase = TEXT("Press Release Issued");
    LoadedSaveSummary = LoadedSaveState.ToSummaryText();
    LastSaveStatus = TEXT("Press release issued. Save current state to persist credibility and announcement history.");
    RefreshLoginWidget();
    return FReply::Handled();
}
FReply ALoginHUD::HandleReloadPreviousSaveClicked()
{
    const FString PrimarySavePath = LoadedSavePath;
    if (PrimarySavePath.IsEmpty())
    {
        LastSaveStatus = TEXT("No local save path is available for protected reload.");
        RefreshLoginWidget();
        return FReply::Handled();
    }

    FString ReloadPath;
    FString ReloadError;
    if (!FDemocracySaveGameRuntime::GetProtectedReloadSavePath(PrimarySavePath, ReloadPath, ReloadError))
    {
        LastSaveStatus = ReloadError;
        RefreshLoginWidget();
        return FReply::Handled();
    }

    if (LoadSinglePlayerSaveIntoRuntime(ReloadPath))
    {
        LoadedSavePath = PrimarySavePath;
        LoadedSaveState.SavePath = PrimarySavePath;
        GameOverReason.Empty();
        GameOverDetails.Empty();
        LastSaveStatus = FString::Printf(TEXT("Reloaded protected previous save: %s"), *ReloadPath);
        EnterOfficePrototype(false);
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToDifficultyClicked()
{
    ShowScreen(ELoginFlowScreen::DifficultySelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToLocalSavesClicked()
{
    ShowScreen(ELoginFlowScreen::LocalSaveSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToModeSelectionClicked()
{
    ShowScreen(ELoginFlowScreen::GameModeSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBackToOnlineStatesClicked()
{
    ShowScreen(ELoginFlowScreen::MultiplayerStateSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectLocalSave(FString SaveName)
{
    const FString SavePath = FPaths::ProjectDir() / TEXT("Saves") / SaveName;
    if (LoadSinglePlayerSaveIntoRuntime(SavePath))
    {
        EnterOfficePrototype(false);
    }
    else
    {
        RefreshLoginWidget();
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleDeleteLocalSave(FString SaveName)
{
    const FString SavePath = FPaths::ProjectDir() / TEXT("Saves") / SaveName;
    const bool bDeleted = IFileManager::Get().Delete(*SavePath, false, true, true);

    if (bDeleted)
    {
        LastSaveStatus = FString::Printf(TEXT("Deleted local save %s."), *SaveName);
        if (LoadedSavePath.Equals(SavePath, ESearchCase::IgnoreCase))
        {
            LoadedSavePath.Empty();
            LoadedStateName.Empty();
            LoadedSaveSummary.Empty();
            LoadedSaveError.Empty();
            bHasLoadedRuntimeState = false;
        }
    }
    else
    {
        LastSaveStatus = FString::Printf(TEXT("Could not delete local save %s. Check that the file is not open or read-only."), *SaveName);
    }

    RefreshLoginWidget();
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectOnlineState(FString StateName)
{
    SelectedOnlineState = StateName;
    ShowScreen(ELoginFlowScreen::ServerSelection);
    return FReply::Handled();
}

FReply ALoginHUD::HandleRefreshOnlineStatesClicked()
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: refresh online states placeholder. Server API will populate this screen later."));
    return FReply::Handled();
}

FReply ALoginHUD::HandleSelectServer(FString ServerName)
{
    UE_LOG(LogTemp, Log, TEXT("Login flow: selected server '%s' for online state '%s'. Connection placeholder."), *ServerName, *SelectedOnlineState);
    return FReply::Handled();
}

FReply ALoginHUD::HandleChangeKeybind(FString ActionName)
{
    UE_LOG(LogTemp, Log, TEXT("Settings: keybind capture placeholder for '%s'."), *ActionName);
    return FReply::Handled();
}

FReply ALoginHUD::HandleBeginOfficeFromBriefingClicked()
{
    TearDownLoginWidget();

    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        if (OfficePlayerPawn.IsValid())
        {
            PlayerController->Possess(OfficePlayerPawn.Get());
            PlayerController->SetViewTarget(OfficePlayerPawn.Get());
            PlayerController->SetControlRotation(FRotator(0.0f, 90.0f, 0.0f));
        }

        PlayerController->bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleCloseOfficeOverlayClicked()
{
    TearDownLoginWidget();

    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        if (OfficePlayerPawn.IsValid())
        {
            PlayerController->Possess(OfficePlayerPawn.Get());
            PlayerController->SetViewTarget(OfficePlayerPawn.Get());
        }

        PlayerController->bShowMouseCursor = false;
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleExitOfficeClicked()
{
    bInOfficeMode = false;
    ShowScreen(ELoginFlowScreen::LoadedGame);

    if (APlayerController* PlayerController = GetOwningPlayerController())
    {
        PlayerController->bShowMouseCursor = true;
        FInputModeUIOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }

    return FReply::Handled();
}

FReply ALoginHUD::HandleEnterOfficeClicked()
{
    EnterOfficePrototype(false);
    return FReply::Handled();
}

FString ALoginHUD::GetRememberLoginPath() const
{
    return FPaths::ProjectSavedDir() / TEXT("DemocracyLoginDetails.ini");
}

void ALoginHUD::LoadRememberedLoginDetails()
{
    FString Contents;
    if (!FFileHelper::LoadFileToString(Contents, *GetRememberLoginPath()))
    {
        bRememberLoginDetails = false;
        return;
    }

    TArray<FString> Lines;
    Contents.ParseIntoArrayLines(Lines, false);
    for (const FString& Line : Lines)
    {
        FString Key;
        FString Value;
        if (!Line.Split(TEXT("="), &Key, &Value))
        {
            continue;
        }

        if (Key.Equals(TEXT("Remember"), ESearchCase::IgnoreCase))
        {
            bRememberLoginDetails = Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
        }
        else if (Key.Equals(TEXT("UserName"), ESearchCase::IgnoreCase))
        {
            MockUserName = Value;
        }
        else if (Key.Equals(TEXT("Password"), ESearchCase::IgnoreCase))
        {
            MockPassword = Value;
        }
    }

    if (!bRememberLoginDetails)
    {
        MockUserName.Empty();
        MockPassword.Empty();
    }
}

void ALoginHUD::SaveRememberedLoginDetails() const
{
    if (!bRememberLoginDetails)
    {
        return;
    }

    IFileManager::Get().MakeDirectory(*FPaths::ProjectSavedDir(), true);
    const FString Contents = FString::Printf(TEXT("Remember=true\nUserName=%s\nPassword=%s\n"), *MockUserName, *MockPassword);
    FFileHelper::SaveStringToFile(Contents, *GetRememberLoginPath());
}

void ALoginHUD::ClearRememberedLoginDetails() const
{
    IFileManager::Get().Delete(*GetRememberLoginPath(), false, true);
}
FReply ALoginHUD::HandleExitClicked()
{
    UKismetSystemLibrary::QuitGame(GetWorld(), GetOwningPlayerController(), EQuitPreference::Quit, false);
    return FReply::Handled();
}

void ALoginHUD::HandleMockUserNameChanged(const FText& UserNameText)
{
    MockUserName = UserNameText.ToString();
}

void ALoginHUD::HandleMockPasswordChanged(const FText& PasswordText)
{
    MockPassword = PasswordText.ToString();
}

void ALoginHUD::HandleRememberLoginDetailsChanged(ECheckBoxState NewState)
{
    bRememberLoginDetails = NewState == ECheckBoxState::Checked;
    if (bRememberLoginDetails)
    {
        SaveRememberedLoginDetails();
    }
    else
    {
        ClearRememberedLoginDetails();
    }
    RefreshLoginWidget();
}

void ALoginHUD::HandleLocalSaveSearchChanged(const FText& SearchText)
{
    LocalSaveSearchText = SearchText.ToString();
    RefreshLoginWidget();
}

void ALoginHUD::HandlePendingStateNameChanged(const FText& StateNameText)
{
    PendingStateName = StateNameText.ToString();
}

void ALoginHUD::HandleRecentLocalSavesChanged(ECheckBoxState NewState)
{
    bShowRecentLocalSavesOnly = NewState == ECheckBoxState::Checked;
    RefreshLoginWidget();
}

void ALoginHUD::HandleServerSearchChanged(const FText& SearchText)
{
    ServerSearchText = SearchText.ToString();
    RefreshLoginWidget();
}

void ALoginHUD::HandleRecommendedServersChanged(ECheckBoxState NewState)
{
    bShowRecommendedServersOnly = NewState == ECheckBoxState::Checked;
    RefreshLoginWidget();
}

void ALoginHUD::HandleFullscreenChanged(ECheckBoxState NewState)
{
    bFullscreen = NewState == ECheckBoxState::Checked;
}

void ALoginHUD::HandleVSyncChanged(ECheckBoxState NewState)
{
    bVSync = NewState == ECheckBoxState::Checked;
}

void ALoginHUD::HandleInvertLookYChanged(ECheckBoxState NewState)
{
    bInvertLookY = NewState == ECheckBoxState::Checked;
    if (OfficePlayerPawn.IsValid())
    {
        OfficePlayerPawn->SetInvertLookY(bInvertLookY);
    }
}

void ALoginHUD::HandleMasterVolumeChanged(float NewValue)
{
    MasterVolume = NewValue;
}

void ALoginHUD::HandleMusicVolumeChanged(float NewValue)
{
    MusicVolume = NewValue;
}

void ALoginHUD::HandleEffectsVolumeChanged(float NewValue)
{
    EffectsVolume = NewValue;
}

void ALoginHUD::HandleVoiceVolumeChanged(float NewValue)
{
    VoiceVolume = NewValue;
}

void ALoginHUD::HandleBrightnessChanged(float NewValue)
{
    Brightness = NewValue;
}

void ALoginHUD::HandleUiScaleChanged(float NewValue)
{
    UiScale = FMath::Clamp(NewValue, 0.50f, 1.50f);
}



















