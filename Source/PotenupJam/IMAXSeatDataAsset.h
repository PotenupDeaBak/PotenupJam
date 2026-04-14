#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "IMAXSeatDataAsset.generated.h"

// 각 분단(좌, 중, 우)의 배치 정보를 담는 구조체
USTRUCT(BlueprintType)
struct FSeatBlockConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seat Block")
	FName BlockName; // 예: "LeftWing", "Center", "RightWing"

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seat Block", meta=(ClampMin="1"))
	int32 Rows; // 행 수 (예: A~P열이면 16)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seat Block", meta=(ClampMin="1"))
	int32 Columns; // 열 수

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seat Block")
	FVector BlockOffset; // 중앙 기준점으로부터 해당 분단이 시작되는 상대 위치

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Seat Block")
	float FocusYawAngle; // 스크린을 바라보기 위한 Z축 회전값 (예: 좌측 분단은 15도, 우측 분단은 -15도)
};

/**
 * 용산 IMAX 상영관의 전체 좌석 메타데이터를 관리하는 Data Asset
 * 공식 문서 참조: Data Assets in Unreal Engine
 */
UCLASS()
class UIMAXSeatDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 스폰할 실제 좌석 액터 클래스 (블루프린트로 파생된 클래스 할당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="IMAX Configuration")
	TSubclassOf<AActor> SeatActorClass;

	// 앞뒤 좌석 간격 (X축)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="IMAX Configuration")
	float SeatSpacingX = 120.0f; 

	// 좌우 좌석 간격 (Y축)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="IMAX Configuration")
	float SeatSpacingY = 80.0f; 

	// 상영관 내 배치될 분단들의 배열
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="IMAX Configuration")
	TArray<FSeatBlockConfig> SeatBlocks;
};