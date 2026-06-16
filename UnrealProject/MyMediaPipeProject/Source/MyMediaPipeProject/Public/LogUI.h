// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LogUI.generated.h"

class ASocketPlayer;
enum class EMediaPipeTransportType : uint8;

/**
 * 
 */
UCLASS()
class MYMEDIAPIPEPROJECT_API ULogUI : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UTextBlock> TextLog;

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION(BlueprintCallable)
	void SetLogText(const FString& NewText);

	void SetSocketPlayer(class ASocketPlayer* InSocketPlayer);
	void SyncTransportSelection(EMediaPipeTransportType TransportType);

private:
	void EnsureRuntimeControls();

	UPROPERTY()
	TObjectPtr<class UVerticalBox> RootBox;

	UPROPERTY()
	TObjectPtr<class UComboBoxString> TransportComboBox;

	UPROPERTY()
	TObjectPtr<class UButton> StartButton;

	UPROPERTY()
	TObjectPtr<class UTextBlock> StartButtonLabel;

	UPROPERTY()
	TWeakObjectPtr<class ASocketPlayer> SocketPlayer;
};
