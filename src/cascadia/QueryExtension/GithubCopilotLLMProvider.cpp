// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GithubCopilotLLMProvider.h"
#include "../../types/inc/utils.hpp"
#include "LibraryResources.h"
#include "WindowsTerminalIDAndSecret.h"

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

// branding data
static constexpr wil::zwstring_view headerIconPath{ L"ms-appx:///ProfileIcons/githubCopilotLogo.png" };
static constexpr wil::zwstring_view badgeIconPath{ L"ms-appx:///ProfileIcons/githubCopilotBadge.png" };

// header and request strings
static constexpr std::wstring_view applicationJsonString{ L"application/json" };
static constexpr std::wstring_view bearerString{ L"Bearer" };
static constexpr std::wstring_view copilotIntegrationIdString{ L"Copilot-Integration-Id" };
static constexpr std::wstring_view clientIdKey{ L"client_id" };
static constexpr std::wstring_view clientSecretKey{ L"client_secret" };
static constexpr std::wstring_view endpointAndUsernameRequestString{ L"{ viewer { copilotEndpoints { api } login } }" };

// json keys
static constexpr std::wstring_view accessTokenKey{ L"access_token" };
static constexpr std::wstring_view refreshTokenKey{ L"refresh_token" };
static constexpr std::wstring_view stateKey{ L"state" };
static constexpr std::wstring_view urlKey{ L"url" };
static constexpr std::wstring_view queryKey{ L"query" };
static constexpr std::wstring_view codeKey{ L"code" };
static constexpr std::wstring_view errorKey{ L"error" };
static constexpr std::wstring_view errorDescriptionKey{ L"error_description" };
static constexpr std::wstring_view dataKey{ L"data" };
static constexpr std::wstring_view apiKey{ L"api" };
static constexpr std::wstring_view viewerKey{ L"viewer" };
static constexpr std::wstring_view copilotEndpointsKey{ L"copilotEndpoints" };
static constexpr std::wstring_view loginKey{ L"login" };
static constexpr std::wstring_view grantTypeKey{ L"grant_type" };
static constexpr std::wstring_view contentKey{ L"content" };
static constexpr std::wstring_view messageKey{ L"message" };
static constexpr std::wstring_view messagesKey{ L"messages" };
static constexpr std::wstring_view choicesKey{ L"choices" };
static constexpr std::wstring_view roleKey{ L"role" };
static constexpr std::wstring_view assistantKey{ L"assistant" };
static constexpr std::wstring_view userKey{ L"user" };
static constexpr std::wstring_view systemKey{ L"system" };

// endpoints
static constexpr std::wstring_view githubGraphQLEndpoint{ L"https://api.github.com/graphql" };
static constexpr std::wstring_view chatCompletionSuffix{ L"/chat/completions" };
static constexpr std::wstring_view accessTokenEndpoint{ L"https://github.com/login/oauth/access_token" };

