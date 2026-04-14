#include "JamClient.h"
#include "Camera/CameraActor.h" // 가상 카메라 액터 사용을 위한 필수 헤더
#include "WebSocketsModule.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Async/Async.h"

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

   // [해결책] Standalone에서는 BeginPlay 즉시 실행 시 뷰가 초기화될 수 있으므로
   // 0.2~0.5초 정도 지연 후에 초기 카메라 위치를 잡도록 타이머를 설정합니다.
   FTimerHandle InitTimerHandle;
   GetWorldTimerManager().SetTimer(InitTimerHandle, [this]()
   {
       // 1. 태그를 기반으로 좌석 찾기 (Standalone에서 가장 안전한 방법)
       FString DefaultSeat = TEXT("A3");
       AActor* FoundActor = nullptr;
        
       for (TActorIterator<AActor> It(GetWorld()); It; ++It)
       {
           AActor* Actor = *It;
           // GetActorLabel() 대신 ActorHasTag를 우선적으로 사용합니다.
           if (Actor && (Actor->ActorHasTag(FName(*DefaultSeat)) || Actor->GetName() == DefaultSeat))
           {
               FoundActor = Actor;
               break;
           }
       }
        
       APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
       if (PC && FoundActor)
       {
           LookAtScreen(PC, FoundActor);
       }
   }, 0.2f, false);

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

    Socket = FWebSocketsModule::Get().CreateWebSocket(ServerURL, TEXT(""));

    Socket->OnConnected().AddUObject(this, &AJamClient::OnConnected);
    Socket->OnConnectionError().AddUObject(this, &AJamClient::OnConnectionError);
    Socket->OnClosed().AddUObject(this, &AJamClient::OnClosed);
    Socket->OnMessage().AddUObject(this, &AJamClient::OnMessage);

    Socket->Connect();
}

void AJamClient::OnConnected()
{
    UE_LOG(LogTemp, Log, TEXT("AJamClient: Connected successfully. Sending registration message..."));

    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());
    RootObject->SetStringField(TEXT("type"), TEXT("register"));
    RootObject->SetStringField(TEXT("role"), TEXT("unreal"));

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

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
    AsyncTask(ENamedThreads::GameThread, [this, Message]()
    {
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
       FString MsgType;
       if (JsonObject->TryGetStringField(TEXT("type"), MsgType) && MsgType == TEXT("highlight_seat"))
       {
          FString TargetSeat;
          if (JsonObject->TryGetStringField(TEXT("seat_id"), TargetSeat))
          {
             UE_LOG(LogTemp, Log, TEXT("AJamClient: Switching view to seat %s"), *TargetSeat);

             AActor* FoundActor = nullptr;
             for (TActorIterator<AActor> It(GetWorld()); It; ++It)
             {
                AActor* Actor = *It;
                if (Actor && (Actor->GetName() == TargetSeat || 
                           Actor->ActorHasTag(FName(*TargetSeat)) ||
#if WITH_EDITOR
                           Actor->GetActorLabel() == TargetSeat
#else
                           false
#endif
                   ))
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
                   // 여기서 직접 블렌딩하지 않고 LookAtScreen으로 통제권을 넘깁니다.
                   LookAtScreen(PC, FoundActor);
                }
             }
             else
             {
                UE_LOG(LogTemp, Warning, TEXT("AJamClient: Actor for seat %s not found."), *TargetSeat);
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
       if (Actor && (Actor->GetName() == TargetScreen || 
                  Actor->ActorHasTag(FName(*TargetScreen)) ||
#if WITH_EDITOR
                  Actor->GetActorLabel() == TargetScreen
#else
                  false
#endif
          ))
       {
          ScreenActor = Actor;
          break;
       }
    }

   if (ScreenActor)
   {
      // 1. 시선의 시작점 (의자 기준 사람 눈높이)
      FVector EyeLocation = ViewTarget->GetActorLocation() + FVector(10.0f, 0.0f, 120.0f);

      // 2. [수정됨] 복잡한 Bounds 계산 없이, Target Point의 정확한 좌표를 그대로 사용!
      FVector ScreenCenter = ScreenActor->GetActorLocation();

      // 3. 눈높이에서 타겟 포인트를 바라보는 회전 각도 산출
      FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(EyeLocation, ScreenCenter);

      // 핑퐁 가상 카메라 로직
      static bool bUseCamA = true;
      FName CamTag = bUseCamA ? TEXT("VirtualCamA") : TEXT("VirtualCamB");
      bUseCamA = !bUseCamA;

      ACameraActor* TargetCamera = nullptr;
      for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
      {
         if (It->ActorHasTag(CamTag))
         {
            TargetCamera = *It;
            break;
         }
      }

      if (!TargetCamera)
      {
         TargetCamera = GetWorld()->SpawnActor<ACameraActor>();
         TargetCamera->Tags.Add(CamTag);
      }

      // 카메라 세팅 및 뷰 블렌딩 전환
      TargetCamera->SetActorLocationAndRotation(EyeLocation, LookAtRot);
      PC->SetControlRotation(LookAtRot);
      PC->SetViewTargetWithBlend(TargetCamera, BlendTime);

      // 정확한 조준점 로그 출력
      UE_LOG(LogTemp, Log, TEXT("AJamClient: Virtual Camera is looking at EXACT Point: %s"), *ScreenCenter.ToString());
   }
}