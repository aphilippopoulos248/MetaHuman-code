// Fill out your copyright notice in the Description page of Project Settings.

#include "OpenAIHelper.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWaveProcedural.h"
#include "Kismet/GameplayStatics.h"
#include "DSP/BufferVectorOperations.h"
#include "AudioDevice.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "AudioDecompress.h"

#include "Sound/AudioSettings.h"
#include "Runtime/Engine/Classes/Sound/SoundWaveProcedural.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"

FString UOpenAIHelper::StoredAIName = "";
FString UOpenAIHelper::StoredAIPersonality = "";
FString UOpenAIHelper::StoredAIUniquePersonality = "";
FString UOpenAIHelper::StoredVoiceID = "";
TArray<TSharedPtr<FJsonValue>> UOpenAIHelper::AiMemory;
TArray<TSharedPtr<FJsonObject>> UOpenAIHelper::MessageHistory;
FString UOpenAIHelper::StoredMessage = "";

// A constructor that configures the NPC's name, personality, and voice
void UOpenAIHelper::BuildNPC(const FString &AIName, const FString &AIPersonality, const FString &AIUniquePersonality, const FString &VoiceID)
{
    StoredAIName = AIName;
    StoredAIPersonality = AIPersonality;
    StoredAIUniquePersonality = AIUniquePersonality;
    StoredVoiceID = VoiceID;

    // Clear previous conversation and insert new system prompt
    AiMemory.Empty();

    TSharedPtr<FJsonObject> SystemMsg = MakeShareable(new FJsonObject);
    SystemMsg->SetStringField("role", "system");
    SystemMsg->SetStringField("content",
        FString("You are a human named ") + AIName + TEXT(". ") + AIPersonality + TEXT(". ") + AIUniquePersonality);
    AiMemory.Add(MakeShareable(new FJsonValueObject(SystemMsg)));
}

// Function that generates GPT responses using the ChatGPT API
void UOpenAIHelper::TalkToNPC(const FString &Prompt)
{
    if (Prompt == "clear")
    {
        MessageHistory.Empty();
    }

    // Using OpenAI API key to access API (add your ChatGPT API key in the TEXT bracket)
    FString OpenAIKey = TEXT("YOUR_CHATGPT_KEY");

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

    Request->SetURL("https://api.openai.com/v1/chat/completions");
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("Authorization", FString("Bearer ") + OpenAIKey);

    // Prepare new AiMemory for this request
    TArray<TSharedPtr<FJsonValue>> RequestMessages;

    // Add system message (build or cache it)
    RequestMessages.Add(AiMemory[0]);

    // Add previous conversation history
    for (const TSharedPtr<FJsonObject>& Pair : MessageHistory)
    {
        // User message
        TSharedPtr<FJsonObject> UserMsg = MakeShareable(new FJsonObject);
        UserMsg->SetStringField("role", "user");
        UserMsg->SetStringField("content", Pair->GetStringField("user"));
        RequestMessages.Add(MakeShareable(new FJsonValueObject(UserMsg)));

        // Assistant message
        TSharedPtr<FJsonObject> AssistantMsg = MakeShareable(new FJsonObject);
        AssistantMsg->SetStringField("role", "assistant");
        AssistantMsg->SetStringField("content", Pair->GetStringField("assistant"));
        RequestMessages.Add(MakeShareable(new FJsonValueObject(AssistantMsg)));
    }

    // Add current user prompt as last message
    TSharedPtr<FJsonObject> CurrentUserMsg = MakeShareable(new FJsonObject);
    CurrentUserMsg->SetStringField("role", "user");
    CurrentUserMsg->SetStringField("content", Prompt);
    RequestMessages.Add(MakeShareable(new FJsonValueObject(CurrentUserMsg)));

    // Create JSON payload
    TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
    Json->SetStringField("model", "gpt-3.5-turbo");
    Json->SetArrayField("messages", RequestMessages);

    FString Body;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);
    Request->SetContentAsString(Body);

    // Capture CurrentPair for lambda (this allows for storing message history)
    TSharedPtr<FJsonObject> CurrentPair = MakeShareable(new FJsonObject);
    CurrentPair->SetStringField("user", Prompt);

    // Send request
    Request->OnProcessRequestComplete().BindLambda([CurrentPair](FHttpRequestPtr RequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (bWasSuccessful && Response->GetResponseCode() == 200)
        {
            TSharedPtr<FJsonObject> JsonResponse;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
            if (FJsonSerializer::Deserialize(Reader, JsonResponse))
            {
                const auto Choices = JsonResponse->GetArrayField("choices");
                const auto Message = Choices[0]->AsObject()->GetObjectField("message")->GetStringField("content");

                // Add assistant reply to message history pair
                CurrentPair->SetStringField("assistant", Message);
                MessageHistory.Add(CurrentPair);

                UE_LOG(LogTemp, Warning, TEXT("%s Says: %s"), *StoredAIName, *Message);

                StoredMessage = Message;

                RequestElevenLabsTTS(StoredMessage);

                //PrintMessageHistory();
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("OpenAI API Request Failed: %s"), *Response->GetContentAsString());
        }
    });

    Request->ProcessRequest();
}

