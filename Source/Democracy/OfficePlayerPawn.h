#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "OfficePlayerPawn.generated.h"

class AOfficeInteractableActor;
class UCameraComponent;

UCLASS()
class DEMOCRACY_API AOfficePlayerPawn : public ACharacter
{
    GENERATED_BODY()

public:
    AOfficePlayerPawn();

    virtual void Tick(float DeltaSeconds) override;
    void SetInvertLookY(bool bShouldInvertLookY);

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Office")
    TObjectPtr<UCameraComponent> CameraComponent;

    bool bWasInteractPressed = false;
    bool bInvertLookY = false;
    TWeakObjectPtr<AOfficeInteractableActor> CurrentTargetInteractable;

    void HandleMovement(float DeltaSeconds);
    void HandleInteraction();
    AOfficeInteractableActor* FindBestInteractableTarget() const;
    void UpdateInteractionPrompt(AOfficeInteractableActor* TargetInteractable);
};
