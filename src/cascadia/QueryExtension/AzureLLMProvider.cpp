// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AzureLLMProvider.h"
#include "../../types/inc/utils.hpp"
#include "LibraryResources.h"

#include "AzureLLMProvider.g.cpp"
#include "AzureResponse.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;
namespace WWH = ::winrt::Windows::Web::Http;
namespace WSS = ::winrt::Windows::Storage::Streams;
namespace WDJ = ::winrt::Windows::Data::Json;

static constexpr std::wstring_view acceptedModel{ L"gpt-35-turbo" };
static constexpr std::wstring_view acceptedSeverityLevel{ L"safe" };

const std::wregex azureOpenAIEndpointRegex{ LR"(^https.*openai\.azure\.com)" };

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    AzureLLMProvider::AzureLLMProvider(winrt::hstring endpoint, winrt::hstring key)
    {
        _AIEndpoint = endpoint;
        _AIKey = key;
        _httpClient = winrt::Windows::Web::Http::HttpClient{};
        _httpClient.DefaultRequestHeaders().Accept().TryParseAdd(L"application/json");
        _httpClient.DefaultRequestHeaders().Append(L"api-key", _AIKey);
    }

    void AzureLLMProvider::ClearMessageHistory()
    {
        _jsonMessages.Clear();
    }

    void AzureLLMProvider::SetSystemPrompt(const winrt::hstring& systemPrompt)
    {
        WDJ::JsonObject systemMessageObject;
        winrt::hstring systemMessageContent{ systemPrompt };
        systemMessageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"system"));
        systemMessageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(systemMessageContent));
        _jsonMessages.Append(systemMessageObject);
    }

    void AzureLLMProvider::SetContext(Extension::IContext context)
    {
        _context = context;
    }

    winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> AzureLLMProvider::GetResponseAsync(const winrt::hstring& userPrompt)
    {
        // Use a flag for whether the response the user receives is an error message
        // we pass this flag back to the caller so they can handle it appropriately (specifically, ExtensionPalette will send the correct telemetry event)
        // there is only one case downstream from here that sets this flag to false, so start with it being true
        bool isError{ true };
        hstring message{};

        // If the AI endpoint is not an azure open AI endpoint, return an error message
        if (!std::regex_search(_AIEndpoint.c_str(), azureOpenAIEndpointRegex))
        {
            message = RS_(L"InvalidEndpointMessage");
        }

        // If we don't have a message string, that means the endpoint exists and matches the regex
        // that we allow - now we can actually make the http request
        if (message.empty())
        {
            // Make a copy of the prompt because we are switching threads
            const auto promptCopy{ userPrompt };

            // Make sure we are on the background thread for the http request
            co_await winrt::resume_background();

            WWH::HttpRequestMessage request{ WWH::HttpMethod::Post(), Uri{ _AIEndpoint } };
            request.Headers().Accept().TryParseAdd(L"application/json");

            WDJ::JsonObject jsonContent;
            WDJ::JsonObject messageObject;

            // _ActiveCommandline should be set already, we request for it the moment we become visible
            winrt::hstring engineeredPrompt{ promptCopy };
            if (_context && !_context.ActiveCommandline().empty())
            {
                engineeredPrompt = promptCopy + L". The shell I am running is " + _context.ActiveCommandline();
            }
            messageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"user"));
            messageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(engineeredPrompt));
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
                const auto response = _httpClient.SendRequestAsync(request).get();
                // Parse out the suggestion from the response
                const auto string{ response.Content().ReadAsStringAsync().get() };
                const auto jsonResult{ WDJ::JsonObject::Parse(string) };
                if (jsonResult.HasKey(L"error"))
                {
                    const auto errorObject = jsonResult.GetNamedObject(L"error");
                    message = errorObject.GetNamedString(L"message");
                }
                else
                {
                    if (_verifyModelIsValidHelper(jsonResult))
                    {
                        const auto choices = jsonResult.GetNamedArray(L"choices");
                        const auto firstChoice = choices.GetAt(0).GetObject();
                        const auto messageObject = firstChoice.GetNamedObject(L"message");
                        message = messageObject.GetNamedString(L"content");
                        isError = false;
                    }
                    else
                    {
                        message = RS_(L"InvalidModelMessage");
                    }
                }
            }
            catch (...)
            {
                message = RS_(L"UnknownErrorMessage");
            }
        }

        // Also make a new entry in our jsonMessages list, so the AI knows the full conversation so far
        WDJ::JsonObject responseMessageObject;
        responseMessageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"assistant"));
        responseMessageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(message));
        _jsonMessages.Append(responseMessageObject);

        co_return winrt::make<AzureResponse>(message, isError);
    }

    bool AzureLLMProvider::_verifyModelIsValidHelper(const WDJ::JsonObject jsonResponse)
    {
        if (jsonResponse.GetNamedString(L"model") != acceptedModel)
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
            if (filterLevel.HasKey(L"severity"))
            {
                if (filterLevel.GetNamedString(L"severity") != acceptedSeverityLevel)
                {
                    return false;
                }
            }
        }
        return true;
    }
}
