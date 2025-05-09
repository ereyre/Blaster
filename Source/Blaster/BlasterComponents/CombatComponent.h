// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blaster/HUD/BlasterHUD.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

class ABlasterHUD;
class ABlasterPlayerController;
class AWeapon;
class ABlasterCharacter;

#define TRACE_LENGHT  80000.f

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	
	UCombatComponent();
	friend class ABlasterCharacter;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	

	void EquipWeapon(AWeapon* WeaponToEquip);

protected:
	
	virtual void BeginPlay() override;
	void SetAiming(bool bIsAiming);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bIsAiming);

	UFUNCTION()
	void OnRep_EquippedWeapon();
	void Fire();

	void FireButtonPressed(bool bPressed);

	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize TraceHitTarget );

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize TraceHitTarget);


	void TraceUnderCrosshairs(FHitResult& TraceHitResult);

	void SetHUDCrosshairs(float DeltaTime);
	

private:
	ABlasterCharacter* Character;
	ABlasterPlayerController* PlayerController;
	ABlasterHUD* HUD;

	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon)
	AWeapon* EquippedWeapon;

	UPROPERTY(Replicated)
	bool bAiming;

	UPROPERTY(EditDefaultsOnly)
	float BaseWalkSpeed;

	UPROPERTY(EditDefaultsOnly)
	float AimWalkSpeed;
	
	bool bFireButtonPressed;

	/**
	 * HUD and crosshairs
	 */

	float CrosshairVelocityFactor;
	float CrosshairsInAirFactor;
	float CrosshairsAimFactor;
	float CrosshairsShootingFactor;

	FVector HitTarget;

	FHUDPackage HUDPackage;

	/**
	 * Aiming and FOV
	 */

	// Field of view when not aiming, set to camera FOV in beginplay
	float DefaultFOV;

	UPROPERTY(EditAnywhere, Category=Combat)
	float ZoomedFOV = 30.f;

	float CurrentFOV;
	
	UPROPERTY(EditAnywhere, Category=Combat)
	float ZoomedInterpSpeed = 20.f;

	void InterpFOV(float DeltaTime);


	/**
	 *  Automatic Fire
	*/

	FTimerHandle FireTimer;


	bool bCanFire = true;
	
	void StartFireTimer();
	void FireTimerFinished();
		
};
