#include "OfficeLevelBuilder.h"

#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "OfficeInteractableActor.h"
#include "UObject/ConstructorHelpers.h"

AOfficeLevelBuilder::AOfficeLevelBuilder()
{
    PrimaryActorTick.bCanEverTick = false;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMeshFinder.Succeeded())
    {
        CubeMesh = CubeMeshFinder.Object;
    }
}

void AOfficeLevelBuilder::BeginPlay()
{
    Super::BeginPlay();
    BuildOffice();
}

static void HideEditorOnlyLightVisual(AActor* LightActor)
{
    if (!LightActor)
    {
        return;
    }

#if WITH_EDITOR
    LightActor->SetIsTemporarilyHiddenInEditor(true);
    TArray<UActorComponent*> Components;
    LightActor->GetComponents(Components);
    for (UActorComponent* Component : Components)
    {
        if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
        {
            if (!PrimitiveComponent->IsA<ULightComponent>())
            {
                PrimitiveComponent->SetVisibility(false, true);
                PrimitiveComponent->SetHiddenInGame(true);
            }
        }
    }
#endif
}

static void ApplyPrimitiveTint(UStaticMeshComponent* MeshComponent, const FLinearColor& Tint)
{
    if (!MeshComponent)
    {
        return;
    }

    UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!BaseMaterial)
    {
        return;
    }

    const int32 MaterialCount = FMath::Max(1, MeshComponent->GetNumMaterials());
    for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
    {
        MeshComponent->SetMaterial(MaterialIndex, BaseMaterial);
        if (UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
        {
            DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Tint);
            DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), Tint);
        }
    }
}
AActor* AOfficeLevelBuilder::SpawnBox(const FString& Name, const FVector& Location, const FVector& Scale, const FLinearColor& Color, bool bBlocksPlayer)
{
    if (!GetWorld() || !CubeMesh)
    {
        return nullptr;
    }

    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, FRotator::ZeroRotator);
    if (!Actor)
    {
        return nullptr;
    }

    Actor->SetActorLabel(Name);
    Actor->SetActorScale3D(Scale);

    UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->SetStaticMesh(CubeMesh);
    Actor->SetActorEnableCollision(bBlocksPlayer);
    MeshComponent->SetCollisionEnabled(bBlocksPlayer ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionObjectType(ECC_WorldStatic);
    MeshComponent->SetCollisionResponseToAllChannels(bBlocksPlayer ? ECR_Block : ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, bBlocksPlayer ? ECR_Block : ECR_Ignore);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->RecreatePhysicsState();
    DrawDebugBox(GetWorld(), Location, Scale * 50.0f, Color.ToFColor(true), true, 600.0f, 0, 5.0f);

    UMaterialInstanceDynamic* Material = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    }

    return Actor;
}
AActor* AOfficeLevelBuilder::SpawnRotatedBox(const FString& Name, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color, bool bBlocksPlayer)
{
    if (!GetWorld() || !CubeMesh)
    {
        return nullptr;
    }

    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation);
    if (!Actor)
    {
        return nullptr;
    }

    Actor->SetActorLabel(Name);
    Actor->SetActorScale3D(Scale);

    UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->SetStaticMesh(CubeMesh);
    Actor->SetActorEnableCollision(bBlocksPlayer);
    MeshComponent->SetCollisionEnabled(bBlocksPlayer ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionObjectType(ECC_WorldStatic);
    MeshComponent->SetCollisionResponseToAllChannels(bBlocksPlayer ? ECR_Block : ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, bBlocksPlayer ? ECR_Block : ECR_Ignore);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->RecreatePhysicsState();

    UMaterialInstanceDynamic* Material = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    }

    return Actor;
}

bool AOfficeLevelBuilder::SpawnAssetMesh(const FString& Name, const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, bool bBlocksPlayer)
{
    if (!GetWorld())
    {
        return false;
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
    if (!Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("Office asset mesh missing: %s"), MeshPath);
        return false;
    }

    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation);
    if (!Actor)
    {
        return false;
    }

    Actor->SetActorLabel(Name);
    Actor->SetActorScale3D(Scale);

    UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->SetStaticMesh(Mesh);
    Actor->SetActorEnableCollision(bBlocksPlayer);
    MeshComponent->SetCollisionEnabled(bBlocksPlayer ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionObjectType(ECC_WorldStatic);
    MeshComponent->SetCollisionResponseToAllChannels(bBlocksPlayer ? ECR_Block : ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, bBlocksPlayer ? ECR_Block : ECR_Ignore);
    MeshComponent->SetGenerateOverlapEvents(false);
    MeshComponent->RecreatePhysicsState();
    if (Name.Contains(TEXT("Wooden Monkey Statue")))
    {
        ApplyPrimitiveTint(MeshComponent, FLinearColor(0.42f, 0.23f, 0.10f, 1.0f));
    }
    return true;
}


bool AOfficeLevelBuilder::SpawnSkeletalAssetMesh(const FString& Name, const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, bool bBlocksPlayer)
{
    if (!GetWorld())
    {
        return false;
    }

    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
    if (!Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("Office skeletal asset mesh missing: %s"), MeshPath);
        return false;
    }

    ASkeletalMeshActor* Actor = GetWorld()->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), Location, Rotation);
    if (!Actor)
    {
        return false;
    }

    Actor->SetActorLabel(Name);
    Actor->SetActorScale3D(Scale);
    USkeletalMeshComponent* MeshComponent = Actor->GetSkeletalMeshComponent();
    MeshComponent->SetMobility(EComponentMobility::Movable);
    MeshComponent->SetSkeletalMesh(Mesh);
    Actor->SetActorEnableCollision(bBlocksPlayer);
    MeshComponent->SetCollisionEnabled(bBlocksPlayer ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    MeshComponent->SetCollisionObjectType(ECC_WorldStatic);
    MeshComponent->SetCollisionResponseToAllChannels(bBlocksPlayer ? ECR_Block : ECR_Ignore);
    MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, bBlocksPlayer ? ECR_Block : ECR_Ignore);
    MeshComponent->RecreatePhysicsState();
    return true;
}

void AOfficeLevelBuilder::SpawnInteractable(const FString& Name, const FString& Message, const FVector& Location, const FVector& Scale, const FLinearColor& Color)
{
    if (!GetWorld())
    {
        return;
    }

    AOfficeInteractableActor* Actor = GetWorld()->SpawnActor<AOfficeInteractableActor>(AOfficeInteractableActor::StaticClass(), Location, FRotator::ZeroRotator);
    if (!Actor)
    {
        return;
    }

    Actor->SetActorLabel(Name);
    Actor->SetActorScale3D(Scale);
    Actor->ConfigureInteractable(Name, Message, Color);
    DrawDebugBox(GetWorld(), Location, Scale * 50.0f, Color.ToFColor(true), true, 600.0f, 0, 7.0f);
}



void AOfficeLevelBuilder::SpawnInteractableTarget(const FString& Name, const FString& Message, const FVector& Location, const FVector& Scale)
{
    if (!GetWorld())
    {
        return;
    }

    AOfficeInteractableActor* Actor = GetWorld()->SpawnActor<AOfficeInteractableActor>(AOfficeInteractableActor::StaticClass(), Location, FRotator::ZeroRotator);
    if (!Actor)
    {
        return;
    }

    Actor->SetActorLabel(Name + TEXT(" Target"));
    Actor->SetActorScale3D(Scale);
    Actor->ConfigureInteractable(Name, Message, FLinearColor::Transparent);
    Actor->MeshComponent->SetVisibility(false, true);
    Actor->MeshComponent->SetHiddenInGame(true);
    Actor->MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Actor->MeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
    Actor->MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    Actor->MeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    Actor->MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);
    Actor->MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Actor->MeshComponent->RecreatePhysicsState();
}

