// Fill out your copyright notice in the Description page of Project Settings.


#include "Blaster/Character/BlasterCharacter.h"

#include "Camera/CameraComponent.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Blaster/Blaster.h"
#include "Blaster/BlasterComponents/CombatComponent.h"
#include "Blaster/GameMode/BlasterGameMode.h"
#include "Blaster/PlayerController/BlasterPlayerController.h"
#include "Blaster/Weapon/Weapon.h"
#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/UnitConversion.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ABlasterCharacter::ABlasterCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.0f;
	CameraBoom->bUsePawnControlRotation= true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 850.f);

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

	
	SetNetUpdateFrequency(66.0f);
	SetMinNetUpdateFrequency(33.0f);
	
}

// Called when the game starts or when spawned
void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();

	BlasterPlayerController =  BlasterPlayerController == nullptr ?  Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(BlasterPlayerController->GetLocalPlayer()))
		{
			if (InputMapping)
			{
				Subsystem->AddMappingContext(InputMapping, 0);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("InputMapping is null!"));
			}
		}
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ABlasterCharacter::ReceiveDamage);
	}
}

// Called every frame
void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetLocalRole()> ROLE_SimulatedProxy && IsLocallyControlled())
	{
		AimOffset(DeltaTime);
	}
	else
	{
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f)
		{
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
	HideCameraIfCharacterClose();
}
void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(ABlasterCharacter, Health);
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
}

void ABlasterCharacter::PlayFireMontage(bool bAiming)
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayElimMontage()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ElimMontage)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();

	
	SimProxiesTurn();
	
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::Elim_Implementation()
{
	bElimmed = true;
	PlayElimMontage();
}


void ABlasterCharacter::PlayHitReactMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage && !bElimmed)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::ReceiveDamage(AActor* DamageActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedActor, AActor* DamageCauser)
{
	Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
	UpdateHUDHealth();
	PlayHitReactMontage();

	if (Health <= 0.0f)
	{
		ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>();
		if (BlasterGameMode)
		{
			BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, Cast<ABlasterPlayerController>(InstigatedActor) );
		}
	}
	
}

void ABlasterCharacter::UpdateHUDHealth()
{
	BlasterPlayerController =  BlasterPlayerController == nullptr ?  Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void ABlasterCharacter::OnRep_Health()
{
	UpdateHUDHealth();
	PlayHitReactMontage();
}





void ABlasterCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();
	const FRotator Rotation(0.f, Controller->GetControlRotation().Yaw, Controller->GetControlRotation().Roll);
	const FVector RightDirection( FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y));
	const FVector ForwardDirection( FRotationMatrix(Rotation).GetUnitAxis(EAxis::X));
	AddMovementInput(RightDirection, MovementVector.X);
	AddMovementInput(ForwardDirection, MovementVector.Y);
}

void ABlasterCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void ABlasterCharacter::BlasterJump(const FInputActionValue& Value)
{
	UE_LOG(LogTemp, Display, TEXT("JumpAction"));
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();	
	}
}

void ABlasterCharacter::EquipButtonPressed(const FInputActionValue& Value)
{
	if (Combat )
	{
		if (HasAuthority())
		{
			Combat->EquipWeapon(OverlappingWeapon);
		}else
		{
			ServerEquipButtonPressed();
		}
	}
}

void ABlasterCharacter::CrouchButtonPressed(const FInputActionValue& Value)
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();	
	}
	
}

