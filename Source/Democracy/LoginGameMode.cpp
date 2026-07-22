#include "LoginGameMode.h"
#include "LoginHUD.h"
#include "LoginPlayerController.h"

ALoginGameMode::ALoginGameMode()
{
    HUDClass = ALoginHUD::StaticClass();
    PlayerControllerClass = ALoginPlayerController::StaticClass();
}

