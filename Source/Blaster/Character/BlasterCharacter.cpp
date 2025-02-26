// Fill out your copyright notice in the Description page of Project Settings.


#include "Blaster/Character/BlasterCharacter.h"

#include "Camera/CameraComponent.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

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
}

// Called when the game starts or when spawned
void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (Controller)
	{
		UE_LOG(LogTemp, Display, TEXT("%s is possessed by %s"), *GetName(), *Controller->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s is not possessed"), *GetName());
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		UE_LOG(LogTemp, Display, TEXT("Player Controller is %s"), *PlayerController->GetName());
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			if (InputMapping)
			{
				Subsystem->AddMappingContext(InputMapping, 0);
				UE_LOG(LogTemp, Display, TEXT("Input Mapping Context added"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("InputMapping is null!"));
			}
		}
	}
	
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
	Super::Jump();
	UE_LOG(LogTemp, Display, TEXT("JumpAction"));
}

// Called every frame
void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
	}

}