bool AOfficeLevelBuilder::SpawnInteractableAsset(const FString& Name, const FString& Message, const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& FallbackColor)
{
    if (!GetWorld())
    {
        return false;
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
    if (!Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("Office interactable asset mesh missing: %s"), MeshPath);
        SpawnInteractable(Name, Message, Location, Scale, FallbackColor);
        return false;
    }

    AOfficeInteractableActor* Actor = GetWorld()->SpawnActor<AOfficeInteractableActor>(AOfficeInteractableActor::StaticClass(), Location, Rotation);
    if (!Actor)
    {
        return false;
    }

    Actor->SetActorLabel(Name);
    Actor->SetActorScale3D(Scale);
    Actor->ConfigureInteractable(Name, Message, FallbackColor);
    Actor->MeshComponent->SetMobility(EComponentMobility::Movable);
    Actor->MeshComponent->SetStaticMesh(Mesh);
    Actor->MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Actor->MeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
    Actor->MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
    Actor->MeshComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Actor->MeshComponent->RecreatePhysicsState();
    return true;
}

void AOfficeLevelBuilder::BuildOffice()
{
    if (bOfficeBuilt)
    {
        UE_LOG(LogTemp, Log, TEXT("OfficeLevelBuilder already built; skipping duplicate build."));
        return;
    }

    bOfficeBuilt = true;
    UE_LOG(LogTemp, Warning, TEXT("Building office prototype geometry and debug wireframes."));

    const auto SpawnSolidFloor = [this](const FString& Name, const FVector& Location, const FVector& Scale)
    {
        if (AActor* Floor = SpawnBox(Name, Location, Scale, FLinearColor(0.08f, 0.08f, 0.08f), true))
        {
            Floor->SetActorHiddenInGame(true);
            if (AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Floor))
            {
                if (UStaticMeshComponent* MeshComponent = MeshActor->GetStaticMeshComponent())
                {
                    MeshComponent->SetVisibility(false, true);
                    MeshComponent->SetHiddenInGame(true);
                }
            }
        }
    };

    if (UWorld* World = GetWorld())
    {
        ADirectionalLight* KeyLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(-260.0f, -420.0f, 520.0f), FRotator(-38.0f, 35.0f, 0.0f));
        if (KeyLight && KeyLight->GetLightComponent())
        {
            KeyLight->SetActorLabel(TEXT("Office Key Light"));
            HideEditorOnlyLightVisual(KeyLight);
            KeyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            KeyLight->GetLightComponent()->SetIntensity(0.8f);
        }

        ASkyLight* AmbientLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
        if (AmbientLight && AmbientLight->GetLightComponent())
        {
            AmbientLight->SetActorLabel(TEXT("Office Ambient Light"));
            HideEditorOnlyLightVisual(AmbientLight);
            AmbientLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
            AmbientLight->GetLightComponent()->SetIntensity(3.5f);
        }

        APointLight* CeilingLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(0.0f, 80.0f, 280.0f), FRotator::ZeroRotator);
        if (CeilingLight && CeilingLight->PointLightComponent)
        {
            CeilingLight->SetActorLabel(TEXT("Office Ceiling Light"));
            HideEditorOnlyLightVisual(CeilingLight);
            CeilingLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            CeilingLight->PointLightComponent->SetIntensity(5200.0f);
            CeilingLight->PointLightComponent->SetAttenuationRadius(1800.0f);
        }

        const FVector OfficeOverheadLights[] = {
            FVector(-520.0f, -260.0f, 295.0f), FVector(0.0f, -260.0f, 295.0f), FVector(520.0f, -260.0f, 295.0f),
            FVector(-520.0f, 320.0f, 295.0f), FVector(0.0f, 320.0f, 295.0f), FVector(520.0f, 320.0f, 295.0f)
        };
        for (int32 LightIndex = 0; LightIndex < UE_ARRAY_COUNT(OfficeOverheadLights); ++LightIndex)
        {
            APointLight* OverheadLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), OfficeOverheadLights[LightIndex], FRotator::ZeroRotator);
            if (OverheadLight && OverheadLight->PointLightComponent)
            {
                OverheadLight->SetActorLabel(FString::Printf(TEXT("Office Overhead Light %02d"), LightIndex + 1));
                HideEditorOnlyLightVisual(OverheadLight);
                OverheadLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
                OverheadLight->PointLightComponent->SetIntensity(1800.0f);
                OverheadLight->PointLightComponent->SetAttenuationRadius(650.0f);
                OverheadLight->PointLightComponent->SetCastShadows(false);
            }
        }

        APointLight* DeskLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(260.0f, 85.0f, 150.0f), FRotator::ZeroRotator);
        if (DeskLight && DeskLight->PointLightComponent)
        {
            DeskLight->SetActorLabel(TEXT("Office Desk Light"));
            HideEditorOnlyLightVisual(DeskLight);
            DeskLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            DeskLight->PointLightComponent->SetIntensity(2200.0f);
            DeskLight->PointLightComponent->SetAttenuationRadius(850.0f);
        }

        APointLight* FrontFillLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(-420.0f, -350.0f, 230.0f), FRotator::ZeroRotator);
        if (FrontFillLight && FrontFillLight->PointLightComponent)
        {
            FrontFillLight->SetActorLabel(TEXT("Office Front Fill Light"));
            HideEditorOnlyLightVisual(FrontFillLight);
            FrontFillLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            FrontFillLight->PointLightComponent->SetIntensity(2200.0f);
            FrontFillLight->PointLightComponent->SetAttenuationRadius(1100.0f);
        }

        APointLight* BackWallLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(0.0f, 520.0f, 250.0f), FRotator::ZeroRotator);
        if (BackWallLight && BackWallLight->PointLightComponent)
        {
            BackWallLight->SetActorLabel(TEXT("Office Back Wall Light"));
            HideEditorOnlyLightVisual(BackWallLight);
            BackWallLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            BackWallLight->PointLightComponent->SetIntensity(2200.0f);
            BackWallLight->PointLightComponent->SetAttenuationRadius(950.0f);
        }
    }

    // Room shell: generated layout with real imported/museum meshes where available.
    if (!SpawnAssetMesh(TEXT("Museum Floor Tile Center"), TEXT("/Game/Museum/Meshes/SM_FloorTile01.SM_FloorTile01"), FVector(-200.0f, 200.0f, 0.0f), FRotator::ZeroRotator, FVector(4.55f, 3.55f, 1.0f)))
    {
        SpawnBox(TEXT("Office Floor"), FVector(0.0f, 0.0f, -8.0f), FVector(18.2f, 14.2f, 0.16f), FLinearColor(0.24f, 0.18f, 0.12f));
    }

    SpawnSolidFloor(TEXT("Office Guaranteed Floor Collision"), FVector(0.0f, 0.0f, -8.0f), FVector(18.2f, 14.2f, 0.08f));
    SpawnSolidFloor(TEXT("Office Doorway Guaranteed Floor Collision"), FVector(0.0f, -760.0f, -8.0f), FVector(5.8f, 1.2f, 0.08f));

    if (!SpawnAssetMesh(TEXT("Museum Ceiling Panel"), TEXT("/Game/Museum/Meshes/SM_ModularCeiling01.SM_ModularCeiling01"), FVector(0.0f, 0.0f, 415.0f), FRotator::ZeroRotator, FVector(6.05f, 4.75f, 1.0f), false))
    {
        SpawnBox(TEXT("Office Ceiling"), FVector(0.0f, 0.0f, 405.0f), FVector(18.2f, 14.2f, 0.10f), FLinearColor(0.40f, 0.39f, 0.36f));
    }

    SpawnBox(TEXT("Back Wall"), FVector(0.0f, 705.0f, 200.0f), FVector(18.2f, 0.12f, 4.0f), FLinearColor(0.36f, 0.34f, 0.31f));
    SpawnBox(TEXT("Front Wall"), FVector(0.0f, -705.0f, 200.0f), FVector(18.2f, 0.12f, 4.0f), FLinearColor(0.34f, 0.32f, 0.30f));
    SpawnBox(TEXT("Left Wall"), FVector(-905.0f, 0.0f, 200.0f), FVector(0.12f, 14.2f, 4.0f), FLinearColor(0.34f, 0.32f, 0.30f));
    SpawnBox(TEXT("Right Wall"), FVector(905.0f, 0.0f, 200.0f), FVector(0.12f, 14.2f, 4.0f), FLinearColor(0.34f, 0.32f, 0.30f));

    // Windows and architectural panels.
    if (!SpawnAssetMesh(TEXT("Museum Window Left"), TEXT("/Game/Museum/Meshes/SM_window_01.SM_window_01"), FVector(-510.0f, 690.0f, 110.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(1.05f, 1.0f, 1.0f), false))
    {
        SpawnBox(TEXT("Window Glow Left"), FVector(-360.0f, 692.0f, 200.0f), FVector(1.6f, 0.04f, 2.2f), FLinearColor(0.74f, 0.86f, 1.0f), false);
    }

    if (!SpawnAssetMesh(TEXT("Museum Window Right"), TEXT("/Game/Museum/Meshes/SM_window_01.SM_window_01"), FVector(210.0f, 690.0f, 110.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(1.05f, 1.0f, 1.0f), false))
    {
        SpawnBox(TEXT("Window Glow Right"), FVector(360.0f, 692.0f, 200.0f), FVector(1.6f, 0.04f, 2.2f), FLinearColor(0.74f, 0.86f, 1.0f), false);
    }

    SpawnBox(TEXT("Wall Panel Left"), FVector(-680.0f, 690.0f, 150.0f), FVector(0.65f, 0.05f, 2.1f), FLinearColor(0.48f, 0.45f, 0.40f), false);
    SpawnBox(TEXT("Wall Panel Right"), FVector(680.0f, 690.0f, 150.0f), FVector(0.65f, 0.05f, 2.1f), FLinearColor(0.48f, 0.45f, 0.40f), false);
    if (AActor* MonaLisaCanvas = SpawnBox(TEXT("Mona Lisa Canvas"), FVector(0.0f, 685.0f, 246.0f), FVector(1.10f, 0.018f, 1.04f), FLinearColor(0.70f, 0.64f, 0.52f), false))
    {
        if (AStaticMeshActor* CanvasMeshActor = Cast<AStaticMeshActor>(MonaLisaCanvas))
        {
            if (UMaterialInterface* MonaLisaMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DemocracyOffice/Props/MonaLisa/M_MonaLisa_Bright.M_MonaLisa_Bright")))
            {
                CanvasMeshActor->GetStaticMeshComponent()->SetMaterial(0, MonaLisaMaterial);
            }
        }
    }

    bool bSpawnedCarpetTiles = false;
    for (int32 XIndex = 0; XIndex < 9; ++XIndex)
    {
        for (int32 YIndex = 0; YIndex < 9; ++YIndex)
        {
            const float TileCenterX = -800.0f + static_cast<float>(XIndex) * 200.0f;
            const float TileCenterY = -600.0f + static_cast<float>(YIndex) * 200.0f;
            const FString TileName = FString::Printf(TEXT("Red Carpet Floor Tile %02d_%02d"), XIndex, YIndex);
            bSpawnedCarpetTiles |= SpawnAssetMesh(TileName, TEXT("/Game/DemocracyOffice/Props/RedCarpet/red_carpet_tiles_with_gold_trim/StaticMeshes/Object_4.Object_4"), FVector(TileCenterX, TileCenterY + 340.0f, 1.5f), FRotator::ZeroRotator, FVector(2.0f, 2.0f, 1.0f), false);
        }
    }
    if (!bSpawnedCarpetTiles)
    {
        SpawnBox(TEXT("Office Carpet"), FVector(0.0f, 120.0f, 3.0f), FVector(17.2f, 13.2f, 0.035f), FLinearColor(0.20f, 0.025f, 0.035f), false);
    }

    // Desk area inspired by the generated still office image.
    if (!SpawnAssetMesh(TEXT("Antique Partner Desk"), TEXT("/Game/DemocracyOffice/Props/Desk/LOD_0.LOD_0"), FVector(0.0f, 210.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(3.35f, 2.35f, 1.25f)))
    {
        SpawnBox(TEXT("Executive Desk Top"), FVector(0.0f, 210.0f, 105.0f), FVector(5.2f, 2.20f, 0.26f), FLinearColor(0.18f, 0.09f, 0.035f));
        SpawnBox(TEXT("Desk Front"), FVector(0.0f, 330.0f, 46.0f), FVector(5.2f, 0.24f, 0.92f), FLinearColor(0.14f, 0.065f, 0.025f));
        SpawnBox(TEXT("Desk Left Pedestal"), FVector(-365.0f, 185.0f, 46.0f), FVector(0.82f, 1.05f, 0.92f), FLinearColor(0.12f, 0.055f, 0.025f));
        SpawnBox(TEXT("Desk Right Pedestal"), FVector(365.0f, 185.0f, 46.0f), FVector(0.82f, 1.05f, 0.92f), FLinearColor(0.12f, 0.055f, 0.025f));
    }

    if (!SpawnAssetMesh(TEXT("Executive Office Chair"), TEXT("/Game/DemocracyOffice/Props/ExecutiveChair/executive_office_chair1.executive_office_chair1"), FVector(0.0f, 470.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), FVector(1.15f, 1.15f, 1.15f)) && !SpawnAssetMesh(TEXT("Office Chair Mesh"), TEXT("/Game/Museum/Meshes/SM_Chair_01.SM_Chair_01"), FVector(0.0f, 470.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), FVector(1.35f, 1.35f, 1.35f)))
    {
        SpawnBox(TEXT("Chair Back"), FVector(0.0f, 490.0f, 115.0f), FVector(1.15f, 0.22f, 1.35f), FLinearColor(0.02f, 0.021f, 0.024f));
        SpawnBox(TEXT("Chair Seat"), FVector(0.0f, 445.0f, 25.0f), FVector(1.05f, 0.85f, 0.25f), FLinearColor(0.025f, 0.026f, 0.030f));
    }

    // Office supplies and interactive objects. Desk top items use one shared surface height so they sit on the imported desk instead of inside it.
    const float DeskSurfaceZ = 62.0f;
    if (SpawnSkeletalAssetMesh(TEXT("Briefing Document Folder"), TEXT("/Game/DemocracyOffice/Props/DocumentFileFolder/scene/SkeletalMeshes/Folder_1Shape.Folder_1Shape"), FVector(-108.0f, 270.0f, DeskSurfaceZ + 40.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(0.165f, 0.165f, 0.165f), false))
    {
        SpawnAssetMesh(TEXT("Briefing Folder Paper Stack"), TEXT("/Game/DemocracyOffice/Props/DocumentFileFolder/scene/StaticMeshes/A4_Paper_StackShape_8_0.A4_Paper_StackShape_8_0"), FVector(-106.0f, 270.0f, DeskSurfaceZ + 42.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(0.165f, 0.165f, 0.165f), false);
        SpawnAssetMesh(TEXT("Briefing Folder Page"), TEXT("/Game/DemocracyOffice/Props/DocumentFileFolder/scene/StaticMeshes/A4_Page3Shape_2_0.A4_Page3Shape_2_0"), FVector(-106.0f, 270.0f, DeskSurfaceZ + 43.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(0.165f, 0.165f, 0.165f), false);
    }
    else
    {
        SpawnBox(TEXT("Briefing Folder"), FVector(-86.0f, 292.0f, DeskSurfaceZ + 7.0f), FVector(0.62f, 0.40f, 0.035f), FLinearColor(0.12f, 0.09f, 0.045f), false);
        SpawnBox(TEXT("Briefing Folder Papers"), FVector(-78.0f, 298.0f, DeskSurfaceZ + 10.0f), FVector(0.54f, 0.34f, 0.018f), FLinearColor(0.82f, 0.78f, 0.66f), false);
    }
    SpawnInteractableTarget(TEXT("BriefingFolder"), TEXT("Open the briefing."), FVector(-108.0f, 270.0f, DeskSurfaceZ + 56.0f), FVector(1.15f, 0.92f, 0.85f));
    SpawnBox(TEXT("Paper Basket Tray"), FVector(-120.0f, 185.0f, DeskSurfaceZ + 44.0f), FVector(0.55f, 0.38f, 0.10f), FLinearColor(0.10f, 0.105f, 0.11f), false);
    SpawnBox(TEXT("Paper Stack In Basket"), FVector(-120.0f, 185.0f, DeskSurfaceZ + 50.0f), FVector(0.48f, 0.32f, 0.025f), FLinearColor(0.82f, 0.80f, 0.72f), false);
    SpawnAssetMesh(TEXT("Vintage Pencil Holder"), TEXT("/Game/DemocracyOffice/Props/PencilHolder/Holder.Holder"), FVector(155.0f, 170.0f, DeskSurfaceZ + 54.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(0.75f, 0.75f, 0.75f), false);
    SpawnBox(TEXT("Pencil Holder Cup"), FVector(155.0f, 170.0f, DeskSurfaceZ + 50.0f), FVector(0.12f, 0.12f, 0.22f), FLinearColor(0.06f, 0.07f, 0.08f), false);
    SpawnBox(TEXT("Pencils In Holder"), FVector(155.0f, 170.0f, DeskSurfaceZ + 74.0f), FVector(0.07f, 0.07f, 0.26f), FLinearColor(0.88f, 0.64f, 0.18f), false);

    if (!SpawnAssetMesh(TEXT("Vintage Desk Lamp"), TEXT("/Game/DemocracyOffice/Props/VintageDeskLamp/lamp_02.lamp_02"), FVector(-185.0f, 228.0f, DeskSurfaceZ + 36.0f), FRotator(0.0f, 200.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f), false))
    {
        SpawnBox(TEXT("Desk Lamp"), FVector(-185.0f, 228.0f, DeskSurfaceZ + 64.0f), FVector(0.24f, 0.24f, 0.70f), FLinearColor(0.65f, 0.56f, 0.40f), false);
    }
    SpawnInteractableTarget(TEXT("Lamp"), TEXT("Toggle desk lamp."), FVector(-185.0f, 228.0f, DeskSurfaceZ + 92.0f), FVector(1.05f, 1.05f, 1.45f));

    if (UWorld* World = GetWorld())
    {
        APointLight* ToggleLampLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(-175.0f, 225.0f, DeskSurfaceZ + 126.0f), FRotator::ZeroRotator);
        if (ToggleLampLight && ToggleLampLight->PointLightComponent)
        {
            ToggleLampLight->SetActorLabel(TEXT("Desk Lamp Toggle Light"));
            ToggleLampLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            ToggleLampLight->PointLightComponent->SetIntensity(0.0f);
            ToggleLampLight->PointLightComponent->SetAttenuationRadius(430.0f);
        }
    }

    const FVector ComputerOrigin(0.0f, 350.0f, DeskSurfaceZ - 13.5f);
    const FVector ComputerScale(0.45f, 0.45f, 0.45f);
    SpawnAssetMesh(TEXT("Sleek Computer Base"), TEXT("/Game/DemocracyOffice/Props/SleekComputer/PC/StaticMeshes/Base.Base"), ComputerOrigin, FRotator::ZeroRotator, ComputerScale, false);
    SpawnAssetMesh(TEXT("Sleek Computer Keyboard"), TEXT("/Game/DemocracyOffice/Props/SleekComputer/PC/StaticMeshes/Keyboard.Keyboard"), FVector(-35.0f, 390.0f, DeskSurfaceZ - 11.5f), FRotator::ZeroRotator, ComputerScale, false);
    SpawnAssetMesh(TEXT("Sleek Computer Mouse"), TEXT("/Game/DemocracyOffice/Props/SleekComputer/PC/StaticMeshes/Mouse.Mouse"), FVector(8.0f, 389.0f, DeskSurfaceZ - 11.5f), FRotator::ZeroRotator, ComputerScale, false);
    SpawnAssetMesh(TEXT("Sleek Computer Monitor"), TEXT("/Game/DemocracyOffice/Props/SleekComputer/PC/StaticMeshes/Monitor.Monitor"), ComputerOrigin, FRotator::ZeroRotator, ComputerScale, false);
    SpawnInteractableTarget(TEXT("Computer"), TEXT("Open national dashboard."), FVector(-10.0f, 325.0f, DeskSurfaceZ + 70.0f), FVector(2.25f, 1.75f, 1.65f));

    SpawnAssetMesh(TEXT("Ceramic Mug"), TEXT("/Game/DemocracyOffice/Props/CeramicMug/cup.cup"), FVector(-155.0f, 165.0f, DeskSurfaceZ + 2.0f), FRotator(0.0f, 35.0f, 0.0f), FVector(0.18f, 0.18f, 0.18f), false);
    SpawnAssetMesh(TEXT("Phone Mesh"), TEXT("/Game/DemocracyOffice/Props/Phone/nt_vis200_01/StaticMeshes/nt_vis200_01.nt_vis200_01"), FVector(155.0f, 245.0f, DeskSurfaceZ - 10.0f), FRotator(0.0f, 45.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f), false);
    SpawnInteractableTarget(TEXT("Phone"), TEXT("Open advisor warnings."), FVector(155.0f, 245.0f, DeskSurfaceZ + 54.0f), FVector(1.25f, 1.05f, 1.20f));

    if (SpawnSkeletalAssetMesh(TEXT("Classic Wooden Door"), TEXT("/Game/DemocracyOffice/Props/ClassicWoodenDoor/door/SkeletalMeshes/door.door"), FVector(0.0f, -705.0f, 133.0f), FRotator::ZeroRotator, FVector(1.35f, 1.35f, 1.35f)))
    {
        SpawnInteractableTarget(TEXT("Door"), TEXT("Enter the hallway."), FVector(0.0f, -590.0f, 150.0f), FVector(3.2f, 1.25f, 3.2f));
    }
    else
    {
        SpawnInteractableAsset(TEXT("Door"), TEXT("Enter the hallway."), TEXT("/Game/Museum/Meshes/SM_DoorWithFrame_01.SM_DoorWithFrame_01"), FVector(-90.0f, -695.0f, 0.0f), FRotator::ZeroRotator, FVector(1.8f, 1.0f, 1.35f), FLinearColor(0.22f, 0.11f, 0.045f));
    }

    // Room dressing.
    if (!SpawnAssetMesh(TEXT("Wooden Monkey Statue"), TEXT("/Game/DemocracyOffice/Props/WoodenMonkey/monkey_1001.monkey_1001"), FVector(705.0f, -300.0f, 102.0f), FRotator(0.0f, -35.0f, 0.0f), FVector(0.18f, 0.18f, 0.18f), false))
    {
        SpawnAssetMesh(TEXT("Wooden Monkey Statue"), TEXT("/Game/DemocracyOffice/Props/WoodenMonkey/monkey.monkey"), FVector(705.0f, -300.0f, 102.0f), FRotator(0.0f, -35.0f, 0.0f), FVector(0.18f, 0.18f, 0.18f), false);
    }
    SpawnAssetMesh(TEXT("Trash Can"), TEXT("/Game/DemocracyOffice/Props/TrashCan/trash_can/StaticMeshes/trash_can.trash_can"), FVector(330.0f, 240.0f, 0.0f), FRotator::ZeroRotator, FVector(0.35f, 0.35f, 0.35f), true);
    SpawnBox(TEXT("Plant Left"), FVector(-710.0f, -330.0f, 55.0f), FVector(0.55f, 0.55f, 1.10f), FLinearColor(0.08f, 0.28f, 0.12f));
    SpawnBox(TEXT("Plant Right"), FVector(710.0f, -330.0f, 55.0f), FVector(0.55f, 0.55f, 1.10f), FLinearColor(0.08f, 0.28f, 0.12f));

    // Newly created hallway beyond the office door. The stem opens into a larger T-shaped corridor.
    SpawnBox(TEXT("Hallway Stem Floor"), FVector(0.0f, -1185.0f, -5.0f), FVector(5.8f, 8.5f, 0.10f), FLinearColor(0.20f, 0.17f, 0.13f));
    SpawnSolidFloor(TEXT("Hallway Stem Guaranteed Floor Collision"), FVector(0.0f, -1185.0f, -8.0f), FVector(5.8f, 8.5f, 0.08f));
    SpawnBox(TEXT("Hallway Stem Left Wall"), FVector(-290.0f, -1185.0f, 160.0f), FVector(0.14f, 8.5f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Hallway Stem Right Wall"), FVector(290.0f, -1185.0f, 160.0f), FVector(0.14f, 8.5f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Hallway Stem Ceiling"), FVector(0.0f, -1185.0f, 316.0f), FVector(5.8f, 8.5f, 0.16f), FLinearColor(0.38f, 0.37f, 0.34f), false);

    SpawnBox(TEXT("Hallway T Floor"), FVector(0.0f, -1960.0f, -5.0f), FVector(16.8f, 7.0f, 0.10f), FLinearColor(0.20f, 0.17f, 0.13f));
    SpawnSolidFloor(TEXT("Hallway T Guaranteed Floor Collision"), FVector(0.0f, -1960.0f, -8.0f), FVector(16.8f, 7.0f, 0.08f));
    SpawnSolidFloor(TEXT("Meeting Door Transition Guaranteed Floor Collision"), FVector(-520.0f, -2455.0f, -8.0f), FVector(3.2f, 3.0f, 0.08f));
    SpawnSolidFloor(TEXT("Press Door Transition Guaranteed Floor Collision"), FVector(520.0f, -2455.0f, -8.0f), FVector(3.2f, 3.0f, 0.08f));
    SpawnBox(TEXT("Hallway T Back Wall"), FVector(0.0f, -2310.0f, 160.0f), FVector(16.8f, 0.14f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Hallway T Front Wall Left"), FVector(-565.0f, -1610.0f, 160.0f), FVector(5.5f, 0.14f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Hallway T Front Wall Right"), FVector(565.0f, -1610.0f, 160.0f), FVector(5.5f, 0.14f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Hallway T Left End Wall"), FVector(-840.0f, -1960.0f, 160.0f), FVector(0.14f, 7.0f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Hallway T Right End Wall"), FVector(840.0f, -1960.0f, 160.0f), FVector(0.14f, 7.0f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Hallway T Ceiling"), FVector(0.0f, -1960.0f, 316.0f), FVector(16.8f, 7.0f, 0.16f), FLinearColor(0.38f, 0.37f, 0.34f), false);
    SpawnInteractableTarget(TEXT("HallwayReturn"), TEXT("Return to the office."), FVector(0.0f, -780.0f, 150.0f), FVector(3.0f, 1.0f, 3.0f));

    if (!SpawnSkeletalAssetMesh(TEXT("Hallway Side Door"), TEXT("/Game/DemocracyOffice/Props/ClassicWoodenDoor/door/SkeletalMeshes/door.door"), FVector(285.0f, -1220.0f, 133.0f), FRotator(0.0f, -90.0f, 0.0f), FVector(1.15f, 1.15f, 1.15f), false))
    {
        SpawnBox(TEXT("Hallway Side Door Placeholder"), FVector(285.0f, -1220.0f, 145.0f), FVector(0.12f, 1.55f, 2.9f), FLinearColor(0.22f, 0.11f, 0.045f), false);
    }
    SpawnInteractableTarget(TEXT("HallwaySideDoor"), TEXT("Enter the RTS command view."), FVector(235.0f, -1220.0f, 150.0f), FVector(1.0f, 2.9f, 3.0f));

    if (!SpawnSkeletalAssetMesh(TEXT("Hallway Left Branch Door"), TEXT("/Game/DemocracyOffice/Props/ClassicWoodenDoor/door/SkeletalMeshes/door.door"), FVector(-520.0f, -2305.0f, 133.0f), FRotator(0.0f, 180.0f, 0.0f), FVector(1.15f, 1.15f, 1.15f), false))
    {
        SpawnBox(TEXT("Hallway Left Branch Door Placeholder"), FVector(-520.0f, -2305.0f, 145.0f), FVector(1.55f, 0.12f, 2.9f), FLinearColor(0.22f, 0.11f, 0.045f), false);
    }
    SpawnInteractableTarget(TEXT("MeetingRoomDoor"), TEXT("Enter the meeting room."), FVector(-520.0f, -2248.0f, 150.0f), FVector(2.9f, 1.15f, 3.0f));

    if (!SpawnSkeletalAssetMesh(TEXT("Hallway Right Branch Door"), TEXT("/Game/DemocracyOffice/Props/ClassicWoodenDoor/door/SkeletalMeshes/door.door"), FVector(520.0f, -2305.0f, 133.0f), FRotator(0.0f, 180.0f, 0.0f), FVector(1.15f, 1.15f, 1.15f), false))
    {
        SpawnBox(TEXT("Hallway Right Branch Door Placeholder"), FVector(520.0f, -2305.0f, 145.0f), FVector(1.55f, 0.12f, 2.9f), FLinearColor(0.22f, 0.11f, 0.045f), false);
    }
    SpawnInteractableTarget(TEXT("PressRoomDoor"), TEXT("Enter the press release room."), FVector(520.0f, -2248.0f, 150.0f), FVector(2.9f, 1.15f, 3.0f));

    // Meeting room connected to the left T-hall branch.
    SpawnBox(TEXT("Meeting Room Floor"), FVector(-520.0f, -3430.0f, -5.0f), FVector(16.0f, 16.4f, 0.10f), FLinearColor(0.19f, 0.17f, 0.14f));
    SpawnSolidFloor(TEXT("Meeting Room Guaranteed Floor Collision"), FVector(-520.0f, -3430.0f, -8.0f), FVector(16.0f, 16.4f, 0.08f));
    SpawnBox(TEXT("Meeting Room Carpet"), FVector(-520.0f, -3430.0f, 2.0f), FVector(15.2f, 15.6f, 0.035f), FLinearColor(0.18f, 0.025f, 0.035f), false);
    SpawnBox(TEXT("Meeting Room Ceiling"), FVector(-520.0f, -3430.0f, 316.0f), FVector(16.0f, 16.4f, 0.16f), FLinearColor(0.38f, 0.37f, 0.34f), false);
    SpawnBox(TEXT("Meeting Room Back Wall"), FVector(-520.0f, -4250.0f, 160.0f), FVector(16.0f, 0.14f, 3.2f), FLinearColor(0.32f, 0.31f, 0.29f));
    SpawnBox(TEXT("Meeting Room Left Wall"), FVector(-1320.0f, -3430.0f, 160.0f), FVector(0.14f, 16.4f, 3.2f), FLinearColor(0.32f, 0.31f, 0.29f));
    SpawnBox(TEXT("Meeting Room Right Wall"), FVector(280.0f, -3430.0f, 160.0f), FVector(0.14f, 16.4f, 3.2f), FLinearColor(0.32f, 0.31f, 0.29f));
    SpawnBox(TEXT("Meeting Room Front Wall"), FVector(-520.0f, -2610.0f, 160.0f), FVector(16.0f, 0.14f, 3.2f), FLinearColor(0.32f, 0.31f, 0.29f));
    if (!SpawnSkeletalAssetMesh(TEXT("Meeting Room Door"), TEXT("/Game/DemocracyOffice/Props/ClassicWoodenDoor/door/SkeletalMeshes/door.door"), FVector(-520.0f, -2615.0f, 133.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(1.15f, 1.15f, 1.15f), false))
    {
        SpawnBox(TEXT("Meeting Room Door Placeholder"), FVector(-520.0f, -2615.0f, 145.0f), FVector(1.55f, 0.12f, 2.9f), FLinearColor(0.22f, 0.11f, 0.045f), false);
    }
    SpawnInteractableTarget(TEXT("MeetingRoomReturn"), TEXT("Return to the hallway."), FVector(-520.0f, -2675.0f, 150.0f), FVector(2.9f, 1.15f, 3.0f));

    const FVector MeetingTableOrigin(-520.0f, -3430.0f, 0.0f);
    const FVector MeetingFurnitureScale(1.30f, 1.30f, 1.30f);
    const float MeetingTableSurfaceZ = 80.0f;
    // Conference set pieces from the asset pack. Capsule001 and Line* pieces are excluded because they form the vase/incense prop.
    SpawnAssetMesh(TEXT("Conference Table Object002"), TEXT("/Game/DemocracyOffice/Props/MeetingRoom/ConferenceTableChairs/Object002.Object002"), MeetingTableOrigin, FRotator::ZeroRotator, MeetingFurnitureScale, false);
    SpawnAssetMesh(TEXT("Conference Table Object011"), TEXT("/Game/DemocracyOffice/Props/MeetingRoom/ConferenceTableChairs/Object011.Object011"), MeetingTableOrigin, FRotator::ZeroRotator, MeetingFurnitureScale, false);
    SpawnAssetMesh(TEXT("Conference Table Box028"), TEXT("/Game/DemocracyOffice/Props/MeetingRoom/ConferenceTableChairs/Box028.Box028"), MeetingTableOrigin, FRotator::ZeroRotator, MeetingFurnitureScale, false);
    SpawnAssetMesh(TEXT("Conference Chair Object025"), TEXT("/Game/DemocracyOffice/Props/MeetingRoom/ConferenceTableChairs/Object025.Object025"), MeetingTableOrigin, FRotator::ZeroRotator, MeetingFurnitureScale, false);
    SpawnAssetMesh(TEXT("Conference Chair Object026"), TEXT("/Game/DemocracyOffice/Props/MeetingRoom/ConferenceTableChairs/Object026.Object026"), MeetingTableOrigin, FRotator::ZeroRotator, MeetingFurnitureScale, false);
    SpawnAssetMesh(TEXT("Meeting Whiteboard"), TEXT("/Game/DemocracyOffice/Props/MeetingRoom/Whiteboard/theappxr_Lavagna_1_NODRACO_standard/StaticMeshes/theappxr_Lavagna_1_NODRACO_standard.theappxr_Lavagna_1_NODRACO_standard"), FVector(-520.0f, -4240.0f, 190.0f), FRotator(0.0f, 180.0f, 0.0f), FVector(1.65f, 1.65f, 1.65f), false);
    SpawnAssetMesh(TEXT("Meeting Room Phone"), TEXT("/Game/DemocracyOffice/Props/Phone/nt_vis200_01/StaticMeshes/nt_vis200_01.nt_vis200_01"), FVector(-520.0f, -3430.0f, MeetingTableSurfaceZ - 8.0f), FRotator(0.0f, -25.0f, 0.0f), FVector(0.50f, 0.50f, 0.50f), false);
    SpawnAssetMesh(TEXT("Meeting Desk Outlet"), TEXT("/Game/DemocracyOffice/Props/MeetingRoom/DeskOutlet/deskoutlet_quads.deskoutlet_quads"), FVector(-610.0f, -3385.0f, MeetingTableSurfaceZ + 28.0f), FRotator(0.0f, 15.0f, 0.0f), FVector(0.55f, 0.55f, 0.55f), false);
    const struct FMeetingAdvisorPlacement
    {
        const TCHAR* Label;
        const TCHAR* TargetName;
        const TCHAR* Prompt;
        FVector ChairLocation;
        FLinearColor Color;
    } AdvisorPlacements[] = {
        { TEXT("Resource Manager Placard"), TEXT("MeetingAdvisor_Resources"), TEXT("Meet with the resource manager."), FVector(-624.0f, -3560.0f, 72.0f), FLinearColor(0.12f, 0.35f, 0.16f) },
        { TEXT("Military Advisor Placard"), TEXT("MeetingAdvisor_Military"), TEXT("Meet with the military advisor."), FVector(-520.0f, -3560.0f, 72.0f), FLinearColor(0.36f, 0.10f, 0.09f) },
        { TEXT("Social Advisor Placard"), TEXT("MeetingAdvisor_Social"), TEXT("Meet with the social advisor."), FVector(-416.0f, -3560.0f, 72.0f), FLinearColor(0.12f, 0.22f, 0.48f) },
        { TEXT("Economic Advisor Placard"), TEXT("MeetingAdvisor_Economy"), TEXT("Meet with the economic advisor."), FVector(-624.0f, -3300.0f, 72.0f), FLinearColor(0.58f, 0.42f, 0.10f) },
        { TEXT("Diplomacy Advisor Placard"), TEXT("MeetingAdvisor_Diplomacy"), TEXT("Meet with the diplomacy advisor."), FVector(-520.0f, -3300.0f, 72.0f), FLinearColor(0.38f, 0.18f, 0.45f) },
        { TEXT("Infrastructure Advisor Placard"), TEXT("MeetingAdvisor_Infrastructure"), TEXT("Meet with the infrastructure advisor."), FVector(-416.0f, -3300.0f, 72.0f), FLinearColor(0.38f, 0.32f, 0.24f) },
        { TEXT("Security Advisor Placard"), TEXT("MeetingAdvisor_Intelligence"), TEXT("Meet with the security advisor."), FVector(-714.0f, -3430.0f, 72.0f), FLinearColor(0.08f, 0.08f, 0.10f) },
        { TEXT("Public Welfare Advisor Placard"), TEXT("MeetingAdvisor_Welfare"), TEXT("Meet with the public welfare advisor."), FVector(-326.0f, -3430.0f, 72.0f), FLinearColor(0.12f, 0.45f, 0.42f) }
    };

    for (const FMeetingAdvisorPlacement& Advisor : AdvisorPlacements)
    {
        const bool bIsEndTableAdvisor = FCString::Strcmp(Advisor.TargetName, TEXT("MeetingAdvisor_Intelligence")) == 0 || FCString::Strcmp(Advisor.TargetName, TEXT("MeetingAdvisor_Welfare")) == 0;
        if (bIsEndTableAdvisor)
        {
            SpawnBox(Advisor.Label, Advisor.ChairLocation + FVector(0.0f, 0.0f, 18.0f), FVector(0.18f, 0.40f, 0.08f), Advisor.Color, false);
        }
        else
        {
            SpawnBox(Advisor.Label, Advisor.ChairLocation + FVector(0.0f, 0.0f, 18.0f), FVector(0.40f, 0.18f, 0.08f), Advisor.Color, false);
        }
        SpawnInteractableTarget(Advisor.TargetName, Advisor.Prompt, Advisor.ChairLocation + FVector(0.0f, 0.0f, 58.0f), FVector(0.95f, 0.95f, 1.35f));
    }    // Press release room connected to the right T-hall branch.
    SpawnBox(TEXT("Press Room Floor"), FVector(1680.0f, -3480.0f, -5.0f), FVector(16.0f, 18.0f, 0.10f), FLinearColor(0.18f, 0.17f, 0.16f));
    SpawnSolidFloor(TEXT("Press Room Guaranteed Floor Collision"), FVector(1680.0f, -3480.0f, -8.0f), FVector(16.0f, 18.0f, 0.08f));
    SpawnBox(TEXT("Press Room Carpet"), FVector(1680.0f, -3480.0f, 2.0f), FVector(15.2f, 17.2f, 0.035f), FLinearColor(0.10f, 0.045f, 0.035f), false);
    SpawnBox(TEXT("Press Room Ceiling"), FVector(1680.0f, -3480.0f, 316.0f), FVector(16.0f, 18.0f, 0.16f), FLinearColor(0.38f, 0.37f, 0.34f), false);
    SpawnBox(TEXT("Press Room Back Wall"), FVector(1680.0f, -4380.0f, 160.0f), FVector(16.0f, 0.14f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Press Room Left Wall"), FVector(880.0f, -3480.0f, 160.0f), FVector(0.14f, 18.0f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Press Room Right Wall"), FVector(2480.0f, -3480.0f, 160.0f), FVector(0.14f, 18.0f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    SpawnBox(TEXT("Press Room Front Wall"), FVector(1680.0f, -2580.0f, 160.0f), FVector(16.0f, 0.14f, 3.2f), FLinearColor(0.31f, 0.30f, 0.28f));
    if (!SpawnSkeletalAssetMesh(TEXT("Press Room Door"), TEXT("/Game/DemocracyOffice/Props/ClassicWoodenDoor/door/SkeletalMeshes/door.door"), FVector(1680.0f, -2585.0f, 133.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(1.15f, 1.15f, 1.15f), false))
    {
        SpawnBox(TEXT("Press Room Door Placeholder"), FVector(1680.0f, -2585.0f, 145.0f), FVector(1.55f, 0.12f, 2.9f), FLinearColor(0.22f, 0.11f, 0.045f), false);
    }
    SpawnInteractableTarget(TEXT("PressRoomReturn"), TEXT("Return to the hallway."), FVector(1680.0f, -2645.0f, 150.0f), FVector(2.9f, 1.15f, 3.0f));

    SpawnBox(TEXT("Press Stage"), FVector(1680.0f, -4200.0f, 18.0f), FVector(8.6f, 2.8f, 0.36f), FLinearColor(0.14f, 0.075f, 0.035f));
    SpawnRotatedBox(TEXT("Press Stage Left Side Ramp"), FVector(1085.0f, -4200.0f, 18.0f), FRotator(7.0f, 0.0f, 0.0f), FVector(3.25f, 1.45f, 0.08f), FLinearColor(0.14f, 0.075f, 0.035f));
    SpawnRotatedBox(TEXT("Press Stage Right Side Ramp"), FVector(2275.0f, -4200.0f, 18.0f), FRotator(-7.0f, 0.0f, 0.0f), FVector(3.25f, 1.45f, 0.08f), FLinearColor(0.14f, 0.075f, 0.035f));
    SpawnBox(TEXT("Press Podium Base"), FVector(1680.0f, -4080.0f, 42.0f), FVector(0.72f, 0.52f, 0.84f), FLinearColor(0.16f, 0.085f, 0.035f));
    SpawnBox(TEXT("Press Podium Top"), FVector(1680.0f, -4090.0f, 91.0f), FVector(0.92f, 0.62f, 0.16f), FLinearColor(0.20f, 0.11f, 0.045f));
    SpawnBox(TEXT("Press Podium Mic Left"), FVector(1654.0f, -4115.0f, 112.0f), FVector(0.035f, 0.035f, 0.35f), FLinearColor(0.02f, 0.02f, 0.02f), false);
    SpawnBox(TEXT("Press Podium Mic Right"), FVector(1706.0f, -4115.0f, 112.0f), FVector(0.035f, 0.035f, 0.35f), FLinearColor(0.02f, 0.02f, 0.02f), false);
    SpawnInteractableTarget(TEXT("PressPodium"), TEXT("Make a press announcement."), FVector(1680.0f, -4090.0f, 130.0f), FVector(1.7f, 1.4f, 2.1f));

    const float PressChairXOffsets[6] = { -330.0f, -220.0f, -110.0f, 110.0f, 220.0f, 330.0f };
    for (int32 RowIndex = 0; RowIndex < 5; ++RowIndex)
    {
        const float ChairY = -3150.0f - static_cast<float>(RowIndex) * 145.0f;
        for (int32 ChairIndex = 0; ChairIndex < 6; ++ChairIndex)
        {
            const FVector ChairLocation(1680.0f + PressChairXOffsets[ChairIndex], ChairY, 12.0f);
            const FString ChairBaseName = FString::Printf(TEXT("Press Chair %02d_%02d"), RowIndex + 1, ChairIndex + 1);
            SpawnBox(ChairBaseName + TEXT(" Seat"), ChairLocation + FVector(0.0f, 0.0f, 18.0f), FVector(0.42f, 0.42f, 0.16f), FLinearColor(0.045f, 0.045f, 0.052f));
            SpawnBox(ChairBaseName + TEXT(" Back"), ChairLocation + FVector(0.0f, 24.0f, 58.0f), FVector(0.42f, 0.08f, 0.70f), FLinearColor(0.035f, 0.035f, 0.042f));
            SpawnBox(ChairBaseName + TEXT(" Leg"), ChairLocation + FVector(0.0f, 0.0f, -3.0f), FVector(0.30f, 0.30f, 0.30f), FLinearColor(0.02f, 0.02f, 0.024f), false);
        }
    }


    if (UWorld* World = GetWorld())
    {
        APointLight* HallwayLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(0.0f, -1040.0f, 235.0f), FRotator::ZeroRotator);
        if (HallwayLight && HallwayLight->PointLightComponent)
        {
            HallwayLight->SetActorLabel(TEXT("Hallway Light"));
            HideEditorOnlyLightVisual(HallwayLight);
            HallwayLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            HallwayLight->PointLightComponent->SetIntensity(6500.0f);
            HallwayLight->PointLightComponent->SetAttenuationRadius(850.0f);
        }

        APointLight* HallwayFarLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(0.0f, -1540.0f, 235.0f), FRotator::ZeroRotator);
        if (HallwayFarLight && HallwayFarLight->PointLightComponent)
        {
            HallwayFarLight->SetActorLabel(TEXT("Hallway Far Light"));
            HideEditorOnlyLightVisual(HallwayFarLight);
            HallwayFarLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            HallwayFarLight->PointLightComponent->SetIntensity(5200.0f);
            HallwayFarLight->PointLightComponent->SetAttenuationRadius(850.0f);
        }

        APointLight* HallwayLeftBranchLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(-600.0f, -1960.0f, 235.0f), FRotator::ZeroRotator);
        if (HallwayLeftBranchLight && HallwayLeftBranchLight->PointLightComponent)
        {
            HallwayLeftBranchLight->SetActorLabel(TEXT("Hallway Left Branch Light"));
            HideEditorOnlyLightVisual(HallwayLeftBranchLight);
            HallwayLeftBranchLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            HallwayLeftBranchLight->PointLightComponent->SetIntensity(5200.0f);
            HallwayLeftBranchLight->PointLightComponent->SetAttenuationRadius(850.0f);
        }

        APointLight* HallwayRightBranchLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), FVector(600.0f, -1960.0f, 235.0f), FRotator::ZeroRotator);
        if (HallwayRightBranchLight && HallwayRightBranchLight->PointLightComponent)
        {
            HallwayRightBranchLight->SetActorLabel(TEXT("Hallway Right Branch Light"));
            HideEditorOnlyLightVisual(HallwayRightBranchLight);
            HallwayRightBranchLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
            HallwayRightBranchLight->PointLightComponent->SetIntensity(5200.0f);
            HallwayRightBranchLight->PointLightComponent->SetAttenuationRadius(850.0f);
        }
        const FVector PressRoomOverheadLights[] = {
            FVector(1180.0f, -3000.0f, 270.0f), FVector(1680.0f, -3000.0f, 270.0f), FVector(2180.0f, -3000.0f, 270.0f),
            FVector(1180.0f, -3600.0f, 270.0f), FVector(1680.0f, -3600.0f, 270.0f), FVector(2180.0f, -3600.0f, 270.0f),
            FVector(1180.0f, -4200.0f, 270.0f), FVector(1680.0f, -4200.0f, 270.0f), FVector(2180.0f, -4200.0f, 270.0f)
        };
        for (int32 LightIndex = 0; LightIndex < UE_ARRAY_COUNT(PressRoomOverheadLights); ++LightIndex)
        {
            APointLight* PressRoomLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), PressRoomOverheadLights[LightIndex], FRotator::ZeroRotator);
            if (PressRoomLight && PressRoomLight->PointLightComponent)
            {
                PressRoomLight->SetActorLabel(FString::Printf(TEXT("Press Room Overhead Light %02d"), LightIndex + 1));
                HideEditorOnlyLightVisual(PressRoomLight);
                PressRoomLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
                PressRoomLight->PointLightComponent->SetIntensity(2100.0f);
                PressRoomLight->PointLightComponent->SetAttenuationRadius(760.0f);
                PressRoomLight->PointLightComponent->SetCastShadows(false);
            }
        }

        const FVector MeetingRoomOverheadLights[] = {
            FVector(-1020.0f, -3030.0f, 270.0f), FVector(-520.0f, -3030.0f, 270.0f), FVector(-20.0f, -3030.0f, 270.0f),
            FVector(-1020.0f, -3430.0f, 270.0f), FVector(-520.0f, -3430.0f, 270.0f), FVector(-20.0f, -3430.0f, 270.0f),
            FVector(-1020.0f, -3830.0f, 270.0f), FVector(-520.0f, -3830.0f, 270.0f), FVector(-20.0f, -3830.0f, 270.0f)
        };
        for (int32 LightIndex = 0; LightIndex < UE_ARRAY_COUNT(MeetingRoomOverheadLights); ++LightIndex)
        {
            APointLight* MeetingRoomLight = World->SpawnActor<APointLight>(APointLight::StaticClass(), MeetingRoomOverheadLights[LightIndex], FRotator::ZeroRotator);
            if (MeetingRoomLight && MeetingRoomLight->PointLightComponent)
            {
                MeetingRoomLight->SetActorLabel(FString::Printf(TEXT("Meeting Room Overhead Light %02d"), LightIndex + 1));
                HideEditorOnlyLightVisual(MeetingRoomLight);
                MeetingRoomLight->PointLightComponent->SetMobility(EComponentMobility::Movable);
                MeetingRoomLight->PointLightComponent->SetIntensity(1900.0f);
                MeetingRoomLight->PointLightComponent->SetAttenuationRadius(700.0f);
                MeetingRoomLight->PointLightComponent->SetCastShadows(false);
            }
        }
    }

    // Imported globe is a multi-part GLB. Spawn the pieces together and use one invisible target for interaction.
    const FVector GlobeLocation(740.0f, 555.0f, 0.0f);
    const FRotator GlobeRotation(0.0f, -35.0f, 0.0f);
    const FVector GlobeScale(1.65f, 1.65f, 1.65f);
    SpawnAssetMesh(TEXT("Earth Globe Object 2"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_2.Object_2"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 3"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_3.Object_3"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 4"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_4.Object_4"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 5"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_5.Object_5"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 6"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_6.Object_6"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 7"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_7.Object_7"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 8"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_8.Object_8"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 9"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_9.Object_9"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 10"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_10.Object_10"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 11"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_11.Object_11"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnAssetMesh(TEXT("Earth Globe Object 12"), TEXT("/Game/DemocracyOffice/Props/EarthGlobe/earth_globe8k/StaticMeshes/Object_12.Object_12"), GlobeLocation, GlobeRotation, GlobeScale, false);
    SpawnInteractableTarget(TEXT("Globe"), TEXT("Open the world map."), FVector(740.0f, 555.0f, 155.0f), FVector(3.8f, 3.8f, 3.6f));
}
