#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Flashlight/DarkAreaLightState.h"
#include "Flashlight/DarkAreaFlashlightComponent.h"
#include "Flashlight/DarkAreaFlashlightSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkAreaDelayTest, "RE01.DarkAreaFlashlight.GracePeriod",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkAreaDelayTest::RunTest(const FString& Parameters)
{
    FDarkAreaLightState State;
    TestFalse(TEXT("Spawn outside stays off"), State.Update(false, true, 0.0, 0.15));
    TestTrue(TEXT("Enter A lights immediately"), State.Update(true, true, 1.0, 0.15));
    TestTrue(TEXT("A+B seam stays on"), State.Update(true, true, 2.0, 0.15));
    TestTrue(TEXT("Leave A while inside B stays on"), State.Update(true, true, 3.0, 0.15));
    TestTrue(TEXT("Leaving all areas starts grace"), State.Update(false, true, 4.0, 0.15));
    TestTrue(TEXT("Still on before 0.15s"), State.Update(false, true, 4.149, 0.15));
    TestFalse(TEXT("Off after 0.15s and fresh outside result"), State.Update(false, true, 4.151, 0.15));
    TestFalse(TEXT("Outside does not restart grace"), State.bWaitingToTurnOff);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkAreaReentryTest, "RE01.DarkAreaFlashlight.ReentryAndPermission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkAreaReentryTest::RunTest(const FString& Parameters)
{
    FDarkAreaLightState State;
    TestTrue(TEXT("Spawn inside lights immediately"), State.Update(true, true, 0.0, 0.15));
    State.Update(false, true, 1.0, 0.15);
    TestTrue(TEXT("Enter B during grace"), State.Update(true, true, 1.1, 0.15));
    TestFalse(TEXT("Reentry cancels pending off"), State.bWaitingToTurnOff);
    TestTrue(TEXT("Old deadline cannot turn off light"), State.Update(true, true, 1.2, 0.15));
    State.Update(false, true, 2.0, 0.15);
    TestTrue(TEXT("Reentry on expiry wins"), State.Update(true, true, 2.16, 0.15));
    State.Update(false, true, 3.0, 0.15);
    TestTrue(TEXT("New exit has a fresh full grace period"), State.Update(false, true, 3.14, 0.15));
    TestFalse(TEXT("New grace eventually expires"), State.Update(false, true, 3.16, 0.15));
    TestFalse(TEXT("Permission blocks entry"), State.Update(true, false, 4.0, 0.15));
    TestTrue(TEXT("Enabling permission while inside lights up"), State.Update(true, true, 4.1, 0.15));
    TestFalse(TEXT("Revoking permission suppresses light"), State.Update(true, false, 4.2, 0.15));
    TestFalse(TEXT("Enabling while outside does not light up"), State.Update(false, true, 4.3, 0.15));
    State.Update(true, true, 5.0, 0.0);
    TestFalse(TEXT("Zero delay is supported"), State.Update(false, true, 5.1, 0.0));
    return true;
}

// Uses the REAL saved BP assets in a disposable world. Never saves or edits them.
// Does not run character BeginPlay (its unrelated weapon/UI setup needs a full game).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDarkAreaGeometryTest, "RE01.DarkAreaFlashlight.ExistingBlueprintIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDarkAreaGeometryTest::RunTest(const FString& Parameters)
{
    UClass* ShooterClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/Variant_Shooter/Blueprints/BP_ShooterCharacter.BP_ShooterCharacter_C"));
    UClass* AreaClass = LoadClass<AActor>(nullptr, TEXT("/Game/Actors/BP_DarkArea.BP_DarkArea_C"));
    UClass* PickupClass = LoadClass<AActor>(nullptr, TEXT("/Game/Actors/BP_FlashLight.BP_FlashLight_C"));
    if (!TestNotNull(TEXT("Existing shooter Blueprint loads"), ShooterClass)
        || !TestNotNull(TEXT("Existing dark-area Blueprint loads"), AreaClass)
        || !TestNotNull(TEXT("Existing pickup Blueprint loads"), PickupClass))
    {
        return false;
    }
    TestEqual(TEXT("Shooter still has its original Blueprint parent"), ShooterClass->GetSuperClass()->GetName(), FString(TEXT("BP_FirstPersonCharacter_C")));

    UWorld::InitializationValues IVS;
    IVS.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
    if (!TestNotNull(TEXT("Disposable physics world created"), World))
    {
        return false;
    }
    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    struct FWorldCleanup
    {
        UWorld* World;
        ~FWorldCleanup()
        {
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
    } Cleanup{World};

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AActor* AreaA = World->SpawnActor<AActor>(AreaClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    AActor* AreaB = World->SpawnActor<AActor>(AreaClass, FVector(200, 0, 0), FRotator::ZeroRotator, Spawn);
    ACharacter* Player = World->SpawnActor<ACharacter>(ShooterClass, FVector(-500, 0, 0), FRotator::ZeroRotator, Spawn);
    AActor* Pickup = World->SpawnActor<AActor>(PickupClass, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Area A spawned"), AreaA) || !TestNotNull(TEXT("Area B spawned"), AreaB)
        || !TestNotNull(TEXT("Existing shooter spawned"), Player)
        || !TestNotNull(TEXT("Existing pickup spawned"), Pickup))
    {
        return false;
    }
    UBoxComponent* BoxA = AreaA->FindComponentByClass<UBoxComponent>();
    UBoxComponent* BoxB = AreaB->FindComponentByClass<UBoxComponent>();
    TInlineComponentArray<USpotLightComponent*> LightsBefore(Player);
    if (!TestNotNull(TEXT("Existing Box A"), BoxA) || !TestNotNull(TEXT("Existing Box B"), BoxB)
        || !TestEqual(TEXT("One existing spotlight"), LightsBefore.Num(), 1))
    {
        return false;
    }
    AddInfo(FString::Printf(TEXT("Saved defaults: BoxA collision=%d, generatesOverlap=%d; spotlight=%s, intensity=%.1f, visible=%d"),
        int32(BoxA->GetCollisionEnabled()), BoxA->GetGenerateOverlapEvents(), *LightsBefore[0]->GetName(), LightsBefore[0]->Intensity, LightsBefore[0]->IsVisible()));
    TestTrue(TEXT("Existing Box supports collision queries without setup"), BoxA->IsQueryCollisionEnabled());

    // Normalize ONLY the disposable instances into an A -> seam -> B test corridor.
    AreaA->SetActorScale3D(FVector::OneVector);
    AreaB->SetActorScale3D(FVector::OneVector);
    BoxA->SetWorldTransform(FTransform(FRotator::ZeroRotator, FVector::ZeroVector));
    BoxB->SetWorldTransform(FTransform(FRotator::ZeroRotator, FVector(200, 0, 0)));
    BoxA->SetBoxExtent(FVector(100, 100, 200), true);
    BoxB->SetBoxExtent(FVector(100, 100, 200), true);

    UDarkAreaFlashlightSubsystem* Registry = World->GetSubsystem<UDarkAreaFlashlightSubsystem>();
    if (!TestNotNull(TEXT("Subsystem created automatically"), Registry))
    {
        return false;
    }
    Registry->OnWorldBeginPlay(*World);
    UDarkAreaFlashlightComponent* Controller = Player->FindComponentByClass<UDarkAreaFlashlightComponent>();
    if (!TestNotNull(TEXT("Controller attached automatically to existing BP"), Controller))
    {
        return false;
    }
    Controller->bLogTransitions = true;
    auto Move = [Player, Controller](FVector Position)
    {
        Player->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
        Controller->RefreshFlashlight();
    };
    // Synchronous automation runs in one engine frame. The TimerManager deliberately
    // ticks only once per GFrameCounter, so simulate distinct frames and restore it.
    TGuardValue<uint64> RestoreFrameCounter(GFrameCounter, GFrameCounter);
    auto Advance = [World, Controller](float Seconds)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, Seconds);
        Controller->RefreshFlashlight();
    };

    Controller->RefreshFlashlight();
    TestFalse(TEXT("Outside initially off"), LightsBefore[0]->IsVisible());
    Move(FVector(200, 0, 0));
    TestFalse(TEXT("Entering B before pickup cannot light up"), LightsBefore[0]->IsVisible());
    Controller->SetFlashlightAllowed(true);
    TestFalse(TEXT("Extra permission cannot bypass pickup ownership"), LightsBefore[0]->IsVisible());
    Move(FVector::ZeroVector);
    TestEqual(TEXT("In A: one Box"), Controller->ActiveBoxCount, 1);
    TestFalse(TEXT("Contact with ball does not unlock immediately"), Controller->HasCollectedFlashlight());
    TestFalse(TEXT("Inside A during pickup countdown stays off"), LightsBefore[0]->IsVisible());
    Advance(0.10f);
    TestTrue(TEXT("Pickup still exists after 0.10 seconds"), IsValid(Pickup));
    TestFalse(TEXT("Not unlocked before 0.2 seconds"), Controller->HasCollectedFlashlight());
    // A second contact/refresh must not reset the deadline.
    Controller->RefreshFlashlight();
    Advance(0.11f);
    TestFalse(TEXT("Ball destroyed after 0.2 seconds"), IsValid(Pickup));
    TestTrue(TEXT("Pickup completion unlocks flashlight"), Controller->HasCollectedFlashlight());
    TestTrue(TEXT("In A: actual existing spotlight is visible"), LightsBefore[0]->IsVisible());
    Move(FVector(100, 0, 0));
    TestEqual(TEXT("Touching seam: capsule overlaps both Boxes"), Controller->ActiveBoxCount, 2);
    TestTrue(TEXT("Seam light on"), LightsBefore[0]->IsVisible());
    Move(FVector(200, 0, 0));
    TestEqual(TEXT("In B after leaving A: one Box"), Controller->ActiveBoxCount, 1);
    TestTrue(TEXT("Leave A: light stays on"), LightsBefore[0]->IsVisible());
    Move(FVector(500, 0, 0));
    TestEqual(TEXT("Outside: zero Boxes"), Controller->ActiveBoxCount, 0);
    TestTrue(TEXT("Exit starts pending off"), Controller->bPendingTurnOff);
    Advance(0.10f);
    TestTrue(TEXT("Actual component holds light during grace"), LightsBefore[0]->IsVisible());
    Move(FVector(200, 0, 0));
    Advance(0.10f);
    TestTrue(TEXT("Reentry cancels old expiry"), LightsBefore[0]->IsVisible());
    TestFalse(TEXT("No pending off after reentry"), Controller->bPendingTurnOff);
    Move(FVector(500, 0, 0));
    Advance(0.10f);
    TestTrue(TEXT("Second exit gets new grace period"), LightsBefore[0]->IsVisible());
    Advance(0.06f);
    TestFalse(TEXT("Outside for 0.16s switches actual light off"), LightsBefore[0]->IsVisible());

    Move(FVector(200, 0, 0));
    Controller->SetFlashlightAllowed(false);
    TestFalse(TEXT("Extra condition blocks actual light"), LightsBefore[0]->IsVisible());
    Controller->SetFlashlightAllowed(true);
    TestTrue(TEXT("Condition default/true allows light"), LightsBefore[0]->IsVisible());
    LightsBefore[0]->SetVisibility(false);
    Controller->RefreshFlashlight();
    TestTrue(TEXT("Legacy visibility write cannot persist across refresh"), LightsBefore[0]->IsVisible());

    BoxA->SetWorldTransform(FTransform(FRotator(0, 45, 0), FVector(200, 0, 0), FVector(2, 1, 1)));
    Controller->RefreshFlashlight();
    TestEqual(TEXT("Rotated/scaled overlapping Boxes"), Controller->ActiveBoxCount, 2);
    AreaA->Destroy();
    Controller->RefreshFlashlight();
    TestEqual(TEXT("Destroyed A removed without losing B"), Controller->ActiveBoxCount, 1);
    TestTrue(TEXT("Destroy A while in B stays on"), LightsBefore[0]->IsVisible());
    AreaB->Destroy();
    Controller->RefreshFlashlight();
    Advance(0.16f);
    TestFalse(TEXT("Destroy last area triggers delayed off"), LightsBefore[0]->IsVisible());

    TInlineComponentArray<USpotLightComponent*> LightsAfter(Player);
    TestEqual(TEXT("No extra spotlight created"), LightsAfter.Num(), LightsBefore.Num());
    TestTrue(TEXT("The exact original spotlight is reused"), LightsAfter[0] == LightsBefore[0]);
    TestTrue(TEXT("All controller work happens after normal gameplay"), Controller->PrimaryComponentTick.TickGroup == TG_PostUpdateWork);

    ACharacter* Unrelated = World->SpawnActor<ACharacter>(ACharacter::StaticClass(), FVector(2000, 0, 0), FRotator::ZeroRotator, Spawn);
    Advance(0.02f);
    TestNull(TEXT("Unrelated characters receive no component"), Unrelated->FindComponentByClass<UDarkAreaFlashlightComponent>());
    ACharacter* Respawn = World->SpawnActor<ACharacter>(ShooterClass, FVector(2000, 0, 0), FRotator::ZeroRotator, Spawn);
    Advance(0.02f);
    UDarkAreaFlashlightComponent* RespawnController = Respawn->FindComponentByClass<UDarkAreaFlashlightComponent>();
    if (!TestNotNull(TEXT("New shooter gets controller automatically"), RespawnController))
    {
        return false;
    }
    TestFalse(TEXT("New character does not inherit pickup ownership"), RespawnController->HasCollectedFlashlight());
    AActor* SecondPickup = World->SpawnActor<AActor>(PickupClass, FVector(2000, 0, 0), FRotator::ZeroRotator, Spawn);
    Advance(0.02f); // Deferred registration of the newly spawned pickup.
    RespawnController->RefreshFlashlight();
    Respawn->SetActorLocation(FVector(2500, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
    Advance(0.10f);
    RespawnController->RefreshFlashlight();
    TestTrue(TEXT("Leaving ball does not remove it before delay"), IsValid(SecondPickup));
    TestFalse(TEXT("Outside pickup also waits full delay"), RespawnController->HasCollectedFlashlight());
    Advance(0.11f);
    RespawnController->RefreshFlashlight();
    TestFalse(TEXT("Touch then walk away still consumes ball after delay"), IsValid(SecondPickup));
    TestTrue(TEXT("Pickup outside dark areas grants ownership"), RespawnController->HasCollectedFlashlight());
    TestFalse(TEXT("Pickup outside does not turn light on"), RespawnController->bIsFlashlightOn);
    return true;
}

#endif
