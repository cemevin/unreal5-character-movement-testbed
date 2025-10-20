// Fill out your copyright notice in the Description page of Project Settings.


#include "Public/Components/WallRunComponent.h"

#include "Public/CharacterTestCharacter.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Public/CharacterTestAnimInstance.h"

// Sets default values for this component's properties
UWallRunComponent::UWallRunComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UWallRunComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWallRunComponent, WallRunState);
}

// Called when the game starts
void UWallRunComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<ACharacterTestCharacter>(GetOwner());
	check(CharacterOwner);

	CharacterOwner->OnJumpAttempted.AddUniqueDynamic(this, &UWallRunComponent::OnJumpAttempted);
	
}

bool UWallRunComponent::CanWallRun()
{
	return WallRunState == EWallRunState::WallRunNone && !CharacterOwner->IsMoveInputIgnored() && CharacterOwner->GetCharacterMovement()->IsFalling();
}

bool UWallRunComponent::CanWallRunFrom(const FHitResult& HitResult) const
{
	// server validates if client can wall run from this position
	AActor* Wall = HitResult.GetActor();

	if (!Wall || Wall->IsPendingKillPending() || !Wall->ActorHasTag(WallRunTag))
	{
		return false;
	}

	constexpr float MaxDistFromWallPoint = 250;
	const bool bFacingWall = CharacterOwner->GetActorForwardVector().Dot(HitResult.ImpactNormal) < 0;
	if (!bFacingWall)
	{
		return false;
	}
	
	const bool bCloseToWall = (CharacterOwner->GetActorLocation() - HitResult.ImpactPoint).SizeSquared() < MaxDistFromWallPoint * MaxDistFromWallPoint;
	if (!bCloseToWall)
	{
		return false;
	}
	
	const bool bSameWallTimeout = LastTimeWallRun != 0 && Wall == WallRunHitResult.GetActor() && GetWorld()->TimeSeconds - LastTimeWallRun < WallRunTimeout;
	if (bSameWallTimeout)
	{
		return false;
	}
	
	return true;
}

void UWallRunComponent::OnStartWallRunning(const FHitResult& HitResult)
{
	WallRunHitResult = HitResult;
	LastTimeWallRunStarted = GetWorld()->TimeSeconds;

	bool bIsWallRunningRight =  WallRunHitResult.ImpactNormal.Dot(CharacterOwner->GetActorRightVector()) > 0;
	WallRunState = bIsWallRunningRight ? EWallRunState::WallRunRight : EWallRunState::WallRunLeft;
	
	CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	CharacterOwner->GetCharacterMovement()->StopActiveMovement();

	if (bOverrideWalkSpeed)
	{
		CachedSpeed = CharacterOwner->GetCharacterMovement()->MaxFlySpeed;
		CachedAcceleration = CharacterOwner->GetCharacterMovement()->MaxAcceleration;
		CharacterOwner->GetCharacterMovement()->MaxFlySpeed = WallRunSpeed;
		CharacterOwner->GetCharacterMovement()->MaxAcceleration = WallRunAcceleration;
	}
	
	FRotator Rot = UKismetMathLibrary::MakeRotFromYZ(WallRunHitResult.ImpactNormal * (bIsWallRunningRight ? 1 : -1), FVector::UpVector);
	CharacterOwner->SetActorRotation(Rot);
	
	CharacterOwner->SetActorLocation(WallRunHitResult.ImpactPoint + CharacterOwner->GetActorRightVector() * (bIsWallRunningRight ? 1 : -1) * CharacterOwner->GetSimpleCollisionRadius());

	if (UCharacterTestAnimInstance* AnimInstance = Cast<UCharacterTestAnimInstance>(CharacterOwner->GetMesh()->GetAnimInstance()))
	{
		AnimInstance->bIsWallRunningRight = bIsWallRunningRight;
		AnimInstance->bIsWallRunningLeft = !bIsWallRunningRight;
	}
	
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	PC->ClientIgnoreLookInput(true);
	PC->ClientIgnoreMoveInput(true);
}

