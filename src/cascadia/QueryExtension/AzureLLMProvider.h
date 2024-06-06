// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AzureLLMProvider.g.h"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    struct AzureLLMProvider : AzureLLMProviderT<AzureLLMProvider>
    {
        AzureLLMProvider(winrt::hstring endpoint, winrt::hstring key);
        void Initialize();

        WINRT_PROPERTY(winrt::hstring, Thing);

    private:
        winrt::hstring _AIEndpoint;
        winrt::hstring _AIKey;
        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };

        winrt::Windows::Data::Json::JsonArray _jsonMessages;
    };
}

namespace winrt::Microsoft::Terminal::Query::Extension::factory_implementation
{
    BASIC_FACTORY(AzureLLMProvider);
}
