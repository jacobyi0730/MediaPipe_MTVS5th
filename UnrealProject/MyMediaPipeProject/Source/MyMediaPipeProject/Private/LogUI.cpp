// Fill out your copyright notice in the Description page of Project Settings.


#include "MyMediaPipeProject/Public/LogUI.h"
#include "MyMediaPipeProject/Public/SocketPlayer.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void ULogUI::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureRuntimeControls();
}

void ULogUI::EnsureRuntimeControls()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!RootBox)
	{
		RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RuntimeRootBox"));
		WidgetTree->RootWidget = RootBox;
		TextLog = nullptr;
		TransportComboBox = nullptr;
		StartButton = nullptr;
		StartButtonLabel = nullptr;
	}

	if (!TransportComboBox)
	{
		TransportComboBox = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), TEXT("TransportComboBox"));
		TransportComboBox->AddOption(TEXT("UDP"));
		TransportComboBox->AddOption(TEXT("TCP"));
		TransportComboBox->AddOption(TEXT("WebSocket"));
		TransportComboBox->SetSelectedOption(TEXT("UDP"));
		RootBox->AddChildToVerticalBox(TransportComboBox);
	}

	if (!StartButton)
	{
		StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
		StartButton->OnClicked.AddDynamic(this, &ULogUI::HandleStartClicked);
		RootBox->AddChildToVerticalBox(StartButton);
	}

	if (!StartButtonLabel)
	{
		StartButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartButtonLabel"));
		StartButtonLabel->SetText(FText::FromString(TEXT("Start Selected Transport")));
		StartButton->AddChild(StartButtonLabel);
	}

	if (!TextLog)
	{
		TextLog = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TextLog"));
		TextLog->SetText(FText::FromString(TEXT("Select UDP, TCP, or WebSocket and press Start.")));
		RootBox->AddChildToVerticalBox(TextLog);
	}
}

void ULogUI::SetLogText(const FString& NewText)
{
	if (TextLog)
	{
		TextLog->SetText(FText::FromString(NewText));
	}
}

void ULogUI::HandleStartClicked()
{
	if (!SocketPlayer.IsValid() || !TransportComboBox)
	{
		return;
	}

	const FString SelectedTransport = TransportComboBox->GetSelectedOption();
	EMediaPipeTransportType TransportType = EMediaPipeTransportType::UDP;

	if (SelectedTransport == TEXT("TCP"))
	{
		TransportType = EMediaPipeTransportType::TCP;
	}
	else if (SelectedTransport == TEXT("WebSocket"))
	{
		TransportType = EMediaPipeTransportType::WebSocket;
	}

	SocketPlayer->StartMediaPipeWithTransport(TransportType);
}

void ULogUI::SetSocketPlayer(ASocketPlayer* InSocketPlayer)
{
	SocketPlayer = InSocketPlayer;
	EnsureRuntimeControls();
	if (SocketPlayer.IsValid())
	{
		SyncTransportSelection(SocketPlayer->TransportType);
	}
}

void ULogUI::SyncTransportSelection(EMediaPipeTransportType TransportType)
{
	if (!TransportComboBox)
	{
		return;
	}

	switch (TransportType)
	{
	case EMediaPipeTransportType::UDP:
		TransportComboBox->SetSelectedOption(TEXT("UDP"));
		break;
	case EMediaPipeTransportType::TCP:
		TransportComboBox->SetSelectedOption(TEXT("TCP"));
		break;
	case EMediaPipeTransportType::WebSocket:
		TransportComboBox->SetSelectedOption(TEXT("WebSocket"));
		break;
	}
}
