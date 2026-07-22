#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LoginPlayerController.generated.h"

UCLASS()
class DEMOCRACY_API ALoginPlayerController : public APlayerController
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
};

