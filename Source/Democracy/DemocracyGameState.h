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
    FString CountryName;
    FString ContinentName;
    FString Climate;
    FString PoliticalType;
    FString DiplomaticAlignment;
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
    int32 ContinentCount = 8;
    int32 TotalCountryCount = 0;
    int32 DemocraticAllyCount = 0;
    int32 NonDemocraticCountryCount = 0;
    FString GenerationRule;
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
    int32 TerritoryDelta = 0;
    int32 Casualties = 0;
    int32 ResourceDisruption = 0;
    int32 WarFatigueDelta = 0;
    int32 DiplomaticDamage = 0;
    int32 StabilityDelta = 0;
    int32 InvasionRiskDelta = 0;
    int32 BudgetStrain = 0;
    bool bAppliedToSimulation = false;
    FString Summary;
    TArray<FString> ConsequenceTags;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyRtsBackflowState
{
    int32 LastAppliedTurn = 0;
    int32 PendingOutcomeCount = 0;
    int32 TotalTerritoryDelta = 0;
    int32 TotalCasualties = 0;
    int32 WarFatigue = 0;
    int32 ResourceDisruptionPressure = 0;
    int32 BudgetStrainPressure = 0;
    int32 DiplomaticDamagePressure = 0;
    FString LastOutcomeSummary = TEXT("No RTS outcomes have been applied to the simulation yet.");
    TArray<FDemocracyRtsOutcomeState> PendingOutcomes;
    TArray<FDemocracyRtsOutcomeState> OutcomeHistory;

    FString ToJson(int32 IndentSpaces = 2) const;
};

struct FDemocracyProvinceOwnershipState
{
    FString ProvinceId;
    FString ProvinceName;
    FString ContinentName;
    FString OriginalCountryName;
    FString CurrentOwnerCountryName;
    FString CurrentControllerCountryName;
    FString GovernmentType;
    FString Climate;
    FString ResourceFocus;
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
    FString CountryName;
    FString ContinentName;
    FString GovernmentType;
    int32 TotalProvinces = 0;
    int32 ControlledProvinces = 0;
    int32 OccupiedProvinces = 0;
    int32 LostProvinces = 0;
    int32 BorderProvinces = 0;
    int32 ResourceBase = 0;
    int32 MilitaryValue = 0;
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
    int32 LastUpdatedTurn = 1;
    int32 TotalCountries = 0;
    int32 TotalProvinces = 0;
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

struct FDemocracyRtsWorldState
{
    int32 SimulationSecond = 0;
    int32 ControlledTerritories = 1;
    int32 BorderTerritories = 2;
    int32 KnownRivalCountries = 3;
    TArray<FString> ActiveStrategicLayers;
    TArray<FDemocracyRivalCountryState> Rivals;
    FDemocracyRtsBackflowState Backflow;
    FDemocracyMapOwnershipState Ownership;

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
    bool bSoftVictoryAchieved = false;
    int32 SoftVictoryTurn = 0;
    bool bSimulationContinuesAfterVictory = true;
    int32 RegressionRisk = 0;
    FString LongTermObjective = TEXT("Stabilize the state and convert dictatorships through diplomacy, policy pressure, and influence.");
    FString ObjectiveSummary = TEXT("Objective state has not been evaluated yet.");
    TArray<FString> ActiveObjectiveNotes;
    TArray<FString> AllianceRules;

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
    FDemocracyRtsWorldState RtsWorld;
    FDemocracyCommandAuthorityState CommandAuthority;
    FDemocracyObjectiveState ObjectiveState;

    FString ToJson(int32 IndentSpaces = 2) const;
};

class FDemocracyGameStateFactory
{
public:
    static FDemocracySimulationState CreateInitialState(
        const FString& StateName,
        const FString& LeaderGender,
        const FString& AddressTitle,
        const FString& Climate,
        const FDifficultyProfile& DifficultyProfile);
};




