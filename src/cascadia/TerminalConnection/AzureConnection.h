// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AzureConnection.g.h"

#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>
#include <cpprest/ws_client.h>
#include <mutex>
#include <condition_variable>

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct AzureConnection : AzureConnectionT<AzureConnection>
    {
        AzureConnection(const uint32_t rows, const uint32_t cols);

        winrt::event_token TerminalOutput(TerminalConnection::TerminalOutputEventArgs const& handler);
        void TerminalOutput(winrt::event_token const& token) noexcept;
        winrt::event_token TerminalDisconnected(TerminalConnection::TerminalDisconnectedEventArgs const& handler);
        void TerminalDisconnected(winrt::event_token const& token) noexcept;
        void Start();
        void WriteInput(hstring const& data);
        void Resize(uint32_t rows, uint32_t columns);
        void Close();

    private:
        winrt::event<TerminalConnection::TerminalOutputEventArgs> _outputHandlers;
        winrt::event<TerminalConnection::TerminalDisconnectedEventArgs> _disconnectHandlers;

        uint32_t _initialRows{};
        uint32_t _initialCols{};
        int _tenantNumber{ -1 };
        int _maxSize;
        std::condition_variable _canProceed;
        std::mutex _tenantNumMutex;

        bool _connected{};
        bool _termConnected{ false };
        bool _needInput{ false };
        std::atomic<bool> _closing{ false };

        wil::unique_handle _hOutputThread;

        static DWORD WINAPI StaticOutputThreadProc(LPVOID lpParameter);
        DWORD _OutputThread();

        const utility::string_t _loginUri{ U("https://login.microsoftonline.com/") };
        const utility::string_t _resourceUri{ U("https://management.azure.com/") };
        const utility::string_t _wantedResource{ U("https://management.core.windows.net/") };
        utility::string_t _tenantID;
        utility::string_t _accessToken;
        utility::string_t _refreshToken;
        utility::string_t _cloudShellUri;
        utility::string_t _terminalID;

        web::json::value _requestHelper(web::http::client::http_client theClient, web::http::http_request theRequest);
        web::json::value _getDeviceCode();
        web::json::value _waitForUser(utility::string_t deviceCode, int pollInterval, int expiresIn);
        web::json::value _getTenants();
        web::json::value _refreshTokens();
        web::json::value _getCloudShellUserSettings();
        utility::string_t _getCloudShell();
        utility::string_t _getTerminal(utility::string_t shellType);
        void _headerHelper(web::http::http_request theRequest);

        web::websockets::client::websocket_client _cloudShellSocket;
    };
}

namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    struct AzureConnection : AzureConnectionT<AzureConnection, implementation::AzureConnection>
    {
    };
}
