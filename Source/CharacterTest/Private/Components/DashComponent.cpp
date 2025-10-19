// Fill out your copyright notice in the Description page of Project Settings.


#include "Public/Components/DashComponent.h"

#include "Public/CharacterTestAnimInstance.h"
#include "Public/CharacterTestCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"


// Sets default values for this component's properties
UDashComponent::UDashComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UDashComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<ACharacterTestCharacter>(GetOwner());
	check(CharacterOwner);
}

bool UDashComponent::CanDash() const
{
	auto* PC = Cast<APlayerController>(CharacterOwner->GetController());
	bool bCooldown = (LastTimeDashed != 0 && GetWorld()->TimeSeconds - LastTimeDashed < DashCooldown);
	return !bCooldown && PC && !PC->IsMoveInputIgnored();
}

void UDashComponent::DoDash()
{
	if (!CharacterOwner || !CharacterOwner->IsLocallyControlled() || !CanDash())
	{
		return;
	}

	if (!CharacterOwner->HasAuthority())
	{
		Server_DoDash();
	}
	StartDashing();
}

void UDashComponent::Server_DoDash_Implementation()
{
	if (CanDash())
	{
		bDashing = true;
		OnStartDashing();
	}
	else
	{
		bDashing = false;
		Client_CancelDash();
	}
}


// Called every frame
void UDashComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDashing && CharacterOwner->IsLocallyControlled())
	{
		float Alpha = FMath::Min(1, (GetWorld()->TimeSeconds-LastTimeDashed) / DashDuration);
		if (Alpha == 1 || (CharacterOwner->GetVelocity().IsNearlyZero() && Alpha > 0.1f))
		{
			StopDashing();
		}
		else
		{
			float Speed = FMath::InterpEaseInOut(0.f, DashSpeed, Alpha, 3.f);
			FVector Velocity = CharacterOwner->GetActorForwardVector() * Speed;
			FHitResult Hit;
			CharacterOwner->SetActorLocation(CharacterOwner->GetActorLocation() + Velocity * DeltaTime, true, &Hit, ETeleportType::None);

			if (Hit.bBlockingHit)
			{
				StopDashing();
			}
			else
			{
				float FOV = FMath::InterpEaseInOut(CachedFOV, DashFOV, Alpha, 3.f);
				UGameplayStatics::GetPlayerCameraManager(this, 0)->SetFOV(FOV);

				if (bLerpCameraRotationToPlayer)
				{
					FRotator CameraRot = CharacterOwner->GetControlRotation();
					FRotator TargetCameraRot = CameraRot;
					TargetCameraRot.Yaw = CharacterOwner->GetActorRotation().Yaw;
					CharacterOwner->GetController()->SetControlRotation(FMath::RInterpTo(CameraRot, TargetCameraRot, DeltaTime, CameraInterpSpeed));
				}
			}
			
		}
	}
}

void UDashComponent::StartDashing()
{
	bDashing = true;
	OnStartDashing();
}

void UDashComponent::StopDashing()
{
	if (CharacterOwner->IsLocallyControlled() && !CharacterOwner->HasAuthority())
	{
		Server_StopDash();
	}
	
	bDashing = false;
	OnStopDashing();
}

void UDashComponent::Server_StopDash_Implementation()
{
	if (bDashing)
	{
		StopDashing();
	}
}

void UDashComponent::Client_CancelDash_Implementation()
{
	if (bDashing)
	{
		StopDashing();
	}
}

void UDashComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDashComponent, bDashing);
}

void UDashComponent::OnRep_Dashing(bool bWasDashing)
{
	if (UCharacterTestAnimInstance* AnimInstance = Cast<UCharacterTestAnimInstance>(CharacterOwner->GetMesh()->GetAnimInstance()))
	{
		AnimInstance->bIsDashing = bDashing;
	}

	if (bWasDashing != bDashing)
	{
		if (CharacterOwner->IsLocallyControlled())
		{
			if (!bDashing)
			{
				OnStopDashing();
			}
			else if (bDashing)
			{
				OnStartDashing();
			}
		}
	}
	
}

void UDashComponent::OnStartDashing()
{
	LastTimeDashed = GetWorld()->TimeSeconds;
	CharacterOwner->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = true;
	auto* PC = Cast<APlayerController>(CharacterOwner->GetController());

	if (bDashTowardsControllerRotation)
	{
		FRotator ControlRotation = PC->GetControlRotation();
		ControlRotation.Roll = ControlRotation.Pitch = 0;
		CharacterOwner->SetActorRotation(ControlRotation);	
	}

	CachedFOV = UGameplayStatics::GetPlayerCameraManager(this, 0)->GetFOVAngle();

	CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Flying);

	PC->ClientIgnoreMoveInput(true);
	PC->ClientIgnoreLookInput(true);

	if (UCharacterTestAnimInstance* AnimInstance = Cast<UCharacterTestAnimInstance>(CharacterOwner->GetMesh()->GetAnimInstance()))
	{
		AnimInstance->bIsDashing = true;
	}

	OnDashStarted.Broadcast();
}

void UDashComponent::OnStopDashing()
{
	CharacterOwner->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false;
	auto* PC = Cast<APlayerController>(CharacterOwner->GetController());
	CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	CharacterOwner->GetCharacterMovement()->StopActiveMovement();
	
	UGameplayStatics::GetPlayerCameraManager(this, 0)->SetFOV(CachedFOV);

	PC->ClientIgnoreMoveInput(false);
	PC->ClientIgnoreLookInput(false);	
	
	if (UCharacterTestAnimInstance* AnimInstance = Cast<UCharacterTestAnimInstance>(CharacterOwner->GetMesh()->GetAnimInstance()))
	{
		AnimInstance->bIsDashing = false;
	}
	
	OnDashEnded.Broadcast();
}

