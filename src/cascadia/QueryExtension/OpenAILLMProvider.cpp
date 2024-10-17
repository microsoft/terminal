// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "OpenAILLMProvider.h"
#include "../../types/inc/utils.hpp"
#include "LibraryResources.h"

#include "OpenAILLMProvider.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;
namespace WWH = ::winrt::Windows::Web::Http;
namespace WSS = ::winrt::Windows::Storage::Streams;
namespace WDJ = ::winrt::Windows::Data::Json;

static constexpr std::wstring_view applicationJson{ L"application/json" };
static constexpr std::wstring_view acceptedModel{ L"gpt-3.5-turbo" };
static constexpr std::wstring_view openAIEndpoint{ L"https://api.openai.com/v1/chat/completions" };

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    void OpenAILLMProvider::SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues)
    {
        _AIKey = unbox_value_or<hstring>(authValues.TryLookup(L"key").try_as<IPropertyValue>(), L"");
        _httpClient = winrt::Windows::Web::Http::HttpClient{};
        _httpClient.DefaultRequestHeaders().Accept().TryParseAdd(applicationJson);
        _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ L"Bearer", _AIKey });
    }

    void OpenAILLMProvider::ClearMessageHistory()
    {
        _jsonMessages.Clear();
    }

    void OpenAILLMProvider::SetSystemPrompt(const winrt::hstring& systemPrompt)
    {
        WDJ::JsonObject systemMessageObject;
        winrt::hstring systemMessageContent{ systemPrompt };
        systemMessageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"system"));
        systemMessageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(systemMessageContent));
        _jsonMessages.Append(systemMessageObject);
    }

    void OpenAILLMProvider::SetContext(const Extension::IContext context)
    {
        _context = context;
    }

    winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> OpenAILLMProvider::GetResponseAsync(const winrt::hstring userPrompt)
    {
        // Use the ErrorTypes enum to flag whether the response the user receives is an error message
        // we pass this enum back to the caller so they can handle it appropriately (specifically, ExtensionPalette will send the correct telemetry event)
        ErrorTypes errorType{ ErrorTypes::None };
        hstring message{};

        // Make sure we are on the background thread for the http request
        co_await winrt::resume_background();

        WWH::HttpRequestMessage request{ WWH::HttpMethod::Post(), Uri{ openAIEndpoint } };
        request.Headers().Accept().TryParseAdd(applicationJson);

        WDJ::JsonObject jsonContent;
        WDJ::JsonObject messageObject;

        winrt::hstring engineeredPrompt{ userPrompt };
        if (_context && !_context.ActiveCommandline().empty())
        {
            engineeredPrompt = userPrompt + L". The shell I am running is " + _context.ActiveCommandline();
        }
        messageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"user"));
        messageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(engineeredPrompt));
        _jsonMessages.Append(messageObject);
        jsonContent.SetNamedValue(L"model", WDJ::JsonValue::CreateStringValue(acceptedModel));
        jsonContent.SetNamedValue(L"messages", _jsonMessages);
        jsonContent.SetNamedValue(L"temperature", WDJ::JsonValue::CreateNumberValue(0));
        const auto stringContent = jsonContent.ToString();
        WWH::HttpStringContent requestContent{
            stringContent,
            WSS::UnicodeEncoding::Utf8,
            applicationJson
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
                errorType = ErrorTypes::FromProvider;
            }
            else
            {
                const auto choices = jsonResult.GetNamedArray(L"choices");
                const auto firstChoice = choices.GetAt(0).GetObject();
                const auto messageObject = firstChoice.GetNamedObject(L"message");
                message = messageObject.GetNamedString(L"content");
            }
        }
        catch (...)
        {
            message = RS_(L"UnknownErrorMessage");
            errorType = ErrorTypes::Unknown;
        }

        // Also make a new entry in our jsonMessages list, so the AI knows the full conversation so far
        WDJ::JsonObject responseMessageObject;
        responseMessageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"assistant"));
        responseMessageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(message));
        _jsonMessages.Append(responseMessageObject);

        co_return winrt::make<OpenAIResponse>(message, errorType);
    }
}
