// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "OpenAILLMProvider.g.h"
#include "OpenAIResponse.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct OpenAILLMProvider : OpenAILLMProviderT<OpenAILLMProvider>
    {
        OpenAILLMProvider() = default;

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(const Extension::IContext context);

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring userPrompt);

        void SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues);
        TYPED_EVENT(AuthChanged, winrt::Microsoft::Terminal::Query::Extension::ILMProvider, winrt::hstring);

    private:
        winrt::hstring _AIKey;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };

        Extension::IContext _context;

        winrt::Windows::Data::Json::JsonArray _jsonMessages;
    };

    struct OpenAIResponse : OpenAIResponseT<OpenAIResponse>
    {
        OpenAIResponse(const winrt::hstring& message, const bool isError) :
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
    BASIC_FACTORY(OpenAILLMProvider);
    BASIC_FACTORY(OpenAIResponse);
}
