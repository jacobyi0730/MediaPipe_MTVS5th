// Fill out your copyright notice in the Description page of Project Settings.


#include "MyMediaPipeProject/Public/SocketPlayer.h"
#include "Common/UdpSocketBuilder.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Blueprint/UserWidget.h"
#include "MyMediaPipeProject/Public/LogUI.h"
#include "SocketSubsystem.h"


// Sets default values
ASocketPlayer::ASocketPlayer()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ASocketPlayer::BeginPlay()
{
	Super::BeginPlay();
	
	if (LogUI = Cast<ULogUI>(CreateWidget(GetWorld(), LogUIFactory)))
	{
		LogUI->AddToViewport();
		LogUI->SetLogText(LastDisplayText);
	}

	InitializeUdpSocket();
}

void ASocketPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownUdpSocket();
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ASocketPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	PollUdpMessages();
}

// Called to bind functionality to input
void ASocketPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ASocketPlayer::InitializeUdpSocket()
{
	FIPv4Address Ip;
	if (!FIPv4Address::Parse(BindAddress, Ip))
	{
		Ip = FIPv4Address::Any;
	}

	const FIPv4Endpoint Endpoint(Ip, ListenPort);
	ListenSocket = FUdpSocketBuilder(TEXT("MediaPipeUdpListener"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.WithReceiveBufferSize(2 * 1024 * 1024);

	if (!ListenSocket)
	{
		LastDisplayText = TEXT("Failed to bind UDP socket.");
		if (LogUI)
		{
			LogUI->SetLogText(LastDisplayText);
		}
	}
}

void ASocketPlayer::ShutdownUdpSocket()
{
	if (ListenSocket)
	{
		ListenSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
		ListenSocket = nullptr;
	}
}

void ASocketPlayer::PollUdpMessages()
{
	if (!ListenSocket)
	{
		return;
	}

	uint32 PendingSize = 0;
	while (ListenSocket->HasPendingData(PendingSize))
	{
		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(FMath::Min(PendingSize, 65507u) + 1);

		int32 BytesRead = 0;
		if (ListenSocket->Recv(Buffer.GetData(), Buffer.Num(), BytesRead) && BytesRead > 0)
		{
			HandlePacket(Buffer, BytesRead);
		}
		else
		{
			break;
		}
	}
}

void ASocketPlayer::HandlePacket(const TArray<uint8>& PacketData, int32 BytesRead)
{
	if (BytesRead < 12)
	{
		LastDisplayText = FString::Printf(TEXT("Invalid packet size: %d bytes"), BytesRead);
		if (LogUI)
		{
			LogUI->SetLogText(LastDisplayText);
		}
		return;
	}

	const uint8* Data = PacketData.GetData();
	const uint16 Identifier = (static_cast<uint16>(Data[0]) << 8) | static_cast<uint16>(Data[1]);
	const uint16 Cmd = (static_cast<uint16>(Data[2]) << 8) | static_cast<uint16>(Data[3]);
	const uint32 PayloadLength =
		(static_cast<uint32>(Data[4]) << 24) |
		(static_cast<uint32>(Data[5]) << 16) |
		(static_cast<uint32>(Data[6]) << 8) |
		static_cast<uint32>(Data[7]);

	if (Identifier != ExpectedIdentifier)
	{
		LastDisplayText = FString::Printf(TEXT("Unexpected identifier: 0x%04X"), Identifier);
		if (LogUI)
		{
			LogUI->SetLogText(LastDisplayText);
		}
		return;
	}

	if (Cmd != GestureCommand)
	{
		LastDisplayText = FString::Printf(TEXT("Unexpected cmd: 0x%04X"), Cmd);
		if (LogUI)
		{
			LogUI->SetLogText(LastDisplayText);
		}
		return;
	}

	if (PayloadLength != ExpectedPayloadLength || BytesRead < 8 + static_cast<int32>(PayloadLength))
	{
		LastDisplayText = FString::Printf(
			TEXT("Invalid payload length: header=%u bytes, packet=%d bytes"),
			PayloadLength,
			BytesRead
		);
		if (LogUI)
		{
			LogUI->SetLogText(LastDisplayText);
		}
		return;
	}

	const uint32 GestureCode =
		(static_cast<uint32>(Data[8]) << 24) |
		(static_cast<uint32>(Data[9]) << 16) |
		(static_cast<uint32>(Data[10]) << 8) |
		static_cast<uint32>(Data[11]);

	LastDisplayText = BuildDisplayText(Identifier, Cmd, PayloadLength, GestureCode);
	if (LogUI)
	{
		LogUI->SetLogText(LastDisplayText);
	}
}

FString ASocketPlayer::BuildDisplayText(uint16 Identifier, uint16 Cmd, uint32 PayloadLength, uint32 GestureCode) const
{
	return FString::Printf(
		TEXT("MediaPipe UDP\nIdentifier: 0x%04X\nCmd: 0x%04X\nPayload Length: %u\nGesture Code: %u\nGesture: %s\nPort: %d"),
		Identifier,
		Cmd,
		PayloadLength,
		GestureCode,
		*GetGestureName(GestureCode),
		ListenPort
	);
}

FString ASocketPlayer::GetGestureName(uint32 GestureCode) const
{
	switch (GestureCode)
	{
	case 1:
		return TEXT("Fist");
	case 2:
		return TEXT("Scissors");
	case 3:
		return TEXT("Paper");
	default:
		return TEXT("Unknown");
	}
}

