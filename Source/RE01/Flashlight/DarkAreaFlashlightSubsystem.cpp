#include "DarkAreaFlashlightSubsystem.h"

#include "DarkAreaFlashlightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkAreaRegistry, Log, All);

bool UDarkAreaFlashlightSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDarkAreaFlashlightSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    ShooterClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter.BP_ShooterCharacter_C"));
    DarkAreaClass = LoadClass<AActor>(nullptr, TEXT("/Game/Actors/BP_DarkArea.BP_DarkArea_C"));
    PickupClass = LoadClass<AActor>(nullptr, TEXT("/Game/Actors/BP_FlashLight.BP_FlashLight_C"));
    if (!PickupClass)
    {
        UE_LOG(LogDarkAreaRegistry, Warning, TEXT("BP_FlashLight could not be loaded. Flashlight remains locked until a valid pickup is collected."));
    }
    if (!ShooterClass || !DarkAreaClass)
    {
        UE_LOG(LogDarkAreaRegistry, Error, TEXT("Automatic flashlight: required existing Blueprint class could not be loaded."));
        return;
    }

    // Register ALL areas before a component's BeginPlay can query them.
    for (TActorIterator<AActor> It(&InWorld); It; ++It)
    {
        if (It->IsA(DarkAreaClass))
        {
            DarkAreas.Add(*It);
        }
        if (PickupClass && It->IsA(PickupClass))
        {
            RegisterPickup(*It);
        }
    }
    for (TActorIterator<AActor> It(&InWorld, ShooterClass); It; ++It)
    {
        RegisterActor(*It);
    }
    ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::OnActorSpawned));
    LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::OnLevelAdded);
    UE_LOG(LogDarkAreaRegistry, Display, TEXT("Automatic flashlight ready; %d existing dark-area actors registered."), DarkAreas.Num());
}

void UDarkAreaFlashlightSubsystem::RegisterActor(AActor* Actor)
{
    if (!IsValid(Actor) || Actor->GetWorld() != GetWorld())
    {
        return;
    }
    if (DarkAreaClass && Actor->IsA(DarkAreaClass))
    {
        DarkAreas.Add(Actor);
    }
    if (PickupClass && Actor->IsA(PickupClass))
    {
        RegisterPickup(Actor);
    }
    if (ShooterClass && Actor->IsA(ShooterClass) && !Actor->FindComponentByClass<UDarkAreaFlashlightComponent>())
    {
        auto* Controller = NewObject<UDarkAreaFlashlightComponent>(Actor, TEXT("AutomaticDarkAreaFlashlight"), RF_Transient);
        Actor->AddInstanceComponent(Controller);
        Controller->RegisterComponent();
        UE_LOG(LogDarkAreaRegistry, Display, TEXT("Attached automatic flashlight to %s; existing Blueprint parent unchanged."), *Actor->GetName());
    }
}

void UDarkAreaFlashlightSubsystem::OnActorSpawned(AActor* Actor)
{
    if (DarkAreaClass && Actor->IsA(DarkAreaClass))
    {
        DarkAreas.Add(Actor);
    }
    if ((ShooterClass && Actor->IsA(ShooterClass)) || (PickupClass && Actor->IsA(PickupClass)))
    {
        // Spawn notification can precede the Blueprint construction script.
        // Defer component discovery until its existing SpotLight has been created.
        TWeakObjectPtr<AActor> WeakActor(Actor);
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakActor]()
        {
            RegisterActor(WeakActor.Get());
        }));
    }
}

void UDarkAreaFlashlightSubsystem::RegisterPickup(AActor* Actor)
{
    if (Pickups.Contains(Actor))
    {
        return;
    }
    Pickups.Add(Actor);
    TInlineComponentArray<UPrimitiveComponent*> Shapes(Actor);
    for (UPrimitiveComponent* Shape : Shapes)
    {
        if (Shape->IsQueryCollisionEnabled())
        {
            // Reuse the existing Sphere mesh collision. Change only this pickup's
            // Pawn response, not enemy/player collision presets or any saved asset.
            Shape->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
        }
    }
}