void UWallRunComponent::Server_StartWallRunning_Implementation(const FHitResult& HitResult)
{
	if (!CanWallRunFrom(HitResult))
	{
		Client_CancelWallRun();
		OnStopWallRunning(false);
	}
	else
	{
		OnStartWallRunning(HitResult);
	}
}

void UWallRunComponent::Client_CancelWallRun_Implementation()
{
	OnStopWallRunning(false);
}

void UWallRunComponent::Server_StopWallRunning_Implementation(bool bLaunch)
{
	if (IsWallRunning())
	{
		OnStopWallRunning(bLaunch);
	}
}

void UWallRunComponent::OnRep_WallRunState(EWallRunState PrevState)
{
	if (CharacterOwner->HasAuthority())
	{
		return;
	}

	if (CharacterOwner->IsLocallyControlled())
	{
		if (WallRunState != PrevState)
		{
			if (WallRunState == EWallRunState::WallRunNone)
			{
				OnStopWallRunning(false);
			}
		}
	}
	else
	{
		if (UCharacterTestAnimInstance* AnimInstance = Cast<UCharacterTestAnimInstance>(CharacterOwner->GetMesh()->GetAnimInstance()))
		{
			AnimInstance->bIsWallRunningRight = WallRunState == EWallRunState::WallRunRight;
			AnimInstance->bIsWallRunningLeft = WallRunState == EWallRunState::WallRunLeft;
		}
	}
}

void UWallRunComponent::StartWallRunning(const FHitResult& HitResult)
{
	// same wall check
	if (LastTimeWallRun != 0 && HitResult.GetActor() == WallRunHitResult.GetActor() && GetWorld()->TimeSeconds - LastTimeWallRun < WallRunTimeout)
	{
		return;
	}

	if (!CharacterOwner->HasAuthority())
	{
		Server_StartWallRunning(HitResult);
	}
	
	OnStartWallRunning(HitResult);
}

void UWallRunComponent::StopWallRun(bool bLaunch)
{
	if (!CharacterOwner->HasAuthority())
	{
		Server_StopWallRunning(bLaunch);
	}

	OnStopWallRunning(bLaunch);
}


void UWallRunComponent::OnStopWallRunning(bool bLaunch)
{
	EWallRunState PreviousState = WallRunState;
	WallRunState = EWallRunState::WallRunNone;
	LastTimeWallRun = GetWorld()->GetTimeSeconds();

	CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	
	if (bOverrideWalkSpeed)
	{
		CharacterOwner->GetCharacterMovement()->MaxFlySpeed = CachedSpeed;
		CharacterOwner->GetCharacterMovement()->MaxAcceleration = CachedAcceleration;
	}

	if (UCharacterTestAnimInstance* AnimInstance = Cast<UCharacterTestAnimInstance>(CharacterOwner->GetMesh()->GetAnimInstance()))
	{
		AnimInstance->bIsWallRunningRight = false;
		AnimInstance->bIsWallRunningLeft = false;
	}
	
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	PC->ClientIgnoreLookInput(false);
	PC->ClientIgnoreMoveInput(false);

	if (bLaunch)
	{
		bool bWasWallRunningRight = PreviousState == EWallRunState::WallRunRight;
		FVector Launch;
		if (bOverrideLaunchVelocity)
		{
			Launch = LaunchVelocity;
		}
		else
		{
			Launch = {CharacterOwner->GetCharacterMovement()->MaxWalkSpeed, 0, CharacterOwner->GetCharacterMovement()->JumpZVelocity};
		}

		Launch = CharacterOwner->GetActorQuat().RotateVector(Launch);
		Launch = Launch.RotateAngleAxis(LaunchAngle * (bWasWallRunningRight ? 1 : -1), FVector::UpVector);

		CharacterOwner->SetActorRotation(UKismetMathLibrary::MakeRotFromXZ(Launch.GetSafeNormal2D(), FVector::UpVector));
		
		CharacterOwner->LaunchCharacter(Launch, true, true);
	}
}

void UWallRunComponent::OnJumpAttempted()
{
	if (IsWallRunning())
	{
		StopWallRun(true);
	}
}

