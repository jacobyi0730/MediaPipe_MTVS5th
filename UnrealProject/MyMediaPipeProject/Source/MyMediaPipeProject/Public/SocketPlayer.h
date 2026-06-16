// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformProcess.h"
#include "Sockets.h"
#include "SocketPlayer.generated.h"

class IWebSocket;

UENUM(BlueprintType)
enum class EMediaPipeTransportType : uint8
{
	UDP UMETA(DisplayName="UDP"),
	TCP UMETA(DisplayName="TCP"),
	WebSocket UMETA(DisplayName="WebSocket")
};

UCLASS()
class MYMEDIAPIPEPROJECT_API ASocketPlayer : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ASocketPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeTransport();
	void ShutdownTransport();
	void PollTransport();
	void InitializeUdpSocket();
	void ShutdownUdpSocket();
	void PollUdpMessages();
	void InitializeTcpSocket();
	void ShutdownTcpSocket();
	void PollTcpMessages();
	void InitializeWebSocket();
	void ShutdownWebSocket();
	void HandlePacket(const TArray<uint8>& PacketData, int32 BytesRead);
	void ParsePacketBuffer(TArray<uint8>& PacketBuffer);
	void UpdateDisplayText(const FString& NewText);
	FString BuildDisplayText(uint16 Identifier, uint16 Cmd, uint32 PayloadLength, uint32 GestureCode) const;
	FString GetGestureName(uint32 GestureCode) const;
	FString GetTransportName() const;
	FString GetServerScriptPath() const;
	FString GetDefaultPythonPath() const;
	FString BuildServerArguments() const;
	void LaunchMediaPipeServer();
	void StopMediaPipeServer();
	bool ShouldRetryConnection() const;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category="MediaPipe")
	void StartMediaPipeWithTransport(EMediaPipeTransportType NewTransportType);
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<class ULogUI> LogUIFactory;
	
	UPROPERTY()
	TObjectPtr<class ULogUI> LogUI;

	UPROPERTY(EditAnywhere, Category="MediaPipe")
	EMediaPipeTransportType TransportType = EMediaPipeTransportType::UDP;

	UPROPERTY(EditAnywhere, Category="MediaPipe")
	bool bAutoLaunchServer = false;

	UPROPERTY(EditAnywhere, Category="MediaPipe")
	bool bShowServerPreview = true;

	UPROPERTY(EditAnywhere, Category="MediaPipe")
	FString ServerHost = TEXT("127.0.0.1");

	UPROPERTY(EditAnywhere, Category="MediaPipe")
	int32 ServerPort = 7001;

	UPROPERTY(EditAnywhere, Category="MediaPipe")
	FString PythonExecutablePath;

	UPROPERTY(EditAnywhere, Category="MediaPipe")
	FString ServerScriptPath;

private:
	FSocket* UdpSocket = nullptr;
	FSocket* TcpSocket = nullptr;
	TArray<uint8> TcpPacketBuffer;
	TSharedPtr<IWebSocket> WebSocket;
	TArray<uint8> WebSocketPacketBuffer;
	FProcHandle MediaPipeServerProcess;
	FString LastDisplayText = TEXT("Waiting for MediaPipe packets...");
	bool bTcpConnected = false;
	bool bWebSocketConnected = false;
	double NextReconnectTimeSeconds = 0.0;
	static constexpr uint16 ExpectedIdentifier = 0x4D50;
	static constexpr uint16 GestureCommand = 0x0001;
	static constexpr uint32 ExpectedPayloadLength = 4;
};
