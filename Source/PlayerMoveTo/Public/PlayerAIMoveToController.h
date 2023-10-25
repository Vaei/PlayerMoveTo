// Copyright (c) Jared Taylor. All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameplayTaskOwnerInterface.h"
#include "GameFramework/PlayerController.h"
#include "PlayerAIMoveToController.generated.h"

/**
 * This PlayerController is required when using UPlayerAITask_MoveTo, but not UGameplayPlayerAITask_MoveTo
 * If you're using only with gameplay abilities, it is not needed
 */
UCLASS(config=Game, BlueprintType, Blueprintable)
class PLAYERMOVETO_API APlayerAIMoveToController : public APlayerController, public IGameplayTaskOwnerInterface
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UGameplayTasksComponent> CachedGameplayTasksComponent;
	
	//----------------------------------------------------------------------//
	// IGameplayTaskOwnerInterface
	//----------------------------------------------------------------------//
	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const override;
	virtual AActor* GetGameplayTaskOwner(const UGameplayTask* Task) const override;
	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override;
	virtual uint8 GetGameplayTaskDefaultPriority() const override;

	FORCEINLINE UGameplayTasksComponent* GetGameplayTasksComponent() const { return CachedGameplayTasksComponent; }

public:
	void CacheGameplayTasksComponent(APawn* InPawn);
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetPawn(APawn* InPawn) override;
};
