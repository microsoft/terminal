// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AzureConnection.g.h"

#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>
#include <cpprest/ws_client.h>
#include <mutex>
#include <condition_variable>

#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct AzureConnection : AzureConnectionT<AzureConnection>
    {
        static bool IsAzureConnectionAvailable();
        AzureConnection(const uint32_t rows, const uint32_t cols);

        winrt::event_token TerminalOutput(TerminalConnection::TerminalOutputEventArgs const& handler);
        void TerminalOutput(winrt::event_token const& token) noexcept;
        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

        UNTYPED_EVENT(StateChanged, StateChangedEventArgs);

    private:
        winrt::event<TerminalConnection::TerminalOutputEventArgs> _outputHandlers;

        uint32_t _initialRows{};
        uint32_t _initialCols{};
        int _storedNumber{ -1 };
        int _maxStored;
        int _tenantNumber{ -1 };
        int _maxSize;
        std::condition_variable _canProceed;
        std::mutex _commonMutex;

        enum class State
        {
            AccessStored,
            DeviceFlow,
            TenantChoice,
            StoreTokens,
            TermConnecting,
            TermConnected,
            NoConnect
        };

        State _state{ State::AccessStored };

        std::optional<bool> _store;
        std::optional<bool> _removeOrNew;

        bool _connected{};
        std::atomic<bool> _closing{ false };

        wil::unique_handle _hOutputThread;

        static DWORD WINAPI StaticOutputThreadProc(LPVOID lpParameter);
        DWORD _OutputThread();
        HRESULT _AccessHelper();
        HRESULT _DeviceFlowHelper();
        HRESULT _TenantChoiceHelper();
        HRESULT _StoreHelper();
        HRESULT _ConnectHelper();

        const utility::string_t _loginUri{ U("https://login.microsoftonline.com/") };
        const utility::string_t _resourceUri{ U("https://management.azure.com/") };
        const utility::string_t _wantedResource{ U("https://management.core.windows.net/") };
        int _expireLimit{ 2700 };
        web::json::value _tenantList;
        utility::string_t _displayName;
        utility::string_t _tenantID;
        utility::string_t _accessToken;
        utility::string_t _refreshToken;
        int _expiry;
        utility::string_t _cloudShellUri;
        utility::string_t _terminalID;

        void _WriteStringWithNewline(const winrt::hstring& str);
        web::json::value _RequestHelper(web::http::client::http_client theClient, web::http::http_request theRequest);
        web::json::value _GetDeviceCode();
        web::json::value _WaitForUser(utility::string_t deviceCode, int pollInterval, int expiresIn);
        web::json::value _GetTenants();
        web::json::value _RefreshTokens();
        web::json::value _GetCloudShellUserSettings();
        utility::string_t _GetCloudShell();
        utility::string_t _GetTerminal(utility::string_t shellType);
        void _HeaderHelper(web::http::http_request theRequest);
        void _StoreCredential();
        void _RemoveCredentials();

        web::websockets::client::websocket_client _cloudShellSocket;
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct AzureConnection : AzureConnectionT<AzureConnection, implementation::AzureConnection>
    {
    };
}
