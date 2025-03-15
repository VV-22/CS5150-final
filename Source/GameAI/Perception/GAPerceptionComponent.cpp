#include "GAPerceptionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GAPerceptionSystem.h"

UGAPerceptionComponent::UGAPerceptionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;

	// Default vision parameters
	VisionParameters.VisionAngle = 90.0f;
	VisionParameters.VisionDistance = 1000.0;
}


void UGAPerceptionComponent::OnRegister()
{
	Super::OnRegister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->RegisterPerceptionComponent(this);
	}
}

void UGAPerceptionComponent::OnUnregister()
{
	Super::OnUnregister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->UnregisterPerceptionComponent(this);
	}
}


APawn* UGAPerceptionComponent::GetOwnerPawn() const
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		APawn* Pawn = Cast<APawn>(Owner);
		if (Pawn)
		{
			return Pawn;
		}
		else
		{
			AController* Controller = Cast<AController>(Owner);
			if (Controller)
			{
				return Controller->GetPawn();
			}
		}
	}

	return NULL;
}



// Returns the Target this AI is attending to right now.

UGATargetComponent* UGAPerceptionComponent::GetCurrentTarget() const
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);

	if (PerceptionSystem && PerceptionSystem->TargetComponents.Num() > 0)
	{
		UGATargetComponent* TargetComponent = PerceptionSystem->TargetComponents[0];
		if (TargetComponent->IsKnown())
		{
			return PerceptionSystem->TargetComponents[0];
		}
	}

	return NULL;
}

bool UGAPerceptionComponent::HasTarget() const
{
	return GetCurrentTarget() != NULL;
}


bool UGAPerceptionComponent::GetCurrentTargetState(FTargetCache& TargetStateOut, FTargetData& TargetDataOut) const
{
	UGATargetComponent* Target = GetCurrentTarget();
	if (Target)
	{
		const FTargetData* TargetData = TargetMap.Find(Target->TargetGuid);
		if (TargetData)
		{
			TargetStateOut = Target->LastKnownState;
			TargetDataOut = *TargetData;
			return true;
		}

	}
	return false;
}


void UGAPerceptionComponent::GetAllTargetStates(bool OnlyKnown, TArray<FTargetCache>& TargetCachesOut, TArray<FTargetData>& TargetDatasOut) const
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGATargetComponent>>& TargetComponents = PerceptionSystem->GetAllTargetComponents();
		for (UGATargetComponent* TargetComponent : TargetComponents)
		{
			const FTargetData* TargetData = TargetMap.Find(TargetComponent->TargetGuid);
			if (TargetData)
			{
				if (!OnlyKnown || TargetComponent->IsKnown())
				{
					TargetCachesOut.Add(TargetComponent->LastKnownState);
					TargetDatasOut.Add(*TargetData);
				}
			}
		}
	}
}


void UGAPerceptionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateAllTargetData();
}


void UGAPerceptionComponent::UpdateAllTargetData()
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGATargetComponent>>& TargetComponents = PerceptionSystem->GetAllTargetComponents();
		for (UGATargetComponent* TargetComponent : TargetComponents)
		{
			UpdateTargetData(TargetComponent);
		}
	}
}

