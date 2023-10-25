// Copyright (c) Jared Taylor. All Rights Reserved


#include "PlayerAIMoveToController.h"

#include "GameplayTasksComponent.h"

UGameplayTasksComponent* APlayerAIMoveToController::GetGameplayTasksComponent(const UGameplayTask& Task) const
{
	return GetGameplayTasksComponent();
}

AActor* APlayerAIMoveToController::GetGameplayTaskOwner(const UGameplayTask* Task) const
{
	return const_cast<APlayerAIMoveToController*>(this);
}

AActor* APlayerAIMoveToController::GetGameplayTaskAvatar(const UGameplayTask* Task) const
{
	return GetPawn();
}

uint8 APlayerAIMoveToController::GetGameplayTaskDefaultPriority() const
{
	return FGameplayTasks::DefaultPriority - 1;
}

void APlayerAIMoveToController::CacheGameplayTasksComponent(APawn* InPawn)
{
	// a Pawn controlled by AI _requires_ a GameplayTasksComponent, so if Pawn 
	// doesn't have one we need to create it
	if (CachedGameplayTasksComponent == nullptr)
	{
		UGameplayTasksComponent* GTComp = InPawn->FindComponentByClass<UGameplayTasksComponent>();
		if (GTComp == nullptr)
		{
			GTComp = NewObject<UGameplayTasksComponent>(InPawn, TEXT("GameplayTasksComponent"));
			GTComp->RegisterComponent();
		}
		CachedGameplayTasksComponent = GTComp;
	}
}

void APlayerAIMoveToController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	CacheGameplayTasksComponent(InPawn);
}
