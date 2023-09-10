// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayPlayerAITask_MoveTo.h"
#include "UObject/Package.h"
#include "TimerManager.h"
#include "AISystem.h"
#include "AIController.h"
#include "VisualLogger/VisualLogger.h"
#include "AIResources.h"
#include "GameplayTasksComponent.h"
#include "NavigationSystem.h"
#include "Tasks/AITask.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayPlayerAITask_MoveTo)

UGameplayPlayerAITask_MoveTo::UGameplayPlayerAITask_MoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPausable = true;
	MoveRequestID = FAIRequestID::InvalidRequest;

	MoveRequest.SetAcceptanceRadius(GET_AI_CONFIG_VAR(AcceptanceRadius));
	MoveRequest.SetReachTestIncludesAgentRadius(GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap));
	MoveRequest.SetAllowPartialPath(GET_AI_CONFIG_VAR(bAcceptPartialPaths));
	MoveRequest.SetUsePathfinding(true);

	AddRequiredResource(UAIResource_Movement::StaticClass());
	AddClaimedResource(UAIResource_Movement::StaticClass());
	
	MoveResult = EPathFollowingResult::Invalid;
	bUseContinuousTracking = false;
}

UGameplayPlayerAITask_MoveTo* UGameplayPlayerAITask_MoveTo::GameplayPlayerAIMoveTo(UGameplayAbility* OwningAbility,
	FName TaskInstanceName, APlayerController* Controller, FVector InGoalLocation, AActor* InGoalActor,
	float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, EAIOptionFlag::Type AcceptPartialPath,
	bool bUsePathfinding, bool bUseContinuousGoalTracking, EAIOptionFlag::Type ProjectGoalOnNavigation,
	TSubclassOf<UNavigationQueryFilter> FilterClass)
{
	UGameplayPlayerAITask_MoveTo* MyTask = NewAbilityTask<UGameplayPlayerAITask_MoveTo>(OwningAbility, TaskInstanceName);
	if (MyTask)
	{
		FAIMoveRequest MoveReq;
		if (InGoalActor)
		{
			MoveReq.SetGoalActor(InGoalActor);
		}
		else
		{
			MoveReq.SetGoalLocation(InGoalLocation);
		}

		MoveReq.SetAcceptanceRadius(AcceptanceRadius);
		MoveReq.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(StopOnOverlap, MoveReq.IsReachTestIncludingAgentRadius()));
		MoveReq.SetAllowPartialPath(FAISystem::PickAIOption(AcceptPartialPath, MoveReq.IsUsingPartialPaths()));
		MoveReq.SetUsePathfinding(bUsePathfinding);
		MoveReq.SetProjectGoalLocation(FAISystem::PickAIOption(ProjectGoalOnNavigation, MoveReq.IsProjectingGoal()));
		if (Controller)
		{
			MoveReq.SetNavigationFilter(FilterClass);
		}

		MyTask->SetUp(Controller, MoveReq);
		MyTask->SetContinuousGoalTracking(bUseContinuousGoalTracking);
	}

	return MyTask;
}

void UGameplayPlayerAITask_MoveTo::InitMoveTask(APlayerController& PlayerControllerOwner, IGameplayTaskOwnerInterface& InTaskOwner, uint8 InPriority)
{
	OwnerController = &PlayerControllerOwner;
	InitTask(InTaskOwner, InPriority);
}

void UGameplayPlayerAITask_MoveTo::SetUp(APlayerController* Controller, const FAIMoveRequest& InMoveRequest)
{
	OwnerController = Controller;
	MoveRequest = InMoveRequest;
}

UPathFollowingComponent* UGameplayPlayerAITask_MoveTo::InitNavigationControl(AController& Controller)
{
	const AAIController* AsAIController = Cast<AAIController>(&Controller);
	UPathFollowingComponent* PathFollowingComp;

	if (AsAIController)
	{
		PathFollowingComp = AsAIController->GetPathFollowingComponent();
	}
	else
	{
		PathFollowingComp = Controller.FindComponentByClass<UPathFollowingComponent>();
		if (PathFollowingComp == nullptr)
		{
			PathFollowingComp = NewObject<UPathFollowingComponent>(&Controller);
			PathFollowingComp->RegisterComponentWithWorld(Controller.GetWorld());
			PathFollowingComp->Initialize();
		}
	}

	return PathFollowingComp;
}

