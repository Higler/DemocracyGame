#include "LoginPlayerController.h"

void ALoginPlayerController::BeginPlay()
{
    Super::BeginPlay();

    bShowMouseCursor = true;

    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
}

