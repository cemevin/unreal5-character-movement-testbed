// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WallRunComponent.generated.h"


class ACharacterTestCharacter;

UENUM(BlueprintType)
enum class EWallRunState : uint8
{
	WallRunNone,
	WallRunRight,
	WallRunLeft
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class CHARACTERTEST_API UWallRunComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// Sets default values for this component's properties
	UWallRunComponent();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	// Called when the game starts
	virtual void BeginPlay() override;

	bool CanWallRun();
	bool CanWallRunFrom(const FHitResult& HitResult) const;

	void OnStartWallRunning(const FHitResult& HitResult);

	bool IsWallRunning() const { return WallRunState != EWallRunState::WallRunNone; }

	UFUNCTION(Reliable, Server)
	void Server_StartWallRunning(const FHitResult& HitResult);

	UFUNCTION(Reliable, Server)
	void Server_StopWallRunning(bool bLaunch);

	UFUNCTION(Reliable, Client)
	void Client_CancelWallRun();

	UFUNCTION()
	void OnRep_WallRunState(EWallRunState PrevState);

	void StartWallRunning(const FHitResult& HitResult);

	void OnStopWallRunning(bool bLaunch);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnWallRun();

	void StopWallRun(bool bLaunch);

	UFUNCTION()
	void OnJumpAttempted();

	UPROPERTY(EditAnywhere)
	FName WallRunTag = TEXT("WallRun");
	
	UPROPERTY(BlueprintReadOnly)
	ACharacterTestCharacter* CharacterOwner;

	UPROPERTY(EditAnywhere)
	bool bOverrideWalkSpeed = false;

	UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideWalkSpeed"))
	float WallRunSpeed = 300;

	UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideWalkSpeed"))
	float WallRunAcceleration = 1200;

	UPROPERTY(EditAnywhere)
	float WallRunOffset = 35;

	UPROPERTY(EditAnywhere)
	float WallRunHeightOffset = 35;

	UPROPERTY(EditAnywhere)
	float WallRunTimeout = 0.5f;

	UPROPERTY(EditAnywhere)
	float CameraInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere)
	float LaunchAngle = 45.f;

	UPROPERTY(EditAnywhere)
	bool bMatchZHeightWithWallRunSection = true;

	UPROPERTY(EditAnywhere)
	bool bOverrideLaunchVelocity = true;

	UPROPERTY(EditAnywhere, meta=(EditCondition="bOverrideLaunchVelocity"))
	FVector LaunchVelocity{600,0,300};

public:

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(ReplicatedUsing=OnRep_WallRunState)
	EWallRunState WallRunState = EWallRunState::WallRunNone;
	
	FHitResult WallRunHitResult;
	float LastTimeWallRun;
	float LastTimeWallRunStarted;
	int FrameCounter;
	float CachedSpeed;
	float CachedAcceleration;
	
};