void UGameplayPlayerAITask_MoveTo::SetContinuousGoalTracking(bool bEnable)
{
	bUseContinuousTracking = bEnable;
}

void UGameplayPlayerAITask_MoveTo::FinishMoveTask(EPathFollowingResult::Type InResult)
{
	if (MoveRequestID.IsValid())
	{
		if (PathFollowingComp && PathFollowingComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			ResetObservers();
			PathFollowingComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	MoveResult = InResult;
	EndTask();

	if (InResult == EPathFollowingResult::Invalid)
	{
		OnRequestFailed.Broadcast();
	}
	else
	{
		OnMoveFinished.Broadcast(InResult, OwnerController);
	}
}

void UGameplayPlayerAITask_MoveTo::Activate()
{
	Super::Activate();

	PathFollowingComp = InitNavigationControl(*OwnerController);

	UE_CVLOG(bUseContinuousTracking, GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Continuous goal tracking requested, moving to: %s"),
		MoveRequest.IsMoveToActorRequest() ? TEXT("actor => looping successful moves!") : TEXT("location => will NOT loop"));

	MoveRequestID = FAIRequestID::InvalidRequest;
	ConditionalPerformMove();
}

void UGameplayPlayerAITask_MoveTo::ConditionalPerformMove()
{
	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(MoveRetryTimerHandle, this, &UGameplayPlayerAITask_MoveTo::ConditionalPerformMove, 0.2f, false);
	}
	else
	{
		MoveRetryTimerHandle.Invalidate();
		PerformMove();
	}
}

void UGameplayPlayerAITask_MoveTo::PerformMove()
{
	if (PathFollowingComp == nullptr)
	{
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	ResetObservers();
	ResetTimers();

	// start new move request
	FNavPathSharedPtr FollowedPath;
	const FPathFollowingRequestResult ResultData = MoveTo(MoveRequest, &FollowedPath);

	switch (ResultData.Code)
	{
	case EPathFollowingRequestResult::Failed:
		FinishMoveTask(EPathFollowingResult::Invalid);
		break;

	case EPathFollowingRequestResult::AlreadyAtGoal:
		MoveRequestID = ResultData.MoveId;
		OnRequestFinished(ResultData.MoveId, FPathFollowingResult(EPathFollowingResult::Success, FPathFollowingResultFlags::AlreadyAtGoal));
		break;

	case EPathFollowingRequestResult::RequestSuccessful:
		MoveRequestID = ResultData.MoveId;
		PathFinishDelegateHandle = PathFollowingComp->OnRequestFinished.AddUObject(this, &UGameplayPlayerAITask_MoveTo::OnRequestFinished);
		SetObservedPath(FollowedPath);

		if (IsFinished())
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s> re-Activating Finished task!"), *GetName());
		}
		break;

	default:
		checkNoEntry();
		break;
	}
}

FPathFollowingRequestResult UGameplayPlayerAITask_MoveTo::MoveTo(const FAIMoveRequest& InMoveRequest, FNavPathSharedPtr* OutPath)
{
	FPathFollowingRequestResult ResultData;
	ResultData.Code = EPathFollowingRequestResult::Failed;

	if (InMoveRequest.IsValid() == false)
	{
		UE_VLOG(this, LogGameplayTasks, Error, TEXT("MoveTo request failed due MoveRequest not being valid. Most probably desired Goal Actor not longer exists. MoveRequest: '%s'"), *MoveRequest.ToString());
		return ResultData;
	}

	if (PathFollowingComp == nullptr)
	{
		UE_VLOG(this, LogGameplayTasks, Error, TEXT("MoveTo request failed due missing PathFollowingComponent"));
		return ResultData;
	}

	// Actually, it works fine without one
	// ensure(InMoveRequest.GetNavigationFilter());

	bool bCanRequestMove = true;
	bool bAlreadyAtGoal;
	
	if (!InMoveRequest.IsMoveToActorRequest())
	{
		if (InMoveRequest.GetGoalLocation().ContainsNaN() || FAISystem::IsValidLocation(InMoveRequest.GetGoalLocation()) == false)
		{
			UE_VLOG(this, LogGameplayTasks, Error, TEXT("UGameplayPlayerAITask_MoveTo::MoveTo: Destination is not valid! Goal(%s)"), TEXT_AI_LOCATION(InMoveRequest.GetGoalLocation()));
			bCanRequestMove = false;
		}

		// fail if projection to navigation is required but it failed
		if (bCanRequestMove && InMoveRequest.IsProjectingGoal())
		{
			UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
			const FNavAgentProperties& AgentProps = OwnerController->GetNavAgentPropertiesRef();
			FNavLocation ProjectedLocation;

			if (NavSys && !NavSys->ProjectPointToNavigation(InMoveRequest.GetGoalLocation(), ProjectedLocation, INVALID_NAVEXTENT, &AgentProps))
			{
				if (InMoveRequest.IsUsingPathfinding())
				{
					UE_VLOG_LOCATION(this, LogGameplayTasks, Error, InMoveRequest.GetGoalLocation(), 30.f, FColor::Red, TEXT("UGameplayPlayerAITask_MoveTo::MoveTo failed to project destination location to navmesh"));
				}
				else
				{
					UE_VLOG_LOCATION(this, LogGameplayTasks, Error, InMoveRequest.GetGoalLocation(), 30.f, FColor::Red, TEXT("UGameplayPlayerAITask_MoveTo::MoveTo failed to project destination location to navmesh, path finding is disabled perhaps disable goal projection ?"));
				}

				bCanRequestMove = false;
			}

			InMoveRequest.UpdateGoalLocation(ProjectedLocation.Location);
		}

		bAlreadyAtGoal = bCanRequestMove && PathFollowingComp->HasReached(InMoveRequest);
	}
	else 
	{
		bAlreadyAtGoal = bCanRequestMove && PathFollowingComp->HasReached(InMoveRequest);
	}

	if (bAlreadyAtGoal)
	{
		UE_VLOG(this, LogGameplayTasks, Log, TEXT("MoveTo: already at goal!"));
		ResultData.MoveId = PathFollowingComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
		ResultData.Code = EPathFollowingRequestResult::AlreadyAtGoal;
	}
	else if (bCanRequestMove)
	{
		FPathFindingQuery PFQuery;

		const bool bValidQuery = BuildPathfindingQuery(InMoveRequest, PFQuery);
		if (bValidQuery)
		{
			FNavPathSharedPtr NewPath;
			FindPathForMoveRequest(InMoveRequest, PFQuery, NewPath);

			const FAIRequestID RequestID = PathFollowingComp && NewPath.IsValid() ? PathFollowingComp->RequestMove(InMoveRequest, NewPath) : FAIRequestID::InvalidRequest;
			if (RequestID.IsValid())
			{
				// bAllowStrafe = InMoveRequest.CanStrafe();
				ResultData.MoveId = RequestID;
				ResultData.Code = EPathFollowingRequestResult::RequestSuccessful;

				if (OutPath)
				{
					*OutPath = NewPath;
				}
			}
		}
	}

	if (ResultData.Code == EPathFollowingRequestResult::Failed)
	{
		ResultData.MoveId = PathFollowingComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
	}

	return ResultData;
}

bool UGameplayPlayerAITask_MoveTo::BuildPathfindingQuery(const FAIMoveRequest& InMoveRequest, FPathFindingQuery& Query) const
{
	bool bResult = false;

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* NavData = (NavSys == nullptr) ? nullptr :
		InMoveRequest.IsUsingPathfinding() ? NavSys->GetNavDataForProps(OwnerController->GetNavAgentPropertiesRef(), OwnerController->GetNavAgentLocation()) :
		NavSys->GetAbstractNavData();

	if (NavData)
	{
		FVector GoalLocation = InMoveRequest.GetGoalLocation();
		if (InMoveRequest.IsMoveToActorRequest())
		{
			const INavAgentInterface* NavGoal = Cast<const INavAgentInterface>(InMoveRequest.GetGoalActor());
			if (NavGoal)
			{
				const FVector Offset = NavGoal->GetMoveGoalOffset(OwnerController->GetPawn());
				GoalLocation = FQuatRotationTranslationMatrix(InMoveRequest.GetGoalActor()->GetActorQuat(), NavGoal->GetNavAgentLocation()).TransformPosition(Offset);
			}
			else
			{
				GoalLocation = InMoveRequest.GetGoalActor()->GetActorLocation();
			}
		}

		const FSharedConstNavQueryFilter NavFilter = UNavigationQueryFilter::GetQueryFilter(*NavData, this, InMoveRequest.GetNavigationFilter());
		Query = FPathFindingQuery(OwnerController, *NavData, OwnerController->GetNavAgentLocation(), GoalLocation, NavFilter);
		Query.SetAllowPartialPaths(InMoveRequest.IsUsingPartialPaths());

		if (PathFollowingComp)
		{
			PathFollowingComp->OnPathfindingQuery(Query);
		}

		bResult = true;
	}
	else
	{
		if (NavSys == nullptr)
		{
			UE_VLOG(this, LogGameplayTasks, Warning, TEXT("Unable UGameplayPlayerAITask_MoveTo::BuildPathfindingQuery due to no NavigationSystem present. Note that even pathfinding-less movement requires presence of NavigationSystem."));
		}
		else 
		{
			UE_VLOG(this, LogGameplayTasks, Warning, TEXT("Unable to find NavigationData instance while calling UGameplayPlayerAITask_MoveTo::BuildPathfindingQuery"));
		}
	}

	return bResult;
}

void UGameplayPlayerAITask_MoveTo::FindPathForMoveRequest(const FAIMoveRequest& InMoveRequest, FPathFindingQuery& Query,
	FNavPathSharedPtr& OutPath) const
{
	SCOPE_CYCLE_COUNTER(STAT_AI_Overall);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys)
	{
		const FPathFindingResult PathResult = NavSys->FindPathSync(Query);
		if (PathResult.Result != ENavigationQueryResult::Error)
		{
			if (PathResult.IsSuccessful() && PathResult.Path.IsValid())
			{
				if (InMoveRequest.IsMoveToActorRequest())
				{
					PathResult.Path->SetGoalActorObservation(*InMoveRequest.GetGoalActor(), 100.0f);
				}

				PathResult.Path->EnableRecalculationOnInvalidation(true);
				OutPath = PathResult.Path;
			}
		}
		else
		{
			UE_VLOG(this, LogGameplayTasks, Error, TEXT("Trying to find path to %s resulted in Error")
				, InMoveRequest.IsMoveToActorRequest() ? *GetNameSafe(InMoveRequest.GetGoalActor()) : *InMoveRequest.GetGoalLocation().ToString());
			UE_VLOG_SEGMENT(this, LogGameplayTasks, Error, OwnerController->GetPawn() ? OwnerController->GetPawn()->GetActorLocation() : FAISystem::InvalidLocation
				, InMoveRequest.GetGoalLocation(), FColor::Red, TEXT("Failed move to %s"), *GetNameSafe(InMoveRequest.GetGoalActor()));
		}
	}
}

