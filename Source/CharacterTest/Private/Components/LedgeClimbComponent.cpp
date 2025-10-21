// Fill out your copyright notice in the Description page of Project Settings.


#include "Public/Components/LedgeClimbComponent.h"

#include "Public/CharacterTestCharacter.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
ULedgeClimbComponent::ULedgeClimbComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void ULedgeClimbComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ULedgeClimbComponent, LedgeClimbState);
}


// Called when the game starts
void ULedgeClimbComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner = Cast<ACharacterTestCharacter>(GetOwner());
	check(CharacterOwner);
	
}

bool ULedgeClimbComponent::CanLedgeClimb()
{
	return !LedgeClimbState.bIsLedgeClimbing && !CharacterOwner->IsMoveInputIgnored() && CharacterOwner->GetCharacterMovement()->IsFalling();
}

bool ULedgeClimbComponent::CanLedgeClimbTo(const FHitResult& HitResult, const FVector& LedgeLocation)
{
	if (CanLedgeClimb())
	{
		AActor* HitActor = HitResult.GetActor();
		if (HitActor && HitActor->ActorHasTag(LedgeTag) )
		{
			FVector CharToLedge = (LedgeLocation - CharacterOwner->GetActorLocation());
			float ForwardDot = CharToLedge.Dot(CharacterOwner->GetActorForwardVector());
			float UpDot = FMath::Abs(CharToLedge.Dot(FVector::UpVector));
			
			const bool bCloseEnough = UpDot < DetectionMaxHeight * DetectionMaxHeight * 1.1f
				&& ForwardDot > 0 && ForwardDot < DetectionDistanceForward * DetectionDistanceForward * 1.1f;

			return bCloseEnough;
		}
	}

	return false;
}

void ULedgeClimbComponent::ServerStartLedgeClimbing_Implementation(const FHitResult& HitResult, const FVector& FeetPosition,
                                                                   const FVector& LedgeLocation, const FRotator& LedgeRotation, bool bJumpingFromBelow)
{
	if (!CanLedgeClimbTo(HitResult, LedgeLocation))
	{
		Client_CancelLedgeClimb();
	}
	else
	{
		StartLedgeClimbing(FeetPosition, LedgeLocation, LedgeRotation, bJumpingFromBelow);
	}
}

void ULedgeClimbComponent::Client_CancelLedgeClimb_Implementation()
{
	if (LedgeClimbState.bIsLedgeClimbing)
	{
		OnLedgeClimbEnded();
		CharacterOwner->GetMesh()->GetAnimInstance()->StopAllMontages(0);
	}
}

void ULedgeClimbComponent::OnRep_LedgeClimb(const FLedgeClimbState& PrevState)
{
	if (!CharacterOwner->IsLocallyControlled())
	{
		if (LedgeClimbState.bIsLedgeClimbing)
		{
			OnStartLedgeClimbing();
		}
		else
		{
			OnLedgeClimbEnded();
		}
	}
}

void ULedgeClimbComponent::OnStartLedgeClimbing()
{
	FRotator CharacterRotation = LedgeClimbState.LedgeRotation;
	CharacterRotation.Yaw += 180;
	CharacterOwner->SetActorRotation(CharacterRotation);

	BP_OnLedgeClimb(LedgeClimbState.bJumpingFromBelow);
	
	FMotionWarpingTarget Target;
	Target.Location = LedgeClimbState.LedgeLocation;
	Target.Name = WarpTargetNameUp;
	CharacterOwner->GetMotionWarping()->AddOrUpdateWarpTarget(Target);
	
	FMotionWarpingTarget TargetFwd;
	Target.Location = LedgeClimbState.FeetPosition;
	Target.Name = WarpTargetNameForward;
	CharacterOwner->GetMotionWarping()->AddOrUpdateWarpTarget(TargetFwd);

	CharacterOwner->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = true;
	CharacterOwner->GetCharacterMovement()->StopActiveMovement();
	CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	CharacterOwner->SetActorEnableCollision(false);

	if (CharacterOwner->IsLocallyControlled() || CharacterOwner->HasAuthority())
	{
		LedgeClimbState.bIsLedgeClimbing = true;

		if (CharacterOwner->IsLocallyControlled())
		{
			APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
			PC->ClientIgnoreLookInput(true);
			PC->ClientIgnoreMoveInput(true);
		}
	}
}

