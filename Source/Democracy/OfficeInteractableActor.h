#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OfficeInteractableActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class DEMOCRACY_API AOfficeInteractableActor : public AActor
{
    GENERATED_BODY()

public:
    AOfficeInteractableActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office")
    FString InteractionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Office")
    FString InteractionMessage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Office")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    void ConfigureInteractable(const FString& Name, const FString& Message, const FLinearColor& DisplayColor);
    void Interact();
};
