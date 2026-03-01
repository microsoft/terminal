// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AzureLLMProvider.h"
#include "../../types/inc/utils.hpp"
#include "LibraryResources.h"

#include "AzureLLMProvider.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;
namespace WWH = ::winrt::Windows::Web::Http;
namespace WSS = ::winrt::Windows::Storage::Streams;
namespace WDJ = ::winrt::Windows::Data::Json;

static constexpr std::wstring_view acceptedModels[] = {
    L"gpt-35-turbo",
    L"gpt4",
    L"gpt4-32k",
    L"gpt4o",
    L"gpt-35-turbo-16k"
};
static constexpr std::wstring_view acceptedSeverityLevel{ L"safe" };
static constexpr std::wstring_view applicationJson{ L"application/json" };
static constexpr std::wstring_view endpointString{ L"endpoint" };
static constexpr std::wstring_view keyString{ L"key" };
static constexpr std::wstring_view roleString{ L"role" };
static constexpr std::wstring_view contentString{ L"content" };
static constexpr std::wstring_view messageString{ L"message" };
static constexpr std::wstring_view errorString{ L"error" };
static constexpr std::wstring_view severityString{ L"severity" };

static constexpr std::wstring_view expectedScheme{ L"https" };
static constexpr std::wstring_view expectedHostSuffix{ L".openai.azure.com" };

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    void AzureLLMProvider::SetAuthentication(const winrt::hstring& authValues)
    {
        _httpClient = winrt::Windows::Web::Http::HttpClient{};
        _httpClient.DefaultRequestHeaders().Accept().TryParseAdd(applicationJson);

        if (!authValues.empty())
        {
            // Parse out the endpoint and key from the authValues string
            WDJ::JsonObject authValuesObject{ WDJ::JsonObject::Parse(authValues) };
            if (authValuesObject.HasKey(endpointString) && authValuesObject.HasKey(keyString))
            {
                _azureEndpoint = authValuesObject.GetNamedString(endpointString);
                _azureKey = authValuesObject.GetNamedString(keyString);
                _httpClient.DefaultRequestHeaders().Append(L"api-key", _azureKey);
            }
        }
    }

    void AzureLLMProvider::ClearMessageHistory()
    {
        _jsonMessages.Clear();
    }

    void AzureLLMProvider::SetSystemPrompt(const winrt::hstring& systemPrompt)
    {
        WDJ::JsonObject systemMessageObject;
        winrt::hstring systemMessageContent{ systemPrompt };
        systemMessageObject.Insert(roleString, WDJ::JsonValue::CreateStringValue(L"system"));
        systemMessageObject.Insert(contentString, WDJ::JsonValue::CreateStringValue(systemMessageContent));
        _jsonMessages.Append(systemMessageObject);
    }

    void AzureLLMProvider::SetContext(Extension::IContext context)
    {
        _context = std::move(context);
    }

    winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> AzureLLMProvider::GetResponseAsync(const winrt::hstring& userPrompt)
    {
        // Use the ErrorTypes enum to flag whether the response the user receives is an error message
        // we pass this enum back to the caller so they can handle it appropriately (specifically, ExtensionPalette will send the correct telemetry event)
        ErrorTypes errorType{ ErrorTypes::None };
        hstring message{};

        if (_azureEndpoint.empty())
        {
            message = RS_(L"CouldNotFindKeyErrorMessage");
            errorType = ErrorTypes::InvalidAuth;
        }
        else
        {
            // If the AI endpoint is not an azure open AI endpoint, return an error message
            Windows::Foundation::Uri parsedUri{ _azureEndpoint };
            if (parsedUri.SchemeName() != expectedScheme ||
                !til::ends_with(parsedUri.Host(), expectedHostSuffix))
            {
                message = RS_(L"InvalidEndpointMessage");
                errorType = ErrorTypes::InvalidAuth;
            }
        }

        // If we don't have a message string, that means the endpoint exists and matches the regex
        // that we allow - now we can actually make the http request
        if (message.empty())
        {
            // Make a copy of the prompt because we are switching threads
            const auto promptCopy{ userPrompt };

            // Make sure we are on the background thread for the http request
            co_await winrt::resume_background();

            WWH::HttpRequestMessage request{ WWH::HttpMethod::Post(), Uri{ _azureEndpoint } };
            request.Headers().Accept().TryParseAdd(applicationJson);

            WDJ::JsonObject jsonContent;
            WDJ::JsonObject messageObject;

            // _ActiveCommandline should be set already, we request for it the moment we become visible
            winrt::hstring engineeredPrompt{ promptCopy };
            if (_context && !_context.ActiveCommandline().empty())
            {
                engineeredPrompt = promptCopy + L". The shell I am running is " + _context.ActiveCommandline();
            }
            messageObject.Insert(roleString, WDJ::JsonValue::CreateStringValue(L"user"));
            messageObject.Insert(contentString, WDJ::JsonValue::CreateStringValue(engineeredPrompt));
            _jsonMessages.Append(messageObject);
            jsonContent.SetNamedValue(L"messages", _jsonMessages);
            jsonContent.SetNamedValue(L"max_tokens", WDJ::JsonValue::CreateNumberValue(800));
            jsonContent.SetNamedValue(L"temperature", WDJ::JsonValue::CreateNumberValue(0.7));
            jsonContent.SetNamedValue(L"frequency_penalty", WDJ::JsonValue::CreateNumberValue(0));
            jsonContent.SetNamedValue(L"presence_penalty", WDJ::JsonValue::CreateNumberValue(0));
            jsonContent.SetNamedValue(L"top_p", WDJ::JsonValue::CreateNumberValue(0.95));
            jsonContent.SetNamedValue(L"stop", WDJ::JsonValue::CreateStringValue(L"None"));
            const auto stringContent = jsonContent.ToString();
            WWH::HttpStringContent requestContent{
                stringContent,
                WSS::UnicodeEncoding::Utf8,
                L"application/json"
            };

            request.Content(requestContent);

            // Send the request
            try
            {
                const auto sendRequestOperation = _httpClient.SendRequestAsync(request);

                // if the caller cancels this operation, make sure to cancel the http request as well
                auto cancellationToken{ co_await winrt::get_cancellation_token() };
                cancellationToken.callback([sendRequestOperation] {
                    sendRequestOperation.Cancel();
                });

                if (sendRequestOperation.wait_for(std::chrono::seconds(5)) == AsyncStatus::Completed)
                {
                    // Parse out the suggestion from the response
                    const auto response = sendRequestOperation.GetResults();
                    const auto string{ co_await response.Content().ReadAsStringAsync() };
                    const auto jsonResult{ WDJ::JsonObject::Parse(string) };
                    if (jsonResult.HasKey(errorString))
                    {
                        const auto errorObject = jsonResult.GetNamedObject(errorString);
                        message = errorObject.GetNamedString(messageString);
                        errorType = ErrorTypes::FromProvider;
                    }
                    else
                    {
                        if (_verifyModelIsValidHelper(jsonResult))
                        {
                            const auto choices = jsonResult.GetNamedArray(L"choices");
                            const auto firstChoice = choices.GetAt(0).GetObject();
                            const auto messageObject = firstChoice.GetNamedObject(messageString);
                            message = messageObject.GetNamedString(contentString);
                        }
                        else
                        {
                            message = RS_(L"InvalidModelMessage");
                            errorType = ErrorTypes::InvalidModel;
                        }
                    }
                }
                else
                {
                    // if the http request takes too long, cancel the http request and return an error
                    sendRequestOperation.Cancel();
                    message = RS_(L"UnknownErrorMessage");
                    errorType = ErrorTypes::Unknown;
                }
            }
            catch (...)
            {
                message = RS_(L"UnknownErrorMessage");
                errorType = ErrorTypes::Unknown;
            }
        }

        // Also make a new entry in our jsonMessages list, so the AI knows the full conversation so far
        WDJ::JsonObject responseMessageObject;
        responseMessageObject.Insert(roleString, WDJ::JsonValue::CreateStringValue(L"assistant"));
        responseMessageObject.Insert(contentString, WDJ::JsonValue::CreateStringValue(message));
        _jsonMessages.Append(responseMessageObject);

        co_return winrt::make<AzureResponse>(message, errorType, winrt::hstring{});
    }

    bool AzureLLMProvider::_verifyModelIsValidHelper(const WDJ::JsonObject jsonResponse)
    {
        const auto model = jsonResponse.GetNamedString(L"model");
        bool modelIsAccepted{ false };
        for (const auto acceptedModel : acceptedModels)
        {
            if (model == acceptedModel)
            {
                modelIsAccepted = true;
            }
            break;
        }
        if (!modelIsAccepted)
        {
            return false;
        }
        WDJ::JsonObject contentFiltersObject;
        // For some reason, sometimes the content filter results are in a key called "prompt_filter_results"
        // and sometimes they are in a key called "prompt_annotations". Check for either.
        if (jsonResponse.HasKey(L"prompt_filter_results"))
        {
            contentFiltersObject = jsonResponse.GetNamedArray(L"prompt_filter_results").GetObjectAt(0);
        }
        else if (jsonResponse.HasKey(L"prompt_annotations"))
        {
            contentFiltersObject = jsonResponse.GetNamedArray(L"prompt_annotations").GetObjectAt(0);
        }
        else
        {
            return false;
        }
        const auto contentFilters = contentFiltersObject.GetNamedObject(L"content_filter_results");
        if (Feature_TerminalChatJailbreakFilter::IsEnabled() && !contentFilters.HasKey(L"jailbreak"))
        {
            return false;
        }
        for (const auto filterPair : contentFilters)
        {
            const auto filterLevel = filterPair.Value().GetObjectW();
            if (filterLevel.HasKey(severityString))
            {
                if (filterLevel.GetNamedString(severityString) != acceptedSeverityLevel)
                {
                    return false;
                }
            }
        }
        return true;
    }
}
