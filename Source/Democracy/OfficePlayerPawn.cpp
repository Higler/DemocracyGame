#include "OfficePlayerPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LoginHUD.h"
#include "OfficeInteractableActor.h"

AOfficePlayerPawn::AOfficePlayerPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    GetCharacterMovement()->MaxWalkSpeed = 420.0f;
    GetCharacterMovement()->MaxFlySpeed = 420.0f;
    GetCharacterMovement()->BrakingDecelerationFlying = 1600.0f;
    GetCharacterMovement()->GravityScale = 1.0f;
    GetCharacterMovement()->MaxStepHeight = 45.0f;
    GetCharacterMovement()->SetWalkableFloorAngle(50.0f);
    GetCharacterMovement()->bOrientRotationToMovement = false;
    bUseControllerRotationYaw = true;

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
    CameraComponent->SetupAttachment(GetCapsuleComponent());
    CameraComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
    CameraComponent->SetFieldOfView(82.0f);
    CameraComponent->SetAutoActivate(true);
    CameraComponent->Activate(true);
    CameraComponent->bUsePawnControlRotation = true;
}

void AOfficePlayerPawn::SetInvertLookY(bool bShouldInvertLookY)
{
    bInvertLookY = bShouldInvertLookY;
}

void AOfficePlayerPawn::BeginPlay()
{
    Super::BeginPlay();
    GetCharacterMovement()->GravityScale = 1.0f;
    GetCharacterMovement()->MaxStepHeight = 45.0f;
    GetCharacterMovement()->SetWalkableFloorAngle(50.0f);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Green, TEXT("Office prototype: WASD move, arrow keys look, E interact."));
        GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Cyan, FString::Printf(TEXT("Office pawn spawned at %s"), *GetActorLocation().ToCompactString()));
    }
}

void AOfficePlayerPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (GetActorLocation().Z < -250.0f)
    {
        SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, 90.0f), false, nullptr, ETeleportType::TeleportPhysics);
        GetCharacterMovement()->Velocity = FVector::ZeroVector;

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Office pawn fell below room safety floor; reset to office height."));
        }
    }

    HandleMovement(DeltaSeconds);
    HandleInteraction();
}

void AOfficePlayerPawn::HandleMovement(float DeltaSeconds)
{
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController)
    {
        return;
    }

    const float ForwardInput = (PlayerController->IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f) - (PlayerController->IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f);
    const float RightInput = (PlayerController->IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) - (PlayerController->IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);
    const float YawInput = (PlayerController->IsInputKeyDown(EKeys::Right) ? 1.0f : 0.0f) - (PlayerController->IsInputKeyDown(EKeys::Left) ? 1.0f : 0.0f);
    const float RawPitchInput = (PlayerController->IsInputKeyDown(EKeys::Down) ? 1.0f : 0.0f) - (PlayerController->IsInputKeyDown(EKeys::Up) ? 1.0f : 0.0f);
    const float PitchInput = bInvertLookY ? -RawPitchInput : RawPitchInput;

    const FVector2D MoveInput(ForwardInput, RightInput);
    if (!MoveInput.IsNearlyZero())
    {
        const FRotator YawOnlyRotation(0.0f, PlayerController->GetControlRotation().Yaw, 0.0f);
        const FVector Forward = FRotationMatrix(YawOnlyRotation).GetUnitAxis(EAxis::X);
        const FVector Right = FRotationMatrix(YawOnlyRotation).GetUnitAxis(EAxis::Y);
        const FVector MoveDirection = (Forward * ForwardInput + Right * RightInput).GetSafeNormal();
        AddMovementInput(MoveDirection, 1.0f);
    }

    if (!FMath::IsNearlyZero(YawInput))
    {
        AddControllerYawInput(YawInput * 95.0f * DeltaSeconds);
    }

    if (!FMath::IsNearlyZero(PitchInput))
    {
        AddControllerPitchInput(PitchInput * 65.0f * DeltaSeconds);
    }
}

void AOfficePlayerPawn::HandleInteraction()
{
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!PlayerController)
    {
        return;
    }

    const bool bEscapePressed = PlayerController->IsInputKeyDown(EKeys::Escape);
    if (bEscapePressed && !bWasEscapePressed)
    {
        bWasEscapePressed = true;
        if (ALoginHUD* LoginHUD = Cast<ALoginHUD>(PlayerController->GetHUD()))
        {
            LoginHUD->CloseOfficeOverlayFromInput();
        }
    }
    else if (!bEscapePressed)
    {
        bWasEscapePressed = false;
    }

    AOfficeInteractableActor* TargetInteractable = FindBestInteractableTarget();
    UpdateInteractionPrompt(TargetInteractable);

    const bool bInteractPressed = PlayerController->IsInputKeyDown(EKeys::E);
    if (!bInteractPressed || bWasInteractPressed)
    {
        bWasInteractPressed = bInteractPressed;
        return;
    }

    bWasInteractPressed = true;

    if (TargetInteractable)
    {
        TargetInteractable->Interact();
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("No interactable targeted."));
    }
}

AOfficeInteractableActor* AOfficePlayerPawn::FindBestInteractableTarget() const
{
    if (!GetWorld() || !CameraComponent)
    {
        return nullptr;
    }

    const FVector Start = CameraComponent->GetComponentLocation();
    const FVector Forward = CameraComponent->GetForwardVector().GetSafeNormal();
    const FVector End = Start + Forward * 950.0f;

    TArray<FHitResult> HitResults;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OfficeInteractTrace), false, this);
    QueryParams.bFindInitialOverlaps = true;

    if (!GetWorld()->SweepMultiByChannel(HitResults, Start, End, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(125.0f), QueryParams))
    {
        return nullptr;
    }

    AOfficeInteractableActor* BestInteractable = nullptr;
    float BestScore = TNumericLimits<float>::Lowest();

    for (const FHitResult& HitResult : HitResults)
    {
        AOfficeInteractableActor* Interactable = Cast<AOfficeInteractableActor>(HitResult.GetActor());
        if (!Interactable)
        {
            continue;
        }

        const FVector ToTarget = Interactable->GetActorLocation() - Start;
        const float Distance = ToTarget.Size();
        if (Distance > 1050.0f || Distance <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const FVector Direction = ToTarget / Distance;
        const float AimDot = FVector::DotProduct(Forward, Direction);
        if (AimDot < 0.20f)
        {
            continue;
        }

        const float AlongTrace = FMath::Max(0.0f, FVector::DotProduct(ToTarget, Forward));
        const FVector ClosestPointOnTrace = Start + Forward * AlongTrace;
        const float SideDistance = FVector::Dist(Interactable->GetActorLocation(), ClosestPointOnTrace);

        const float Score = AimDot * 1000.0f - SideDistance * 2.75f - Distance * 0.08f;
        if (Score > BestScore)
        {
            BestScore = Score;
            BestInteractable = Interactable;
        }
    }

    return BestInteractable;
}

void AOfficePlayerPawn::UpdateInteractionPrompt(AOfficeInteractableActor* TargetInteractable)
{
    AOfficeInteractableActor* PreviousTarget = CurrentTargetInteractable.Get();
    if (PreviousTarget && PreviousTarget != TargetInteractable)
    {
        PreviousTarget->SetFocused(false);
    }

    CurrentTargetInteractable = TargetInteractable;

    if (TargetInteractable)
    {
        TargetInteractable->SetFocused(true);
    }
}
