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

	/** WebSocket 인스턴스 */
	TSharedPtr<IWebSocket> Socket;

	/** 소켓 연결 시도 */
	void Connect();

	/** 소켓 이벤트 핸들러 */
	void OnConnected();
	void OnConnectionError(const FString& Error);
	void OnClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnMessage(const FString& Message);

	/** JSON 데이터 처리 및 뷰 전환 */
	void ProcessSocketData(const FString& JsonString);

	UPROPERTY(EditAnywhere, Category = "Socket")
	FString ServerURL = TEXT("ws://172.16.15.53:8001/ws");

	UPROPERTY(EditAnywhere, Category = "Socket")
	float BlendTime = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Socket")
	FString TargetScreen = TEXT("Screen");

private:
	void LookAtScreen(APlayerController* PC, AActor* ViewTarget);
};
