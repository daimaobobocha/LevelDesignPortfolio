#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DarkAreaLightState.h"
#include "DarkAreaFlashlightComponent.generated.h"

class USpotLightComponent;

/** Controls only the owner's existing flashlight. Never creates a second light. */
UCLASS(ClassGroup=(Lighting), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class RE01_API UDarkAreaFlashlightComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDarkAreaFlashlightComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Area Flashlight", meta=(ClampMin="0.0", Units="s"))
    float TurnOffDelay = 0.15f;

    /** Extra permission condition. Changing this to false immediately suppresses the light. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Area Flashlight")
    bool bAllowFlashlight = true;

    /** Name of the existing component in BP_ShooterCharacter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Area Flashlight")
    FName SpotLightComponentName = TEXT("SpotLight");

    /** Optional extension point for batteries, equipment, etc. Defaults to true. */
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Dark Area Flashlight")
    bool CanTurnOnFlashlight() const;
    virtual bool CanTurnOnFlashlight_Implementation() const;

    UFUNCTION(BlueprintCallable, Category="Dark Area Flashlight")
    void SetFlashlightAllowed(bool bAllowed);

    UFUNCTION(BlueprintCallable, Category="Dark Area Flashlight")
    void RefreshFlashlight();

    UFUNCTION(BlueprintPure, Category="Dark Area Flashlight|Pickup")
    bool HasCollectedFlashlight() const { return bHasCollectedFlashlight; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Dark Area Flashlight|Debug")
    int32 ActiveBoxCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Dark Area Flashlight|Debug")
    bool bIsFlashlightOn = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Dark Area Flashlight|Debug")
    bool bPendingTurnOff = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dark Area Flashlight|Debug")
    bool bLogTransitions = false;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    friend class UDarkAreaFlashlightSubsystem;

    /** Per-character ownership: a new play session/character starts without the pickup. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Dark Area Flashlight|Pickup", meta=(AllowPrivateAccess="true"))
    bool bHasCollectedFlashlight = false;

    void ResolveSpotLight();
    TWeakObjectPtr<USpotLightComponent> SpotLight;
    FDarkAreaLightState State;
    bool bReportedMissingLight = false;
};