void UDarkAreaFlashlightSubsystem::UpdateFlashlightPickups(const ACharacter& Character, UDarkAreaFlashlightComponent& Controller)
{
    const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
    if (!IsValid(Capsule) || !Capsule->IsRegistered())
    {
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    bool bAlreadyPending = false;
    for (const auto& Entry : Pickups)
    {
        bAlreadyPending |= Entry.Value.Collector.Get() == &Controller && Entry.Key.IsValid();
    }
    const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    TArray<TWeakObjectPtr<AActor>> Completed;
    for (auto It = Pickups.CreateIterator(); It; ++It)
    {
        AActor* Pickup = It.Key().Get();
        if (!IsValid(Pickup) || Pickup->IsActorBeingDestroyed())
        {
            It.RemoveCurrent();
            continue;
        }
        if (!Pickup->GetLevel() || !Pickup->GetLevel()->bIsVisible)
        {
            It.Value() = FPickupProgress();
            continue;
        }
        FPickupProgress& Progress = It.Value();
        if (Progress.Collector.IsValid())
        {
            // Contact starts a one-shot countdown. Walking away does not restart it.
            if (Progress.Collector.Get() == &Controller && Now - Progress.ContactTime >= 0.2)
            {
                Completed.Add(Pickup);
                It.RemoveCurrent();
            }
            continue;
        }
        if (Controller.HasCollectedFlashlight() || bAlreadyPending)
        {
            continue;
        }
        TInlineComponentArray<UPrimitiveComponent*> Shapes(Pickup);
        for (UPrimitiveComponent* Shape : Shapes)
        {
            if (IsValid(Shape) && Shape->IsRegistered() && Shape->IsQueryCollisionEnabled()
                && Shape->OverlapComponent(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), CapsuleShape))
            {
                Progress.Collector = &Controller;
                Progress.ContactTime = Now;
                bAlreadyPending = true;
                UE_LOG(LogDarkAreaRegistry, Display, TEXT("%s touched %s; pickup completes in 0.2 game seconds."), *Character.GetName(), *Pickup->GetName());
                break;
            }
        }
    }
    // Remove entries before destroying actors; EndPlay callbacks cannot invalidate
    // an active registry iterator or award this pickup to a second character.
    for (const TWeakObjectPtr<AActor>& WeakPickup : Completed)
    {
        if (AActor* Pickup = WeakPickup.Get())
        {
            if (Pickup->Destroy())
            {
                Controller.bHasCollectedFlashlight = true;
                UE_LOG(LogDarkAreaRegistry, Display, TEXT("%s collected flashlight; automatic dark-area lighting unlocked."), *Character.GetName());
            }
        }
    }
}

void UDarkAreaFlashlightSubsystem::OnLevelAdded(ULevel* Level, UWorld* World)
{
    if (World == GetWorld() && Level)
    {
        for (AActor* Actor : Level->Actors)
        {
            RegisterActor(Actor);
        }
    }
}

void UDarkAreaFlashlightSubsystem::CollectOverlappingBoxes(const ACharacter& Character, TSet<TWeakObjectPtr<UBoxComponent>>& OutBoxes)
{
    OutBoxes.Reset();
    const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
    if (!IsValid(Capsule) || !Capsule->IsRegistered())
    {
        return;
    }

    const FVector Position = Capsule->GetComponentLocation();
    const FQuat Rotation = Capsule->GetComponentQuat();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    for (auto It = DarkAreas.CreateIterator(); It; ++It)
    {
        AActor* Area = It->Get();
        if (!IsValid(Area))
        {
            It.RemoveCurrent();
            continue;
        }
        if (!Area->GetLevel() || !Area->GetLevel()->bIsVisible || Area->IsActorBeingDestroyed())
        {
            continue;
        }
        TInlineComponentArray<UBoxComponent*> Boxes(Area);
        for (UBoxComponent* Box : Boxes)
        {
            // Query actual geometry, not event counts. Mesh/weapon overlaps cannot remove
            // the capsule, and Begin/End event order at a seam cannot momentarily clear it.
            if (IsValid(Box) && Box->IsRegistered() && Box->IsQueryCollisionEnabled()
                && Box->OverlapComponent(Position, Rotation, Shape))
            {
                OutBoxes.Add(Box);
            }
        }
    }
}

void UDarkAreaFlashlightSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
    }
    FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
    DarkAreas.Reset();
    Pickups.Reset();
    Super::Deinitialize();
}
