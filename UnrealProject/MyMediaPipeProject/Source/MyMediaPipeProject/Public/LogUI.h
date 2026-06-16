// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LogUI.generated.h"

/**
 * 
 */
UCLASS()
class MYMEDIAPIPEPROJECT_API ULogUI : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UTextBlock> TextLog;

	UFUNCTION(BlueprintCallable)
	void SetLogText(const FString& NewText);
};
