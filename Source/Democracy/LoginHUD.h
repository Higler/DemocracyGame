#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Input/Reply.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "TimerManager.h"
#include "DemocracySaveGameRuntime.h"
#include "LoginHUD.generated.h"

class SWidget;
struct FButtonStyle;
struct FSlateColorBrush;
struct FSlateImageBrush;
struct FSlateDynamicImageBrush;
class AOfficeLevelBuilder;
class AOfficePlayerPawn;
class UUserWidget;

enum class ELoginFlowScreen : uint8
{
    Login,
    Settings,
    GameModeSelection,
    LocalSaveSelection,
    DifficultySelection,
    NewStateSetup,
    LoadedGame,
    MultiplayerStateSelection,
    ServerSelection,
    OfficeNoOverlay,
    OfficeOpeningBriefing,
    OfficeDashboard,
    OfficeComputerMenu,
    OfficePolicies,
    OfficeEvents,
    OfficeDemographics,
    OfficeBudget,
    OfficeResourceChains,
    OfficeDepartments,
    OfficeDevelopment,
    OfficeApprovalStability,
    OfficeDecisionHistory,
    OfficeWorldRts,
    OfficeAdvisorWarnings,
    OfficeMeetingAdvisor,
    OfficePressRelease,
    GameOver
};

UCLASS()
class DEMOCRACY_API ALoginHUD : public AHUD
{
    GENERATED_BODY()

public:
    void HandleOfficeInteractable(const FString& InteractionName);
    void CloseOfficeOverlayFromInput();

    UFUNCTION(Exec)
    void DemocracySetDebugRole(const FString& RoleName);

    UFUNCTION(Exec)
    void DemocracyClearDebugRole();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    TSharedPtr<SWidget> LoginScreenWidget;
    TSharedPtr<SScrollBox> NewCountrySetupScrollBox;
    TSharedPtr<FSlateImageBrush> BackgroundBrush;
    TSharedPtr<FSlateDynamicImageBrush> WorldMapBrush;
    TSharedPtr<FSlateDynamicImageBrush> RtsLandMapBrush;
    TSharedPtr<FSlateDynamicImageBrush> SetupCountryMapBrush;
    TSharedPtr<FSlateColorBrush> RtsWaterBrush;
    TSharedPtr<FSlateColorBrush> OverlayBrush;
    TSharedPtr<FSlateColorBrush> PanelBrush;
    TSharedPtr<FSlateColorBrush> RowBrush;
    TSharedPtr<FButtonStyle> LoginButtonStyle;

    UPROPERTY(Transient)
    TSubclassOf<UUserWidget> SettingsMenuWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> SettingsMenuWidget;
    ELoginFlowScreen CurrentScreen = ELoginFlowScreen::Login;
    FString MockUserName;
    FString MockPassword;
    FString LocalSaveSearchText;
    FString PendingDifficulty;
    FString PendingStateName;
    FString PendingClimate;
    FString PendingLeaderGender;
    FString PendingAddressTitle;
    FString PendingStartingCountryName;
    FString PendingStartingCountrySearchText;
    int32 PendingStartingCountryMapIndex = 0;
    float PendingStartingCountryListScrollOffset = 0.0f;
    float PendingNewCountrySetupScrollOffset = 0.0f;
    bool bNewCountryClimateExpanded = true;
    bool bNewCountryGenderExpanded = true;
    bool bNewCountryLocationExpanded = true;
    FString LoadedStateName;
    FString LoadedSavePath;
    FString LoadedSaveSummary;
    FString LoadedSaveError;
    FString LastSaveStatus;
    FString GameOverReason;
    FString GameOverDetails;
    FString SelectedMeetingAdvisorName;
    FString SelectedMeetingAdvisorFocus;
    FString WorldRtsEntryMode = TEXT("Globe");
    float RtsMapZoom = 1.0f;
    FVector2D RtsMapPan = FVector2D::ZeroVector;
    bool bIsDraggingRtsMap = false;
    bool bRtsMapDragMoved = false;
    FVector2D LastRtsMapDragScreenPosition = FVector2D::ZeroVector;
    FVector2D RtsMapMouseDownScreenPosition = FVector2D::ZeroVector;
    FString RtsSelectedCountryName;
    FString RtsSelectedProvinceId;
    FString RtsSelectedArmyId;
    FString PendingRtsOrderType;
    FString PendingRtsOrderTargetProvinceId;
    FString PendingRtsOrderConfirmationText;
    FDemocracyLoadedSaveState LoadedSaveState;
    bool bHasLoadedRuntimeState = false;
    FTimerHandle SimulationTickTimerHandle;
    int32 SimulationTickCount = 0;
    FString SimulationTickSummary;
    FString OpeningScriptText;
    FString SelectedOnlineState;
    FString ServerSearchText;
#if !UE_BUILD_SHIPPING
    FString DebugToolRole = TEXT("Player");
#endif

