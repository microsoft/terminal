// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GithubCopilotLLMProvider.h"
#include "../../types/inc/utils.hpp"
#include "LibraryResources.h"

#include "GithubCopilotLLMProvider.g.cpp"
#include "GithubCopilotResponse.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;
namespace WWH = ::winrt::Windows::Web::Http;
namespace WSS = ::winrt::Windows::Storage::Streams;
namespace WDJ = ::winrt::Windows::Data::Json;

static constexpr std::wstring_view acceptedModel{ L"gpt-3.5-turbo" };

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    GithubCopilotLLMProvider::GithubCopilotLLMProvider(const winrt::hstring& authToken, const winrt::hstring& refreshToken)
    {
        _authToken = authToken;
        _refreshToken = refreshToken;
        _httpClient = winrt::Windows::Web::Http::HttpClient{};
        _httpClient.DefaultRequestHeaders().Accept().TryParseAdd(L"application/json");
        _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ L"Bearer", _authToken });
        _httpClient.DefaultRequestHeaders().Append(L"Copilot-Integration-Id", L"windows-terminal-chat");
    }

    void GithubCopilotLLMProvider::ClearMessageHistory()
    {
        _jsonMessages.Clear();
    }

    void GithubCopilotLLMProvider::SetSystemPrompt(const winrt::hstring& systemPrompt)
    {
        WDJ::JsonObject systemMessageObject;
        winrt::hstring systemMessageContent{ systemPrompt };
        systemMessageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"system"));
        systemMessageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(systemMessageContent));
        _jsonMessages.Append(systemMessageObject);
        //winrt::hstring uri{ fmt::format(L"https://github.com/login/device/code") };
        //WWH::HttpFormUrlEncodedContent content{
        //    std::unordered_map<winrt::hstring, winrt::hstring>{
        //        { winrt::hstring{ L"client_id" }, winrt::hstring{ L"Iv1.b0870d058e4473a1" } }
        //    }
        //};
        //_SendRequestReturningJson(uri, content);
    }

    void GithubCopilotLLMProvider::SetContext(const Extension::IContext context)
    {
        _context = context;
    }

    winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GithubCopilotLLMProvider::GetResponseAsync(const winrt::hstring& userPrompt)
    {
        // Use a flag for whether the response the user receives is an error message
        // we pass this flag back to the caller so they can handle it appropriately (specifically, ExtensionPalette will send the correct telemetry event)
        // there is only one case downstream from here that sets this flag to false, so start with it being true
        bool isError{ true };
        hstring message{};
        bool refreshAttempted{ false };

        // Make a copy of the prompt because we are switching threads
        const auto promptCopy{ userPrompt };

        // Make sure we are on the background thread for the http request
        co_await winrt::resume_background();

        WWH::HttpRequestMessage request{ WWH::HttpMethod::Post(), Uri{ L"https://api.githubcopilot.com/chat/completions" } };
        request.Headers().Accept().TryParseAdd(L"application/json");

        WDJ::JsonObject jsonContent;
        WDJ::JsonObject messageObject;

        winrt::hstring engineeredPrompt{ promptCopy };
        if (_context && !_context.ActiveCommandline().empty())
        {
            //engineeredPrompt = promptCopy + L". The shell I am running is " + _context.ActiveCommandline();
            engineeredPrompt = promptCopy;
        }
        messageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"user"));
        messageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(engineeredPrompt));
        _jsonMessages.Append(messageObject);
        jsonContent.SetNamedValue(L"messages", _jsonMessages);
        const auto stringContent = jsonContent.ToString();
        WWH::HttpStringContent requestContent{
            stringContent,
            WSS::UnicodeEncoding::Utf8,
            L"application/json"
        };

        request.Content(requestContent);

        // Send the request
        do
        {
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
                    const auto choices = jsonResult.GetNamedArray(L"choices");
                    const auto firstChoice = choices.GetAt(0).GetObject();
                    const auto messageObject = firstChoice.GetNamedObject(L"message");
                    message = messageObject.GetNamedString(L"content");
                    isError = false;
                }
            }
            catch (...)
            {
                // unknown failure, if we have already attempted a refresh report failure
                // otherwise, try refreshing the auth token
                if (refreshAttempted)
                {
                    message = RS_(L"UnknownErrorMessage");
                    break;
                }
                else
                {
                    _refreshAuthTokens();
                    refreshAttempted = true;
                }
            }
        } while (refreshAttempted);

        // Also make a new entry in our jsonMessages list, so the AI knows the full conversation so far
        WDJ::JsonObject responseMessageObject;
        responseMessageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"assistant"));
        responseMessageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(message));
        _jsonMessages.Append(responseMessageObject);

        co_return winrt::make<GithubCopilotResponse>(message, isError);
    }

    winrt::Windows::Foundation::IAsyncOperation<Windows::Data::Json::JsonObject> GithubCopilotLLMProvider::_SendRequestReturningJson(winrt::hstring uri, const WWH::IHttpContent& content, WWH::HttpMethod method, const Windows::Foundation::Uri referer)
    {
        if (!method)
        {
            method = content == nullptr ? WWH::HttpMethod::Get() : WWH::HttpMethod::Post();
        }

        WWH::HttpRequestMessage request{ WWH::HttpMethod::Post(), Uri{ uri } };
        request.Headers().Accept().TryParseAdd(L"application/json");

        WDJ::JsonObject jsonContent;

        jsonContent.SetNamedValue(L"client_id", WDJ::JsonValue::CreateStringValue(L"Iv1.b0870d058e4473a1"));
        const auto stringContent = jsonContent.ToString();
        WWH::HttpStringContent requestContent{
            stringContent,
            WSS::UnicodeEncoding::Utf8,
            L"application/json"
        };

        request.Content(requestContent);

        co_await winrt::resume_background();

        const auto response{ _httpClient.SendRequestAsync(request).get() };
        const auto string{ response.Content().ReadAsStringAsync().get() };
        const auto jsonResult{ WDJ::JsonObject::Parse(string) };

        WWH::HttpRequestMessage request2{ WWH::HttpMethod::Post(), Uri{ L"https://github.com/login/oauth/access_token" } };
        request2.Headers().Accept().TryParseAdd(L"application/json");

        WDJ::JsonObject jsonContent2;
        const auto devCode = jsonResult.GetNamedString(L"device_code");

        jsonContent2.SetNamedValue(L"client_id", WDJ::JsonValue::CreateStringValue(L"Iv1.b0870d058e4473a1"));
        jsonContent2.SetNamedValue(L"device_code", WDJ::JsonValue::CreateStringValue(devCode));
        jsonContent2.SetNamedValue(L"grant_type", WDJ::JsonValue::CreateStringValue(L"urn:ietf:params:oauth:grant-type:device_code"));
        const auto stringContent2 = jsonContent2.ToString();
        WWH::HttpStringContent requestContent2{
            stringContent2,
            WSS::UnicodeEncoding::Utf8,
            L"application/json"
        };

        request2.Content(requestContent2);

        const auto response2{ _httpClient.SendRequestAsync(request2).get() };
        const auto string2{ response2.Content().ReadAsStringAsync().get() };
        const auto jsonResult2{ WDJ::JsonObject::Parse(string2) };

        co_return jsonResult2;
    }

    void GithubCopilotLLMProvider::_refreshAuthTokens()
    {
        WWH::HttpRequestMessage request{ WWH::HttpMethod::Post(), Uri{ L"https://github.com/login/oauth/access_token" } };
        request.Headers().Accept().TryParseAdd(L"application/json");

        WDJ::JsonObject jsonContent;

        jsonContent.SetNamedValue(L"client_id", WDJ::JsonValue::CreateStringValue(L"Iv1.b0870d058e4473a1"));
        jsonContent.SetNamedValue(L"grant_type", WDJ::JsonValue::CreateStringValue(L"refresh_token"));
        jsonContent.SetNamedValue(L"client_secret", WDJ::JsonValue::CreateStringValue(L"FineKeepYourSecrets"));
        jsonContent.SetNamedValue(L"refresh_token", WDJ::JsonValue::CreateStringValue(_refreshToken));
        const auto stringContent = jsonContent.ToString();
        WWH::HttpStringContent requestContent{
            stringContent,
            WSS::UnicodeEncoding::Utf8,
            L"application/json"
        };

        request.Content(requestContent);

        try
        {
            const auto response{ _httpClient.SendRequestAsync(request).get() };
            const auto string{ response.Content().ReadAsStringAsync().get() };
            const auto jsonResult{ WDJ::JsonObject::Parse(string) };

            _authToken = jsonResult.GetNamedString(L"access_token");
            _refreshToken = jsonResult.GetNamedString(L"refresh_token");
            _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ L"Bearer", _authToken });
            // todo: this doesn't currently work
            // todo: we should send the new tokens back to settings for storage
            //       or should the refreshing happen in terminal page itself? we need the client secret again
            //       ...but that require re-initializing the palette
        }
        catch (...)
        {
        }
    }
}
