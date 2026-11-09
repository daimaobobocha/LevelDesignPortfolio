#include "DarkAreaFlashlightComponent.h"

#include "DarkAreaFlashlightSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDarkAreaFlashlight, Log, All);

UDarkAreaFlashlightComponent::UDarkAreaFlashlightComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UDarkAreaFlashlightComponent::BeginPlay()
{
    Super::BeginPlay();
    // Run after character movement and existing Blueprint gameplay/latent actions.
    AddTickPrerequisiteActor(GetOwner());
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        AddTickPrerequisiteComponent(Character->GetCharacterMovement());
    }
    ResolveSpotLight();
    RefreshFlashlight();
}

void UDarkAreaFlashlightComponent::ResolveSpotLight()
{
    if (SpotLight.IsValid() || !GetOwner())
    {
        return;
    }
    TInlineComponentArray<USpotLightComponent*> Lights(GetOwner());
    for (USpotLightComponent* Light : Lights)
    {
        if (Light->GetFName() == SpotLightComponentName)
        {
            SpotLight = Light;
            return;
        }
    }
    // SCS runtime names may differ from their editor display label.
    // A unique existing spotlight is safe; multiple ambiguous lights are not.
    if (Lights.Num() == 1)
    {
        SpotLight = Lights[0];
    }
    else if (!bReportedMissingLight)
    {
        UE_LOG(LogDarkAreaFlashlight, Warning, TEXT("%s: cannot uniquely resolve existing SpotLight '%s' (%d candidates). No light created."),
            *GetNameSafe(GetOwner()), *SpotLightComponentName.ToString(), Lights.Num());
        bReportedMissingLight = true;
    }
}

bool UDarkAreaFlashlightComponent::CanTurnOnFlashlight_Implementation() const
{
    return true;
}

void UDarkAreaFlashlightComponent::SetFlashlightAllowed(bool bAllowed)
{
    bAllowFlashlight = bAllowed;
    RefreshFlashlight();
}

void UDarkAreaFlashlightComponent::RefreshFlashlight()
{
    UWorld* World = GetWorld();
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!World || !World->IsGameWorld() || !Character)
    {
        return;
    }
    UDarkAreaFlashlightSubsystem* Registry = World->GetSubsystem<UDarkAreaFlashlightSubsystem>();
    if (!Registry)
    {
        return;
    }
    ResolveSpotLight();
    Registry->UpdateFlashlightPickups(*Character, *this);
    TSet<TWeakObjectPtr<UBoxComponent>> Boxes;
    Registry->CollectOverlappingBoxes(*Character, Boxes);
    const int32 PreviousCount = ActiveBoxCount;
    const bool bPreviousOn = bIsFlashlightOn;
    ActiveBoxCount = Boxes.Num();

    // The geometry is freshly queried on EVERY evaluation, including expiry.
    // Re-entering any box cancels the deadline, even on the expiry frame.
    bIsFlashlightOn = State.Update(ActiveBoxCount > 0, bHasCollectedFlashlight && bAllowFlashlight && CanTurnOnFlashlight(),
        World->GetTimeSeconds(), FMath::Max(0.0f, TurnOffDelay));
    bPendingTurnOff = State.bWaitingToTurnOff;

    if (USpotLightComponent* Light = SpotLight.Get())
    {
        // Enforce the final render state if an old Blueprint event also writes visibility.
        // This touches only this existing light, never its children or other gameplay.
        Light->SetVisibility(bIsFlashlightOn, false);
        Light->SetHiddenInGame(!bIsFlashlightOn, false);
    }
    if (bLogTransitions && (PreviousCount != ActiveBoxCount || bPreviousOn != bIsFlashlightOn))
    {
        UE_LOG(LogDarkAreaFlashlight, Display, TEXT("%s Boxes=%d Light=%s PendingOff=%s Time=%.3f"),
            *Character->GetName(), ActiveBoxCount, bIsFlashlightOn ? TEXT("ON") : TEXT("OFF"),
            bPendingTurnOff ? TEXT("YES") : TEXT("NO"), World->GetTimeSeconds());
    }
}

void UDarkAreaFlashlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    RefreshFlashlight();
}

void UDarkAreaFlashlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    State = FDarkAreaLightState();
    SpotLight.Reset();
    Super::EndPlay(EndPlayReason);
}
