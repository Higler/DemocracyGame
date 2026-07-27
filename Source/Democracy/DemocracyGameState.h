#pragma once

#include "CoreMinimal.h"
#include "DifficultyProfile.h"

struct FDemocracyResourceInventory
{
    int32 Food = 0;
    int32 GasOil = 0;
    int32 Wood = 0;
    int32 Metals = 0;
    int32 Water = 0;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyResourceChainEntry
{
    FString ResourceName;
    int32 Production = 0;
    int32 Consumption = 0;
    int32 Imports = 0;
    int32 Exports = 0;
    int32 Reserve = 0;
    int32 ReserveTarget = 100;
    int32 Shortage = 0;
    int32 Surplus = 0;
    int32 StrategicValue = 0;
    FString Role;
    FString Status;
    TArray<FString> Drivers;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyResourceProductionChainState
{
    TArray<FDemocracyResourceChainEntry> Chains;
    int32 TotalShortagePressure = 0;
    int32 TradeBalance = 0;
    int32 LastUpdatedTurn = 1;
    FString Summary = TEXT("Resource production chain awaiting first simulation tick.");

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyPolicyState
{
    FString EconomicPolicy = TEXT("Balanced Budget");
    FString EnvironmentalPolicy = TEXT("Managed Development");
    FString MilitaryPolicy = TEXT("Defensive Readiness");
    FString DiplomacyPolicy = TEXT("Neutral Engagement");
    FString CivilPolicy = TEXT("Public Stability");
    int32 PolicyChangeCount = 0;
    int32 LastEconomicPolicyTurn = -100;
    int32 LastEnvironmentalPolicyTurn = -100;
    int32 LastMilitaryPolicyTurn = -100;
    int32 LastDiplomacyPolicyTurn = -100;
    int32 LastCivilPolicyTurn = -100;
    int32 PolicyCooldownTurns = 2;
    FString LastPolicyChangeSummary = TEXT("Initial policy platform.");
    TArray<FString> ActivePolicyEffects;
    TArray<FString> PolicyRuleStatus;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyFailureRiskState
{
    int32 StabilityWarningThreshold = 35;
    int32 StabilityCriticalThreshold = 20;
    int32 UnrestWarningThreshold = 55;
    int32 UnrestCriticalThreshold = 75;
    int32 CurrentAssassinationRisk = 0;
    int32 AssassinationRiskTrigger = 100;
    FString WarningLevel = TEXT("Standard");
    FString GameOverReason = TEXT("Assassination");
    bool bGameOverOnAssassination = true;
    TArray<FString> ActiveUnrestCauses;
    TArray<FString> AdvisorWarnings;
    TArray<FString> RecoveryTips;

    FString ToJson(int32 IndentSpaces = 2) const;
};


struct FDemocracyInvasionRiskState
{
    int32 MilitaryReadinessWarningThreshold = 45;
    int32 MilitaryReadinessCriticalThreshold = 25;
    int32 BorderPressureWarningThreshold = 45;
    int32 BorderPressureCriticalThreshold = 70;
    int32 TerritorialLossWarningThreshold = 1;
    int32 TerritorialLossCriticalThreshold = 3;
    int32 CurrentInvasionRisk = 0;
    int32 InvasionRiskTrigger = 100;
    FString WarningLevel = TEXT("Standard");
    FString GameOverReason = TEXT("Foreign Takeover");
    bool bGameOverOnTakeover = true;
    TArray<FString> ActiveInvasionCauses;
    TArray<FString> AdvisorWarnings;
    TArray<FString> RecoveryTips;

    FString ToJson(int32 IndentSpaces = 2) const;
};


struct FDemocracyAdvisorReport
{
    FString AdvisorName;
    FString Category;
    FString IssueReport;
    FString Recommendation;
    FString Warning;
    FString TradeoffExplanation;
    FString GuidanceLevel = TEXT("Standard");
    int32 Severity = 0;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyAdvisorSystemState
{
    FString GuidanceLevel = TEXT("Standard");
    int32 AdvisorCount = 4;
    int32 LastUpdatedTurn = 1;
    TArray<FDemocracyAdvisorReport> Reports;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyEventChoiceState
{
    FString ChoiceId;
    FString Label;
    FString Description;
    FString ConsequencePreview;
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
    int32 WaterDelta = 0;
    int32 GasOilDelta = 0;
    int32 WoodDelta = 0;
    int32 MetalsDelta = 0;
    int32 AssassinationRiskDelta = 0;
    int32 InvasionRiskDelta = 0;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyActiveEventState
{
    FString EventId;
    FString EventType;
    FString Title;
    FString Description;
    FString TriggerReason;
    int32 CreatedTurn = 1;
    int32 DeadlineTurn = 0;
    int32 Severity = 0;
    bool bTriggered = false;
    bool bResolved = false;
    FString CompletionState = TEXT("Active");
    FString SelectedChoiceId;
    FString ResolutionSummary;
    FString UnresolvedPenaltySummary;
    FString FollowUpEventType;
    FString FollowUpTitle;
    FString FollowUpDescription;
    int32 FollowUpSeverityDelta = 10;
    TArray<FDemocracyEventChoiceState> Choices;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyEventSystemState
{
    int32 LastEventTurn = 0;
    int32 EventCounter = 0;
    int32 ActiveEventLimit = 3;
    TArray<FDemocracyActiveEventState> ActiveEvents;
    TArray<FString> EventHistory;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyCitizenGroupState
{
    FString GroupName;
    int32 PopulationShare = 0;
    int32 Approval = 50;
    int32 NeedFood = 50;
    int32 NeedWater = 50;
    int32 NeedJobs = 50;
    int32 NeedSecurity = 50;
    int32 NeedHealthcare = 50;
    int32 UnrestPressure = 0;
    TArray<FString> UnrestSources;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRegionState
{
    FString RegionName;
    FString Climate;
    int32 PopulationShare = 0;
    int32 Approval = 50;
    int32 Stability = 50;
    int32 Unrest = 20;
    int32 FoodAccess = 55;
    int32 WaterAccess = 55;
    int32 Jobs = 55;
    int32 Security = 55;
    int32 Infrastructure = 50;
    TArray<FString> UnrestSources;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyDemographicsState
{
    int32 TotalPopulationThousands = 0;
    int32 AverageGroupApproval = 50;
    int32 AverageRegionalApproval = 50;
    int32 NationalNeedsPressure = 0;
    int32 DemographicUnrestPressure = 0;
    TArray<FDemocracyCitizenGroupState> CitizenGroups;
    TArray<FDemocracyRegionState> Regions;
    TArray<FString> NationalUnrestSources;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyEconomyBudgetState
{
    int32 TaxRate = 22;
    FString TaxPolicy = TEXT("Balanced Taxation");
    int32 PublicServicesSpending = 35;
    int32 InfrastructureSpending = 25;
    int32 DefenseSpending = 25;
    int32 Debt = 0;
    int32 Income = 0;
    int32 Expenses = 0;
    int32 Deficit = 0;
    int32 Inflation = 2;
    int32 PublicServices = 55;
    int32 ProductionEfficiency = 50;
    int32 DebtCapacity = 1200;
    int32 SpendingLimit = 120;
    int32 CreditStress = 0;
    bool bSpendingLimited = false;
    FString SpendingPosture = TEXT("Balanced Services");
    FString BudgetConstraintStatus = TEXT("Budget constraints awaiting first recalculation.");
    FString LastBudgetSummary = TEXT("Initial budget loaded.");

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyDepartmentState
{
    FString DepartmentName;
    FString MinisterTitle;
    FString Domain;
    int32 BudgetShare = 10;
    int32 Staffing = 50;
    int32 Effectiveness = 50;
    int32 PublicTrust = 50;
    int32 Priority = 50;
    FString CurrentAction = TEXT("Routine Operations");
    FString PolicyInterface;
    FString AdvisorySummary;
    TArray<FString> ActionEffects;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyDepartmentSystemState
{
    TArray<FDemocracyDepartmentState> Departments;
    int32 LastUpdatedTurn = 1;
    int32 Coordination = 50;
    FString Summary = TEXT("Departments awaiting first simulation review.");

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyApprovalCauseState
{
    FString CauseName;
    FString Category;
    int32 ApprovalImpact = 0;
    int32 UnrestImpact = 0;
    int32 StabilityImpact = 0;
    int32 Severity = 0;
    FString SourceMetric;
    FString CurrentStatus;
    TArray<FString> SuggestedResponses;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyApprovalStabilityModelState
{
    TArray<FDemocracyApprovalCauseState> Causes;
    int32 NetApprovalPressure = 0;
    int32 NetUnrestPressure = 0;
    int32 NetStabilityPressure = 0;
    int32 LastUpdatedTurn = 1;
    FString Summary = TEXT("Approval and stability causes awaiting first simulation review.");

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyPressReleaseRecordState
{
    int32 Turn = 1;
    FString AnnouncementType;
    FString MessageQuality;
    bool bTruthful = true;
    int32 ApprovalDelta = 0;
    int32 StabilityDelta = 0;
    int32 DiplomacyDelta = 0;
    int32 UnrestDelta = 0;
    int32 CredibilityDelta = 0;
    int32 CredibilityAfter = 70;
    FString Summary;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyPressOfficeState
{
    int32 Credibility = 70;
    int32 ConsecutiveEmptyAnnouncements = 0;
    int32 ConsecutiveFalseAnnouncements = 0;
    int32 TotalAnnouncements = 0;
    int32 LastUpdatedTurn = 1;
    int32 MaxRecords = 40;
    FString LastAnnouncementSummary = TEXT("No press releases have been made yet.");
    TArray<FDemocracyPressReleaseRecordState> Records;

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyMeetingRecordState
{
    int32 Turn = 1;
    FString MeetingType;
    FString ParticipantName;
    FString AgendaItem;
    FString OutcomeSummary;
    int32 ApprovalDelta = 0;
    int32 StabilityDelta = 0;
    int32 UnrestDelta = 0;
    int32 DiplomacyDelta = 0;
    int32 TreasuryDelta = 0;
    int32 EconomyDelta = 0;
    int32 MilitaryDelta = 0;
    int32 InfrastructureDelta = 0;
    int32 AdvisorCoordinationDelta = 0;
    int32 ForeignTrustDelta = 0;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyMeetingSystemState
{
    TArray<FDemocracyMeetingRecordState> Records;
    int32 TotalMeetings = 0;
    int32 AdvisorCoordination = 50;
    int32 ForeignTrust = 50;
    int32 LastUpdatedTurn = 1;
    int32 MaxRecords = 50;
    FString LastMeetingSummary = TEXT("No meetings have been held yet.");

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyDevelopmentTrackState
{
    FString TrackName;
    FString FocusArea;
    int32 Level = 1;
    int32 Progress = 0;
    int32 ProgressTarget = 100;
    int32 TreasuryCost = 25;
    int32 WoodCost = 0;
    int32 MetalsCost = 0;
    int32 FuelCost = 0;
    FString CurrentProject;
    FString StrategicBenefit;
    TArray<FString> Unlocks;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyDevelopmentSystemState
{
    TArray<FDemocracyDevelopmentTrackState> Tracks;
    FString ActiveFocus = TEXT("Infrastructure");
    int32 DevelopmentPoints = 0;
    int32 LastUpdatedTurn = 1;
    FString Summary = TEXT("Development system awaiting first review.");

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyDecisionRecordState
{
    int32 Turn = 1;
    FString Category;
    FString DecisionTitle;
    FString DecisionDetail;
    FString ConsequenceSummary;
    int32 ApprovalAfter = 0;
    int32 StabilityAfter = 0;
    int32 UnrestAfter = 0;
    int32 TreasuryAfter = 0;
    int32 EconomyAfter = 0;
    int32 MilitaryAfter = 0;
    int32 Severity = 0;
    FString TimestampUtc;
    TArray<FString> Tags;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyDecisionHistoryState
{
    TArray<FDemocracyDecisionRecordState> Records;
    int32 LastUpdatedTurn = 1;
    int32 MaxRecords = 80;
    FString Summary = TEXT("No major decisions logged yet.");

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyGeneratedCountryState
{
    FString CountryId;
    FString MapRegionId;
    FString CountryName;
    FString ContinentName;
    FString Climate;
    FString PoliticalType;
    FString DiplomaticAlignment;
    int32 MapCountryIndex = 0;
    int32 DesiredProvinceCount = 0;
    int32 PopulationWeight = 0;
    int32 AreaWeight = 0;
    int32 PowerScore = 40;
    int32 Stability = 50;
    int32 BorderPressure = 0;
    bool bAlliedWithPlayer = false;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyContinentState
{
    FString ContinentName;
    FString Climate;
    int32 CountryCount = 0;
    TArray<FDemocracyGeneratedCountryState> Countries;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyWorldMapState
{
    FString PlanetName = TEXT("Dulia");
    FString MapDataVersion = TEXT("DuliaMapData.v1");
    int32 ContinentCount = 8;
    int32 DurableCountryTarget = 195;
    int32 ActiveCountryCount = 0;
    int32 TotalCountryCount = 0;
    int32 TotalProvinceCount = 0;
    int32 TotalMapRegionCount = 0;
    int32 DemocraticAllyCount = 0;
    int32 NonDemocraticCountryCount = 0;
    FString GenerationRule;
    FString MapDataSummary = TEXT("Planet Dulia map data has not been generated yet.");
    TArray<FDemocracyContinentState> Continents;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyDiplomacyRelationshipState
{
    FString CountryName;
    FString ContinentName;
    FString GovernmentType;
    FString RelationshipStatus = TEXT("Neutral");
    bool bTradePartner = false;
    bool bSanctionsActive = false;
    FString TreatyStatus = TEXT("None");
    int32 BorderTension = 0;
    int32 Trust = 50;
    int32 TradeValue = 0;
    int32 LastChangedTurn = 1;
    TArray<FString> ActiveTreaties;
    TArray<FString> Notes;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyDiplomacyMatrixState
{
    int32 LastUpdatedTurn = 1;
    int32 AllyCount = 0;
    int32 NeutralCount = 0;
    int32 RivalCount = 0;
    int32 HostileCount = 0;
    int32 TradePartnerCount = 0;
    int32 SanctionsCount = 0;
    int32 TreatyCount = 0;
    int32 AverageBorderTension = 0;
    FString Summary = TEXT("Diplomacy relationship matrix has not been initialized yet.");
    TArray<FDemocracyDiplomacyRelationshipState> Relationships;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyGovernmentDiplomacyRuleState
{
    FString RuleId;
    FString RuleName;
    FString RuleType;
    FString Description;
    bool bEnabled = true;
    int32 TrustThreshold = 0;
    int32 BorderTensionThreshold = 0;
    int32 StabilityCost = 0;
    int32 UnrestCost = 0;
    int32 DiplomacyCost = 0;
    int32 TurnsRequired = 0;
    TArray<FString> AllowedGovernmentTypes;
    TArray<FString> BlockedGovernmentTypes;
    TArray<FString> Consequences;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyGovernmentDiplomacyRulesState
{
    int32 LastUpdatedTurn = 1;
    FString PlayerGovernmentType = TEXT("Democracy");
    FString TargetGovernmentType;
    int32 TransitionProgress = 0;
    int32 TransitionTurnsRemaining = 0;
    int32 TransitionStabilityCost = 0;
    int32 TransitionUnrestCost = 0;
    int32 TransitionDiplomacyCost = 0;
    int32 AllowedAllianceCount = 0;
    int32 BlockedAllianceCount = 0;
    int32 ActiveTreatyCount = 0;
    int32 ActiveSanctionsCount = 0;
    int32 HighBorderTensionCount = 0;
    FString Summary = TEXT("Government/diplomacy rules have not been evaluated yet.");
    TArray<FDemocracyGovernmentDiplomacyRuleState> Rules;
    TArray<FString> ActiveRestrictions;
    TArray<FString> SideSwitchConsequences;

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyCountryState
{
    FString CountryName;
    FString LeaderGender;
    FString AddressTitle;
    FString Climate;
    FString Difficulty;
    FString CountrySize;
    int32 CountrySizeScore = 1;
    int32 PublicApproval = 50;
    int32 Stability = 50;
    int32 Unrest = 20;
    int32 Treasury = 0;
    int32 EconomicHealth = 50;
    int32 DiplomaticStanding = 50;
    int32 Technology = 1;
    int32 MilitaryReadiness = 25;
    int32 Infrastructure = 35;
    int32 EnvironmentalHealth = 55;
    FDemocracyResourceInventory Resources;
    FDemocracyPolicyState Policies;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRivalCountryState
{
    FString CountryName;
    FString Temperament;
    FString RelationToPlayer;
    int32 PowerScore = 50;
    int32 BorderPressure = 0;
    int32 TradeValue = 0;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsOutcomeState
{
    int32 Turn = 1;
    FString OutcomeId;
    FString ConflictName;
    FString OpponentCountry;
    FString OutcomeType = TEXT("Stalemate");
    FString ImportEventType = TEXT("Unspecified");
    FString AttentionCategory = TEXT("General");
    FString AffectedCountryName;
    FString AffectedProvinceId;
    FString AffectedProvinceName;
    FString AffectedResource;
    int32 AttentionDeadlineTurn = 0;
    int32 AttentionSeverity = 0;
    int32 TerritoryDelta = 0;
    int32 Casualties = 0;
    int32 ResourceDisruption = 0;
    int32 WarFatigueDelta = 0;
    int32 DiplomaticDamage = 0;
    int32 StabilityDelta = 0;
    int32 InvasionRiskDelta = 0;
    int32 BudgetStrain = 0;
    bool bRequiresSimulationAttention = true;
    bool bAcknowledgedBySimulation = false;
    bool bAppliedToSimulation = false;
    FString SimulationAttentionStatus = TEXT("Queued");
    FString AttentionSummary;
    FString Summary;
    TArray<FString> ConsequenceTags;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsBackflowState
{
    int32 LastAppliedTurn = 0;
    int32 PendingOutcomeCount = 0;
    int32 PendingAttentionCount = 0;
    int32 BattleLossCount = 0;
    int32 ProvinceCaptureCount = 0;
    int32 CapitalThreatCount = 0;
    int32 SupplyRouteBreakCount = 0;
    int32 TotalTerritoryDelta = 0;
    int32 TotalCasualties = 0;
    int32 WarFatigue = 0;
    int32 ResourceDisruptionPressure = 0;
    int32 BudgetStrainPressure = 0;
    int32 DiplomaticDamagePressure = 0;
    FString LastOutcomeSummary = TEXT("No RTS outcomes have been applied to the simulation yet.");
    FString LastImportQueueSummary = TEXT("No RTS import queue items are waiting for simulation attention.");
    TArray<FDemocracyRtsOutcomeState> PendingOutcomes;
    TArray<FDemocracyRtsOutcomeState> OutcomeHistory;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyProvinceOwnershipState
{
    FString ProvinceId;
    FString CountryId;
    FString MapRegionId;
    FString ProvinceName;
    FString ContinentName;
    FString OriginalCountryName;
    FString CurrentOwnerCountryName;
    FString CurrentControllerCountryName;
    FString GovernmentType;
    FString Climate;
    FString ResourceFocus;
    FString TerrainType;
    int32 ProvinceIndex = 0;
    int32 PopulationWeight = 0;
    int32 AreaWeight = 0;
    int32 StrategicValue = 1;
    int32 Stability = 50;
    int32 Unrest = 20;
    bool bPlayerControlled = false;
    bool bBorderProvince = false;
    int32 LastChangedTurn = 1;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyCountryOwnershipState
{
    FString CountryId;
    FString MapRegionId;
    FString CountryName;
    FString ContinentName;
    FString GovernmentType;
    int32 MapCountryIndex = 0;
    int32 TotalProvinces = 0;
    int32 ControlledProvinces = 0;
    int32 OccupiedProvinces = 0;
    int32 LostProvinces = 0;
    int32 BorderProvinces = 0;
    int32 ResourceBase = 0;
    int32 MilitaryValue = 0;
    int32 PopulationWeight = 0;
    int32 AreaWeight = 0;
    bool bPlayerCountry = false;
    bool bCapitalControlled = true;
    TArray<FString> ProvinceIds;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyContinentOwnershipState
{
    FString ContinentName;
    FString Climate;
    int32 CountryCount = 0;
    int32 ProvinceCount = 0;
    int32 PlayerControlledProvinces = 0;
    int32 ContestedProvinces = 0;
    TArray<FString> CountryNames;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyMapOwnershipState
{
    FString PlanetName = TEXT("Dulia");
    FString MapDataVersion = TEXT("DuliaMapData.v1");
    int32 DurableCountryTarget = 195;
    int32 LastUpdatedTurn = 1;
    int32 TotalCountries = 0;
    int32 TotalProvinces = 0;
    int32 TotalMapRegionCount = 0;
    int32 TotalPopulationWeight = 0;
    int32 TotalAreaWeight = 0;
    int32 PlayerControlledProvinces = 0;
    int32 ContestedProvinces = 0;
    int32 BorderProvinceCount = 0;
    FString PlayerCountryName;
    FString Summary = TEXT("Map ownership model has not been initialized yet.");
    TArray<FDemocracyProvinceOwnershipState> Provinces;
    TArray<FDemocracyCountryOwnershipState> Countries;
    TArray<FDemocracyContinentOwnershipState> Continents;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsScopeBoundaryState
{
    FString ScopeVersion = TEXT("RTSScope.v1");
    FString ScopeSummary = TEXT("RTS owns tactical execution and city/base activity; simulation owns national governance and strategic authority.");
    TArray<FString> RtsOwns;
    TArray<FString> SimulationOwns;
    TArray<FString> BlockedUntilRts;
    TArray<FString> BackflowRequired;
    TArray<FString> CandidateAssetPacks;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsViewModeState
{
    FString ViewId;
    FString DisplayName;
    FString Purpose;
    bool bImplementedPlaceholder = true;
    bool bDefaultView = false;
    TArray<FString> Interactions;
    TArray<FString> VisibleLayers;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsBuildingState
{
    FString BuildingId;
    FString DisplayName;
    FString BuildingType;
    FString ResourceFocus;
    FString CandidateAssetHint;
    int32 Level = 1;
    int32 BuildCost = 0;
    int32 UpgradeCost = 0;
    int32 BuildTimeTurns = 1;
    int32 ProductionPerTick = 0;
    int32 DefenseValue = 0;
    bool bConstructed = true;
    bool bUpgradeQueued = false;
    FString Status = TEXT("Operational");
    TArray<FString> Prerequisites;
    TArray<FString> RuntimeTags;
    int32 MaxHealth = 100;
    int32 CurrentHealth = 100;
    int32 DamagePercent = 0;
    int32 RepairCost = 0;
    bool bDisabled = false;
    FString DisabledReason;

    FString ToJson(int32 IndentSpaces = 2) const;
};


struct FDemocracyRtsUnitDefinitionState
{
    FString UnitId;
    FString DisplayName;
    FString UnitCategory;
    FString Role;
    FString ProducedByBuildingId;
    FString CandidateAssetHint;
    int32 BuildCost = 0;
    int32 BuildTimeTurns = 1;
    int32 SupplyCost = 1;
    int32 AttackPower = 0;
    int32 DefensePower = 0;
    int32 Mobility = 0;
    int32 Range = 0;
    int32 CargoCapacity = 0;
    int32 ReconValue = 0;
    bool bUnlocked = true;
    bool bDefensiveOnly = false;
    TArray<FString> Prerequisites;
    TArray<FString> TacticalTags;

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyRtsArmyGroupState
{
    FString ArmyId;
    FString DisplayName;
    FString CurrentCountryName;
    FString CurrentProvinceId;
    FString DestinationProvinceId;
    FString RallyPointId;
    FString MovementState = TEXT("Idle");
    int32 InfantryCount = 0;
    int32 VehicleCount = 0;
    int32 AircraftCount = 0;
    int32 LogisticsCount = 0;
    int32 ScoutCount = 0;
    int32 DefensiveUnitCount = 0;
    int32 TotalStrength = 0;
    int32 SupplyStatus = 100;
    int32 MovementTurnsRemaining = 0;
    bool bSelected = false;
    FString ActiveOrderType = TEXT("Defend");
    FString OrderTargetProvinceId;
    FString OrderTargetType = TEXT("Province");
    int32 OrderTurnsRemaining = 0;
    int32 Morale = 70;
    bool bSupplyRouteBroken = false;
    TArray<FString> AssignedUnitIds;
    TArray<FString> Orders;

    FString ToJson(int32 IndentSpaces = 2) const;
};



struct FDemocracyRtsMovementOrderState
{
    FString OrderId;
    FString ArmyId;
    FString OrderType = TEXT("Move");
    FString SourceProvinceId;
    FString TargetProvinceId;
    FString RallyPointId;
    int32 IssuedTurn = 0;
    int32 TotalTurns = 1;
    int32 TurnsRemaining = 1;
    bool bActive = true;
    bool bComplete = false;
    bool bCancelled = false;
    FString StatusSummary;
    TArray<FString> AllowedFollowUps;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsSupplyRouteState
{
    FString RouteId;
    FString ArmyId;
    FString SourceProvinceId;
    FString DestinationProvinceId;
    int32 SupplyStatus = 100;
    int32 DistancePenalty = 0;
    int32 Disruption = 0;
    bool bBroken = false;
    FString StatusSummary;
    TArray<FString> Risks;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsBattleResolutionState
{
    FString BattleId;
    FString ArmyId;
    FString ProvinceId;
    FString OpponentCountry;
    FString TerrainType;
    int32 PlayerScore = 0;
    int32 OpponentScore = 0;
    int32 ReadinessModifier = 0;
    int32 TerrainModifier = 0;
    int32 SupplyModifier = 0;
    int32 TechModifier = 0;
    int32 MoraleModifier = 0;
    FString Result = TEXT("Unresolved");
    FString Summary;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsConstructionQueueEntryState
{
    FString QueueId;
    FString BuildingId;
    FString DisplayName;
    FString QueueType = TEXT("Build");
    int32 TargetLevel = 1;
    int32 TotalTurns = 1;
    int32 TurnsRemaining = 1;
    int32 FoodCost = 0;
    int32 FuelCost = 0;
    int32 WoodCost = 0;
    int32 MetalsCost = 0;
    int32 TreasuryCost = 0;
    bool bCanCancel = true;
    bool bCancelled = false;
    bool bComplete = false;
    FString CancelRefundRule = TEXT("Refund 50 percent of unspent resources while construction is active.");
    TArray<FString> Prerequisites;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsResourceCollectionState
{
    int32 LastUpdatedTurn = 0;
    int32 FoodFromBuildings = 0;
    int32 FuelFromBuildings = 0;
    int32 WoodFromBuildings = 0;
    int32 MetalsFromBuildings = 0;
    int32 FoodFromProvinces = 0;
    int32 FuelFromProvinces = 0;
    int32 WoodFromProvinces = 0;
    int32 MetalsFromProvinces = 0;
    int32 FoodSentToSimulation = 0;
    int32 FuelSentToSimulation = 0;
    int32 WoodSentToSimulation = 0;
    int32 MetalsSentToSimulation = 0;
    int32 DisruptionPenalty = 0;
    FString Summary = TEXT("RTS resource collection has not ticked yet.");
    TArray<FString> CollectionSources;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsSelectableTargetState
{
    FString TargetId;
    FString DisplayName;
    FString TargetType;
    FString CountryName;
    FString ProvinceId;
    FString InteractionMode;
    int32 StrategicValue = 0;
    bool bSelectable = true;
    bool bSelected = false;
    TArray<FString> AvailableActions;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsWorldInteractionState
{
    FString ActiveSelectionId;
    FString ActiveSelectionType;
    FString HoveredTargetId;
    FString LastInteractionSummary = TEXT("No world-map target selected.");
    TArray<FDemocracyRtsSelectableTargetState> SelectableTargets;

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyRtsCityBaseState
{
    FString BaseId = TEXT("capital-base");
    FString DisplayName = TEXT("Capital Command District");
    FString LinkedCountryName;
    FString LinkedProvinceId;
    FString ViewModeId = TEXT("city_base");
    int32 GridWidth = 12;
    int32 GridHeight = 18;
    int32 BuildQueueCount = 0;
    int32 UpgradeQueueCount = 0;
    FString BaseSummary = TEXT("Placeholder RTS city/base layout is ready for buildings, production, and upgrades.");
    TArray<FDemocracyRtsBuildingState> Buildings;
    TArray<FString> BuildQueue;
    TArray<FDemocracyRtsConstructionQueueEntryState> ConstructionQueue;
    TArray<FString> RuntimeNotes;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsWorldState
{
    int32 SimulationSecond = 0;
    int32 ControlledTerritories = 1;
    int32 BorderTerritories = 2;
    int32 KnownRivalCountries = 3;
    FString ActiveViewMode = TEXT("world_map");
    TArray<FString> ActiveStrategicLayers;
    FDemocracyRtsScopeBoundaryState ScopeBoundary;
    TArray<FDemocracyRtsViewModeState> ViewModes;
    FDemocracyRtsCityBaseState CityBase;
    TArray<FDemocracyRtsUnitDefinitionState> UnitCatalog;
    TArray<FDemocracyRtsArmyGroupState> ArmyGroups;
    TArray<FDemocracyRtsMovementOrderState> MovementOrders;
    TArray<FDemocracyRtsSupplyRouteState> SupplyRoutes;
    TArray<FDemocracyRtsBattleResolutionState> BattleHistory;
    FDemocracyRtsResourceCollectionState ResourceCollection;
    FDemocracyRtsWorldInteractionState WorldInteraction;
    TArray<FDemocracyRivalCountryState> Rivals;
    FDemocracyRtsBackflowState Backflow;
    FDemocracyMapOwnershipState Ownership;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsSaveBoundaryState
{
    int32 LastUpdatedTurn = 1;
    FString BoundaryVersion = TEXT("RTSSaveBoundary.v1");
    FString SimulationAuthority = TEXT("Simulation owns national policy, diplomacy, economy, approval, stability, unrest, advisors, events, objectives, and failure risks.");
    FString RtsAuthority = TEXT("RTS owns unit positions, battle resolution, province control deltas, tactical objectives, fronts, and local build/movement state.");
    FString SaveAuthority = TEXT("Single-player saves store both simulation and RTS boundary data locally; multiplayer save data remains server-authoritative.");
    FString MultiplayerAuthority = TEXT("Multiplayer server owns player slots, country ownership, government side, wars, RTS outcomes, and anti-cheat validation.");
    TArray<FString> SimulationOwnedFields;
    TArray<FString> RtsOwnedFields;
    TArray<FString> SharedHandshakeFields;
    TArray<FString> SimulationExportsToRts;
    TArray<FString> RtsImportsToSimulation;
    TArray<FString> ForbiddenSimulationWrites;
    TArray<FString> SaveRules;
    TArray<FString> ServerAuthoritativeFields;
    TArray<FString> ClientRequestOnlyFields;
    TArray<FString> ServerValidationNotes;
    TArray<FString> BoundaryValidationNotes;
    FString BoundarySummary = TEXT("RTS save boundary has not been initialized yet.");

    FString ToJson(int32 IndentSpaces = 2) const;
};
struct FDemocracyRtsRegionInputState
{
    FString RegionName;
    FString Climate;
    FString ResourceFocus;
    int32 Stability = 50;
    int32 Unrest = 20;
    int32 StrategicValue = 1;
    bool bPlayerControlled = true;
    bool bBorderRegion = false;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsDiplomacyInputState
{
    FString CountryName;
    FString RelationshipStatus = TEXT("Neutral");
    FString TreatyStatus = TEXT("None");
    bool bAlly = false;
    bool bEnemy = false;
    bool bTradePartner = false;
    bool bSanctionsActive = false;
    int32 BorderTension = 0;
    int32 Trust = 50;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyWarParticipantState
{
    FString CountryName;
    FString Role = TEXT("Defender");
    FString Alignment = TEXT("Player");
    int32 Commitment = 50;
    int32 WarSupport = 50;
    int32 Casualties = 0;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyWarFrontState
{
    FString FrontName;
    FString RegionName;
    FString ContestedBorder;
    int32 Pressure = 0;
    int32 PlayerControl = 50;
    FString Status = TEXT("Quiet");

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyWarConflictState
{
    FString ConflictId;
    FString ConflictName;
    FString ConflictType = TEXT("Border Tension");
    FString Status = TEXT("Active");
    FString PrimaryObjective;
    FString EnemyObjective;
    int32 StartedTurn = 1;
    int32 LastUpdatedTurn = 1;
    int32 EscalationLevel = 1;
    int32 WarScore = 0;
    int32 VictoryProgress = 0;
    int32 DefeatRisk = 0;
    FString VictoryCondition;
    FString DefeatCondition;
    TArray<FDemocracyWarParticipantState> Participants;
    TArray<FDemocracyWarFrontState> Fronts;
    TArray<FString> ActiveModifiers;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyWarSystemState
{
    int32 LastUpdatedTurn = 1;
    int32 ActiveConflictCount = 0;
    int32 EscalationPressure = 0;
    int32 WarFatigue = 0;
    int32 TotalCasualties = 0;
    FString ReadinessStatus = TEXT("No active durable war state yet.");
    FString Summary = TEXT("War/conflict system has not been initialized yet.");
    TArray<FDemocracyWarConflictState> ActiveConflicts;
    TArray<FDemocracyWarConflictState> ConflictHistory;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracySimulationToRtsContractState
{
    int32 LastUpdatedTurn = 1;
    FString ContractVersion = TEXT("SimToRTS.v1");
    FString PlayerCountryName;
    FString GovernmentType = TEXT("Democracy");
    int32 Treasury = 0;
    int32 MilitaryReadiness = 25;
    int32 Technology = 1;
    int32 Stability = 50;
    int32 Unrest = 20;
    int32 PublicApproval = 50;
    int32 InvasionRisk = 0;
    FDemocracyResourceInventory Resources;
    TArray<FString> ActivePolicies;
    TArray<FString> TechnologyUnlocks;
    TArray<FString> Allies;
    TArray<FString> Enemies;
    TArray<FString> ActiveWars;
    TArray<FString> StrategicPermissions;
    TArray<FDemocracyRtsRegionInputState> Regions;
    TArray<FDemocracyRtsDiplomacyInputState> Diplomacy;
    FString ExportSummary = TEXT("Simulation-to-RTS contract has not been initialized yet.");

    FString ToJson(int32 IndentSpaces = 2) const;
};


struct FDemocracyCommandAuthorityActionState
{
    FString CommandId;
    FString Label;
    FString AuthorityLayer = TEXT("Office");
    FString CommandType = TEXT("Civil");
    FString ExecutionSurface = TEXT("Computer");
    bool bOfficeAllowed = true;
    bool bRtsViewAllowed = false;
    bool bEnabled = true;
    int32 CooldownTurns = 1;
    int32 LastExecutedTurn = -100;
    int32 TreasuryCost = 0;
    int32 ApprovalDelta = 0;
    int32 StabilityDelta = 0;
    int32 UnrestDelta = 0;
    int32 DiplomacyDelta = 0;
    int32 MilitaryDelta = 0;
    int32 InvasionRiskDelta = 0;
    int32 ResourceDelta = 0;
    FString Prerequisite;
    FString EffectPreview;
    FString DisabledReason;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyCommandAuthorityState
{
    int32 LastUpdatedTurn = 1;
    FString ActiveCommandPosture = TEXT("Civil Administration");
    FString OfficeAuthoritySummary = TEXT("Office authority has not been initialized yet.");
    FString RtsAuthoritySummary = TEXT("RTS authority has not been initialized yet.");
    FString LastCommandSummary = TEXT("No command authority orders issued yet.");
    TArray<FDemocracyCommandAuthorityActionState> Actions;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyObjectiveState
{
    FString Mode = TEXT("SinglePlayer");
    FString PlayerGovernmentType = TEXT("Democracy");
    FString GovernmentTransitionTarget;
    int32 GovernmentTransitionProgress = 0;
    int32 GovernmentTransitionTurnsRemaining = 0;
    int32 DemocraticCountryCount = 0;
    int32 DictatorshipCountryCount = 0;
    int32 OtherGovernmentCount = 0;
    int32 TotalTrackedCountryCount = 0;
    int32 DemocracyConversionProgress = 0;
    int32 DictatorshipsRemainingForVictory = 0;
    bool bSoftVictoryAchieved = false;
    int32 SoftVictoryTurn = 0;
    int32 PostVictoryTurnsElapsed = 0;
    bool bSimulationContinuesAfterVictory = true;
    bool bPostVictoryContinuationActive = false;
    bool bRegressionMonitoringActive = false;
    bool bRegressionWarningActive = false;
    int32 RegressionRisk = 0;
    FString VictoryCondition = TEXT("Convert all dictatorships to democracy.");
    FString PostVictoryObjective = TEXT("Prevent democratic regression while time continues.");
    FString MultiplayerServerObjective = TEXT("Ongoing server state with no final win condition.");
    bool bMultiplayerOngoingNoFinalWin = false;
    int32 ServerDemocracySlots = 0;
    int32 ServerDictatorshipSlots = 0;
    FString LongTermObjective = TEXT("Stabilize the state and convert dictatorships through diplomacy, policy pressure, and influence.");
    FString ObjectiveSummary = TEXT("Objective state has not been evaluated yet.");
    TArray<FString> ActiveObjectiveNotes;
    TArray<FString> AllianceRules;
    TArray<FString> ObjectiveHooks;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracySimulationState
{
    int32 Turn = 1;
    FString Phase = TEXT("Initial Setup");
    float RealTimeTickSeconds = 5.0f;
    bool bPaused = false;
    FDemocracyCountryState PlayerCountry;
    FDemocracyFailureRiskState FailureRisk;
    FDemocracyInvasionRiskState InvasionRisk;
    FDemocracyAdvisorSystemState AdvisorSystem;
    FDemocracyEventSystemState EventSystem;
    FDemocracyDemographicsState Demographics;
    FDemocracyEconomyBudgetState EconomyBudget;
    FDemocracyResourceProductionChainState ResourceChains;
    FDemocracyDepartmentSystemState Departments;
    FDemocracyApprovalStabilityModelState ApprovalStability;
    FDemocracyPressOfficeState PressOffice;
    FDemocracyMeetingSystemState MeetingSystem;
    FDemocracyDevelopmentSystemState DevelopmentSystem;
    FDemocracyDecisionHistoryState DecisionHistory;
    FDemocracyWorldMapState WorldMap;
    FDemocracyDiplomacyMatrixState DiplomacyMatrix;
    FDemocracyGovernmentDiplomacyRulesState GovernmentDiplomacyRules;
    FDemocracyRtsWorldState RtsWorld;
    FDemocracyRtsSaveBoundaryState RtsSaveBoundary;
    FDemocracyWarSystemState WarSystem;
    FDemocracySimulationToRtsContractState SimulationToRtsContract;
    FDemocracyCommandAuthorityState CommandAuthority;
    FDemocracyObjectiveState ObjectiveState;

    FString ToJson(int32 IndentSpaces = 2) const;
};

class FDemocracyGameStateFactory
{
public:
    static FDemocracyGovernmentDiplomacyRulesState BuildGovernmentDiplomacyRulesState(const FDemocracySimulationState& State);
    static FDemocracyRtsSaveBoundaryState BuildRtsSaveBoundaryState(const FDemocracySimulationState& State);
    static FDemocracyWarSystemState BuildWarConflictState(const FDemocracySimulationState& State);
    static FDemocracySimulationToRtsContractState BuildSimulationToRtsContractState(const FDemocracySimulationState& State);

    static FDemocracySimulationState CreateInitialState(
        const FString& StateName,
        const FString& LeaderGender,
        const FString& AddressTitle,
        const FString& Climate,
        const FDifficultyProfile& DifficultyProfile);
};






