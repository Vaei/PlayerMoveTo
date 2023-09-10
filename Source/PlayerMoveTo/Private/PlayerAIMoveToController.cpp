// Copyright (c) Jared Taylor. All Rights Reserved


#include "PlayerAIMoveToController.h"

#include "GameplayTasksComponent.h"

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

void APlayerAIMoveToController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);

	// CacheGameplayTasksComponent(InPawn);
}
