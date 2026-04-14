#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IMAXSeatSpawner.generated.h"

class UIMAXSeatDataAsset;

/**
 * 설정된 DataAsset을 기반으로 영화관 좌석 배치를 절차적으로 생성하는 매니저 액터
 */
UCLASS()
class AIMAXSeatSpawner : public AActor
{
	GENERATED_BODY()

public:
	AIMAXSeatSpawner();

protected:
	// 게임 시작 시 좌석을 스폰하거나 에디터에서 미리 스폰할 수 있음
	virtual void BeginPlay() override;

public:
	// 앞서 만든 데이터 에셋을 주입받는 슬롯
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Seat Spawner Setup")
	UIMAXSeatDataAsset* IMAXSeatData;

	// 언리얼 에디터의 디테일 패널에서 클릭할 수 있는 버튼으로 노출
	UFUNCTION(CallInEditor, Category="Seat Spawner Action")
	void GenerateSeats();

	// 기존에 생성된 좌석을 일괄 삭제하는 초기화 함수
	UFUNCTION(CallInEditor, Category="Seat Spawner Action")
	void ClearSeats();

private:
	// 생성된 좌석들을 추적하여 메모리 누수 방지 및 일괄 삭제에 사용
	UPROPERTY(VisibleAnywhere, Category="Seat Spawner Status")
	TArray<AActor*> SpawnedSeats;
};