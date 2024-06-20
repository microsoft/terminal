// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AzureLLMProvider.g.h"
#include "AzureResponse.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct AzureLLMProvider : AzureLLMProviderT<AzureLLMProvider>
    {
        AzureLLMProvider(const winrt::hstring& endpoint, const winrt::hstring& key);

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(const Extension::IContext context);

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring& userPrompt);

    private:
        winrt::hstring _AIEndpoint;
        winrt::hstring _AIKey;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };

        Extension::IContext _context;

        winrt::Windows::Data::Json::JsonArray _jsonMessages;

        bool _verifyModelIsValidHelper(const Windows::Data::Json::JsonObject jsonResponse);
    };

    struct AzureResponse : AzureResponseT<AzureResponse>
    {
        AzureResponse(const winrt::hstring& message, const bool isError) :
            _message{ message },
            _isError{ isError } {}
        winrt::hstring Message() { return _message; };
        bool IsError() { return _isError; };

    private:
        winrt::hstring _message;
        bool _isError;
    };
}

namespace winrt::Microsoft::Terminal::Query::Extension::factory_implementation
{
    BASIC_FACTORY(AzureLLMProvider);
    BASIC_FACTORY(AzureResponse);
}
