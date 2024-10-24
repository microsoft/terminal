// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "OpenAILLMProvider.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct OpenAIBranding : public winrt::implements<OpenAIBranding, winrt::Microsoft::Terminal::Query::Extension::IBrandingData>
    {
        OpenAIBranding() = default;

        winrt::hstring Name() { return _name; };
        winrt::hstring HeaderIconPath() { return _headerIconPath; };
        winrt::hstring HeaderText() { return _headerText; };
        winrt::hstring SubheaderText() { return _subheaderText; };
        winrt::hstring BadgeIconPath() { return _badgeIconPath; };
        winrt::hstring QueryAttribution() { return _queryAttribution; };

    private:
        winrt::hstring _name{ L"OpenAI" };
        winrt::hstring _headerIconPath;
        winrt::hstring _headerText;
        winrt::hstring _subheaderText;
        winrt::hstring _badgeIconPath;
        winrt::hstring _queryAttribution;
    };

    struct OpenAILLMProvider : OpenAILLMProviderT<OpenAILLMProvider>
    {
        OpenAILLMProvider() = default;

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(Extension::IContext context);

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring userPrompt);

        void SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues);
        TYPED_EVENT(AuthChanged, winrt::Microsoft::Terminal::Query::Extension::ILMProvider, winrt::Microsoft::Terminal::Query::Extension::IAuthenticationResult);

        WINRT_PROPERTY(IBrandingData, BrandingData, winrt::make<OpenAIBranding>());

    private:
        winrt::hstring _AIKey;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };

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
