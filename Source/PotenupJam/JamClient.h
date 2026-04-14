#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IWebSocket.h"
#include "JamClient.generated.h"

UCLASS()
class POTENUPJAM_API AJamClient : public AActor
{
	GENERATED_BODY()
    
public: 
	AJamClient();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** WebSocket 인스턴스 */
	TSharedPtr<IWebSocket> Socket;

	/** 소켓 통신 초기화 및 연결 */
	void Connect();

	/** 소켓 델리게이트 콜백 핸들러 */
	void OnConnected();
	void OnConnectionError(const FString& Error);
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnMessage(const FString& Message);

	/** JSON 파싱 및 액터 제어 로직 (반드시 Game Thread에서 실행되어야 함) */
	void ProcessSocketData(const FString& JsonString);
	void LookAtScreen(APlayerController* PC, AActor* ViewTarget);

protected:
	UPROPERTY(EditAnywhere, Category = "Socket Configuration")
	FString ServerURL = TEXT("ws://172.16.15.53:8001/ws");

	UPROPERTY(EditAnywhere, Category = "Socket Configuration")
	float BlendTime = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Socket Configuration")
	FString TargetScreen = TEXT("Screen");
};