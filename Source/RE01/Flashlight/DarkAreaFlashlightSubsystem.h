#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DarkAreaFlashlightSubsystem.generated.h"

class ACharacter;
class UBoxComponent;
class UDarkAreaFlashlightComponent;

/** Adapts the existing Blueprint assets without reparenting or resaving them. */
UCLASS()
class RE01_API UDarkAreaFlashlightSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    void CollectOverlappingBoxes(const ACharacter& Character, TSet<TWeakObjectPtr<UBoxComponent>>& OutBoxes);
    void UpdateFlashlightPickups(const ACharacter& Character, UDarkAreaFlashlightComponent& Controller);

protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
    void RegisterActor(AActor* Actor);
    void RegisterPickup(AActor* Actor);
    void OnActorSpawned(AActor* Actor);
    void OnLevelAdded(ULevel* Level, UWorld* World);

    UPROPERTY(Transient)
    TObjectPtr<UClass> ShooterClass;

    UPROPERTY(Transient)
    TObjectPtr<UClass> DarkAreaClass;

    UPROPERTY(Transient)
    TObjectPtr<UClass> PickupClass;

    struct FPickupProgress
    {
        TWeakObjectPtr<UDarkAreaFlashlightComponent> Collector;
        double ContactTime = 0.0;
    };

    TMap<TWeakObjectPtr<AActor>, FPickupProgress> Pickups;

    TSet<TWeakObjectPtr<AActor>> DarkAreas;
    FDelegateHandle ActorSpawnedHandle;
    FDelegateHandle LevelAddedHandle;
};
