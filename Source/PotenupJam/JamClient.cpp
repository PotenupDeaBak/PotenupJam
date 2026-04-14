#include "JamClient.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AJamClient::AJamClient()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AJamClient::BeginPlay()
{
	Super::BeginPlay();
	
	// 소켓 모듈 초기화 확인
	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::Get().LoadModule("WebSockets");
	}
	
	// 레벨에서 해당 이름의 Actor를 찾음 (기본값 "A1")
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
	if (PC)
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

	// 일부 서버는 핸드셰이크 단계(연결 전)에서 특정 헤더를 요구합니다.
	TMap<FString, FString> UpgradeHeaders;
	UpgradeHeaders.Add(TEXT("type"), TEXT("register"));
	UpgradeHeaders.Add(TEXT("role"), TEXT("unreal"));

	// 두 번째 인자(Protocol)를 빈 문자열로, 세 번째 인자(Headers)를 추가하여 생성
	Socket = FWebSocketsModule::Get().CreateWebSocket(ServerURL, TEXT(""), UpgradeHeaders);

	// 이벤트 바인딩
	Socket->OnConnected().AddUObject(this, &AJamClient::OnConnected);
	Socket->OnConnectionError().AddUObject(this, &AJamClient::OnConnectionError);
	Socket->OnClosed().AddUObject(this, &AJamClient::OnClosed);
	Socket->OnMessage().AddUObject(this, &AJamClient::OnMessage);

	// 연결 시작
	Socket->Connect();
}

void AJamClient::OnConnected()
{
	UE_LOG(LogTemp, Log, TEXT("AJamClient: Connected successfully. Sending registration message..."));

	// 등록 메시지 전송
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
	UE_LOG(LogTemp, Log, TEXT("AJamClient: Received data: %s"), *Message);

	// 받은 UTF-8 데이터를 처리
	ProcessSocketData(Message);
}

void AJamClient::ProcessSocketData(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		FString TargetSeat;
		if (JsonObject->TryGetStringField(TEXT("seat_id"), TargetSeat))
		{
			UE_LOG(LogTemp, Log, TEXT("AJamClient: Target seat is %s"), *TargetSeat);

			// 레벨에서 해당 이름의 Actor를 찾음
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
					UE_LOG(LogTemp, Log, TEXT("AJamClient: Switching view to actor %s"), *FoundActor->GetName());
					PC->SetViewTargetWithBlend(FoundActor, BlendTime);
					LookAtScreen(PC, FoundActor);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AJamClient: Actor with name/label %s not found."), *TargetSeat);
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
		
		// 1. PlayerController의 회전 설정
		PC->SetControlRotation(LookAtRot);

		// 2. ViewTarget(자석/의자 등) 액터 자체의 회전도 변경 (카메라가 액터 회전을 따를 수 있도록)
		ViewTarget->SetActorRotation(LookAtRot);

		UE_LOG(LogTemp, Log, TEXT("AJamClient: Rotating %s to look at %s (Rot: %s)"), 
			*ViewTarget->GetName(), *ScreenActor->GetName(), *LookAtRot.ToString());
	}
}