void ABlasterCharacter::AimButtonPressed(const FInputActionValue& Value)
{
	//UE_LOG(LogTemp, Display, TEXT("AimButtonPressed"));
	if (Combat)
	{
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased(const FInputActionValue& Value)
{
	//UE_LOG(LogTemp, Display, TEXT("AimButtonReleased"));
	if (Combat)
	{
		Combat->SetAiming(false);
	}
}

void ABlasterCharacter::CalculateAO_Pitch()
{
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch >90.f && !IsLocallyControlled())  // all this because of packed data to send less data over the network. 
	{
		// amp pitch from range [270, 360) to [-90, 90)
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped( InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	if (Combat && Combat->EquippedWeapon == nullptr) return; // no weapon -> not aiming !!!! 
		
	float Speed =CalculateSpeed();
	bool bIsInAir = GetCharacterMovement()->IsFalling();
	
	if (Speed == 0.0f && !bIsInAir) // standing still, not jumping
	{
		bRotateRootBone = true;
		FRotator CurrentAimeRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.0f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimeRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
		
		
	}
	if ( Speed > 0.0f || bIsInAir ) // running or jumping
	{
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.0f);
		AO_Pitch = 0.0f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	
	CalculateAO_Pitch();
}

void ABlasterCharacter::SimProxiesTurn()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	bRotateRootBone = false;
	float Speed =CalculateSpeed();
	if (Speed > 0.0f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	
	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	//UE_LOG(LogTemp, Display, TEXT("ProxyYaw: %f"), ProxyYaw);
	if (FMath::Abs(ProxyYaw) >TurnThreshold)
	{
		if (ProxyYaw > TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		else
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void ABlasterCharacter::FireButtonPressed(const FInputActionValue& Value) 
{
	if (Combat)
	{
		Combat->FireButtonPressed(true);
	}
}

void ABlasterCharacter::FireButtonReleased(const FInputActionValue& Value) 
{
	if (Combat)
	{
		Combat->FireButtonPressed(false);
	}
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	//UE_LOG(LogTemp, Display, TEXT("TurnInPlace : AO_Yaw : %f"), AO_Yaw);
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.0f);
		}
		
	}
}



void ABlasterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}

}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	if (Combat && HasAuthority())
	{
		Combat->EquipWeapon(OverlappingWeapon);
	}
}



void ABlasterCharacter::HideCameraIfCharacterClose()
{
	if (!IsLocallyControlled()) return;
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() <= CameraThreshold )
	{
		GetMesh()->SetVisibility(false);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}else
	{
		GetMesh()->SetVisibility(true);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

float ABlasterCharacter::CalculateSpeed()
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.0f;
	return Velocity.Size();
}


void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(false);
		}
	}

	OverlappingWeapon = Weapon;

	if (IsLocallyControlled())
	{
		if (IsLocallyControlled())
		{
			if (OverlappingWeapon)
			{
				OverlappingWeapon->ShowPickupWidget(true);
			}
		}
	}
}

bool ABlasterCharacter::IsWeaponEquipped()
{
	return (Combat && Combat->EquippedWeapon);
}

bool ABlasterCharacter::IsAiming()
{
	return (Combat && Combat->bAiming);
}

AWeapon* ABlasterCharacter::GetEquippedWeapon()
{
	if (Combat == nullptr) return nullptr;

	return  Combat->EquippedWeapon;
}

FVector ABlasterCharacter::GetHitTarget() const
{
	if (Combat == nullptr) return  FVector();
	return  Combat->HitTarget;
	
}


// Called to bind functionality to input
void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			UE_LOG(LogTemp, Display, TEXT("Binding MoveAction"));
			Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::Move);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MoveAction is null!"));
		}
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABlasterCharacter::Look);
		Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ABlasterCharacter::BlasterJump);
		Input->BindAction(EquipAction, ETriggerEvent::Started, this, &ABlasterCharacter::EquipButtonPressed);
		Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ABlasterCharacter::CrouchButtonPressed );
		Input->BindAction(AimAction, ETriggerEvent::Started, this, &ABlasterCharacter::AimButtonPressed );
		Input->BindAction(AimAction, ETriggerEvent::Completed, this, &ABlasterCharacter::AimButtonReleased );
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &ABlasterCharacter::FireButtonPressed );
		Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ABlasterCharacter::FireButtonReleased );
	}

}


