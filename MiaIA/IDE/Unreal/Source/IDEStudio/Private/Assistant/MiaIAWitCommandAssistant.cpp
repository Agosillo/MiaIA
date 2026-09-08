#include "Assistant/MiaIAWitCommandAssistant.h"

#if MIAIA_WITH_WIT_AI

#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <utility>

namespace
{
    constexpr TCHAR WitApiVersion[] = TEXT("20260908");

    std::string Utf8(const FString& Value)
    {
        return TCHAR_TO_UTF8(*Value);
    }

    FString EntityName(const FString& Key)
    {
        int32 separator{};
        return Key.FindChar(TEXT(':'), separator)
            ? Key.Left(separator)
            : Key;
    }

    bool TryEntityValue(
        const TSharedPtr<FJsonObject>& Object,
        FString& Value)
    {
        if (!Object.IsValid())
        {
            return false;
        }

        if (Object->TryGetStringField(TEXT("value"), Value))
        {
            return !Value.IsEmpty();
        }

        double number{};

        if (Object->TryGetNumberField(TEXT("value"), number))
        {
            Value = FString::SanitizeFloat(number, 17);
            return true;
        }

        return false;
    }

    MiaIA::Studio::CommandAssistantUnderstanding ParseUnderstanding(
        const FString& RequestedText,
        const FString& Json)
    {
        MiaIA::Studio::CommandAssistantUnderstanding result;
        result.Text = Utf8(RequestedText);

        TSharedPtr<FJsonObject> root;
        const TSharedRef<TJsonReader<>> reader =
            TJsonReaderFactory<>::Create(Json);

        if (!FJsonSerializer::Deserialize(reader, root) ||
            !root.IsValid())
        {
            result.Error = "Wit.ai returned invalid JSON.";
            return result;
        }

        FString providerError;
        if (root->TryGetStringField(TEXT("error"), providerError) &&
            !providerError.IsEmpty())
        {
            result.Error = "Wit.ai: " + Utf8(providerError);
            return result;
        }

        FString recognizedText;
        if (root->TryGetStringField(TEXT("text"), recognizedText) &&
            !recognizedText.IsEmpty())
        {
            result.Text = Utf8(recognizedText);
        }

        const TArray<TSharedPtr<FJsonValue>>* intents{};
        if (root->TryGetArrayField(TEXT("intents"), intents) &&
            intents != nullptr && !intents->IsEmpty() &&
            (*intents)[0].IsValid() &&
            (*intents)[0]->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> intent =
                (*intents)[0]->AsObject();
            FString name;
            double confidence{};

            if (intent.IsValid() &&
                intent->TryGetStringField(TEXT("name"), name) &&
                intent->TryGetNumberField(
                    TEXT("confidence"),
                    confidence))
            {
                result.Intent = Utf8(name);
                result.Confidence = confidence;
            }
        }

        if (result.Intent.empty())
        {
            result.Error =
                "Wit.ai returned no intent. Verify that the selected "
                "language uses the Client Access Token of the trained app.";
            return result;
        }

        const TSharedPtr<FJsonObject>* entities{};
        if (root->TryGetObjectField(TEXT("entities"), entities) &&
            entities != nullptr && entities->IsValid())
        {
            for (const auto& pair : (*entities)->Values)
            {
                if (!pair.Value.IsValid() ||
                    pair.Value->Type != EJson::Array)
                {
                    continue;
                }

                const TArray<TSharedPtr<FJsonValue>>& values =
                    pair.Value->AsArray();

                for (const TSharedPtr<FJsonValue>& value : values)
                {
                    if (!value.IsValid() || value->Type != EJson::Object)
                    {
                        continue;
                    }

                    const TSharedPtr<FJsonObject> object = value->AsObject();
                    FString entityValue;

                    if (!TryEntityValue(object, entityValue))
                    {
                        continue;
                    }

                    FString role;
                    object->TryGetStringField(TEXT("role"), role);
                    double confidence{};
                    object->TryGetNumberField(
                        TEXT("confidence"),
                        confidence);

                    result.Entities.push_back({
                        Utf8(EntityName(FString(*pair.Key))),
                        Utf8(role),
                        Utf8(entityValue),
                        confidence
                    });
                }
            }
        }

        return result;
    }
}

FMiaIAWitCommandAssistant::FMiaIAWitCommandAssistant(
    FString InClientAccessToken,
    FString InMissingConfigurationMessage)
    : Token(std::move(InClientAccessToken)),
      MissingConfigurationMessage(
          std::move(InMissingConfigurationMessage))
{
    Token.TrimStartAndEndInline();
}

bool FMiaIAWitCommandAssistant::IsAvailable() const
{
    return !Token.IsEmpty();
}

std::string FMiaIAWitCommandAssistant::AvailabilityMessage() const
{
    return IsAvailable()
        ? "Wit.ai online assistant is ready."
        : Utf8(MissingConfigurationMessage);
}

bool FMiaIAWitCommandAssistant::Interpret(
    const std::string& Text,
    MiaIA::Studio::CommandAssistantCompletion Completion)
{
    if (!IsAvailable() || Text.empty() || !Completion)
    {
        return false;
    }

    const FString requestedText = UTF8_TO_TCHAR(Text.c_str());
    const FString endpoint = FString::Printf(
        TEXT("https://api.wit.ai/message?v=%s&q=%s"),
        WitApiVersion,
        *FGenericPlatformHttp::UrlEncode(requestedText));

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> request =
        FHttpModule::Get().CreateRequest();
    request->SetURL(endpoint);
    request->SetVerb(TEXT("GET"));
    request->SetHeader(
        TEXT("Authorization"),
        TEXT("Bearer ") + Token);
    request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    request->OnProcessRequestComplete().BindLambda(
        [requestedText, completion = std::move(Completion)](
            FHttpRequestPtr,
            FHttpResponsePtr response,
            bool connectedSuccessfully) mutable
        {
            MiaIA::Studio::CommandAssistantUnderstanding understanding;
            understanding.Text = Utf8(requestedText);

            if (!connectedSuccessfully || !response.IsValid())
            {
                understanding.Error =
                    "The Wit.ai request could not reach the service.";
                completion(std::move(understanding));
                return;
            }

            const int32 statusCode = response->GetResponseCode();
            if (statusCode < 200 || statusCode >= 300)
            {
                FString providerMessage;
                TSharedPtr<FJsonObject> errorObject;
                const TSharedRef<TJsonReader<>> errorReader =
                    TJsonReaderFactory<>::Create(
                        response->GetContentAsString());

                if (FJsonSerializer::Deserialize(
                        errorReader,
                        errorObject) &&
                    errorObject.IsValid())
                {
                    errorObject->TryGetStringField(
                        TEXT("error"),
                        providerMessage);
                }

                understanding.Error = "Wit.ai returned HTTP " +
                    std::to_string(statusCode);

                if (!providerMessage.IsEmpty())
                {
                    understanding.Error += ": " +
                        Utf8(providerMessage);
                }

                understanding.Error += '.';
                completion(std::move(understanding));
                return;
            }

            completion(ParseUnderstanding(
                requestedText,
                response->GetContentAsString()));
        });

    if (!request->ProcessRequest())
    {
        return false;
    }

    return true;
}

#endif