void UGameplayPlayerAITask_MoveTo::Pause()
{
	if (PathFollowingComp && MoveRequestID.IsValid())
	{
		PathFollowingComp->PauseMove(MoveRequestID);
	}

	ResetTimers();
	Super::Pause();
}

void UGameplayPlayerAITask_MoveTo::Resume()
{
	Super::Resume();

	if (!MoveRequestID.IsValid() || !ResumeMove(MoveRequestID))
	{
		UE_CVLOG(MoveRequestID.IsValid(), GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Resume move failed, starting new one."), *GetName());
		ConditionalPerformMove();
	}
}

bool UGameplayPlayerAITask_MoveTo::ResumeMove(FAIRequestID RequestToResume) const
{
	if (PathFollowingComp != nullptr && RequestToResume.IsEquivalent(PathFollowingComp->GetCurrentRequestId()))
	{
		PathFollowingComp->ResumeMove(RequestToResume);
		return true;
	}
	return false;
}

void UGameplayPlayerAITask_MoveTo::SetObservedPath(const FNavPathSharedPtr& InPath)
{
	if (PathUpdateDelegateHandle.IsValid() && Path.IsValid())
	{
		Path->RemoveObserver(PathUpdateDelegateHandle);
	}

	PathUpdateDelegateHandle.Reset();
	
	Path = InPath;
	if (Path.IsValid())
	{
		// disable auto repaths, it will be handled by move task to include ShouldPostponePathUpdates condition
		Path->EnableRecalculationOnInvalidation(false);
		PathUpdateDelegateHandle = Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UGameplayPlayerAITask_MoveTo::OnPathEvent));
	}
}

