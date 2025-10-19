// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DashComponent.generated.h"


class ACharacterTestCharacter;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CHARACTERTEST_API UDashComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// Sets default values for this component's properties
	UDashComponent();

protected:

	// Called when the game starts
	virtual void BeginPlay() override;
	bool CanDash() const;

	UFUNCTION()
	void OnRep_Dashing(bool bWasDashing);
	void OnStartDashing();
	void OnStopDashing();
	
	UPROPERTY(EditAnywhere, Category="Movement|Dash")
	float DashSpeed = 1500;
	
	UPROPERTY(EditAnywhere, Category="Movement|Dash")
	float DashAcceleration = 6000;
	
	UPROPERTY(EditAnywhere, Category="Movement|Dash")
	float DashCooldown = 3;
	
	UPROPERTY(EditAnywhere, Category="Movement|Dash")
	float DashDuration = 1;
	
	UPROPERTY(EditAnywhere, Category="Movement|Dash")
	float DashFOV = 130;
	
	UPROPERTY(EditAnywhere, Category="Movement|Dash")
	bool bDashTowardsControllerRotation = true;
	
	UPROPERTY(EditAnywhere, Category="Movement|Dash")
	bool bLerpCameraRotationToPlayer = false;
	
	UPROPERTY(EditAnywhere, Category="Movement|Dash", meta=(EditCondition="bLerpCameraRotationToPlayer"))
	float CameraInterpSpeed = 10;

public:


	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	void StartDashing();

	UFUNCTION(Reliable, Server)
	void Server_DoDash();

	UFUNCTION(Reliable, Server)
	void Server_StopDash();

	UFUNCTION(Reliable, Client)
	void Client_CancelDash();


	void DoDash();
	void StopDashing();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;


	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashStarted);
	UPROPERTY(BlueprintAssignable, Category="Movement|Dash")
	FOnDashStarted OnDashStarted;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashEnded);
	UPROPERTY(BlueprintAssignable, Category="Movement|Dash")
	FOnDashStarted OnDashEnded;
	
private:

	float LastTimeDashed = 0;

	UPROPERTY(ReplicatedUsing=OnRep_Dashing)
	bool bDashing = false;
	float CachedFOV;
	float CachedSpeed;
	float CachedAcceleration;

	ACharacterTestCharacter* CharacterOwner;
};
