// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AzureLLMProvider.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct AzureBranding : public winrt::implements<AzureBranding, winrt::Microsoft::Terminal::Query::Extension::IBrandingData>
    {
        AzureBranding() = default;

        winrt::hstring Name() const noexcept { return L"Azure OpenAI"; };
        winrt::hstring HeaderIconPath() const noexcept { return winrt::hstring{}; };
        winrt::hstring HeaderText() const noexcept { return winrt::hstring{}; };
        winrt::hstring SubheaderText() const noexcept { return winrt::hstring{}; };
        winrt::hstring BadgeIconPath() const noexcept { return winrt::hstring{}; };
        winrt::hstring QueryAttribution() const noexcept { return winrt::hstring{}; };
    };

    struct AzureLLMProvider : AzureLLMProviderT<AzureLLMProvider>
    {
        AzureLLMProvider() = default;

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(Extension::IContext context);

        IBrandingData BrandingData() { return _brandingData; };

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring& userPrompt);

        void SetAuthentication(const winrt::hstring& authValues);
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
