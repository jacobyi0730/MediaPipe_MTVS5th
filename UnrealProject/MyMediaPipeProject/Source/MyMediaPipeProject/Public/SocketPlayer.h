// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Sockets.h"
#include "SocketPlayer.generated.h"

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

	void InitializeUdpSocket();
	void ShutdownUdpSocket();
	void PollUdpMessages();
	void HandlePacket(const TArray<uint8>& PacketData, int32 BytesRead);
	FString BuildDisplayText(uint16 Identifier, uint16 Cmd, uint32 PayloadLength, uint32 GestureCode) const;
	FString GetGestureName(uint32 GestureCode) const;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	UPROPERTY(EditAnywhere)
	TSubclassOf<class ULogUI> LogUIFactory;
	
	UPROPERTY()
	TObjectPtr<class ULogUI> LogUI;

	UPROPERTY(EditAnywhere, Category="UDP")
	int32 ListenPort = 7001;

	UPROPERTY(EditAnywhere, Category="UDP")
	FString BindAddress = TEXT("0.0.0.0");

private:
	FSocket* ListenSocket = nullptr;
	FString LastDisplayText = TEXT("Waiting for UDP packets...");
	static constexpr uint16 ExpectedIdentifier = 0x4D50;
	static constexpr uint16 GestureCommand = 0x0001;
	static constexpr uint32 ExpectedPayloadLength = 4;
};
