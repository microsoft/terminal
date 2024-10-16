// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GithubCopilotLLMProvider.h"
#include "../../types/inc/utils.hpp"
#include "LibraryResources.h"

#include "GithubCopilotLLMProvider.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::System;
namespace WWH = ::winrt::Windows::Web::Http;
namespace WSS = ::winrt::Windows::Storage::Streams;
namespace WDJ = ::winrt::Windows::Data::Json;

static constexpr std::wstring_view headerIconPath{ L"ms-appx:///ProfileIcons/githubCopilotLogo.png" };
static constexpr std::wstring_view badgeIconPath{ L"ms-appx:///ProfileIcons/githubCopilotBadge.png" };
static constexpr std::wstring_view chatCompletionSuffix{ L"/chat/completions" };

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    GithubCopilotBranding::GithubCopilotBranding()
    {
        _HeaderIconPath = headerIconPath;
        _HeaderText = RS_(L"GithubCopilot_HeaderText");
        _SubheaderText = RS_(L"GithubCopilot_SubheaderText");
        _BadgeIconPath = badgeIconPath;
        _ResponseMetaData = RS_(L"GithubCopilot_ResponseMetaData");
    }

    void GithubCopilotLLMProvider::SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues)
    {
        _httpClient = winrt::Windows::Web::Http::HttpClient{};
        _httpClient.DefaultRequestHeaders().Accept().TryParseAdd(L"application/json");
        _httpClient.DefaultRequestHeaders().Append(L"Copilot-Integration-Id", L"windows-terminal-chat");

        const auto url = unbox_value_or<hstring>(authValues.TryLookup(L"url").try_as<IPropertyValue>(), L"");
        _authToken = unbox_value_or<hstring>(authValues.TryLookup(L"access_token").try_as<IPropertyValue>(), L"");
        _refreshToken = unbox_value_or<hstring>(authValues.TryLookup(L"refresh_token").try_as<IPropertyValue>(), L"");

        if (!url.empty())
        {
            const Windows::Foundation::Uri parsedUrl{ url };
            const auto randomStateString = unbox_value_or<hstring>(authValues.TryLookup(L"state").try_as<IPropertyValue>(), L"");
            // only handle this if the state strings match
            if (randomStateString == parsedUrl.QueryParsed().GetFirstValueByName(L"state"))
            {
                // we got a valid URL, fire off the URL auth flow
                _completeAuthWithUrl(parsedUrl);
            }
        }
        else if (!_authToken.empty() && !_refreshToken.empty())
        {
            // we got tokens, use them
            _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ L"Bearer", _authToken });
            _obtainUsernameAndRefreshTokensIfNeeded();
        }
    }

    safe_void_coroutine GithubCopilotLLMProvider::_obtainUsernameAndRefreshTokensIfNeeded()
    {
        WWH::HttpRequestMessage request{ WWH::HttpMethod::Get(), Uri{ L"https://api.github.com/user" } };
        request.Headers().Accept().TryParseAdd(L"application/json");
        request.Headers().UserAgent().TryParseAdd(L"Windows Terminal");

        auto strongThis = get_strong();
        co_await winrt::resume_background();

        for (bool refreshAttempted = false;;)
        {
            try
            {
                const auto response = co_await _httpClient.SendRequestAsync(request);
                const auto string = co_await response.Content().ReadAsStringAsync();
                const auto jsonResult{ WDJ::JsonObject::Parse(string) };
                const auto brandingData{ get_self<GithubCopilotBranding>(_BrandingData) };
                brandingData->QueryMetaData(jsonResult.GetNamedString(L"login"));
                break;
            }
            CATCH_LOG();

            // unknown failure, try refreshing the auth token if we haven't already
            if (refreshAttempted)
            {
                break;
            }

            _refreshAuthTokens();
            refreshAttempted = true;
        }
    }

    safe_void_coroutine GithubCopilotLLMProvider::_completeAuthWithUrl(const Windows::Foundation::Uri url)
    {
        WWH::HttpRequestMessage request{ WWH::HttpMethod::Post(), Windows::Foundation::Uri{ L"https://github.com/login/oauth/access_token" } };
        request.Headers().Accept().TryParseAdd(L"application/json");

        WDJ::JsonObject jsonContent;
        jsonContent.SetNamedValue(L"client_id", WDJ::JsonValue::CreateStringValue(L"Iv1.b0870d058e4473a1"));
        jsonContent.SetNamedValue(L"client_secret", WDJ::JsonValue::CreateStringValue(L"FineKeepYourSecrets"));
        jsonContent.SetNamedValue(L"code", WDJ::JsonValue::CreateStringValue(url.QueryParsed().GetFirstValueByName(L"code")));
        const auto stringContent = jsonContent.ToString();
        WWH::HttpStringContent requestContent{
            stringContent,
            WSS::UnicodeEncoding::Utf8,
            L"application/json"
        };

        request.Content(requestContent);

        auto strongThis = get_strong();
        co_await winrt::resume_background();

        try
        {
            const auto response = co_await _httpClient.SendRequestAsync(request);
            // Parse out the suggestion from the response
            const auto string = co_await response.Content().ReadAsStringAsync();

            const auto jsonResult{ WDJ::JsonObject::Parse(string) };
            if (jsonResult.HasKey(L"error"))
            {
                const auto errorMessage = jsonResult.GetNamedString(L"error_description");
                _AuthChangedHandlers(*this, winrt::make<GithubCopilotAuthenticationResult>(errorMessage, nullptr));
            }
            else
            {
                const auto authToken{ jsonResult.GetNamedString(L"access_token") };
                const auto refreshToken{ jsonResult.GetNamedString(L"refresh_token") };
                if (!authToken.empty() && !refreshToken.empty())
                {
                    _authToken = authToken;
                    _refreshToken = refreshToken;
                    _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ L"Bearer", _authToken });

                    // raise the new tokens so the app can store them
                    Windows::Foundation::Collections::ValueSet authValues{};
                    authValues.Insert(L"access_token", Windows::Foundation::PropertyValue::CreateString(_authToken));
                    authValues.Insert(L"refresh_token", Windows::Foundation::PropertyValue::CreateString(_refreshToken));
                    _AuthChangedHandlers(*this, winrt::make<GithubCopilotAuthenticationResult>(L"", authValues));

                    // we also need to get the correct endpoint to use and the username
                    WWH::HttpRequestMessage endpointAndUsernameRequest{ WWH::HttpMethod::Post(), Windows::Foundation::Uri{ L"https://api.github.com/graphql" } };
                    endpointAndUsernameRequest.Headers().UserAgent().TryParseAdd(L"Windows Terminal");
                    WDJ::JsonObject endpointAndUsernameRequestJson;
                    endpointAndUsernameRequestJson.SetNamedValue(L"query", WDJ::JsonValue::CreateStringValue(L"{ viewer { copilotEndpoints { api } login } }"));
                    const auto endpointAndUsernameRequestString = endpointAndUsernameRequestJson.ToString();
                    WWH::HttpStringContent endpointAndUsernameRequestContent{
                        endpointAndUsernameRequestString,
                        WSS::UnicodeEncoding::Utf8,
                        L"application/json"
                    };
                    endpointAndUsernameRequest.Content(endpointAndUsernameRequestContent);
                    const auto endpointAndUsernameResponse = co_await _httpClient.SendRequestAsync(endpointAndUsernameRequest);
                    const auto endpointAndUsernameString = co_await endpointAndUsernameResponse.Content().ReadAsStringAsync();
                    const auto endpointAndUsernameResult{ WDJ::JsonObject::Parse(endpointAndUsernameString) };
                    const auto userName = endpointAndUsernameResult.GetNamedObject(L"data").GetNamedObject(L"viewer").GetNamedString(L"login");
                    const auto copilotEndpoints = endpointAndUsernameResult.GetNamedObject(L"data").GetNamedObject(L"viewer").GetNamedObject(L"copilotEndpoints").GetNamedString(L"api");

                    _endpointUri = copilotEndpoints + chatCompletionSuffix;
                    const auto brandingData{ get_self<GithubCopilotBranding>(_BrandingData) };
                    brandingData->QueryMetaData(userName);
                }
            }
        }
        catch (...)
        {
            _AuthChangedHandlers(*this, winrt::make<GithubCopilotAuthenticationResult>(RS_(L"UnknownErrorMessage"), nullptr));
        }

        co_return;
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
    }

    void GithubCopilotLLMProvider::SetContext(const Extension::IContext context)
    {
        _context = context;
    }

    winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GithubCopilotLLMProvider::GetResponseAsync(const winrt::hstring& userPrompt)
    {
        // Use the ErrorTypes enum to flag whether the response the user receives is an error message
        // we pass this enum back to the caller so they can handle it appropriately (specifically, ExtensionPalette will send the correct telemetry event)
        ErrorTypes errorType{ ErrorTypes::None };
        hstring message{};

        // Make a copy of the prompt because we are switching threads
        const auto promptCopy{ userPrompt };

        // Make sure we are on the background thread for the http request
        co_await winrt::resume_background();

        for (bool refreshAttempted = false;;)
        {
            try
            {
                // create the request object
                // we construct the request object within the while loop because if we do need to attempt
                // a request again after refreshing the tokens, we need a new request object
                WWH::HttpRequestMessage request{ WWH::HttpMethod::Post(), Uri{ _endpointUri } };
                request.Headers().Accept().TryParseAdd(L"application/json");

                WDJ::JsonObject jsonContent;
                WDJ::JsonObject messageObject;

                winrt::hstring engineeredPrompt{ promptCopy };
                if (_context && !_context.ActiveCommandline().empty())
                {
                    engineeredPrompt = promptCopy + L". The shell I am running is " + _context.ActiveCommandline();
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
                    errorType = ErrorTypes::FromProvider;
                }
                break;
            }
            CATCH_LOG();

            // unknown failure, if we have already attempted a refresh report failure
            // otherwise, try refreshing the auth token
            if (refreshAttempted)
            {
                message = RS_(L"UnknownErrorMessage");
                errorType = ErrorTypes::Unknown;
                break;
            }

            _refreshAuthTokens();
            refreshAttempted = true;
        }

        // Also make a new entry in our jsonMessages list, so the AI knows the full conversation so far
        WDJ::JsonObject responseMessageObject;
        responseMessageObject.Insert(L"role", WDJ::JsonValue::CreateStringValue(L"assistant"));
        responseMessageObject.Insert(L"content", WDJ::JsonValue::CreateStringValue(message));
        _jsonMessages.Append(responseMessageObject);

        co_return winrt::make<GithubCopilotResponse>(message, errorType);
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

            // raise the new tokens so the app can store them
            Windows::Foundation::Collections::ValueSet authValues{};
            authValues.Insert(L"access_token", Windows::Foundation::PropertyValue::CreateString(_authToken));
            authValues.Insert(L"refresh_token", Windows::Foundation::PropertyValue::CreateString(_refreshToken));
            _AuthChangedHandlers(*this, winrt::make<GithubCopilotAuthenticationResult>(L"", authValues));
        }
        CATCH_LOG();
    }
}
