// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "OpenAILLMProvider.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct OpenAIBranding : public winrt::implements<OpenAIBranding, winrt::Microsoft::Terminal::Query::Extension::IBrandingData>
    {
        OpenAIBranding() = default;

        winrt::hstring Name() const noexcept { return L"OpenAI"; };
        winrt::hstring HeaderIconPath() const noexcept { return winrt::hstring{}; };
        winrt::hstring HeaderText() const noexcept { return winrt::hstring{}; };
        winrt::hstring SubheaderText() const noexcept { return winrt::hstring{}; };
        winrt::hstring BadgeIconPath() const noexcept { return winrt::hstring{}; };
        winrt::hstring QueryAttribution() const noexcept { return winrt::hstring{}; };
    };

    struct OpenAILLMProvider : OpenAILLMProviderT<OpenAILLMProvider>
    {
        OpenAILLMProvider() = default;

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(Extension::IContext context);

        IBrandingData BrandingData() { return _brandingData; };

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring userPrompt);

        void SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues);
        TYPED_EVENT(AuthChanged, winrt::Microsoft::Terminal::Query::Extension::ILMProvider, winrt::Microsoft::Terminal::Query::Extension::IAuthenticationResult);

    private:
        winrt::hstring _AIKey;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };
        IBrandingData _brandingData{ winrt::make<OpenAIBranding>() };

        Extension::IContext _context;

        winrt::Windows::Data::Json::JsonArray _jsonMessages;
    };

    struct OpenAIResponse : public winrt::implements<OpenAIResponse, winrt::Microsoft::Terminal::Query::Extension::IResponse>
    {
        OpenAIResponse(const winrt::hstring& message, const winrt::Microsoft::Terminal::Query::Extension::ErrorTypes errorType, const winrt::hstring& responseAttribution) :
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
    BASIC_FACTORY(OpenAILLMProvider);
}
