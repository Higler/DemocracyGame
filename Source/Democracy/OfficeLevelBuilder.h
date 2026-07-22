#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OfficeLevelBuilder.generated.h"

class UStaticMesh;
class UMaterialInterface;

UCLASS()
class DEMOCRACY_API AOfficeLevelBuilder : public AActor
{
    GENERATED_BODY()

public:
    AOfficeLevelBuilder();
    void BuildOffice();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> BaseMaterial;
    bool bOfficeBuilt = false;

    AActor* SpawnBox(const FString& Name, const FVector& Location, const FVector& Scale, const FLinearColor& Color, bool bBlocksPlayer = true);
    bool SpawnAssetMesh(const FString& Name, const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, bool bBlocksPlayer = true);
    bool SpawnSkeletalAssetMesh(const FString& Name, const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, bool bBlocksPlayer = true);
    void SpawnInteractable(const FString& Name, const FString& Message, const FVector& Location, const FVector& Scale, const FLinearColor& Color);
    void SpawnInteractableTarget(const FString& Name, const FString& Message, const FVector& Location, const FVector& Scale);
    bool SpawnInteractableAsset(const FString& Name, const FString& Message, const TCHAR* MeshPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& FallbackColor);
};
