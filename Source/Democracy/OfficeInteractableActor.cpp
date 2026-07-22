#include "OfficeInteractableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "LoginHUD.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AOfficeInteractableActor::AOfficeInteractableActor()
{
    PrimaryActorTick.bCanEverTick = false;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(CubeMesh.Object);
    }

    InteractionName = TEXT("Interactable");
    InteractionMessage = TEXT("Interaction placeholder.");
}

void AOfficeInteractableActor::ConfigureInteractable(const FString& Name, const FString& Message, const FLinearColor& DisplayColor)
{
    InteractionName = Name;
    InteractionMessage = Message;

    UMaterialInstanceDynamic* Material = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), DisplayColor);
        Material->SetVectorParameterValue(TEXT("BaseColor"), DisplayColor);
    }
}

void AOfficeInteractableActor::Interact()
{
    UE_LOG(LogTemp, Log, TEXT("Office interaction: %s - %s"), *InteractionName, *InteractionMessage);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan, FString::Printf(TEXT("%s: %s"), *InteractionName, *InteractionMessage));
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PlayerController = World->GetFirstPlayerController())
        {
            if (ALoginHUD* LoginHUD = Cast<ALoginHUD>(PlayerController->GetHUD()))
            {
                LoginHUD->HandleOfficeInteractable(InteractionName);
            }
        }
    }
}
