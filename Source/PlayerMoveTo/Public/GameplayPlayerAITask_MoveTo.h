// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "AITypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameplayPlayerAITask_MoveTo.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGameplayPlayerMoveTaskCompletedSignature, TEnumAsByte<EPathFollowingResult::Type>, Result, APlayerController*, PlayerController);

/**
 * Same as AIMoveTo but works with PlayerController
 * When used in a GameplayAbility use UGameplayGameplayPlayerAITask_MoveTo instead
 * This one requires the use of APlayerAIMoveToController or a controller with equivalent implementations
 */
UCLASS()
class PLAYERMOVETO_API UGameplayPlayerAITask_MoveTo : public UAbilityTask
{
	GENERATED_BODY()

public:
	UGameplayPlayerAITask_MoveTo(const FObjectInitializer& ObjectInitializer);

	/** tries to start move request and handles retry timer */
	void ConditionalPerformMove();

	/** prepare move task for activation */
	void SetUp(APlayerController* Controller, const FAIMoveRequest& InMoveRequest);

	/** taken from UAIBlueprintHelperLibrary to produce a PathFollowingComponent */
	static UPathFollowingComponent* InitNavigationControl(AController& Controller);

	EPathFollowingResult::Type GetMoveResult() const { return MoveResult; }
	bool WasMoveSuccessful() const { return MoveResult == EPathFollowingResult::Success; }
	bool WasMovePartial() const { return Path.IsValid() && Path->IsPartial(); }

	/** Move to Location using PlayerController instead of AIController. */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding,bUseContinuousGoalTracking,ProjectGoalOnNavigation,FilterClass", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE", DisplayName = "Gameplay Player Move To Location or Actor"))
	static UGameplayPlayerAITask_MoveTo* GameplayPlayerAIMoveTo(UGameplayAbility* OwningAbility, FName TaskInstanceName, APlayerController* Controller, FVector GoalLocation, AActor* GoalActor = nullptr,
		float AcceptanceRadius = -1.f, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default,
		bool bUsePathfinding = true, bool bUseContinuousGoalTracking = false, EAIOptionFlag::Type ProjectGoalOnNavigation = EAIOptionFlag::Default, TSubclassOf<UNavigationQueryFilter> FilterClass = nullptr);

	void InitMoveTask(APlayerController& PlayerControllerOwner, IGameplayTaskOwnerInterface& TaskOwner, uint8 InPriority);
	
	/** Allows custom move request tweaking. Note that all MoveRequest need to 
	 *	be performed before PerformMove is called. */
	FAIMoveRequest& GetMoveRequestRef() { return MoveRequest; }

	/** Switch task into continuous tracking mode: keep restarting move toward goal actor. Only pathfinding failure or external cancel will be able to stop this task. */
	void SetContinuousGoalTracking(bool bEnable);

protected:
	UPROPERTY(BlueprintReadOnly, Category="AI|Tasks")
	TObjectPtr<APlayerController> OwnerController;
	
	UPROPERTY()
	TObjectPtr<UPathFollowingComponent> PathFollowingComp;
	
	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
	FGameplayPlayerMoveTaskCompletedSignature OnMoveFinished;

	/** parameters of move request */
	UPROPERTY()
	FAIMoveRequest MoveRequest;

	/** handle of path following's OnMoveFinished delegate */
	FDelegateHandle PathFinishDelegateHandle;

	/** handle of path's update event delegate */
	FDelegateHandle PathUpdateDelegateHandle;

	/** handle of active ConditionalPerformMove timer  */
	FTimerHandle MoveRetryTimerHandle;

	/** handle of active ConditionalUpdatePath timer */
	FTimerHandle PathRetryTimerHandle;

	/** request ID of path following's request */
	FAIRequestID MoveRequestID;

	/** currently followed path */
	FNavPathSharedPtr Path;

	TEnumAsByte<EPathFollowingResult::Type> MoveResult;
	uint8 bUseContinuousTracking : 1;

	virtual void Activate() override;
	virtual void OnDestroy(bool bOwnerFinished) override;

	virtual void Pause() override;
	virtual void Resume() override;

	bool ResumeMove(FAIRequestID RequestToResume) const;

	/** finish task */
	void FinishMoveTask(EPathFollowingResult::Type InResult);

	/** stores path and starts observing its events */
	void SetObservedPath(const FNavPathSharedPtr& InPath);

	/** remove all delegates */
	virtual void ResetObservers();

	/** remove all timers */
	virtual void ResetTimers();

	/** tries to update invalidated path and handles retry timer */
	void ConditionalUpdatePath();

	/** start move request */
	virtual void PerformMove();

	/** Makes AI go toward specified destination. Taken from AAIController
	 *  @param MoveRequest - details about move
	 *  @param OutPath - optional output param, filled in with assigned path
	 *  @return struct holding MoveId and enum code
	 */
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath = nullptr);

	/** Taken from AAIController */
	bool BuildPathfindingQuery(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query) const;
	
	/** Finds path for given move request. Taken from AAIController
	  *  @param MoveRequest - details about move
	 *  @param Query - pathfinding query for navigation system
	 *  @param OutPath - generated path
	 */
	virtual void FindPathForMoveRequest(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query, FNavPathSharedPtr& OutPath) const;

	/** event from followed path */
	virtual void OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event);

	/** event from path following */
	virtual void OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
};
