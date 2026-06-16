// Fill out your copyright notice in the Description page of Project Settings.


#include "MyMediaPipeProject/Public/LogUI.h"

#include "Components/TextBlock.h"

void ULogUI::SetLogText(const FString& NewText)
{
	if (TextLog)
	{
		TextLog->SetText(FText::FromString(NewText));
	}
}
