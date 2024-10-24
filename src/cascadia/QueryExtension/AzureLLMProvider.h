// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AzureLLMProvider.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct AzureBranding : public winrt::implements<AzureBranding, winrt::Microsoft::Terminal::Query::Extension::IBrandingData>
    {
        AzureBranding() = default;

        winrt::hstring Name() { return _name; };
        winrt::hstring HeaderIconPath() { return _headerIconPath; };
        winrt::hstring HeaderText() { return _headerText; };
        winrt::hstring SubheaderText() { return _subheaderText; };
        winrt::hstring BadgeIconPath() { return _badgeIconPath; };
        winrt::hstring QueryAttribution() { return _queryAttribution; };

    private:
        winrt::hstring _name{ L"Azure OpenAI" };
        winrt::hstring _headerIconPath;
        winrt::hstring _headerText;
        winrt::hstring _subheaderText;
        winrt::hstring _badgeIconPath;
        winrt::hstring _queryAttribution;
    };

    struct AzureLLMProvider : AzureLLMProviderT<AzureLLMProvider>
    {
        AzureLLMProvider() = default;

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(Extension::IContext context);

        IBrandingData BrandingData() { return _brandingData; };

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring& userPrompt);

        void SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues);
        TYPED_EVENT(AuthChanged, winrt::Microsoft::Terminal::Query::Extension::ILMProvider, winrt::Microsoft::Terminal::Query::Extension::IAuthenticationResult);

    private:
        winrt::hstring _azureEndpoint;
        winrt::hstring _azureKey;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };
        IBrandingData _brandingData{ winrt::make<AzureBranding>() };

        Extension::IContext _context;

        winrt::Windows::Data::Json::JsonArray _jsonMessages;

        bool _verifyModelIsValidHelper(const Windows::Data::Json::JsonObject jsonResponse);
    };

    struct AzureResponse : public winrt::implements<AzureResponse, winrt::Microsoft::Terminal::Query::Extension::IResponse>
    {
        AzureResponse(const winrt::hstring& message, const winrt::Microsoft::Terminal::Query::Extension::ErrorTypes errorType, const winrt::hstring& responseAttribution) :
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
    BASIC_FACTORY(AzureLLMProvider);
}
