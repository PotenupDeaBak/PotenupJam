#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SeatInterface.generated.h"

UINTERFACE(BlueprintType)
class POTENUPJAM_API USeatInterface : public UInterface
{
	GENERATED_BODY()
};

class POTENUPJAM_API ISeatInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Seat")
	void SetSeatID(const FString& InSeatID);
};