void UGameplayPlayerAITask_MoveTo::ResetObservers()
{
	if (Path.IsValid())
	{
		Path->DisableGoalActorObservation();
	}

	if (PathFinishDelegateHandle.IsValid())
	{
		if (PathFollowingComp)
		{
			PathFollowingComp->OnRequestFinished.Remove(PathFinishDelegateHandle);
		}

		PathFinishDelegateHandle.Reset();
	}

	if (PathUpdateDelegateHandle.IsValid())
	{
		if (Path.IsValid())
		{
			Path->RemoveObserver(PathUpdateDelegateHandle);
		}

		PathUpdateDelegateHandle.Reset();
	}
}

void UGameplayPlayerAITask_MoveTo::ResetTimers()
{
	if (OwnerController)
	{
		// Remove all timers including the ones that might have been set with SetTimerForNextTick 
		OwnerController->GetWorldTimerManager().ClearAllTimersForObject(this);
	}
	MoveRetryTimerHandle.Invalidate();
	PathRetryTimerHandle.Invalidate();
}

void UGameplayPlayerAITask_MoveTo::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);
	
	ResetObservers();
	ResetTimers();

	if (MoveRequestID.IsValid())
	{
		if (PathFollowingComp && PathFollowingComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			PathFollowingComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	// clear the shared pointer now to make sure other systems
	// don't think this path is still being used
	Path = nullptr;
}

void UGameplayPlayerAITask_MoveTo::OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (RequestID == MoveRequestID)
	{
		if (Result.HasFlag(FPathFollowingResultFlags::UserAbort) && Result.HasFlag(FPathFollowingResultFlags::NewRequest) && !Result.HasFlag(FPathFollowingResultFlags::ForcedScript))
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> ignoring OnRequestFinished, move was aborted by new request"), *GetName());
		}
		else
		{
			// reset request Id, FinishMoveTask doesn't need to update path following's state
			MoveRequestID = FAIRequestID::InvalidRequest;

			if (bUseContinuousTracking && MoveRequest.IsMoveToActorRequest() && Result.IsSuccess())
			{
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> received OnRequestFinished and goal tracking is active! Moving again in next tick"), *GetName());
				GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UGameplayPlayerAITask_MoveTo::PerformMove);
			}
			else
			{
				FinishMoveTask(Result.Code);
			}
		}
	}
	else if (IsActive())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Warning, TEXT("%s> received OnRequestFinished with not matching RequestID!"), *GetName());
	}
}

