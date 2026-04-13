#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SeatLayoutManager.generated.h"

class USceneComponent;

USTRUCT(BlueprintType)
struct FSeatRowLayout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat Layout")
	FName RowLabel = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat Layout")
	TArray<int32> SeatNumbers;
};

UCLASS()
class POTENUPJAM_API ASeatLayoutManager : public AActor
{
	GENERATED_BODY()

public:
	ASeatLayoutManager();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Seat Layout")
	void RebuildSeatLayout();

	UFUNCTION(BlueprintCallable, Category = "Seat Layout")
	void ClearSeatLayout();

	UFUNCTION(BlueprintPure, Category = "Seat Layout")
	const TArray<AActor*>& GetSpawnedSeats() const { return SpawnedSeats; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	TSubclassOf<AActor> SeatActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	TArray<FSeatRowLayout> RowSeatLayout;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout", meta = (ClampMin = "0.0"))
	float SeatSpacing = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout", meta = (ClampMin = "0.0"))
	float RowSpacing = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	float RowHeightStep = 47.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	FVector LayoutOrigin = FVector::ZeroVector;
	

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	FVector SeatDirection = FVector(0.0f, -1.0f, 0.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	FVector RowDirection = FVector(1.0f, 0.0f, 0.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	FVector LeftBlockDirection = FVector(0.12f, 0.1f, 0.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	FVector CenterBlockDirection = FVector(0.0f, 0.f, 0.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	FVector RightBlockDirection = FVector(0.12f, -0.1f, 0.0f);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	float LeftBlockYawOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	float CenterBlockYawOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	float RightBlockYawOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seat Layout")
	bool bRebuildOnBeginPlay = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Seat Layout")
	TArray<TObjectPtr<AActor>> SpawnedSeats;

private:
	FTransform MakeSeatTransform(int32 RowIndex, int32 SeatNumber) const;
	FString MakeSeatID(const FName& RowLabel, int32 SeatNumber) const;
	void ApplySeatID(AActor* SpawnedSeat, const FString& SeatID) const;
	void BuildDefaultRowLayout();
	int32 GetSeatOffsetUnits(int32 SeatNumber) const;
	float GetRowCenterOffsetUnits(int32 RowIndex) const;
	FVector ResolveBlockDirection(const FVector& ForwardDirection, const FVector& LateralDirection, const FVector& DesiredDirection, int32 BlockType) const;
};