// Called every frame
void UWallRunComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CharacterOwner->HasAuthority() && !CharacterOwner->IsLocallyControlled())
	{
		return;
	}

	FrameCounter++;

	if (CanWallRun() && CharacterOwner->GetLastMovementInputVector().Dot(CharacterOwner->GetActorForwardVector()) >= 0 && CharacterOwner->IsLocallyControlled())
	{
		FVector Start = CharacterOwner->GetActorLocation() + FVector::UpVector * WallRunHeightOffset;
		FVector End = Start + CharacterOwner->GetActorForwardVector() * (CharacterOwner->GetSimpleCollisionRadius() + WallRunOffset);
		FVector EndLeft = Start + CharacterOwner->GetActorRightVector() * -1 * (CharacterOwner->GetSimpleCollisionRadius() + WallRunOffset);
		FVector EndRight = Start + CharacterOwner->GetActorRightVector() * 1 * (CharacterOwner->GetSimpleCollisionRadius() + WallRunOffset);
		FHitResult Hit;
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(CharacterOwner);

		// hit trace forward, left, right, do one per frame to not bottleneck the performance
		if (FrameCounter % 3 == 0 && GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjectQueryParams, CollisionParams) && Hit.GetActor()->ActorHasTag(WallRunTag))
		{
			StartWallRunning(Hit);
		}
		else if (FrameCounter % 3 == 1 && GetWorld()->LineTraceSingleByObjectType(Hit, Start, EndLeft, ObjectQueryParams, CollisionParams) && Hit.GetActor()->ActorHasTag(WallRunTag))
		{
			StartWallRunning(Hit);
		}
		else if (FrameCounter % 3 == 2 && GetWorld()->LineTraceSingleByObjectType(Hit, Start, EndRight, ObjectQueryParams, CollisionParams) && Hit.GetActor()->ActorHasTag(WallRunTag))
		{
			StartWallRunning(Hit);
		}
	}
	else if (IsWallRunning())
	{
		FHitResult Hit;

		// lerp Z height to match the wall running section (if needed - makes it feel more streamlined but more game-y)
		FVector DeltaVec = (CharacterOwner->GetActorForwardVector() * (bOverrideWalkSpeed ? WallRunSpeed : CharacterOwner->GetCharacterMovement()->MaxWalkSpeed) * DeltaTime);
		FVector FinalPos = CharacterOwner->GetActorLocation() + DeltaVec;
		
		if (bMatchZHeightWithWallRunSection)
		{
			float Z = FMath::FInterpTo(CharacterOwner->GetActorLocation().Z, WallRunHitResult.GetActor()->GetActorLocation().Z, DeltaTime, 10);
			FinalPos.Z = Z;
		}
		
		// CharacterOwner->SetActorLocation(FinalPos, true, &Hit, ETeleportType::TeleportPhysics);
		CharacterOwner->AddMovementInput((FinalPos - CharacterOwner->GetActorLocation()).GetSafeNormal(), 1, true);
		CharacterOwner->GetCharacterMovement()->Velocity.Z = 0;
		
		FRotator CameraRot = CharacterOwner->GetControlRotation();
		FRotator TargetCameraRot = CameraRot;
		TargetCameraRot.Yaw = CharacterOwner->GetActorRotation().Yaw;
		CharacterOwner->GetController()->SetControlRotation(FMath::RInterpTo(CameraRot, TargetCameraRot, DeltaTime, CameraInterpSpeed));

		if (Hit.bBlockingHit)
		{
			StopWallRun(false);
		}
		else
		{
			// update wall run
			FHitResult Result;
			FVector Start = CharacterOwner->GetActorLocation();
			FVector End = CharacterOwner->GetActorLocation() + CharacterOwner->GetActorRightVector() * (WallRunState == EWallRunState::WallRunRight ? -1 : 1) * CharacterOwner->GetSimpleCollisionRadius() * 2;
			FCollisionObjectQueryParams ObjectQueryParams;
			ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
			FCollisionQueryParams CollisionParams;
			CollisionParams.AddIgnoredActor(CharacterOwner);
			bool bHit = (GetWorld()->LineTraceSingleByObjectType(Result, Start, End, ObjectQueryParams, CollisionParams));

			if (!bHit || Result.GetActor() != WallRunHitResult.GetActor())
			{
				StopWallRun(false);
			}
		}
	}
}

