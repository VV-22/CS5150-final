#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GATargetComponent.h"
#include "GAPerceptionComponent.generated.h"


// FTargetData represents the AI's awareness of the target.
// Basically it stored current LOS info and the awareness gauge.
// Note that it does NOT store last known position/velocity. That information
// is stored in the target itself. 
USTRUCT(BlueprintType)
struct FTargetData
{
	GENERATED_USTRUCT_BODY()

	FTargetData() : bClearLos(false), Awareness(0.0f) , bHearingPlayer(false) {}

	// The last LOS check of this target
	// Note: even if the LOS is clear, it doesn't mean the AI is aware of the target (yet)!
	UPROPERTY(BlueprintReadOnly)
	bool bClearLos;

	UPROPERTY(BlueprintReadOnly)
	float Awareness;

	UPROPERTY(BlueprintReadOnly)
	bool bHearingPlayer;
};


// Parameters that control the perceiver's vision.
// Vision angle is with respect to the owning pawn's facing direction.
// NOTE: this is an extremely simple vision model. For bonus points, you could make this more complicated, adding multiple vision regions, a la Splinter Cell: Blacklist
USTRUCT(BlueprintType)
struct FVisionParameters
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float VisionAngle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float VisionDistance;

};


USTRUCT(BlueprintType)
struct FSoundParameters
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float HearingRange = 200;
	
};


UCLASS(BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class UGAPerceptionComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere)
	float TimeToAcknowledge;

	UPROPERTY(EditAnywhere)
	float TimeToLose;

	UPROPERTY(EditAnywhere)
	float HearingDist = 5000;

	UPROPERTY(EditAnywhere)
	float SoundAcknowledgementTime;

	UPROPERTY(EditAnywhere)
	float SoundLoseTime;

	// Needed for some bookkeeping (registering the perception component with the the Perception System)
	// You shouldn't need to touch these.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

	// It is super easy to forget: this component will usually be attached to the CONTROLLER, not the pawn it's controlling
	// A lot of times we want access to the pawn (e.g. when sending signals to its movement component).
	UFUNCTION(BlueprintCallable, BlueprintPure)
	APawn *GetOwnerPawn() const;

	// Returns the Target this AI is attending to right now.
	UFUNCTION(BlueprintCallable)
	UGATargetComponent* GetCurrentTarget() const;

	// Returns whether or not the perceiver currently has a target.
	// Note this will return false if the perceiver doesn't know about any targetable actors
	UFUNCTION(BlueprintCallable)
	bool HasTarget() const;

	// The main function used to access latest known information about the AI's current target.
	// This combined TargetCache information (from the TargetComponent) and TargetData information, which holds THIS AI's individual awareness of the target.
	UFUNCTION(BlueprintCallable)
	bool GetCurrentTargetState(FTargetCache &TargetCacheOut, FTargetData &TargetDataOut) const;

	// Currently only for debugging
	UFUNCTION(BlueprintCallable)
	void GetAllTargetStates(bool OnlyKnown, TArray<FTargetCache> &TargetCachesOut, TArray<FTargetData> &TargetDatasOut) const;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Vision parameters
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FVisionParameters VisionParameters;

	UPROPERTY(BluePrintReadWrite, EditAnywhere)
	FSoundParameters SoundParameters;

	// A map from TargetComponent's TargetGuid to target data
	// This allows each individual perceiving AI to store a little chunk of data for each perceivable target.

	UPROPERTY(BlueprintReadOnly)
	TMap<FGuid, FTargetData> TargetMap;

	void UpdateAllTargetData(float DeltaTime);
	void UpdateTargetData(float DeltaTime, UGATargetComponent* TargetComponent);

	// Return the FTargetData for the given target
	const FTargetData *GetTargetData(FGuid TargetGuid) const;

	bool HasClearLOS(const AActor* TargetActor, const FVector& TargetPoint) const;
	UFUNCTION(BlueprintCallable)
	bool HeardPlayerMove(const AActor* TargetActor, const FVector& TargetPoint) const;
	
	UFUNCTION(BlueprintCallable)
	void ResetTargetState();
};