// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GithubCopilotLLMProvider.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct GithubCopilotLLMProvider : GithubCopilotLLMProviderT<GithubCopilotLLMProvider>
    {
        GithubCopilotLLMProvider() = default;

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(const Extension::IContext context);

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring& userPrompt);

        void SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues);
        TYPED_EVENT(AuthChanged, winrt::Microsoft::Terminal::Query::Extension::ILMProvider, Windows::Foundation::Collections::ValueSet);

    private:
        winrt::hstring _authToken;
        winrt::hstring _refreshToken;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };

        Extension::IContext _context;

        winrt::Windows::Data::Json::JsonArray _jsonMessages;

        void _refreshAuthTokens();
        winrt::fire_and_forget _completeAuthWithUrl(const Windows::Foundation::Uri url);
    };

    struct GithubCopilotResponse : public winrt::implements<GithubCopilotResponse, winrt::Microsoft::Terminal::Query::Extension::IResponse>
    {
        GithubCopilotResponse(const winrt::hstring& message, const winrt::Microsoft::Terminal::Query::Extension::ErrorTypes errorType) :
            Message{ message },
            ErrorType{ errorType } {}

        til::property<winrt::hstring> Message;
        til::property<winrt::Microsoft::Terminal::Query::Extension::ErrorTypes> ErrorType;
    };
}

namespace winrt::Microsoft::Terminal::Query::Extension::factory_implementation
{
    BASIC_FACTORY(GithubCopilotLLMProvider);
}
