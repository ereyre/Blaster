// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapon.generated.h"

class UWidgetComponent;
class UAnimMontage;

UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	EWS_Initial  UMETA(DisplayName = "Initial State"),
	EWS_Equipped  UMETA(DisplayName = "Equipped"),
	EWS_Dropped  UMETA(DisplayName = "Dropped"),

	EWS_MAX  UMETA(DisplayName = "DefaultMAX")
};


class USphereComponent;
class ACasing; 

UCLASS()
class BLASTER_API AWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AWeapon();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	void ShowPickupWidget(bool bShowWidget);
	virtual void Fire(const FVector& HitTarget);
	void Dropped();

	/**
	 *Textures for the weapon crosshair
	 */
	UPROPERTY(EditAnywhere, Category=Crosshairs)
	UTexture2D* CrosshairsCenter;

	UPROPERTY(EditAnywhere, Category=Crosshairs)
	UTexture2D* CrosshairsLeft;

	UPROPERTY(EditAnywhere, Category=Crosshairs)
	UTexture2D* CrosshairsRight;

	UPROPERTY(EditAnywhere, Category=Crosshairs)
	UTexture2D* CrosshairsTop;

	UPROPERTY(EditAnywhere, Category=Crosshairs)
	UTexture2D* CrosshairsBottom;

	/**
	 *Zoomed FOV wwhile aiming
	 */
	UPROPERTY(EditAnywhere)
	float ZoomedFOV = 30.f;

	UPROPERTY(EditAnywhere)
	float ZoomInterpSpeed = 20.f;

	/**
	 *Automatic Fire
	 */
	UPROPERTY(EditAnywhere, Category=Combat)
	float  FireDelay = 0.15f;

	UPROPERTY(EditAnywhere, Category=Combat)
	bool bAutomatic= true;
	

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnSphereOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
		);

	UFUNCTION()
	void OnSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

private:
	UPROPERTY(VisibleAnywhere, Category="Weapon Properties")
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, Category="Weapon Properties")
	USphereComponent* AreaSphere;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_WeaponState ,Category="Weapon Properties")
	EWeaponState WeaponState;

	UFUNCTION()
	void OnRep_WeaponState();
	
	UPROPERTY(VisibleAnywhere, Category="Weapon Properties")
	UWidgetComponent* PickupWidget;

	UPROPERTY(EditAnywhere, Category="Weapon Properties")
	UAnimationAsset* FireAnimation;

	UPROPERTY(EditAnywhere)
	TSubclassOf<ACasing> CasingClass;

	
public:
	void SetWeaponState(EWeaponState State) ;
	FORCEINLINE USphereComponent* GetAreaSphere() {return AreaSphere;}
	FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh;}
	FORCEINLINE float GetZoomedFOV() const { return ZoomedFOV; }
	FORCEINLINE float GetZoomInterpSpeed() const { return ZoomInterpSpeed; }

};

