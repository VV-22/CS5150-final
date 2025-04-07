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

	TimeToAcknowledge = 2.0f;
	TimeToLose = 0.5f;
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

	UpdateAllTargetData(DeltaTime);
}


void UGAPerceptionComponent::UpdateAllTargetData(float DeltaTime)
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGATargetComponent>>& TargetComponents = PerceptionSystem->GetAllTargetComponents();
		for (UGATargetComponent* TargetComponent : TargetComponents)
		{
			UpdateTargetData(DeltaTime, TargetComponent);
		}
	}
}

void UGAPerceptionComponent::UpdateTargetData(float DeltaTime, UGATargetComponent* TargetComponent)
{
	// REMEMBER: the UGAPerceptionComponent is going to be attached to the controller, not the pawn. So we call this special accessor to 
	// get the pawn that our controller is controlling
	APawn* OwnerPawn = GetOwnerPawn();

	FTargetData *TargetData = TargetMap.Find(TargetComponent->TargetGuid);
	if (TargetData == NULL)		// If we don't already have a target data for the given target component, add it
	{
		FTargetData NewTargetData;
		FGuid TargetGuid = TargetComponent->TargetGuid;
		TargetData = &TargetMap.Add(TargetGuid, NewTargetData);
	}

	// TODO PART 3
	// 
	// - Update TargetData->bClearLOS
	//		Use this.VisionParameters to determine whether the target is within the vision cone or not 
	//		(and ideally do so before you case a ray towards it)
	// - Update TargetData->Awareness
	//		On ticks when the AI has a clear LOS, the Awareness should grow
	//		On ticks when the AI does not have a clear LOS, the Awareness should decay
	//
	// Awareness should be clamped to the range [0, 1]
	// You can add parameters to the UGAPerceptionComponent to control the speed at which awareness rises and falls


	if (TargetData)
	{
		AActor* TargetActor = TargetComponent->GetOwner();
		FVector TargetPoint = TargetActor->GetActorLocation();

		TargetData->bClearLos = HasClearLOS(TargetActor, TargetPoint);
			
		float AwarenessChangeTime = TargetData->bClearLos ? TimeToAcknowledge : -TimeToLose;
		float AwarenessChangePerSecond = 1.0f / AwarenessChangeTime;
		float AwarenessDelta = AwarenessChangePerSecond * DeltaTime;
		//I'm considering that the bot can hear the player at half the rate of sight.
		bool canHearPlayer = HeardPlayerMove(TargetActor, TargetPoint);

		float SoundChangeTime = canHearPlayer? SoundAcknowledgementTime : -SoundLoseTime;
		float SoundChangePerSec = 1.0f/SoundChangeTime;
		float SoundDelta = SoundChangePerSec * DeltaTime;
		TargetData->bHearingPlayer = canHearPlayer;
		if (canHearPlayer)
		{
			AwarenessDelta += SoundDelta * 0.5f;
		}

		TargetData->Awareness += AwarenessDelta;
		TargetData->Awareness = FMath::Clamp(TargetData->Awareness, 0.0f, 1.0f);
	}
}


const FTargetData* UGAPerceptionComponent::GetTargetData(FGuid TargetGuid) const
{
	return TargetMap.Find(TargetGuid);
}


bool UGAPerceptionComponent::HasClearLOS(const AActor *TargetActor, const FVector& TargetPoint) const
{
	APawn* OwnerPawn = GetOwnerPawn();
	if (OwnerPawn == NULL)
	{
		return false;
	}

	FVector OwnerLocation = OwnerPawn->GetActorLocation();
	UWorld* World = GetWorld();
	bool ClearLos = false;

	float D = FVector::Dist(TargetPoint, OwnerPawn->GetActorLocation());
	if (D <= VisionParameters.VisionDistance)
	{
		float AngleDot = FMath::Cos(FMath::DegreesToRadians(VisionParameters.VisionAngle/2.0f));
		FVector Forward = OwnerPawn->GetActorForwardVector();
		FVector OwnerToTarget = TargetPoint - OwnerLocation;
		OwnerToTarget.Normalize();

		if ((Forward | OwnerToTarget) >= AngleDot)
		{
			// within the vision angle
			// finally actually trace the line
			FHitResult HitResult;
			FCollisionQueryParams Params;
			FVector Start = OwnerLocation;
			FVector End = TargetPoint;
			Params.AddIgnoredActor(TargetActor);			// Probably want to ignore the player pawn
			Params.AddIgnoredActor(OwnerPawn);			// Probably want to ignore the AI themself
			bool bHitSomething = World->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, Params);
			ClearLos = !bHitSomething;
		}
	}

	return ClearLos;
}

bool UGAPerceptionComponent::HeardPlayerMove(const AActor* TargetActor, const FVector& TargetPoint) const
{
	//Check if player is moving

	float speed = TargetActor->GetVelocity().Size();
	float TargetActorX = TargetActor->GetActorLocation().X;
	float TargetActorY = TargetActor->GetActorLocation().Y;
	
	// This is to make sure that the target actor passed and target location passed are in the same place.
	bool DistanceCheck = FVector::Distance(FVector(TargetActorX, TargetActorY, 0.0f) , FVector(TargetPoint.X, TargetPoint.Y, 0.0f)) < 10.0f;
	
	//Check if perceiver is in range
	APawn* OwnerPawn = GetOwnerPawn();
	float Dist = FVector::Distance(TargetPoint, OwnerPawn->GetActorLocation());

	//Return true if Actor is in range and moving
	if (DistanceCheck && speed > 200.0f && Dist < HearingDist)
		UE_LOG(LogTemp, Display, TEXT("Can hear player."));
	return DistanceCheck && speed > 200.0f && Dist < SoundParameters.HearingRange;
}

void UGAPerceptionComponent::ResetTargetState()
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	PerceptionSystem->ResetAllTargetComponents();
}