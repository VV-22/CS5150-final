#include "GATargetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameAI/Grid/GAGridActor.h"
#include "GAPerceptionSystem.h"
#include "ProceduralMeshComponent.h"



UGATargetComponent::UGATargetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;

	SetTickGroup(ETickingGroup::TG_PostUpdateWork);

	// Generate a new guid
	TargetGuid = FGuid::NewGuid();
}


AGAGridActor* UGATargetComponent::GetGridActor() const
{
	AGAGridActor* Result = GridActor.Get();
	if (Result)
	{
		return Result;
	}
	else
	{
		AActor* GenericResult = UGameplayStatics::GetActorOfClass(this, AGAGridActor::StaticClass());
		if (GenericResult)
		{
			Result = Cast<AGAGridActor>(GenericResult);
			if (Result)
			{
				// Cache the result
				// Note, GridActor is marked as mutable in the header, which is why this is allowed in a const method
				GridActor = Result;
			}
		}

		return Result;
	}
}


void UGATargetComponent::OnRegister()
{
	Super::OnRegister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->RegisterTargetComponent(this);
	}

	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		OccupancyMap = FGAGridMap(Grid, 0.0f);
	}
}

void UGATargetComponent::OnUnregister()
{
	Super::OnUnregister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->UnregisterTargetComponent(this);
	}
}



void UGATargetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool isImmediate = false;

	// update my perception state FSM
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGAPerceptionComponent>> &PerceptionComponents = PerceptionSystem->GetAllPerceptionComponents();
		for (UGAPerceptionComponent* PerceptionComponent : PerceptionComponents)
		{
			const FTargetData* TargetData = PerceptionComponent->GetTargetData(TargetGuid);
			if (TargetData && (TargetData->Awareness >= 1.0f))
			{
				isImmediate = true;
				break;
			}
		}
	}

	if (isImmediate)
	{
		AActor* Owner = GetOwner();
		LastKnownState.State = GATS_Immediate;

		// REFRESH MY STATE
		LastKnownState.Set(Owner->GetActorLocation(), Owner->GetVelocity());

		// Tell the omap to clear out and put all the probability in the observed location
		OccupancyMapSetPosition(LastKnownState.Position);
	}
	else if (IsKnown())
	{
		LastKnownState.State = GATS_Hidden;
	}

	if (LastKnownState.State == GATS_Hidden)
	{
		OccupancyMapUpdate();
	}

	// As long as I'm known, whether I'm immediate or not, diffuse the probability in the omap

	if (IsKnown())
	{
		OccupancyMapDiffuse();
	}

	if (bDebugOccupancyMap)
	{
		AGAGridActor* Grid = GetGridActor();
		Grid->DebugGridMap = OccupancyMap;
		GridActor->RefreshDebugTexture();
		GridActor->DebugMeshComponent->SetVisibility(true);
	}
}


void UGATargetComponent::OccupancyMapSetPosition(const FVector& Position)
{
	// TODO PART 4

	// We've been observed to be in a given position
	// Clear out all probability in the omap, and set the appropriate cell to P = 1.0

	OccupancyMap.ResetData(0.0f);
	const AGAGridActor* Grid = GetGridActor();
	FCellRef PositionCell = Grid->GetCellRef(Position);
	OccupancyMap.SetValue(PositionCell, 1.0f);
}


