// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenAIHelper.generated.h"

/**
 * 
 */
UCLASS()
class GPT_PRACTICE_API UOpenAIHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

private:
    static FString StoredAIName;
    static FString StoredAIPersonality;
    static FString StoredAIUniquePersonality;
    static FString StoredVoiceID;
    static TArray<TSharedPtr<FJsonValue>> AiMemory;
    static TArray<TSharedPtr<FJsonObject>> MessageHistory;
    static FString StoredMessage;

public:
    UFUNCTION(BlueprintCallable, Category = "Build")
    static void BuildNPC(const FString& AIName, const FString& AIPersonality, const FString& AIUniquePersonality, const FString& VoiceID);

    UFUNCTION(BlueprintCallable, Category = "Prompt")
    static void TalkToNPC(const FString& Prompt);

    UFUNCTION(BlueprintCallable, Category = "Voice")
    static void RequestElevenLabsTTS(const FString& TextToSpeak);

    static void PrintMessageHistory();

    UFUNCTION(BlueprintCallable, Category = "Output")
    static FString GetStoredMessage();

    UFUNCTION(BlueprintCallable, Category="File")
    static int64 GetFileSize(const FString& FilePath)
    {
        if (IFileManager::Get().FileExists(*FilePath))
        {
            return IFileManager::Get().FileSize(*FilePath);
        }
        return -1;
    }
};
