#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OfficeInteractableActor.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class DEMOCRACY_API AOfficeInteractableActor : public AActor
{
    GENERATED_BODY()

public:
    AOfficeInteractableActor();

    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office")
    FString InteractionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office")
    FString InteractionMessage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office")
    TObjectPtr<UTextRenderComponent> TargetLabelComponent;

    void ConfigureInteractable(const FString& Name, const FString& Message, const FLinearColor& DisplayColor);
    void SetFocused(bool bFocused);
    void Interact();
};