// Function to convert GPT text to voice speech for MetaHuman
void UOpenAIHelper::RequestElevenLabsTTS(const FString& TextToSpeak)
{
    UE_LOG(LogTemp, Error, TEXT("Eleven Labs TTS"));
    // New HTTP request setup just for ElevenLabs (Add your ElevenLabs API key in the TEXT bracket)
    FString ElevenLabsKey = TEXT("YOUR_ELEVENLABS_KEY");
    FString VoiceID = StoredVoiceID;

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();

    Request->SetURL(FString::Printf(TEXT("https://api.elevenlabs.io/v1/text-to-speech/%s"), *VoiceID));
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("xi-api-key", ElevenLabsKey);

    // JSON body with text to synthesize
    TSharedPtr<FJsonObject> JsonBody = MakeShareable(new FJsonObject);
    JsonBody->SetStringField("text", TextToSpeak);
    JsonBody->SetStringField("format", "mp3");
    // JsonBody->SetStringField("format", "ogg");
    // JsonBody->SetStringField("format", "wav");

    FString Body;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(JsonBody.ToSharedRef(), Writer);
    Request->SetContentAsString(Body);

    Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bSuccess)
    {
        if (bSuccess && Resp->GetResponseCode() == 200)
        {
            // Get raw audio bytes (usually WAV or MP3)
            TArray<uint8> AudioData = Resp->GetContent();

            // Save to disk - example path:
            FString FilePath = FPaths::ProjectContentDir() / TEXT("Voices/ElevenLabsOutput.mp3");

            // Delete old file if it exists
            if (FPaths::FileExists(FilePath))
            {
                IFileManager::Get().Delete(*FilePath);
            }

            bool bSaved = FFileHelper::SaveArrayToFile(AudioData, *FilePath);
            if (bSaved)
            {
                UE_LOG(LogTemp, Warning, TEXT("Audio saved to %s"), *FilePath);

            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to save audio file."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ElevenLabs TTS request failed: %s"), *Resp->GetContentAsString());
        }
            });

    Request->ProcessRequest();
}

// Debugging messages
void UOpenAIHelper::PrintMessageHistory()
{
    UE_LOG(LogTemp, Warning, TEXT("----- MessageHistory Contents -----"));
    for (int32 i = 0; i < MessageHistory.Num(); ++i)
    {
        const TSharedPtr<FJsonObject>& Pair = MessageHistory[i];
        FString User = Pair->GetStringField("user");
        FString Assistant = Pair->GetStringField("assistant");

        UE_LOG(LogTemp, Warning, TEXT("[%d] User: %s | Assistant: %s"), i, *User, *Assistant);
    }
    UE_LOG(LogTemp, Warning, TEXT("----------------------------"));
}

FString UOpenAIHelper::GetStoredMessage()
{
    return StoredMessage;
}
