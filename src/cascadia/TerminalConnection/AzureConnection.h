// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AzureConnection.g.h"

#include <mutex>
#include <condition_variable>

#include "ConnectionStateHolder.h"
#include "AzureClient.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct AzureConnection : AzureConnectionT<AzureConnection>, ConnectionStateHolder<AzureConnection>
    {
        static winrt::guid ConnectionType() noexcept;
        static bool IsAzureConnectionAvailable() noexcept;

        AzureConnection() = default;
        void Initialize(const Windows::Foundation::Collections::ValueSet& settings);

        void Start();
        void WriteInput(const hstring& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        WINRT_CALLBACK(TerminalOutput, TerminalOutputHandler);

    private:
        til::CoordType _initialRows{};
        til::CoordType _initialCols{};

        enum class AzureState
        {
            AccessStored,
            DeviceFlow,
            TenantChoice,
            StoreTokens,
            TermConnecting,
            TermConnected,
        };

        AzureState _state{ AzureState::AccessStored };

        wil::unique_handle _hOutputThread;

        DWORD _OutputThread();
        void _RunAccessState();
        void _RunDeviceFlowState();
        void _RunTenantChoiceState();
        void _RunStoreState();
        void _RunConnectState();

        static constexpr std::wstring_view _loginUri{ L"https://login.microsoftonline.com/" };
        static constexpr std::wstring_view _resourceUri{ L"https://management.azure.com/" };
        static constexpr std::wstring_view _wantedResource{ L"https://management.core.windows.net/" };
        const int _expireLimit{ 2700 };
        winrt::hstring _accessToken;
        winrt::hstring _refreshToken;
        int _expiry{ 0 };
        winrt::hstring _cloudShellUri;
        winrt::hstring _terminalID;

        std::vector<::Microsoft::Terminal::Azure::Tenant> _tenantList;
        std::optional<::Microsoft::Terminal::Azure::Tenant> _currentTenant;

        void _WriteStringWithNewline(const std::wstring_view str);
        void _WriteCaughtExceptionRecord();
        winrt::Windows::Data::Json::JsonObject _SendRequestReturningJson(std::wstring_view uri, const winrt::Windows::Web::Http::IHttpContent& content = nullptr, winrt::Windows::Web::Http::HttpMethod method = nullptr);
        void _setAccessToken(std::wstring_view accessToken);
        winrt::Windows::Data::Json::JsonObject _GetDeviceCode();
        winrt::Windows::Data::Json::JsonObject _WaitForUser(const winrt::hstring& deviceCode, int pollInterval, int expiresIn);
        void _PopulateTenantList();
        void _RefreshTokens();
        winrt::Windows::Data::Json::JsonObject _GetCloudShellUserSettings();
        winrt::hstring _GetCloudShell();
        winrt::hstring _GetTerminal(const winrt::hstring& shellType);
        void _StoreCredential();
        void _RemoveCredentials();

        enum class InputMode
        {
            None = 0,
            Line
        };
        InputMode _currentInputMode{ InputMode::None };
        std::wstring _userInput;
        std::condition_variable _inputEvent;
        std::mutex _inputMutex;

        std::optional<std::wstring> _ReadUserInput(InputMode mode);

        winrt::Windows::Web::Http::HttpClient _httpClient{ nullptr };
        wil::unique_winhttp_hinternet _socketSessionHandle;
        wil::unique_winhttp_hinternet _socketConnectionHandle;
        wil::unique_winhttp_hinternet _webSocket;

        til::u8state _u8State{};
        std::wstring _u16Str;
        std::array<char, 4096> _buffer{};

        static winrt::hstring _ParsePreferredShellType(const winrt::Windows::Data::Json::JsonObject& settingsResponse);
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(AzureConnection);
}
