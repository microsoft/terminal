// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AzureLLMProvider.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct AzureBranding : public winrt::implements<AzureBranding, winrt::Microsoft::Terminal::Query::Extension::IBrandingData>
    {
        AzureBranding() = default;

        WINRT_PROPERTY(winrt::hstring, Name, L"Azure OpenAI");
        WINRT_PROPERTY(winrt::hstring, HeaderIconPath);
        WINRT_PROPERTY(winrt::hstring, HeaderText);
        WINRT_PROPERTY(winrt::hstring, SubheaderText);
        WINRT_PROPERTY(winrt::hstring, BadgeIconPath);
        WINRT_PROPERTY(winrt::hstring, ResponseMetaData);
        WINRT_PROPERTY(winrt::hstring, QueryMetaData);
    };

    struct AzureLLMProvider : AzureLLMProviderT<AzureLLMProvider>
    {
        AzureLLMProvider() = default;

        void ClearMessageHistory();
        void SetSystemPrompt(const winrt::hstring& systemPrompt);
        void SetContext(Extension::IContext context);

        winrt::Windows::Foundation::IAsyncOperation<Extension::IResponse> GetResponseAsync(const winrt::hstring& userPrompt);

        void SetAuthentication(const Windows::Foundation::Collections::ValueSet& authValues);
        TYPED_EVENT(AuthChanged, winrt::Microsoft::Terminal::Query::Extension::ILMProvider, winrt::Microsoft::Terminal::Query::Extension::IAuthenticationResult);

        WINRT_PROPERTY(IBrandingData, BrandingData, winrt::make<AzureBranding>());

    private:
        winrt::hstring _azureEndpoint;
        winrt::hstring _azureKey;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };

        Extension::IContext _context;

        winrt::Windows::Data::Json::JsonArray _jsonMessages;

        bool _verifyModelIsValidHelper(const Windows::Data::Json::JsonObject jsonResponse);
    };

    struct AzureResponse : public winrt::implements<AzureResponse, winrt::Microsoft::Terminal::Query::Extension::IResponse>
    {
        AzureResponse(const winrt::hstring& message, const winrt::Microsoft::Terminal::Query::Extension::ErrorTypes errorType) :
            Message{ message },
            ErrorType{ errorType } {}

        til::property<winrt::hstring> Message;
        til::property<winrt::Microsoft::Terminal::Query::Extension::ErrorTypes> ErrorType;
    };
}

namespace winrt::Microsoft::Terminal::Query::Extension::factory_implementation
{
    BASIC_FACTORY(AzureLLMProvider);
}
