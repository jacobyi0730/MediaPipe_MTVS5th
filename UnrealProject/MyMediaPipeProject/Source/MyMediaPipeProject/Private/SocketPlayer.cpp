// Fill out your copyright notice in the Description page of Project Settings.


#include "MyMediaPipeProject/Public/SocketPlayer.h"
#include "Common/UdpSocketBuilder.h"
#include "Common/TcpSocketBuilder.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Blueprint/UserWidget.h"
#include "IWebSocket.h"
#include "MyMediaPipeProject/Public/LogUI.h"
#include "SocketSubsystem.h"
#include "WebSocketsModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"


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
		LogUI->SetSocketPlayer(this);
		LogUI->AddToViewport();
		LogUI->SyncTransportSelection(TransportType);
		UpdateDisplayText(LastDisplayText);
	}

	if (bAutoLaunchServer)
	{
		StartMediaPipeWithTransport(TransportType);
	}
}

void ASocketPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ShutdownTransport();
	StopMediaPipeServer();
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void ASocketPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	PollTransport();
}

// Called to bind functionality to input
void ASocketPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void ASocketPlayer::StartMediaPipeWithTransport(EMediaPipeTransportType NewTransportType)
{
	ShutdownTransport();
	StopMediaPipeServer();

	TransportType = NewTransportType;
	bTcpConnected = false;
	bWebSocketConnected = false;
	NextReconnectTimeSeconds = 0.0;
	TcpPacketBuffer.Reset();
	WebSocketPacketBuffer.Reset();

	UpdateDisplayText(FString::Printf(TEXT("Starting %s transport..."), *GetTransportName()));

	LaunchMediaPipeServer();

	InitializeTransport();
}

void ASocketPlayer::InitializeTransport()
{
	switch (TransportType)
	{
	case EMediaPipeTransportType::UDP:
		InitializeUdpSocket();
		break;
	case EMediaPipeTransportType::TCP:
		InitializeTcpSocket();
		break;
	case EMediaPipeTransportType::WebSocket:
		InitializeWebSocket();
		break;
	}
}

void ASocketPlayer::ShutdownTransport()
{
	ShutdownUdpSocket();
	ShutdownTcpSocket();
	ShutdownWebSocket();
}

void ASocketPlayer::PollTransport()
{
	switch (TransportType)
	{
	case EMediaPipeTransportType::UDP:
		PollUdpMessages();
		break;
	case EMediaPipeTransportType::TCP:
		PollTcpMessages();
		break;
	case EMediaPipeTransportType::WebSocket:
		if ((!WebSocket.IsValid() || !bWebSocketConnected) && ShouldRetryConnection())
		{
			ShutdownWebSocket();
			InitializeWebSocket();
		}
		break;
	}
}

