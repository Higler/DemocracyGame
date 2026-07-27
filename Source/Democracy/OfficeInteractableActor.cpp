#include "OfficeInteractableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "LoginHUD.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    FString GetInteractionDisplayName(const FString& InteractionName)
    {
        if (InteractionName.Equals(TEXT("BriefingFolder"), ESearchCase::IgnoreCase))
        {
            return TEXT("Briefing Folder");
        }
        return InteractionName;
    }
}
AOfficeInteractableActor::AOfficeInteractableActor()
{
    PrimaryActorTick.bCanEverTick = true;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    RootComponent = MeshComponent;
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
    MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

    TargetLabelComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TargetLabelComponent"));
    TargetLabelComponent->SetupAttachment(RootComponent);
    TargetLabelComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 24.0f));
    TargetLabelComponent->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    TargetLabelComponent->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
    TargetLabelComponent->SetWorldSize(13.0f);
    TargetLabelComponent->SetTextRenderColor(FColor::Cyan);
    TargetLabelComponent->SetHiddenInGame(true);
    TargetLabelComponent->SetVisibility(false, true);
    TargetLabelComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        MeshComponent->SetStaticMesh(CubeMesh.Object);
    }

    InteractionName = TEXT("Interactable");
    InteractionMessage = TEXT("Interaction placeholder.");
}

void AOfficeInteractableActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!TargetLabelComponent || !TargetLabelComponent->IsVisible())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PlayerController = World->GetFirstPlayerController())
        {
            FVector CameraLocation;
            FRotator CameraRotation;
            PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

            TargetLabelComponent->SetWorldLocation(GetActorLocation() + FVector(0.0f, 0.0f, 42.0f));
            TargetLabelComponent->SetWorldScale3D(FVector::OneVector);

            const FVector ToCamera = CameraLocation - TargetLabelComponent->GetComponentLocation();
            if (!ToCamera.IsNearlyZero())
            {
                TargetLabelComponent->SetWorldRotation(ToCamera.Rotation());
            }
        }
    }
}

void AOfficeInteractableActor::ConfigureInteractable(const FString& Name, const FString& Message, const FLinearColor& DisplayColor)
{
    InteractionName = Name;
    InteractionMessage = Message;

    if (TargetLabelComponent)
    {
        TargetLabelComponent->SetText(FText::FromString(FString::Printf(TEXT("%s\nPress E"), *GetInteractionDisplayName(InteractionName))));
    }

    UMaterialInstanceDynamic* Material = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), DisplayColor);
        Material->SetVectorParameterValue(TEXT("BaseColor"), DisplayColor);
    }
}

void AOfficeInteractableActor::SetFocused(bool bFocused)
{
    if (!TargetLabelComponent)
    {
        return;
    }

    TargetLabelComponent->SetHiddenInGame(!bFocused);
    TargetLabelComponent->SetVisibility(bFocused, true);
    TargetLabelComponent->SetText(FText::FromString(FString::Printf(TEXT("%s\nPress E"), *GetInteractionDisplayName(InteractionName))));
}
void AOfficeInteractableActor::Interact()
{
    UE_LOG(LogTemp, Log, TEXT("Office interaction: %s - %s"), *InteractionName, *InteractionMessage);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Cyan, FString::Printf(TEXT("%s: %s"), *GetInteractionDisplayName(InteractionName), *InteractionMessage));
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
