// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AzureLLMProvider.h"
#include "../../types/inc/utils.hpp"
#include "LibraryResources.h"

#include "AzureLLMProvider.g.cpp"

namespace winrt::Microsoft::Terminal::Query::Extension::implementation
{
    AzureLLMProvider::AzureLLMProvider(winrt::hstring endpoint, winrt::hstring key)
    {
        _AIEndpoint = endpoint;
        _AIKey = key;
        _httpClient = winrt::Windows::Web::Http::HttpClient{};
        _httpClient.DefaultRequestHeaders().Accept().TryParseAdd(L"application/json");
        _httpClient.DefaultRequestHeaders().Append(L"api-key", _AIKey);
    }

    void AzureLLMProvider::Initialize()
    {
        _Thing = L"ayy lmao";
    }
}