void UGAPerceptionComponent::UpdateTargetData(UGATargetComponent* TargetComponent)
{
	// REMEMBER: the UGAPerceptionComponent is going to be attached to the controller, not the pawn. So we call this special accessor to 
	// get the pawn that our controller is controlling
	APawn* OwnerPawn = GetOwnerPawn();
	if (OwnerPawn == NULL)
	{
		return;
	}

	FTargetData *TargetData = TargetMap.Find(TargetComponent->TargetGuid);
	if (TargetData == NULL)		// If we don't already have a target data for the given target component, add it
	{
		FTargetData NewTargetData;
		FGuid TargetGuid = TargetComponent->TargetGuid;
		TargetData = &TargetMap.Add(TargetGuid, NewTargetData);
	}

	if (TargetData)
	{
		// TODO PART 3
		// 
		// - Update TargetData->bClearLOS
		//		Use this.VisionParameters to determine whether the target is within the vision cone or not 
		//		(and ideally do so before you cast a ray towards it)
		// - Update TargetData->Awareness
		//		On ticks when the AI has a clear LOS, the Awareness should grow
		//		On ticks when the AI does not have a clear LOS, the Awareness should decay
		//
		// Awareness should be clamped to the range [0, 1]
		// You can add parameters to the UGAPerceptionComponent to control the speed at which awareness rises and falls

		// YOUR CODE HERE

		// finding positions of bot and target

		// cache reference to target's AActor
		AActor* targetActor = TargetComponent->GetOwner();

		FVector currentPos = OwnerPawn->GetActorLocation(); // location of bot
		FVector targetPos = targetActor->GetActorLocation(); // location of player/target

		// finding the angle between the bot and the target
		FVector pawnDirection = OwnerPawn->GetActorForwardVector().GetSafeNormal();
		FVector currentDirection = (targetPos - currentPos).GetSafeNormal();

		float dotProd = FVector::DotProduct(pawnDirection, currentDirection);
		float angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(dotProd, -1.0f, 1.0f))); // clamping to avoid precision issues, then using dot product result to calculate angle in radians, then converting to degrees.
		float deltaTime = GetWorld()->GetDeltaSeconds();
		if (FVector::Distance(currentPos, targetPos) < VisionParameters.VisionDistance && angle < VisionParameters.VisionAngle)
		{
			// player is within the cone. need to raycast and check if in LOS
			UWorld* World = GetWorld();
			FHitResult HitResult;
			FCollisionQueryParams Params;
			FVector Start = currentPos;
			FVector End = targetPos;
			Params.AddIgnoredActor(OwnerPawn);
			Params.AddIgnoredActor(targetActor);
			bool bHitSomething = World->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, Params);
			TargetData->bClearLos = !bHitSomething;
			TargetData->Awareness = FMath::Clamp(bHitSomething ? TargetData->Awareness - (1.0f/ReactionTime) * deltaTime : TargetData->Awareness + (1.0f / ReactionTime) * deltaTime , 0.0f , 1.0f);// awareness should take 1s to go from 0-1 and vice-versa
			//UE_LOG(LogTemp, Warning, TEXT("Result: %f"), TargetData->Awareness);
		}
		else
		{
			// Player is out of range. Gradually reduce the awareness of the bot
			TargetData->Awareness = FMath::Clamp(TargetData->Awareness - (1.0f / ReactionTime) * deltaTime, 0.0f, 1.0f);
		}
		UE_LOG(LogTemp, Warning, TEXT("Result: %f"), TargetData->Awareness);
	}
}


const FTargetData* UGAPerceptionComponent::GetTargetData(FGuid TargetGuid) const
{
	return TargetMap.Find(TargetGuid);
}

bool UGAPerceptionComponent::TestVisibility(FVector& CellPosition) const
{

	// Copied the code from the UpdateTargetData function
	APawn* OwnerPawn = GetOwnerPawn();
	FVector currentPos = OwnerPawn->GetActorLocation(); // location of bot

	FVector targetPos = FVector(CellPosition.X, CellPosition.Y, currentPos.Z); // This is because I will get only the X and Y coordinates properly here. So I need to fake the z axis to be in the same height as the perceiving bot.
	FVector pawnDirection = OwnerPawn->GetActorForwardVector().GetSafeNormal();
	FVector currentDirection = (targetPos - currentPos).GetSafeNormal();

	float dotProd = FVector::DotProduct(pawnDirection, currentDirection);
	float angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(dotProd, -1.0f, 1.0f))); // clamping to avoid precision issues, then using dot product result to calculate angle in radians, then converting to degrees.
	float deltaTime = GetWorld()->GetDeltaSeconds();
	if (FVector::Distance(currentPos, targetPos) < VisionParameters.VisionDistance && angle < VisionParameters.VisionAngle)
	{
		// Cell is within the cone. need to raycast and check if in LOS
		UWorld* World = GetWorld();
		FHitResult HitResult;
		FCollisionQueryParams Params;
		FVector Start = currentPos;
		FVector End = CellPosition;
		Params.AddIgnoredActor(OwnerPawn);
		bool bHitSomething = World->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, Params);
		return !bHitSomething;
	}

	// It's outside the cone of percception of this bot.
	return false;
}