void ASocketPlayer::InitializeUdpSocket()
{
	if (TransportType != EMediaPipeTransportType::UDP)
	{
		return;
	}

	FIPv4Address Ip;
	if (!FIPv4Address::Parse(ServerHost, Ip))
	{
		Ip = FIPv4Address::Any;
	}

	const FIPv4Endpoint Endpoint(Ip, ServerPort);
	UdpSocket = FUdpSocketBuilder(TEXT("MediaPipeUdpListener"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.WithReceiveBufferSize(2 * 1024 * 1024);

	if (!UdpSocket)
	{
		UpdateDisplayText(TEXT("Failed to bind UDP socket."));
	}
}

void ASocketPlayer::ShutdownUdpSocket()
{
	if (UdpSocket)
	{
		UdpSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(UdpSocket);
		UdpSocket = nullptr;
	}
}

void ASocketPlayer::PollUdpMessages()
{
	if (!UdpSocket)
	{
		return;
	}

	uint32 PendingSize = 0;
	while (UdpSocket->HasPendingData(PendingSize))
	{
		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(FMath::Min(PendingSize, 65507u) + 1);

		int32 BytesRead = 0;
		if (UdpSocket->Recv(Buffer.GetData(), Buffer.Num(), BytesRead) && BytesRead > 0)
		{
			HandlePacket(Buffer, BytesRead);
		}
		else
		{
			break;
		}
	}
}

void ASocketPlayer::InitializeTcpSocket()
{
	if (TransportType != EMediaPipeTransportType::TCP)
	{
		return;
	}

	bTcpConnected = false;
	TcpSocket = FTcpSocketBuilder(TEXT("MediaPipeTcpClient")).AsNonBlocking().AsReusable();
	if (!TcpSocket)
	{
		UpdateDisplayText(TEXT("Failed to create TCP socket."));
		NextReconnectTimeSeconds = FPlatformTime::Seconds() + 1.0;
		return;
	}

	FIPv4Address Ip;
	if (!FIPv4Address::Parse(ServerHost, Ip))
	{
		UpdateDisplayText(TEXT("Invalid TCP host address."));
		NextReconnectTimeSeconds = FPlatformTime::Seconds() + 1.0;
		return;
	}

	const FIPv4Endpoint Endpoint(Ip, ServerPort);
	if (!TcpSocket->Connect(*Endpoint.ToInternetAddr()))
	{
		UpdateDisplayText(TEXT("TCP connection pending. Waiting for server..."));
		NextReconnectTimeSeconds = FPlatformTime::Seconds() + 1.0;
	}
	else
	{
		bTcpConnected = true;
		UpdateDisplayText(TEXT("TCP connected."));
	}
}

void ASocketPlayer::ShutdownTcpSocket()
{
	if (TcpSocket)
	{
		TcpSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(TcpSocket);
		TcpSocket = nullptr;
	}
	bTcpConnected = false;
	TcpPacketBuffer.Reset();
}

void ASocketPlayer::PollTcpMessages()
{
	if (!TcpSocket)
	{
		if (ShouldRetryConnection())
		{
			InitializeTcpSocket();
		}
		return;
	}

	if (TcpSocket->GetConnectionState() != SCS_Connected)
	{
		bTcpConnected = false;
		if (ShouldRetryConnection())
		{
			ShutdownTcpSocket();
			InitializeTcpSocket();
		}
		return;
	}

	if (!bTcpConnected)
	{
		bTcpConnected = true;
		UpdateDisplayText(TEXT("TCP connected."));
	}

	uint32 PendingSize = 0;
	while (TcpSocket->HasPendingData(PendingSize))
	{
		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(FMath::Min(PendingSize, 65507u));
		int32 BytesRead = 0;
		if (TcpSocket->Recv(Buffer.GetData(), Buffer.Num(), BytesRead) && BytesRead > 0)
		{
			TcpPacketBuffer.Append(Buffer.GetData(), BytesRead);
			ParsePacketBuffer(TcpPacketBuffer);
		}
		else
		{
			break;
		}
	}
}

void ASocketPlayer::InitializeWebSocket()
{
	if (TransportType != EMediaPipeTransportType::WebSocket)
	{
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
	{
		FModuleManager::Get().LoadModule(TEXT("WebSockets"));
	}

	const FString Url = FString::Printf(TEXT("ws://%s:%d"), *ServerHost, ServerPort);
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(Url);
	WebSocket->OnConnected().AddLambda([this]()
	{
		bWebSocketConnected = true;
		UpdateDisplayText(TEXT("WebSocket connected."));
	});
	WebSocket->OnConnectionError().AddLambda([this](const FString& Error)
	{
		bWebSocketConnected = false;
		NextReconnectTimeSeconds = FPlatformTime::Seconds() + 1.0;
		UpdateDisplayText(FString::Printf(TEXT("WebSocket error: %s"), *Error));
	});
	WebSocket->OnClosed().AddLambda([this](int32, const FString&, bool)
	{
		bWebSocketConnected = false;
		NextReconnectTimeSeconds = FPlatformTime::Seconds() + 1.0;
		UpdateDisplayText(TEXT("WebSocket closed."));
	});
	WebSocket->OnRawMessage().AddLambda([this](const void* Data, SIZE_T Size, SIZE_T BytesRemaining)
	{
		const uint8* Bytes = static_cast<const uint8*>(Data);
		WebSocketPacketBuffer.Append(Bytes, static_cast<int32>(Size));
		if (BytesRemaining == 0)
		{
			ParsePacketBuffer(WebSocketPacketBuffer);
		}
	});
	bWebSocketConnected = false;
	NextReconnectTimeSeconds = FPlatformTime::Seconds() + 1.0;
	WebSocket->Connect();
}

void ASocketPlayer::ShutdownWebSocket()
{
	if (WebSocket.IsValid())
	{
		WebSocket->Close();
		WebSocket.Reset();
	}
	bWebSocketConnected = false;
	WebSocketPacketBuffer.Reset();
}

void ASocketPlayer::HandlePacket(const TArray<uint8>& PacketData, int32 BytesRead)
{
	if (BytesRead < 12)
	{
		UpdateDisplayText(FString::Printf(TEXT("Invalid packet size: %d bytes"), BytesRead));
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
		UpdateDisplayText(FString::Printf(TEXT("Unexpected identifier: 0x%04X"), Identifier));
		return;
	}

	if (Cmd != GestureCommand)
	{
		UpdateDisplayText(FString::Printf(TEXT("Unexpected cmd: 0x%04X"), Cmd));
		return;
	}

	if (PayloadLength != ExpectedPayloadLength || BytesRead < 8 + static_cast<int32>(PayloadLength))
	{
		UpdateDisplayText(FString::Printf(
			TEXT("Invalid payload length: header=%u bytes, packet=%d bytes"),
			PayloadLength,
			BytesRead
		));
		return;
	}

	const uint32 GestureCode =
		(static_cast<uint32>(Data[8]) << 24) |
		(static_cast<uint32>(Data[9]) << 16) |
		(static_cast<uint32>(Data[10]) << 8) |
		static_cast<uint32>(Data[11]);

	UpdateDisplayText(BuildDisplayText(Identifier, Cmd, PayloadLength, GestureCode));
}

void ASocketPlayer::ParsePacketBuffer(TArray<uint8>& PacketBuffer)
{
	while (PacketBuffer.Num() >= 12)
	{
		const uint8* Data = PacketBuffer.GetData();
		const uint32 PayloadLength =
			(static_cast<uint32>(Data[4]) << 24) |
			(static_cast<uint32>(Data[5]) << 16) |
			(static_cast<uint32>(Data[6]) << 8) |
			static_cast<uint32>(Data[7]);
		const int32 PacketLength = 8 + static_cast<int32>(PayloadLength);
		if (PacketBuffer.Num() < PacketLength)
		{
			return;
		}

		TArray<uint8> Packet;
		Packet.Append(PacketBuffer.GetData(), PacketLength);
		PacketBuffer.RemoveAt(0, PacketLength, false);
		HandlePacket(Packet, PacketLength);
	}
}

void ASocketPlayer::UpdateDisplayText(const FString& NewText)
{
	LastDisplayText = NewText;
	if (LogUI)
	{
		LogUI->SetLogText(LastDisplayText);
	}
}

FString ASocketPlayer::BuildDisplayText(uint16 Identifier, uint16 Cmd, uint32 PayloadLength, uint32 GestureCode) const
{
	return FString::Printf(
		TEXT("MediaPipe %s\nIdentifier: 0x%04X\nCmd: 0x%04X\nPayload Length: %u\nGesture Code: %u\nGesture: %s\nHost: %s\nPort: %d"),
		*GetTransportName(),
		Identifier,
		Cmd,
		PayloadLength,
		GestureCode,
		*GetGestureName(GestureCode),
		*ServerHost,
		ServerPort
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

FString ASocketPlayer::GetTransportName() const
{
	switch (TransportType)
	{
	case EMediaPipeTransportType::UDP:
		return TEXT("UDP");
	case EMediaPipeTransportType::TCP:
		return TEXT("TCP");
	case EMediaPipeTransportType::WebSocket:
		return TEXT("WebSocket");
	default:
		return TEXT("Unknown");
	}
}

FString ASocketPlayer::GetServerScriptPath() const
{
	if (!ServerScriptPath.IsEmpty())
	{
		return ServerScriptPath;
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT("mediapipe_gesture_server.py")));
}

FString ASocketPlayer::GetDefaultPythonPath() const
{
	if (!PythonExecutablePath.IsEmpty())
	{
		return PythonExecutablePath;
	}
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT(".."), TEXT(".venv"), TEXT("Scripts"), TEXT("python.exe")));
}

FString ASocketPlayer::BuildServerArguments() const
{
	FString TransportArg = TEXT("udp");
	if (TransportType == EMediaPipeTransportType::TCP)
	{
		TransportArg = TEXT("tcp");
	}
	else if (TransportType == EMediaPipeTransportType::WebSocket)
	{
		TransportArg = TEXT("websocket");
	}

	FString Args = FString::Printf(
		TEXT("\"%s\" --transport %s --host %s --port %d"),
		*GetServerScriptPath(),
		*TransportArg,
		*ServerHost,
		ServerPort
	);
	if (bShowServerPreview)
	{
		Args += TEXT(" --show-preview");
	}
	return Args;
}

void ASocketPlayer::LaunchMediaPipeServer()
{
	if (MediaPipeServerProcess.IsValid())
	{
		return;
	}

	const FString PythonPath = GetDefaultPythonPath();
	const FString Arguments = BuildServerArguments();
	const FString WorkingDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("..")));

	uint32 ProcessId = 0;
	MediaPipeServerProcess = FPlatformProcess::CreateProc(
		*PythonPath,
		*Arguments,
		true,
		false,
		false,
		&ProcessId,
		0,
		*WorkingDirectory,
		nullptr
	);

	if (!MediaPipeServerProcess.IsValid())
	{
		UpdateDisplayText(TEXT("Failed to launch MediaPipe server process."));
	}
}

void ASocketPlayer::StopMediaPipeServer()
{
	if (MediaPipeServerProcess.IsValid())
	{
		FPlatformProcess::TerminateProc(MediaPipeServerProcess, true);
		FPlatformProcess::CloseProc(MediaPipeServerProcess);
		MediaPipeServerProcess.Reset();
	}
}

bool ASocketPlayer::ShouldRetryConnection() const
{
	return FPlatformTime::Seconds() >= NextReconnectTimeSeconds;
}

