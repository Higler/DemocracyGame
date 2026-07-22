#include "OfficePlayerPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "OfficeInteractableActor.h"

AOfficePlayerPawn::AOfficePlayerPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    GetCharacterMovement()->MaxWalkSpeed = 420.0f;
    GetCharacterMovement()->MaxFlySpeed = 420.0f;
    GetCharacterMovement()->BrakingDecelerationFlying = 1600.0f;
    GetCharacterMovement()->GravityScale = 0.0f;
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
    GetCharacterMovement()->GravityScale = 0.0f;
    GetCharacterMovement()->SetMovementMode(MOVE_Flying);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Green, TEXT("Office prototype: WASD move, arrow keys look, E interact."));
        GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Cyan, FString::Printf(TEXT("Office pawn spawned at %s"), *GetActorLocation().ToCompactString()));
    }
}

void AOfficePlayerPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!FMath::IsNearlyEqual(GetActorLocation().Z, 90.0f, 1.0f))
    {
        SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, 90.0f), false, nullptr, ETeleportType::TeleportPhysics);
        GetCharacterMovement()->Velocity = FVector::ZeroVector;

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Office pawn below room safety floor; reset to office height."));
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
        const FVector MoveDelta = (Forward * ForwardInput + Right * RightInput).GetSafeNormal() * GetCharacterMovement()->MaxFlySpeed * DeltaSeconds;
        SetActorLocation(GetActorLocation() + MoveDelta, true);
    }

    GetCharacterMovement()->Velocity = FVector::ZeroVector;

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

    const bool bInteractPressed = PlayerController->IsInputKeyDown(EKeys::E);
    if (!bInteractPressed || bWasInteractPressed)
    {
        bWasInteractPressed = bInteractPressed;
        return;
    }

    bWasInteractPressed = true;

    const FVector Start = CameraComponent->GetComponentLocation();
    const FVector End = Start + CameraComponent->GetForwardVector() * 650.0f;

    FHitResult HitResult;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(OfficeInteractTrace), false, this);

    if (GetWorld() && GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(85.0f), QueryParams))
    {
        if (AOfficeInteractableActor* Interactable = Cast<AOfficeInteractableActor>(HitResult.GetActor()))
        {
            Interactable->Interact();
            return;
        }
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("No interactable in range."));
    }
}
