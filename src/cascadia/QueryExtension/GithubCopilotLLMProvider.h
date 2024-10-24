// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GithubCopilotLLMProvider.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct GithubCopilotBranding : public winrt::implements<GithubCopilotBranding, winrt::Microsoft::Terminal::Query::Extension::IBrandingData>
    {
        GithubCopilotBranding() = default;

        winrt::hstring Name() const noexcept { return L"GitHub Copilot"; };
        winrt::hstring HeaderIconPath() const noexcept;
        winrt::hstring HeaderText() const noexcept;
        winrt::hstring SubheaderText() const noexcept;
        winrt::hstring BadgeIconPath() const noexcept;
        WINRT_PROPERTY(winrt::hstring, QueryAttribution);
    };

    struct GithubCopilotAuthenticationResult : public winrt::implements<GithubCopilotAuthenticationResult, winrt::Microsoft::Terminal::Query::Extension::IAuthenticationResult>
    {
        GithubCopilotAuthenticationResult(const winrt::hstring& errorMessage, const Windows::Foundation::Collections::ValueSet& authValues) :
            ErrorMessage{ errorMessage },
            AuthValues{ authValues } {}

        til::property<winrt::hstring> ErrorMessage;
        til::property<Windows::Foundation::Collections::ValueSet> AuthValues;
    };

    struct GithubCopilotLLMProvider : GithubCopilotLLMProviderT<GithubCopilotLLMProvider>
    {
        GithubCopilotLLMProvider() = default;

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(const Extension::IContext context);

        IBrandingData BrandingData() { return _brandingData; };

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring& userPrompt);

        void SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues);
        TYPED_EVENT(AuthChanged, winrt::Microsoft::Terminal::Query::Extension::ILMProvider, winrt::Microsoft::Terminal::Query::Extension::IAuthenticationResult);

    private:
        winrt::hstring _authToken;
        winrt::hstring _refreshToken;
        winrt::hstring _endpointUri;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };
        IBrandingData _brandingData{ winrt::make<GithubCopilotBranding>() };
        winrt::hstring _lastResponse;

        Extension::IContext _context;

        winrt::Windows::Data::Json::JsonArray _jsonMessages;

        void _refreshAuthTokens();
        safe_void_coroutine _completeAuthWithUrl(const Windows::Foundation::Uri url);
        safe_void_coroutine _obtainUsernameAndRefreshTokensIfNeeded();
        winrt::Windows::Data::Json::JsonObject _SendRequestReturningJson(std::wstring_view uri, const winrt::Windows::Web::Http::IHttpContent& content = nullptr, winrt::Windows::Web::Http::HttpMethod method = nullptr);
    };

    struct GithubCopilotResponse : public winrt::implements<GithubCopilotResponse, winrt::Microsoft::Terminal::Query::Extension::IResponse>
    {
        GithubCopilotResponse(const winrt::hstring& message, const winrt::Microsoft::Terminal::Query::Extension::ErrorTypes errorType, const winrt::hstring& responseAttribution) :
            Message{ message },
            ErrorType{ errorType },
            ResponseAttribution{ responseAttribution } {}

        til::property<winrt::hstring> Message;
        til::property<winrt::Microsoft::Terminal::Query::Extension::ErrorTypes> ErrorType;
        til::property<winrt::hstring> ResponseAttribution;
    };
}

namespace winrt::Microsoft::Terminal::Query::Extension::factory_implementation
{
    BASIC_FACTORY(GithubCopilotLLMProvider);
}
