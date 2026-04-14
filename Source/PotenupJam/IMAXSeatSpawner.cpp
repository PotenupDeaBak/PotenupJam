#include "IMAXSeatSpawner.h"
#include "IMAXSeatDataAsset.h"
#include "Engine/World.h"

// 언리얼 에디터 기능 사용을 위한 헤더 (라벨링 용도)
#if WITH_EDITOR
#include "GameFramework/Actor.h"
#endif

AIMAXSeatSpawner::AIMAXSeatSpawner()
{
    // 스포너 자체는 매 프레임 틱을 돌 필요가 없으므로 최적화를 위해 꺼둡니다.
    PrimaryActorTick.bCanEverTick = false;
}

void AIMAXSeatSpawner::BeginPlay()
{
    Super::BeginPlay();
    
    // 런타임에 좌석이 없다면 생성 (보통 에디터에서 미리 생성해두는 것을 권장)
    if (SpawnedSeats.Num() == 0)
    {
        GenerateSeats();
    }
}

void AIMAXSeatSpawner::ClearSeats()
{
    // 배열을 순회하며 기존에 스폰된 좌석 액터들을 메모리에서 안전하게 소멸시킵니다.
    for (AActor* Seat : SpawnedSeats)
    {
        if (IsValid(Seat))
        {
            Seat->Destroy();
        }
    }
    // 포인터 배열 초기화 (Dangling Pointer 방지)
    SpawnedSeats.Empty();
}

void AIMAXSeatSpawner::GenerateSeats()
{
    // 1. 기존 좌석 초기화 및 유효성 검사
    ClearSeats();

    if (!IMAXSeatData || !IMAXSeatData->SeatActorClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[IMAXSeatSpawner] DataAsset 또는 SeatActorClass가 할당되지 않았습니다."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    // 현재 스포너 액터의 위치를 전체 상영관의 기준점(예: 스크린 정중앙 하단)으로 삼습니다.
    FVector BaseLocation = GetActorLocation();

    // 2. DataAsset에 정의된 각 분단(좌, 중, 우) 순회
    for (const FSeatBlockConfig& Block : IMAXSeatData->SeatBlocks)
    {
        // 해당 분단이 스크린을 바라보도록 Z축(Yaw) 회전값 설정
        FRotator BlockRotation(0.0f, Block.FocusYawAngle, 0.0f);

        // 3. 분단 내의 행(Row, 앞뒤)과 열(Column, 좌우) 순회
        for (int32 Row = 0; Row < Block.Rows; ++Row)
        {
            for (int32 Col = 0; Col < Block.Columns; ++Col)
            {
                // [핵심 1] 로컬 좌표계 계산
                // 스크린 방향을 +X축, 오른쪽을 +Y축으로 가정합니다.
                // 좌석은 뒤로 갈수록 X값이 작아지므로(멀어지므로) - 부호를 붙입니다.
                float LocalX = -(Row * IMAXSeatData->SeatSpacingX); 
                float LocalY = Col * IMAXSeatData->SeatSpacingY;
                FVector LocalLocation(LocalX, LocalY, 0.0f);

                // [핵심 2] 분단 회전각(FocusYawAngle)을 로컬 좌표에 적용
                // RotateVector 함수 내부에 고도로 최적화된 쿼터니언/삼각함수 연산이 포함되어 있습니다.
                // 이 연산을 통해 왼쪽 분단의 우측 끝 좌석이 중앙을 향해 예쁘게 호를 그리며 틀어집니다.
                FVector RotatedLocation = BlockRotation.RotateVector(LocalLocation);

                // [핵심 3] 최종 월드 트랜스폼 계산
                // 스포너 기준점 + 분단의 시작 위치(오프셋) + 회전이 적용된 개별 좌석 위치
                FVector FinalLocation = BaseLocation + Block.BlockOffset + RotatedLocation;
                FTransform SpawnTransform(BlockRotation, FinalLocation);

                FActorSpawnParameters SpawnParams;
                SpawnParams.Owner = this;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                // [핵심 4] 액터 스폰 및 관리
                AActor* NewSeat = World->SpawnActor<AActor>(IMAXSeatData->SeatActorClass, SpawnTransform, SpawnParams);
                if (NewSeat)
                {
#if WITH_EDITOR
                    // 에디터 아웃라이너에서 식별하기 쉽도록 라벨링 (예: "LeftWing_A1", "Center_G12")
                    // 행 번호를 알파벳 A(65)부터 시작하도록 아스키 코드 연산 적용
                    FString SeatLabel = FString::Printf(TEXT("%s_%c%d"), *Block.BlockName.ToString(), 'A' + Row, Col + 1);
                    NewSeat->SetActorLabel(SeatLabel);
#endif
                    SpawnedSeats.Add(NewSeat);
                }
            }
        }
    }
}