void UGameplayPlayerAITask_MoveTo::OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event)
{
	const static UEnum* NavPathEventEnum = StaticEnum<ENavPathEvent::Type>();
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Path event: %s"), *GetName(), *NavPathEventEnum->GetNameStringByValue(Event));

	switch (Event)
	{
	case ENavPathEvent::NewPath:
	case ENavPathEvent::UpdatedDueToGoalMoved:
	case ENavPathEvent::UpdatedDueToNavigationChanged:
		if (InPath && InPath->IsPartial() && !MoveRequest.IsUsingPartialPaths())
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> partial path is not allowed, aborting"));
			UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
			FinishMoveTask(EPathFollowingResult::Aborted);
		}
#if ENABLE_VISUAL_LOG
		else if (!IsActive())
		{
			UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
		}
#endif // ENABLE_VISUAL_LOG
		break;

	case ENavPathEvent::Invalidated:
		ConditionalUpdatePath();
		break;

	case ENavPathEvent::Cleared:
	case ENavPathEvent::RePathFailed:
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> no path, aborting!"));
		FinishMoveTask(EPathFollowingResult::Aborted);
		break;

	case ENavPathEvent::MetaPathUpdate:
	default:
		break;
	}
}

void UGameplayPlayerAITask_MoveTo::ConditionalUpdatePath()
{
	// mark this path as waiting for repath so that PathFollowingComponent doesn't abort the move while we 
	// micro manage repathing moment
	// note that this flag fill get cleared upon repathing end
	if (Path.IsValid())
	{
		Path->SetManualRepathWaiting(true);
	}

	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(PathRetryTimerHandle, this, &UGameplayPlayerAITask_MoveTo::ConditionalUpdatePath, 0.2f, false);
	}
	else
	{
		PathRetryTimerHandle.Invalidate();
		
		ANavigationData* NavData = Path.IsValid() ? Path->GetNavigationDataUsed() : nullptr;
		if (NavData)
		{
			NavData->RequestRePath(Path, ENavPathUpdateType::NavigationChanged);
		}
		else
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> unable to repath, aborting!"), *GetName());
			FinishMoveTask(EPathFollowingResult::Aborted);
		}
	}
}