void ULedgeClimbComponent::StartLedgeClimbing(const FVector& FeetLocation, const FVector& LedgeLocation, const FRotator& LedgeRotation, bool bJumpingFromBelow)
{
	LedgeClimbState.bIsLedgeClimbing = true;
	LedgeClimbState.FeetPosition = FeetLocation;
	LedgeClimbState.LedgeLocation = LedgeLocation;
	LedgeClimbState.LedgeRotation = LedgeRotation;
	LedgeClimbState.bJumpingFromBelow = bJumpingFromBelow;

	OnStartLedgeClimbing();
}

void ULedgeClimbComponent::OnLedgeClimbEnded()
{
	CharacterOwner->GetMotionWarping()->RemoveAllWarpTargets();
	CharacterOwner->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false;
	CharacterOwner->SetActorEnableCollision(true);
	CharacterOwner->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	CharacterOwner->GetCharacterMovement()->StopActiveMovement();
	
	if (CharacterOwner->IsLocallyControlled() || CharacterOwner->HasAuthority())
	{
		LedgeClimbState.bIsLedgeClimbing = false;

		if (CharacterOwner->IsLocallyControlled())
		{
			APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
			PC->ClientIgnoreLookInput(false);
			PC->ClientIgnoreMoveInput(false);
		}
	}
}

// Called every frame
void ULedgeClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CharacterOwner->IsLocallyControlled() && !CharacterOwner->HasAuthority())
	{
		return;
	}
	
	if (CanLedgeClimb() && CharacterOwner->GetLastMovementInputVector().Dot(CharacterOwner->GetActorForwardVector()) >= 0 && CharacterOwner->IsLocallyControlled())
	{
		FVector Start = CharacterOwner->GetActorLocation() + FVector::UpVector * DetectionMinHeight;
		FVector End = CharacterOwner->GetActorLocation() + FVector::UpVector * DetectionMaxHeight + CharacterOwner->GetActorForwardVector() * DetectionDistanceForward;
		TArray<FHitResult> Hits;
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(CharacterOwner);

		if (GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(16), CollisionParams))
		{
			FHitResult LedgeHitResult;
			for (auto& Hit : Hits)
			{
				if (Hit.GetActor()->ActorHasTag(LedgeTag))
				{
					LedgeHitResult = Hit;
					break;
				}
			}

			if (LedgeHitResult.GetActor())
			{
				FHitResult VerticalHitResult;
				if (GetWorld()->LineTraceSingleByObjectType(VerticalHitResult, LedgeHitResult.ImpactPoint + FVector::UpVector * 100, LedgeHitResult.ImpactPoint, ObjectQueryParams, CollisionParams))
				{
					FHitResult HorizontalHitResult;
					if (GetWorld()->LineTraceSingleByObjectType(HorizontalHitResult, VerticalHitResult.ImpactPoint - CharacterOwner->GetActorForwardVector() * 100, VerticalHitResult.ImpactPoint, ObjectQueryParams, CollisionParams))
					{
						FVector LedgeLocation = HorizontalHitResult.ImpactPoint;
						FVector ClimbDirection = LedgeHitResult.GetActor()->GetActorForwardVector() * -1;
						FRotator LedgeRotation = UKismetMathLibrary::MakeRotFromXZ(LedgeHitResult.GetActor()->GetActorForwardVector(), HorizontalHitResult.ImpactNormal);
						FVector FeetLocation = LedgeLocation + CharacterOwner->GetSimpleCollisionRadius() * ClimbDirection;
						bool bJumpingFromBelow = FeetLocation.Z - CharacterOwner->GetActorLocation().Z > MantleFromBelowHeightThreshold;

						if (!CharacterOwner->HasAuthority())
						{
							ServerStartLedgeClimbing(LedgeHitResult, FeetLocation, LedgeLocation + ClimbDirection * MantleHandAdjustment.X + FVector::UpVector * MantleHandAdjustment.Z, LedgeRotation, bJumpingFromBelow);
						}
						StartLedgeClimbing(FeetLocation, LedgeLocation + ClimbDirection * MantleHandAdjustment.X + FVector::UpVector * MantleHandAdjustment.Z, LedgeRotation, bJumpingFromBelow);
					}
				}
			}
		}
	}
	else if (IsLedgeClimbing() && CharacterOwner->IsLocallyControlled())
	{
		FRotator CameraRot = CharacterOwner->GetControlRotation();
		FRotator TargetCameraRot = CameraRot;
		TargetCameraRot.Yaw = CharacterOwner->GetActorRotation().Yaw;
		CharacterOwner->GetController()->SetControlRotation(FMath::RInterpTo(CameraRot, TargetCameraRot, DeltaTime, CameraInterpSpeed));
	}
}