void UGATargetComponent::OccupancyMapUpdate()
{
	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		FGAGridMap VisibilityMap(Grid, 0.0f);

		// TODO PART 4

		// STEP 1: Build a visibility map, based on the perception components of the AIs in the world
		// The visibility map is a simple map where each cell is either 0 (not currently visible to ANY perceiver) or 1 (currently visible to one or more perceivers).
		// 
		
		// Find the bounds of the map here.
		int MinX = VisibilityMap.GridBounds.MinX;
		int MinY = VisibilityMap.GridBounds.MinY;
		int MaxX = VisibilityMap.GridBounds.MaxX;
		int MaxY = VisibilityMap.GridBounds.MaxY;

		UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
		if (PerceptionSystem)
		{
			TArray<TObjectPtr<UGAPerceptionComponent>>& PerceptionComponents = PerceptionSystem->GetAllPerceptionComponents();
			for (UGAPerceptionComponent* PerceptionComponent : PerceptionComponents)
			{
				// Find visible cells for this perceiver.
				// Reminder: Use the PerceptionComponent.VisionParameters when determining whether a cell is visible or not (in addition to a line trace).
				// Suggestion: you might find it useful to add a UGAPerceptionComponent::TestVisibility method to the perception component.

				// For now, I'm going through the entire grid for each perceiver. A more efficient approach might be to only search within the radius of the perceiver's visionParams.

				for (int X = MinX; X <= MaxX; X++)
				{
					for (int Y = MinY; Y <= MaxY; Y++)
					{
						FCellRef TestCell(X, Y);
						FVector VectorPos = Grid->GetCellPosition(TestCell);
						bool bVisible = PerceptionComponent->TestVisibility(VectorPos);
						VisibilityMap.SetValue(TestCell, bVisible ? 1.0f : 0.0f);
					}

				}
				
			}
		}


		// STEP 2: Clear out the probability in the visible cells

		float SumVisibleProb = 0.0f; // This value is required later to renormalize

		// Do I need to check for traversible cells here?

		for (int X = MinX; X <= MaxX; X++)
		{
			for (int Y = MinY; Y <= MaxY; Y++)
			{
				FCellRef CurrentCell(X, Y);
				float OccupancyValue, VisibilityValue;
				VisibilityMap.GetValue(CurrentCell, VisibilityValue);
				if (VisibilityValue == 1.0f)
				{
					// 1.0f = Visible. So the probability is zero in these cells
					OccupancyMap.GetValue(CurrentCell, OccupancyValue);
					SumVisibleProb += OccupancyValue;
					OccupancyMap.SetValue(CurrentCell, 0.0f);
				}
			}
		}

		// STEP 3: Renormalize the OMap, so that it's still a valid probability distribution

		for (int X = MinX; X <= MaxX; X++)
		{
			for (int Y = MinY; Y <= MaxY; Y++)
			{
				FCellRef CurrentCell(X, Y);
				float VisibilityValue;
				VisibilityMap.GetValue(CurrentCell, VisibilityValue);
				if (VisibilityValue == 0.0f)
				{
					// 0.0f = not-visible. So need to renormalize here.
					float OccupancyValue;
					OccupancyMap.GetValue(CurrentCell, OccupancyValue);
					float NormalizedValue = (1.0f / (1 - SumVisibleProb)) * OccupancyValue;
					OccupancyMap.SetValue(CurrentCell, NormalizedValue);
				}
			}
		}
		// STEP 4: Extract the highest-likelihood cell on the omap and refresh the LastKnownState.

		float CurrentMaxValue = 0.0f;
		FCellRef MaxCell;
		for (int X = MinX; X <= MaxX; X++)
		{
			for (int Y = MinY; Y <= MaxY; Y++)
			{
				FCellRef CurrentCell(X, Y);
				float CurrentValue;
				OccupancyMap.GetValue(CurrentCell, CurrentValue);
				if (CurrentValue > CurrentMaxValue)
				{
					CurrentMaxValue = CurrentValue;
					MaxCell = CurrentCell;
				}
			}
		}

		FVector HighestProbabilityPosition = Grid->GetCellPosition(MaxCell);
		LastKnownState.Set(HighestProbabilityPosition, FVector(0.0f)); // At this point, the velocity of the player is unknown, so I'm setting it to zero.
	}

}


void UGATargetComponent::OccupancyMapDiffuse()
{
	// TODO PART 4
	// Diffuse the probability in the OMAP

	float DiffusionCoeff = 0.1f;
	AGAGridActor* Grid = GetGridActor();
	FGAGridMap BufferMap(Grid, 0.0f);
	int MinX = BufferMap.GridBounds.MinX;
	int MinY = BufferMap.GridBounds.MinY;
	int MaxX = BufferMap.GridBounds.MaxX;
	int MaxY = BufferMap.GridBounds.MaxY;
	for (int X = MinX; X <= MaxX; X++)
	{
		for (int Y = MinY; Y <= MaxY; Y++)
		{
			//Each cell is X,
			FCellRef CurrentCell(X, Y);
			float ProbabilityLeft;
			OccupancyMap.GetValue(CurrentCell, ProbabilityLeft);
			float OriginalProb = ProbabilityLeft;
			TArray<FCellRef> Neighbors;
			Grid->GetNeighbors(CurrentCell, true, Neighbors);
			float DiffusedValue;
			for (FCellRef Neighbor : Neighbors)
			{
				if (Neighbor.Distance(CurrentCell) > 1.0f)
				{
					//Diagonal 
					DiffusedValue = DiffusionCoeff * OriginalProb * UE_INV_SQRT_2;
				}
				else
				{
					DiffusedValue = DiffusionCoeff * OriginalProb;
				}

				//Set the value in Buffermap and reduce the value from Probabilityleft
				float CurrentBufferValue;
				BufferMap.GetValue(Neighbor, CurrentBufferValue);
				BufferMap.SetValue(Neighbor, CurrentBufferValue + DiffusedValue);
				ProbabilityLeft = ProbabilityLeft - DiffusedValue;
			}

			float CurrentBufferValue;
			BufferMap.GetValue(CurrentCell, CurrentBufferValue);
			BufferMap.SetValue(CurrentCell, CurrentBufferValue + ProbabilityLeft);
		}
	}
	OccupancyMap = BufferMap;
}