// Windows Terminal specific strings
static constexpr std::wstring_view windowsTerminalUserAgent{ L"Windows Terminal" };
static constexpr std::wstring_view windowsTerminalIntegrationId{ L"windows-terminal-chat" };

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    winrt::hstring GithubCopilotBranding::HeaderIconPath() const noexcept
    {
        return headerIconPath.c_str();
    }

    winrt::hstring GithubCopilotBranding::HeaderText() const noexcept
    {
        return RS_(L"GithubCopilot_HeaderText");
    }

    winrt::hstring GithubCopilotBranding::SubheaderText() const noexcept
    {
        return RS_(L"GithubCopilot_SubheaderText");
    }

    winrt::hstring GithubCopilotBranding::BadgeIconPath() const noexcept
    {
        return badgeIconPath.c_str();
    }

    void GithubCopilotLLMProvider::SetAuthentication(const winrt::hstring& authValues)
    {
        _httpClient = winrt::Windows::Web::Http::HttpClient{};
        _httpClient.DefaultRequestHeaders().Accept().TryParseAdd(applicationJsonString);
        _httpClient.DefaultRequestHeaders().Append(copilotIntegrationIdString, windowsTerminalIntegrationId);
        _httpClient.DefaultRequestHeaders().UserAgent().TryParseAdd(windowsTerminalUserAgent);

        if (!authValues.empty())
        {
            WDJ::JsonObject authValuesObject{ WDJ::JsonObject::Parse(authValues) };
            if (authValuesObject.HasKey(urlKey) && authValuesObject.HasKey(stateKey))
            {
                const Windows::Foundation::Uri parsedUrl{ authValuesObject.GetNamedString(urlKey) };
                // only handle this if the state strings match
                if (authValuesObject.GetNamedString(stateKey) == parsedUrl.QueryParsed().GetFirstValueByName(stateKey))
                {
                    // we got a valid URL, fire off the URL auth flow
                    _completeAuthWithUrl(parsedUrl);
                }
            }
            else if (authValuesObject.HasKey(accessTokenKey) && authValuesObject.HasKey(refreshTokenKey))
            {
                _authToken = authValuesObject.GetNamedString(accessTokenKey);
                _refreshToken = authValuesObject.GetNamedString(refreshTokenKey);

                // we got tokens, use them
                _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ bearerString, _authToken });
                _obtainUsernameAndRefreshTokensIfNeeded();
            }
        }
    }

    IAsyncAction GithubCopilotLLMProvider::_obtainUsernameAndRefreshTokensIfNeeded()
    {
        WDJ::JsonObject endpointAndUsernameRequestJson;
        endpointAndUsernameRequestJson.SetNamedValue(queryKey, WDJ::JsonValue::CreateStringValue(endpointAndUsernameRequestString));
        const auto endpointAndUsernameRequestString = endpointAndUsernameRequestJson.ToString();
        WWH::HttpStringContent endpointAndUsernameRequestContent{
            endpointAndUsernameRequestString,
            WSS::UnicodeEncoding::Utf8,
            applicationJsonString
        };

        auto strongThis = get_strong();
        co_await winrt::resume_background();

        for (bool refreshAttempted = false;;)
        {
            try
            {
                const auto endpointAndUsernameResult = co_await _SendRequestReturningJson(githubGraphQLEndpoint, endpointAndUsernameRequestContent, WWH::HttpMethod::Post());
                const auto viewerObject = endpointAndUsernameResult.GetNamedObject(dataKey).GetNamedObject(viewerKey);
                const auto userName = viewerObject.GetNamedString(loginKey);
                const auto copilotEndpoint = viewerObject.GetNamedObject(copilotEndpointsKey).GetNamedString(apiKey);

                _endpointUri = copilotEndpoint + chatCompletionSuffix;
                const auto brandingData{ get_self<GithubCopilotBranding>(_brandingData) };
                brandingData->QueryAttribution(userName);
                break;
            }
            CATCH_LOG();

            // unknown failure, try refreshing the auth token if we haven't already
            if (refreshAttempted)
            {
                break;
            }

            co_await _refreshAuthTokens();
            refreshAttempted = true;
        }
        co_return;
    }

    IAsyncAction GithubCopilotLLMProvider::_completeAuthWithUrl(const Windows::Foundation::Uri url)
    {
        WDJ::JsonObject jsonContent;
        jsonContent.SetNamedValue(clientIdKey, WDJ::JsonValue::CreateStringValue(windowsTerminalClientID));
        jsonContent.SetNamedValue(clientSecretKey, WDJ::JsonValue::CreateStringValue(windowsTerminalClientSecret));
        jsonContent.SetNamedValue(codeKey, WDJ::JsonValue::CreateStringValue(url.QueryParsed().GetFirstValueByName(codeKey)));
        const auto stringContent = jsonContent.ToString();
        WWH::HttpStringContent requestContent{
            stringContent,
            WSS::UnicodeEncoding::Utf8,
            applicationJsonString
        };

        auto strongThis = get_strong();
        co_await winrt::resume_background();

        try
        {
            // Get the user's oauth token
            const auto jsonResult = co_await _SendRequestReturningJson(accessTokenEndpoint, requestContent, WWH::HttpMethod::Post());
            if (jsonResult.HasKey(errorKey))
            {
                const auto errorMessage = jsonResult.GetNamedString(errorDescriptionKey);
                _AuthChangedHandlers(*this, winrt::make<GithubCopilotAuthenticationResult>(errorMessage, winrt::hstring{}));
            }
            else
            {
                const auto authToken{ jsonResult.GetNamedString(accessTokenKey) };
                const auto refreshToken{ jsonResult.GetNamedString(refreshTokenKey) };
                if (!authToken.empty() && !refreshToken.empty())
                {
                    _authToken = authToken;
                    _refreshToken = refreshToken;
                    _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ bearerString, _authToken });

                    // raise the new tokens so the app can store them
                    Windows::Data::Json::JsonObject authValuesJson;
                    authValuesJson.SetNamedValue(accessTokenKey, WDJ::JsonValue::CreateStringValue(_authToken));
                    authValuesJson.SetNamedValue(refreshTokenKey, WDJ::JsonValue::CreateStringValue(_refreshToken));
                    _AuthChangedHandlers(*this, winrt::make<GithubCopilotAuthenticationResult>(winrt::hstring{}, authValuesJson.ToString()));

                    // we also need to get the correct endpoint to use and the username
                    _obtainUsernameAndRefreshTokensIfNeeded();
                }
            }
        }
        catch (...)
        {
            // some unknown error happened and we didn't get an "error" key, bubble the raw string of the last response if we have one
            const auto errorMessage = _lastResponse.empty() ? RS_(L"UnknownErrorMessage") : _lastResponse;
            _AuthChangedHandlers(*this, winrt::make<GithubCopilotAuthenticationResult>(errorMessage, winrt::hstring{}));
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
        systemMessageObject.Insert(roleKey, WDJ::JsonValue::CreateStringValue(systemKey));
        systemMessageObject.Insert(contentKey, WDJ::JsonValue::CreateStringValue(systemMessageContent));
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
        auto strongThis = get_strong();
        co_await winrt::resume_background();

        for (bool refreshAttempted = false;;)
        {
            try
            {
                // create the request content
                // we construct the request content within the while loop because if we do need to attempt
                // a request again after refreshing the tokens, we need a new request object
                WDJ::JsonObject jsonContent;
                WDJ::JsonObject messageObject;

                winrt::hstring engineeredPrompt{ promptCopy };
                if (_context && !_context.ActiveCommandline().empty())
                {
                    engineeredPrompt = promptCopy + L". The shell I am running is " + _context.ActiveCommandline();
                }
                messageObject.Insert(roleKey, WDJ::JsonValue::CreateStringValue(userKey));
                messageObject.Insert(contentKey, WDJ::JsonValue::CreateStringValue(engineeredPrompt));
                _jsonMessages.Append(messageObject);
                jsonContent.SetNamedValue(messagesKey, _jsonMessages);
                const auto stringContent = jsonContent.ToString();
                WWH::HttpStringContent requestContent{
                    stringContent,
                    WSS::UnicodeEncoding::Utf8,
                    applicationJsonString
                };

                // Send the request
                const auto jsonResult = co_await _SendRequestReturningJson(_endpointUri, requestContent, WWH::HttpMethod::Post());
                if (jsonResult.HasKey(errorKey))
                {
                    const auto errorObject = jsonResult.GetNamedObject(errorKey);
                    message = errorObject.GetNamedString(messageKey);
                    errorType = ErrorTypes::FromProvider;
                }
                else
                {
                    const auto choices = jsonResult.GetNamedArray(choicesKey);
                    const auto firstChoice = choices.GetAt(0).GetObject();
                    const auto messageObject = firstChoice.GetNamedObject(messageKey);
                    message = messageObject.GetNamedString(contentKey);
                }
                break;
            }
            CATCH_LOG();

            // unknown failure, if we have already attempted a refresh report failure
            // otherwise, try refreshing the auth token
            if (refreshAttempted)
            {
                // if we have a last recorded response, bubble that instead of the unknown error message
                // since that's likely going to be more useful
                message = _lastResponse.empty() ? RS_(L"UnknownErrorMessage") : _lastResponse;
                errorType = ErrorTypes::Unknown;
                break;
            }

            co_await _refreshAuthTokens();
            refreshAttempted = true;
        }

        // Also make a new entry in our jsonMessages list, so the AI knows the full conversation so far
        WDJ::JsonObject responseMessageObject;
        responseMessageObject.Insert(roleKey, WDJ::JsonValue::CreateStringValue(assistantKey));
        responseMessageObject.Insert(contentKey, WDJ::JsonValue::CreateStringValue(message));
        _jsonMessages.Append(responseMessageObject);

        co_return winrt::make<GithubCopilotResponse>(message, errorType, RS_(L"GithubCopilot_ResponseMetaData"));
    }

    IAsyncAction GithubCopilotLLMProvider::_refreshAuthTokens()
    {
        WDJ::JsonObject jsonContent;
        jsonContent.SetNamedValue(clientIdKey, WDJ::JsonValue::CreateStringValue(windowsTerminalClientID));
        jsonContent.SetNamedValue(grantTypeKey, WDJ::JsonValue::CreateStringValue(refreshTokenKey));
        jsonContent.SetNamedValue(clientSecretKey, WDJ::JsonValue::CreateStringValue(windowsTerminalClientSecret));
        jsonContent.SetNamedValue(refreshTokenKey, WDJ::JsonValue::CreateStringValue(_refreshToken));
        const auto stringContent = jsonContent.ToString();
        WWH::HttpStringContent requestContent{
            stringContent,
            WSS::UnicodeEncoding::Utf8,
            applicationJsonString
        };

        try
        {
            const auto jsonResult = co_await _SendRequestReturningJson(accessTokenEndpoint, requestContent, WWH::HttpMethod::Post());

            _authToken = jsonResult.GetNamedString(accessTokenKey);
            _refreshToken = jsonResult.GetNamedString(refreshTokenKey);
            _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ bearerString, _authToken });

            // raise the new tokens so the app can store them
            Windows::Data::Json::JsonObject authValuesJson;
            authValuesJson.SetNamedValue(accessTokenKey, WDJ::JsonValue::CreateStringValue(_authToken));
            authValuesJson.SetNamedValue(refreshTokenKey, WDJ::JsonValue::CreateStringValue(_refreshToken));
            _AuthChangedHandlers(*this, winrt::make<GithubCopilotAuthenticationResult>(winrt::hstring{}, authValuesJson.ToString()));
        }
        CATCH_LOG();
        co_return;
    }

    IAsyncOperation<WDJ::JsonObject> GithubCopilotLLMProvider::_SendRequestReturningJson(std::wstring_view uri, const winrt::Windows::Web::Http::IHttpContent& content, winrt::Windows::Web::Http::HttpMethod method)
    {
        if (!method)
        {
            method = content == nullptr ? WWH::HttpMethod::Get() : WWH::HttpMethod::Post();
        }

        WWH::HttpRequestMessage request{ method, Uri{ uri } };
        request.Content(content);

        const auto response{ co_await _httpClient.SendRequestAsync(request) };
        const auto string{ co_await response.Content().ReadAsStringAsync() };
        _lastResponse = string;
        const auto jsonResult{ WDJ::JsonObject::Parse(string) };

        co_return jsonResult;
    }
}
