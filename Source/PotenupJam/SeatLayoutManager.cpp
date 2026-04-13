#include "SeatLayoutManager.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "SeatInterface.h"
#include "UObject/ConstructorHelpers.h"

ASeatLayoutManager::ASeatLayoutManager()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FClassFinder<AActor> SeatClassFinder(TEXT("/Game/HJ/Blueprints/BP_Seat"));
	if (SeatClassFinder.Succeeded())
	{
		SeatActorClass = SeatClassFinder.Class;
	}

	BuildDefaultRowLayout();
}

void ASeatLayoutManager::BeginPlay()
{
	Super::BeginPlay();

	if (bRebuildOnBeginPlay)
	{
		RebuildSeatLayout();
	}
}

void ASeatLayoutManager::RebuildSeatLayout()
{
	UWorld* World = GetWorld();
	if (!World || !SeatActorClass)
	{
		return;
	}

	ClearSeatLayout();

	const FVector SafeSeatDirection = SeatDirection.GetSafeNormal();
	const FVector SafeRowDirection = RowDirection.GetSafeNormal();

	if (SafeSeatDirection.IsNearlyZero() || SafeRowDirection.IsNearlyZero())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 RowIndex = 0; RowIndex < RowSeatLayout.Num(); ++RowIndex)
	{
		const FSeatRowLayout& RowLayout = RowSeatLayout[RowIndex];

		for (int32 SeatIndex = 0; SeatIndex < RowLayout.SeatNumbers.Num(); ++SeatIndex)
		{
			const int32 SeatNumber = RowLayout.SeatNumbers[SeatIndex];
			const FTransform SeatTransform = MakeSeatTransform(RowIndex, SeatNumber);
			AActor* SpawnedSeat = World->SpawnActor<AActor>(SeatActorClass, SeatTransform, SpawnParams);

			if (!SpawnedSeat)
			{
				continue;
			}

			const FString SeatID = MakeSeatID(RowLayout.RowLabel, SeatNumber);
			ApplySeatID(SpawnedSeat, SeatID);
			SpawnedSeats.Add(SpawnedSeat);
		}
	}
}

void ASeatLayoutManager::ClearSeatLayout()
{
	for (AActor* SpawnedSeat : SpawnedSeats)
	{
		if (IsValid(SpawnedSeat))
		{
			SpawnedSeat->Destroy();
		}
	}

	SpawnedSeats.Empty();
}

FTransform ASeatLayoutManager::MakeSeatTransform(int32 RowIndex, int32 SeatNumber) const
{
	const FVector SafeRowDirection = RowDirection.GetSafeNormal();
	const FVector SafeSeatDirection = SeatDirection.GetSafeNormal();
	const FVector RowBaseOffset =
		LayoutOrigin +
		(SafeRowDirection * RowSpacing * RowIndex) +
		(FVector::UpVector * RowHeightStep * RowIndex);

	const float SeatOffsetUnits = static_cast<float>(GetSeatOffsetUnits(SeatNumber));
	const float RowCenterOffsetUnits =
	(static_cast<float>(GetSeatOffsetUnits(1)) +
	 static_cast<float>(GetSeatOffsetUnits(45))) * 0.5f;
	const float RelativeUnits = SeatOffsetUnits - RowCenterOffsetUnits;
	const FVector LocalOffset = SafeSeatDirection * (RelativeUnits * SeatSpacing);

	FTransform SeatTransform = GetActorTransform();
	SeatTransform.SetLocation(GetActorTransform().TransformPosition(RowBaseOffset + LocalOffset));
	return SeatTransform;
}

FString ASeatLayoutManager::MakeSeatID(const FName& RowLabel, int32 SeatNumber) const
{
	return FString::Printf(TEXT("%s%d"), *RowLabel.ToString(), SeatNumber);
}

void ASeatLayoutManager::ApplySeatID(AActor* SpawnedSeat, const FString& SeatID) const
{
	if (!SpawnedSeat)
	{
		return;
	}

	if (SpawnedSeat->GetClass()->ImplementsInterface(USeatInterface::StaticClass()))
	{
		ISeatInterface::Execute_SetSeatID(SpawnedSeat, SeatID);
	}

	SpawnedSeat->Tags.AddUnique(FName(*SeatID));
}

void ASeatLayoutManager::BuildDefaultRowLayout()
{
	if (RowSeatLayout.Num() > 0)
	{
		return;
	}

	RowSeatLayout.Reserve(16);

	for (TCHAR RowChar = TEXT('A'); RowChar <= TEXT('P'); ++RowChar)
	{
		FSeatRowLayout RowLayout;
		RowLayout.RowLabel = FName(*FString::Chr(RowChar));
		RowSeatLayout.Add(RowLayout);
	}
}

int32 ASeatLayoutManager::GetSeatOffsetUnits(int32 SeatNumber) const
{
	static const int32 ExtraGapThresholds[] = { 2, 15, 30, 43 };

	int32 OffsetUnits = FMath::Max(SeatNumber - 1, 0);

	for (const int32 Threshold : ExtraGapThresholds)
	{
		if (SeatNumber > Threshold)
		{
			++OffsetUnits;
		}
	}

	return OffsetUnits;
}

float ASeatLayoutManager::GetRowCenterOffsetUnits(int32 RowIndex) const
{
	if (!RowSeatLayout.IsValidIndex(RowIndex) || RowSeatLayout[RowIndex].SeatNumbers.Num() == 0)
	{
		return 0.0f;
	}

	const TArray<int32>& SeatNumbers = RowSeatLayout[RowIndex].SeatNumbers;
	int32 MinOffsetUnits = TNumericLimits<int32>::Max();
	int32 MaxOffsetUnits = TNumericLimits<int32>::Min();

	for (const int32 SeatNumber : SeatNumbers)
	{
		const int32 OffsetUnits = GetSeatOffsetUnits(SeatNumber);
		MinOffsetUnits = FMath::Min(MinOffsetUnits, OffsetUnits);
		MaxOffsetUnits = FMath::Max(MaxOffsetUnits, OffsetUnits);
	}

	return (static_cast<float>(MinOffsetUnits) + static_cast<float>(MaxOffsetUnits)) * 0.5f;
}

FVector ASeatLayoutManager::ResolveBlockDirection(const FVector& ForwardDirection, const FVector& LateralDirection, const FVector& DesiredDirection, int32 BlockType) const
{
	const FVector DefaultDirection = (ForwardDirection + (static_cast<float>(BlockType) * LateralDirection * 0.18f)).GetSafeNormal();
	const FVector BlockDirection = DesiredDirection.GetSafeNormal();

	if (BlockDirection.IsNearlyZero())
	{
		return DefaultDirection;
	}

	return BlockDirection;
}
