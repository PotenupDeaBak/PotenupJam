#include "JamClient.h"
#include "WebSocketsModule.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Async/Async.h" // AsyncTask 사용을 위한 필수 헤더

AJamClient::AJamClient()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AJamClient::BeginPlay()
{
    Super::BeginPlay();
    
    if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
    {
       FModuleManager::Get().LoadModule("WebSockets");
    }
    
    FString DefaultSeat = TEXT("A1");
    AActor* FoundActor = nullptr;
    
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
       AActor* Actor = *It;
#if WITH_EDITOR
       if (Actor && (Actor->GetName() == DefaultSeat || Actor->GetActorLabel() == DefaultSeat))
#else
       if (Actor && Actor->GetName() == DefaultSeat)
#endif
       {
          FoundActor = Actor;
          break;
       }
    }
    
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (PC && FoundActor)
    {
       PC->SetViewTargetWithBlend(FoundActor, BlendTime);
       LookAtScreen(PC, FoundActor);
    }
    
    Connect();
}

void AJamClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Socket.IsValid())
    {
       Socket->Close();
       Socket.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void AJamClient::Connect()
{
    UE_LOG(LogTemp, Log, TEXT("AJamClient: Connecting to %s"), *ServerURL);

    // 두 번째 인자(Subprotocol)를 TEXT("")로 비워야 순수 표준 웹소켓으로 연결됩니다.
    Socket = FWebSocketsModule::Get().CreateWebSocket(ServerURL, TEXT(""));

    Socket->OnConnected().AddUObject(this, &AJamClient::OnConnected);
    Socket->OnConnectionError().AddUObject(this, &AJamClient::OnConnectionError);
    Socket->OnClosed().AddUObject(this, &AJamClient::OnClosed);
    Socket->OnMessage().AddUObject(this, &AJamClient::OnMessage);

    Socket->Connect();
}

void AJamClient::OnConnected()
{
    UE_LOG(LogTemp, Log, TEXT("AJamClient: Connected successfully. Sending registration JSON..."));

    // 1. JSON 객체 생성
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    
    // 2. 서버가 요구하는 Key-Value 세팅: {"type": "register", "role": "unreal"}
    RootObject->SetStringField(TEXT("type"), TEXT("register"));
    RootObject->SetStringField(TEXT("role"), TEXT("unreal"));

    // 3. JSON 객체를 텍스트(String)로 직렬화(변환)
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    UE_LOG(LogTemp, Warning, TEXT("AJamClient: Register payload = %s"), *OutputString);
    
    // 4. 변환된 문자열을 웹소켓 서버로 전송
    if (Socket.IsValid())
    {
        Socket->Send(OutputString);
    }
}

void AJamClient::OnConnectionError(const FString& Error)
{
    UE_LOG(LogTemp, Error, TEXT("AJamClient: Connection error: %s"), *Error);
}

void AJamClient::OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
    UE_LOG(LogTemp, Warning, TEXT("AJamClient: Connection closed. Code: %d, Reason: %s"), StatusCode, *Reason);
}

void AJamClient::OnMessage(const FString& Message)
{
    UE_LOG(LogTemp, Log, TEXT("AJamClient: Received data: %s"), *Message);

    // [핵심 수정] 네트워크 백그라운드 스레드에서 수신한 데이터를 Game Thread로 디스패치
    // 이를 누락하면 GetWorld() 및 Actor 조작 시 스레드 충돌로 엔진이 즉시 크래시됩니다.
    AsyncTask(ENamedThreads::GameThread, [this, Message]()
    {
        // 비동기 실행 시점에 이 액터(AJamClient)가 파괴되지 않고 유효한지 검사
        if (IsValid(this))
        {
            ProcessSocketData(Message);
        }
    });
}

void AJamClient::ProcessSocketData(const FString& JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // 서버에서 전달하는 메시지 타입 확인
        FString MsgType;
        if (JsonObject->TryGetStringField(TEXT("type"), MsgType))
        {
            // 좌석 하이라이트 이벤트인 경우에만 처리
            if (MsgType == TEXT("highlight_seat"))
            {
                FString TargetSeat;
                if (JsonObject->TryGetStringField(TEXT("seat_id"), TargetSeat))
                {
                    UE_LOG(LogTemp, Log, TEXT("AJamClient: Executing View Transition to %s"), *TargetSeat);

                    AActor* FoundActor = nullptr;
                    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
                    {
                        AActor* Actor = *It;
#if WITH_EDITOR
                        if (Actor && (Actor->GetName() == TargetSeat || Actor->GetActorLabel() == TargetSeat))
#else
                        if (Actor && Actor->GetName() == TargetSeat)
#endif
                        {
                            FoundActor = Actor;
                            break;
                        }
                    }

                    if (FoundActor)
                    {
                        APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
                        if (PC)
                        {
                            PC->SetViewTargetWithBlend(FoundActor, BlendTime);
                            LookAtScreen(PC, FoundActor);
                        }
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("AJamClient: Target Actor [%s] not found in world."), *TargetSeat);
                    }
                }
            }
        }
    }
    else
    {
       UE_LOG(LogTemp, Error, TEXT("AJamClient: Failed to parse JSON: %s"), *JsonString);
    }
}

void AJamClient::LookAtScreen(APlayerController* PC, AActor* ViewTarget)
{
    if (!PC || !ViewTarget) return;

    AActor* ScreenActor = nullptr;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
       AActor* Actor = *It;
#if WITH_EDITOR
       if (Actor && (Actor->GetName() == TargetScreen || Actor->GetActorLabel() == TargetScreen))
#else
       if (Actor && Actor->GetName() == TargetScreen)
#endif
       {
          ScreenActor = Actor;
          break;
       }
    }

    if (ScreenActor)
    {
       FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(ViewTarget->GetActorLocation(), ScreenActor->GetActorLocation());
       
       PC->SetControlRotation(LookAtRot);
       ViewTarget->SetActorRotation(LookAtRot);

       UE_LOG(LogTemp, Log, TEXT("AJamClient: Rotated %s to look at %s"), *ViewTarget->GetName(), *ScreenActor->GetName());
    }
}