    float MasterVolume = 0.80f;
    float MusicVolume = 0.65f;
    float EffectsVolume = 0.75f;
    float VoiceVolume = 0.70f;
    float Brightness = 0.55f;
    float UiScale = 1.00f;
    bool bFullscreen = false;
    bool bVSync = true;
    bool bInvertLookY = false;
    bool bRememberLoginDetails = false;
    bool bShowFirstLoginBriefing = false;
    bool bShowRecentLocalSavesOnly = false;
    bool bShowRecommendedServersOnly = false;
    bool bInOfficeMode = false;
    bool bOfficePrototypeSpawned = false;
    TWeakObjectPtr<AOfficeLevelBuilder> OfficeLevelBuilder;
    TWeakObjectPtr<AOfficePlayerPawn> OfficePlayerPawn;

    void ShowScreen(ELoginFlowScreen Screen);
    void RefreshLoginWidget();
    void TearDownLoginWidget();

    TSharedRef<SWidget> BuildCurrentScreen();
    TSharedRef<SWidget> BuildLoginScreen();
    TSharedRef<SWidget> BuildSettingsScreen();
    TSharedRef<SWidget> BuildSettingsMenuProScreen();
    TSharedRef<SWidget> BuildGameModeSelectionScreen();
    TSharedRef<SWidget> BuildLocalSaveSelectionScreen();
    TSharedRef<SWidget> BuildDifficultySelectionScreen();
    TSharedRef<SWidget> BuildNewStateSetupScreen();
    TSharedRef<SWidget> BuildStartingCountrySelectionWidget();
    TArray<FDemocracyGeneratedCountryState> BuildStartingCountryOptions() const;
    TSharedRef<SWidget> BuildLoadedGameScreen();
    TSharedRef<SWidget> BuildMultiplayerStateSelectionScreen();
    TSharedRef<SWidget> BuildServerSelectionScreen();
    TSharedRef<SWidget> BuildOfficeOpeningBriefingScreen();
    TSharedRef<SWidget> BuildOfficeDashboardScreen();
    TSharedRef<SWidget> BuildOfficeComputerMenuScreen();
    TSharedRef<SWidget> BuildOfficePoliciesScreen();
    TSharedRef<SWidget> BuildOfficeEventsScreen();
    TSharedRef<SWidget> BuildOfficeDemographicsScreen();
    TSharedRef<SWidget> BuildOfficeBudgetScreen();
    TSharedRef<SWidget> BuildOfficeResourceChainsScreen();
    TSharedRef<SWidget> BuildOfficeDepartmentsScreen();
    TSharedRef<SWidget> BuildOfficeDevelopmentScreen();
    TSharedRef<SWidget> BuildOfficeApprovalStabilityScreen();
    TSharedRef<SWidget> BuildOfficeDecisionHistoryScreen();
    TSharedRef<SWidget> BuildOfficeWorldRtsScreen();
    FReply HandleRtsMapMouseWheel(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
    FReply HandleRtsMapMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
    FReply HandleRtsMapMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
    FReply HandleRtsMapMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
    FReply HandleZoomRtsMapInClicked();
    FReply HandleZoomRtsMapOutClicked();
    FReply HandleResetRtsMapViewClicked();
    FReply HandleFocusRtsMapSelectionClicked();
    FReply HandleFocusRtsMapPlayerClicked();
    FString GetRtsZoomModeLabel() const;
    FString BuildRtsSelectedTerritoryText() const;
    FString BuildRtsSelectedArmyText() const;
    FString BuildRtsActionText() const;
    FString BuildRtsBattlePresentationText() const;
    TSharedRef<SWidget> BuildRtsBattlePresentationWidget();
    TSharedRef<SWidget> BuildRtsOccupationActionsWidget();
    TSharedRef<SWidget> BuildRtsSupplyActionsWidget();
    TSharedRef<SWidget> BuildRtsDiplomacyActionsWidget();
    TSharedRef<SWidget> BuildRtsNotificationsWidget();
    FString BuildRtsCommandConsequenceText(const FString& OrderType, const FString& TargetProvinceId) const;
    FString BuildRtsConstructionAvailabilityText(const FDemocracyRtsBuildingState* Building, int32 SlotIndex, const FString& SlotFocus, bool bUpgrade) const;
    FString BuildRtsCityBaseSummaryText() const;
    FString BuildRtsOfficeAlertText() const;
    TSharedRef<SWidget> BuildRtsCityBasePlaceholderWidget();
    TSharedRef<SWidget> BuildRtsResourceNodesWidget() const;
    TSharedRef<SWidget> BuildRtsProvinceStateOverlaysWidget(float MapWidth, float MapHeight) const;
    TSharedRef<SWidget> BuildRtsFogOverlaysWidget(float MapWidth, float MapHeight) const;
    TSharedRef<SWidget> BuildRtsArmyMarkersWidget(float MapWidth, float MapHeight);
    TSharedRef<SWidget> BuildRtsOrderButtonsWidget();
    void SelectRtsMapAtViewportPosition(const FGeometry& Geometry, const FVector2D& ScreenPosition);
    bool TryIssueRtsOrderToProvince(const FString& TargetProvinceId);
    void ClampRtsMapView();
    void FocusRtsMapOnSelection();
    FReply HandleSelectRtsOrder(FString OrderType);
    FReply HandleConfirmRtsOrderClicked();
    FReply HandleCancelRtsOrderConfirmationClicked();
    FReply HandleSelectRtsBuildingSlot(int32 SlotIndex);
    FReply HandleQueueRtsBuildSlot(int32 SlotIndex, FString ResourceFocus);
    FReply HandleQueueRtsUpgrade(FString BuildingId);
    FReply HandleCancelRtsConstruction(FString QueueId);
    FReply HandleRtsBattleFollowUpClicked(FString FollowUpAction);
    FReply HandleRtsOccupationActionClicked(FString ActionName);
    FReply HandleRtsSupplyActionClicked(FString RouteId, FString ActionName);
    FReply HandleRecruitRtsUnitsClicked(FString UnitType);
    FReply HandleRtsMapDiplomacyActionClicked(FString CommandId);
    FReply HandleFocusRtsAlertClicked(FString ProvinceId, FString ArmyId, FString CityBaseId);
    TSharedRef<SWidget> BuildOfficeAdvisorWarningsScreen();
    TSharedRef<SWidget> BuildOfficeMeetingAdvisorScreen();
    TSharedRef<SWidget> BuildOfficePressReleaseScreen();
    TSharedRef<SWidget> BuildGameOverScreen();

    TSharedRef<SWidget> BuildPanel(const FString& Title, const FString& Subtitle, const TSharedRef<SWidget>& Body, float Width = 620.0f, FOnUserScrolled ScrollHandler = FOnUserScrolled(), float InitialScrollOffset = 0.0f, TSharedPtr<SScrollBox>* OutScrollBox = nullptr);
    TSharedRef<SWidget> BuildButton(const FString& Label, FOnClicked ClickHandler, float Width = 300.0f, float Height = 52.0f, bool bEnabled = true) const;
    TSharedRef<SWidget> BuildBackButton();
    TSharedRef<SWidget> BuildInfoRow(const FString& Primary, const FString& Secondary) const;
    TSharedRef<SWidget> BuildSliderRow(const FString& Label, float Value, FOnFloatValueChanged ValueChanged) const;
    TSharedRef<SWidget> BuildCheckRow(const FString& Label, const FString& Detail, bool bChecked, FOnCheckStateChanged CheckChanged) const;
    TSharedRef<SWidget> BuildKeybindRow(const FString& ActionName, const FString& CurrentKey);
    TArray<FString> GetLocalSaveNames() const;
    FString BuildSafeSaveFileName(const FString& StateName) const;
    bool CreateInitialSinglePlayerSave(FString& OutSavePath);
    bool LoadSinglePlayerSaveIntoRuntime(const FString& SavePath);
    FString BuildSimulationStatusText() const;
    FString BuildResourceStatusText() const;
    FString BuildPolicyStatusText() const;
    FString BuildEventStatusText() const;
    FString BuildDemographicsStatusText() const;
    FString BuildEconomyBudgetStatusText() const;
    FString BuildResourceChainStatusText() const;
    FString BuildDepartmentStatusText() const;
    FString BuildDevelopmentStatusText() const;
    FString BuildApprovalStabilityStatusText() const;
    FString BuildPressOfficeStatusText() const;
    FString BuildMeetingSystemStatusText() const;
    FString BuildDecisionHistoryStatusText() const;
    FString BuildObjectiveStatusText() const;
    FString BuildDiplomacyStatusText() const;
    FString BuildGovernmentDiplomacyRulesStatusText() const;
    FString BuildRtsBackflowStatusText() const;
    FString BuildWarConflictStatusText() const;
    FString BuildRtsSaveBoundaryStatusText() const;
    FString BuildMapOwnershipStatusText() const;
    FString BuildSimulationToRtsContractStatusText() const;
    FString BuildCommandAuthorityStatusText() const;
    FString BuildAdvisorWarningText() const;
    FString BuildOngoingBriefingText() const;
    FString BuildTimeControlStatusText() const;
    float GetCurrentSimulationTickInterval() const;
    void ApplySimulationTickInterval(float NewIntervalSeconds);
    void StartSimulationTimer();
    void StopSimulationTimer();
    void RunSimulationTick();
    bool EvaluateFailState();
    bool EnterOfficePrototype(bool bShowOpeningBriefing);
    FString GetOpeningScriptText() const;
    FString GetAddressTitleForGender(const FString& GenderName) const;
    FString GetRememberLoginPath() const;
    void LoadRememberedLoginDetails();
    void SaveRememberedLoginDetails() const;
    void ClearRememberedLoginDetails() const;
#if !UE_BUILD_SHIPPING
    bool IsSinglePlayerDebugContext() const;
    bool HasGameMasterDebugAccess() const;
    bool HasAdministratorDebugAccess() const;
    FString BuildDebugAccessStatusText() const;
#endif

    FReply HandleSignInClicked();
    FReply HandleSignUpClicked();
    FReply HandleSettingsClicked();
    FReply HandleBackToLoginClicked();
    FReply HandleSinglePlayerClicked();
    FReply HandleMultiplayerClicked();
    FReply HandleCreateNewStateClicked();
    FReply HandleSelectDifficulty(FString DifficultyName);
    FReply HandleSelectClimate(FString ClimateName);
    FReply HandleSelectLeaderGender(FString GenderName);
    FReply HandleSelectStartingCountry(FString CountryName, int32 MapCountryIndex);
    FReply HandleToggleNewCountryClimateSectionClicked();
    FReply HandleToggleNewCountryGenderSectionClicked();
    FReply HandleToggleNewCountryLocationSectionClicked();
    FReply HandleStartingCountryMapClicked(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
    FReply HandleCreateInitialSaveClicked();
    FReply HandleResumeSimulationClicked();
    FReply HandlePauseSimulationClicked();
    FReply HandleStepSimulationClicked();
    FReply HandleSlowerSimulationClicked();
    FReply HandleDefaultSimulationSpeedClicked();
    FReply HandleFasterSimulationClicked();
    FReply HandleSaveRuntimeStateClicked();
    FReply HandleRunAutosaveRecoveryTestClicked();
    FReply HandleRunRtsSaveLoadPlaytestClicked();
    FReply HandleDebugAddResourcesClicked();
    FReply HandleDebugTriggerEventClicked();
    FReply HandleDebugForceUnrestClicked();
    FReply HandleDebugForceInvasionRiskClicked();
    FReply HandleDebugAdvanceTimeClicked();
    FReply HandleDebugTestGameOverClicked();
    FReply HandleRunEarlyGameTestScenarioClicked();
    FReply HandleOpenPoliciesClicked();
    FReply HandleOpenEventsClicked();
    FReply HandleOpenDemographicsClicked();
    FReply HandleOpenBudgetClicked();
    FReply HandleOpenResourceChainsClicked();
    FReply HandleOpenAdvisorWarningsClicked();
    FReply HandleOpenDepartmentsClicked();
    FReply HandleOpenDevelopmentClicked();
    FReply HandleOpenApprovalStabilityClicked();
    FReply HandleOpenDecisionHistoryClicked();
    FReply HandleOpenMeetingAdvisorClicked();
    FReply HandleOpenPressReleaseClicked();
    FReply HandleSetPolicy(FString PolicyCategory, FString PolicyName);
    FReply HandleSetTaxPolicy(FString TaxPolicyName);
    FReply HandleSetSpendingPosture(FString SpendingPostureName);
    FReply HandleSetDepartmentAction(FString DepartmentName, FString ActionName);
    FReply HandleApplyResourceAction(FString ResourceActionName);
    FReply HandleApplyAdvisorAction(FString AdvisorActionName);
    FReply HandleExecuteAuthorityCommand(FString CommandId, FString SurfaceName);
    FReply HandleResolveEventChoice(FString EventId, FString ChoiceId);
    FReply HandleMakePressRelease(FString AnnouncementType);
    FReply HandleHoldMeeting(FString MeetingType, FString ParticipantName, FString AgendaItem);
    FReply HandleSetDevelopmentFocus(FString TrackName);
    FReply HandleReloadPreviousSaveClicked();
    FReply HandleBeginOfficeFromBriefingClicked();
    FReply HandleCloseOfficeOverlayClicked();
    FReply HandleExitOfficeClicked();
    FReply HandleBackToDifficultyClicked();
    FReply HandleBackToLocalSavesClicked();
    FReply HandleBackFromLocalSavesClicked();
    FReply HandleBackToModeSelectionClicked();
    FReply HandleBackToOnlineStatesClicked();
    FReply HandleSelectLocalSave(FString SaveName);
    FReply HandleDeleteLocalSave(FString SaveName);
    FReply HandleSelectOnlineState(FString StateName);
    FReply HandleRefreshOnlineStatesClicked();
    FReply HandleSelectServer(FString ServerName);
    FReply HandleChangeKeybind(FString ActionName);
    FReply HandleEnterOfficeClicked();
    FReply HandleExitClicked();

    void HandleMockUserNameChanged(const FText& UserNameText);
    void HandleMockPasswordChanged(const FText& PasswordText);
    void HandleRememberLoginDetailsChanged(ECheckBoxState NewState);
    void HandleLocalSaveSearchChanged(const FText& SearchText);
    void HandlePendingStateNameChanged(const FText& StateNameText);
    void HandleStartingCountrySearchChanged(const FText& SearchText);
    void HandleStartingCountryListScrolled(float ScrollOffset);
    void HandleNewCountrySetupScrolled(float ScrollOffset);
    void HandleRecentLocalSavesChanged(ECheckBoxState NewState);
    void HandleServerSearchChanged(const FText& SearchText);
    void HandleRecommendedServersChanged(ECheckBoxState NewState);
    void HandleFullscreenChanged(ECheckBoxState NewState);
    void HandleVSyncChanged(ECheckBoxState NewState);
    void HandleInvertLookYChanged(ECheckBoxState NewState);
    void HandleMasterVolumeChanged(float NewValue);
    void HandleMusicVolumeChanged(float NewValue);
    void HandleEffectsVolumeChanged(float NewValue);
    void HandleVoiceVolumeChanged(float NewValue);
    void HandleBrightnessChanged(float NewValue);
    void HandleUiScaleChanged(float NewValue);
};
