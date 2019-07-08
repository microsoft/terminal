// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AzureConnection.h"
#include "AzureClientID.h"
#include "AzureConnectionStrings.h"
#include <sstream>
#include <stdlib.h>

#include "AzureConnection.g.cpp"

#include "../../types/inc/Utils.hpp"

using namespace ::Microsoft::Console;

using namespace utility;
using namespace web;
using namespace web::json;
using namespace web::http;
using namespace web::http::client;
using namespace web::websockets::client;
using namespace concurrency::streams;

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    AzureConnection::AzureConnection(const uint32_t initialRows, const uint32_t initialCols) :
        _initialRows{ initialRows },
        _initialCols{ initialCols }
    {
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - registers an output event handler
    // Arguments:
    // - the handler
    // Return value:
    // - the event token for the handler
    winrt::event_token AzureConnection::TerminalOutput(Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs const& handler)
    {
        return _outputHandlers.add(handler);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - revokes an output event handler
    // Arguments:
    // - the event token for the handler
    void AzureConnection::TerminalOutput(winrt::event_token const& token) noexcept
    {
        _outputHandlers.remove(token);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - registers a terminal-disconnected event handler
    // Arguments:
    // - the handler
    // Return value:
    // - the event token for the handler
    winrt::event_token AzureConnection::TerminalDisconnected(Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs const& handler)
    {
        return _disconnectHandlers.add(handler);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - revokes a terminal-disconnected event handler
    // Arguments:
    // - the event token for the handler
    void AzureConnection::TerminalDisconnected(winrt::event_token const& token) noexcept
    {
        _disconnectHandlers.remove(token);
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - creates the output thread (where we will do the authentication and actually connect to Azure)
    void AzureConnection::Start()
    {
        // Create our own output handling thread
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(nullptr,
                                          0,
                                          StaticOutputThreadProc,
                                          this,
                                          0,
                                          nullptr));

        THROW_LAST_ERROR_IF_NULL(_hOutputThread);

        _connected = true;
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - writes input over the connection
    // - (in the case of multiple tenants, we will need input from the user for which tenant they
    // -  wish to connect to, and that input is handled here too)
    // Arguments:
    // the user's input
    void AzureConnection::WriteInput(hstring const& data)
    {
        if (!_connected || _closing.load())
        {
            return;
        }

        // This case only happens when the user has multiple tenants
        // and we require their input for which tenant they want
        if (_needInput)
        {
            int tenantNum = -1;
            while (tenantNum < 0 || tenantNum >= _maxSize)
            {
                if (_closing.load())
                {
                    _needInput = false;
                    return;
                }
                try
                {
                    tenantNum = std::stoi(winrt::to_string(data));
                }
                catch (...)
                {
                    _outputHandlers(winrt::to_hstring(nonNumberError));
                    return;
                }
                if (tenantNum >= _maxSize)
                {
                    _outputHandlers(winrt::to_hstring(numOutOfBoundsError));
                    return;
                }
            }
            std::lock_guard<std::mutex> lg{ _tenantNumMutex };
            _tenantNumber = tenantNum;
            _canProceed.notify_one();
            return;
        }

        if (!_termConnected)
        {
            return;
        }
        // Send the message over the websocket
        websocket_outgoing_message msg;
        const auto str = winrt::to_string(data);
        msg.set_utf8_message(str);

        auto msgSentTask = _cloudShellSocket.send(msg);
        msgSentTask.wait();
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - resizes the terminal
    // Arguments:
    // - the new rows/cols values
    void AzureConnection::Resize(uint32_t rows, uint32_t columns)
    {
        if (!_connected || !_termConnected)
        {
            _initialRows = rows;
            _initialCols = columns;
        }
        else if (!_closing.load())
        {
            // Initialize client
            http_client terminalClient(_cloudShellUri);

            // Initialize the request
            http_request terminalRequest(L"POST");
            terminalRequest.set_request_uri(L"terminals/" + _terminalID + L"/size?cols=" + std::to_wstring(columns) + L"&rows=" + std::to_wstring(rows) + L"&version=2019-01-01");
            _headerHelper(terminalRequest);
            terminalRequest.set_body(json::value(L""));

            // Send the request
            _requestHelper(terminalClient, terminalRequest);
        }
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - closes the websocket connection and the output thread
    void AzureConnection::Close()
    {
        if (!_connected)
        {
            return;
        }

        if (!_closing.exchange(true))
        {
            _canProceed.notify_all();
            if (_termConnected)
            {
                // Close the websocket connection
                auto closedTask = _cloudShellSocket.close();
                closedTask.wait();
            }

            // Tear down our output thread
            WaitForSingleObject(_hOutputThread.get(), INFINITE);
            _hOutputThread.reset();
        }
    }

    DWORD WINAPI AzureConnection::StaticOutputThreadProc(LPVOID lpParameter)
    {
        AzureConnection* const pInstance = (AzureConnection*)lpParameter;
        return pInstance->_OutputThread();
    }

    // Method description:
    // - this is the output thread, where we initiate the connection to Azure
    //  and establish a websocket connection
    // Return value:
    // - return status
    DWORD AzureConnection::_OutputThread()
    {
        // Azure authentication starts here:
        // Step 1a: initiate device code flow
        const auto deviceCodeResponse = _getDeviceCode();

        // Step 1b: print the message and store the device code, polling interval and expiry
        const auto message = deviceCodeResponse.at(L"message").as_string();
        _outputHandlers(message + codeExpiry);
        const auto devCode = deviceCodeResponse.at(L"device_code").as_string();
        const auto pollInterval = std::stoi(deviceCodeResponse.at(L"interval").as_string());
        const auto expiresIn = std::stoi(deviceCodeResponse.at(L"expires_in").as_string());

        // Step 2: wait for user authentication and obtain the access/refresh tokens
        json::value authenticatedResponse;
        try
        {
            authenticatedResponse = _waitForUser(devCode, pollInterval, expiresIn);
        }
        catch (...)
        {
            _outputHandlers(winrt::to_hstring("Exit."));
            return E_FAIL;
        }

        _accessToken = authenticatedResponse.at(L"access_token").as_string();
        _refreshToken = authenticatedResponse.at(L"refresh_token").as_string();

        // Step 3: getting the tenants and the required tenant id
        const auto tenantsResponse = _getTenants();
        const auto tenantList = tenantsResponse.at(L"value").as_array();

        if (tenantList.size() == 0)
        {
            _outputHandlers(winrt::to_hstring(noTenants));
            return E_FAIL;
        }
        else if (tenantList.size() == 1)
        {
            _tenantID = tenantList.at(0).at(L"tenantId").as_string();
        }
        else
        {
            _maxSize = tenantList.size();
            for (int i = 0; i < _maxSize; i++)
            {
                auto str = L"Tenant " + std::to_wstring(i) + L": " + tenantList.at(i).at(L"displayName").as_string() + L" (" + tenantList.at(i).at(L"tenantId").as_string() + L")\r\n";
                _outputHandlers(str);
            }
            const auto str = L"Please enter the desired tenant number (a number from 0 to " + std::to_wstring((_maxSize - 1)) + L" inclusive).\r\n";
            _outputHandlers(str);
            _needInput = true;
            // Use a lock to wait for the user to input a valid number
            std::unique_lock<std::mutex> tenantNumberLock{ _tenantNumMutex };
            _canProceed.wait(tenantNumberLock, [=]() {
                return (_tenantNumber >= 0 && _tenantNumber < _maxSize) || _closing.load();
            });
            // User might have closed the tab while we waited for input
            if (_closing.load())
            {
                return S_FALSE;
            }
            _needInput = false;
            _tenantID = tenantList.at(_tenantNumber).at(L"tenantId").as_string();
        }

        // Step 4: refresh token to get a new one specific to the desired tenant ID
        const auto refreshResponse = _refreshTokens();
        _accessToken = refreshResponse.at(L"access_token").as_string();
        _refreshToken = refreshResponse.at(L"refresh_token").as_string();

        // Step 5: use new access token to get user's cloud shell settings
        const auto settingsResponse = _getCloudShellUserSettings();
        if (settingsResponse.has_field(L"error"))
        {
            _outputHandlers(winrt::to_hstring(noCloudAccount));
            return E_FAIL;
        }

        // Step 6: request for a cloud shell
        _outputHandlers(winrt::to_hstring(requestingCloud));
        _cloudShellUri = _getCloudShell();
        _outputHandlers(winrt::to_hstring(success));

        // Step 7: request for a terminal for said cloud shell
        // We only support bash for now, so don't bother with the user's preferred shell
        // fyi: we can't call powershell yet because it sends VT sequences we don't support yet
        // TODO: GitHub #1883
        //const auto shellType = settingsResponse.at(L"properties").at(L"preferredShellType").as_string();
        const auto shellType = L"bash";
        _outputHandlers(winrt::to_hstring(requestingTerminal));
        const auto socketUri = _getTerminal(shellType);
        _outputHandlers(winrt::to_hstring("\r\n"));

        // Step 8: connecting to said terminal
        const auto connReqTask = _cloudShellSocket.connect(socketUri);
        connReqTask.wait();

        _termConnected = true;

        while (true)
        {
            // Read from websocket
            pplx::task<websocket_incoming_message> msgT;
            try
            {
                msgT = _cloudShellSocket.receive();
                msgT.wait();
            }
            catch (...)
            {
                // Websocket has been closed
                _termConnected = false;
                if (!_closing.load())
                {
                    _disconnectHandlers();
                    return S_FALSE;
                }
                break;
            }
            auto msg = msgT.get();
            auto msgStringTask = msg.extract_string();
            auto msgString = msgStringTask.get();

            // Convert to hstring
            const auto hstr = winrt::to_hstring(msgString);

            // Pass the output to our registered event handlers
            _outputHandlers(hstr);
        }
        return S_OK;
    }

    // Method description:
    // - helper function to send requests and extract responses as json values
    // Arguments:
    // - a http_client
    // - a http_request for the client to send
    // Return value:
    // - the response from the server as a json value
    json::value AzureConnection::_requestHelper(http_client theClient, http_request theRequest)
    {
        const auto responseTask = theClient.request(theRequest);
        responseTask.wait();
        const auto response = responseTask.get();
        const auto responseJsonTask = response.extract_json();
        responseJsonTask.wait();
        return responseJsonTask.get();
    }

    // Method description:
    // - helper function to start the device code flow
    // Return value:
    // - the response to the device code flow initiation
    json::value AzureConnection::_getDeviceCode()
    {
        // Initialize the client
        http_client loginClient(_loginUri);

        // Initialize the request
        http_request commonRequest(L"POST");
        commonRequest.set_request_uri(L"common/oauth2/devicecode");
        const auto body = L"client_id=" + AzureClientID + L"&resource=" + _wantedResource;
        commonRequest.set_body(body, L"application/x-www-form-urlencoded");

        // Send the request and receive the response as a json value
        return _requestHelper(loginClient, commonRequest);
    }

    // Method description:
    // - helper function to wait for the user to authenticate using their web browser
    // Arguments:
    // - the device code that would have been received when authentication was initiated
    // - the polling interval duration
    // - the duration the code is still valid for
    // Return value:
    // - if authentication is done successfully, then return the response from the server
    // - else, throw an exception
    json::value AzureConnection::_waitForUser(const utility::string_t deviceCode, int pollInterval, int expiresIn)
    {
        // Initialize the client
        http_client pollingClient(_loginUri);

        // Continuously send a poll request until the user authenticates
        const auto body = L"grant_type=device_code&resource=" + _wantedResource + L"&client_id=" + AzureClientID + L"&code=" + deviceCode;
        const auto requestUri = L"common/oauth2/token";
        json::value responseJson;
        for (int count = 0; count < expiresIn / pollInterval; count++)
        {
            // User might close the tab while we wait for them to authenticate, this case handles that
            if (_closing.load())
            {
                throw "Tab closed.";
            }
            http_request pollRequest(L"POST");
            pollRequest.set_request_uri(requestUri);
            pollRequest.set_body(body, L"application/x-www-form-urlencoded");

            responseJson = _requestHelper(pollingClient, pollRequest);

            if (responseJson.has_field(L"error"))
            {
                Sleep(pollInterval * 1000); // Sleep takes arguments in milliseconds
                continue; // Still waiting for authentication
            }
            else
            {
                _outputHandlers(winrt::to_hstring("Authenticated.\r\n"));
                break; // Authentication is done, break from loop
            }
        }
        if (responseJson.has_field(L"error"))
        {
            throw "Time out.";
        }
        return responseJson;
    }

    // Method description:
    // - helper function to acquire the user's Azure tenants
    // Return value:
    // - the response which contains a list of the user's Azure tenants
    json::value AzureConnection::_getTenants()
    {
        // Initialize the client
        http_client tenantClient(_resourceUri);

        // Initialize the request
        http_request tenantRequest(L"GET");
        tenantRequest.set_request_uri(L"tenants?api-version=2018-01-01");
        _headerHelper(tenantRequest);

        // Send the request and return the response as a json value
        return _requestHelper(tenantClient, tenantRequest);
    }

    // Method description:
    // - helper function to refresh the access/refresh tokens
    // Return value:
    // - the response with the new tokens
    json::value AzureConnection::_refreshTokens()
    {
        // Initialize the client
        http_client refreshClient(_loginUri);

        // Initialize the request
        http_request refreshRequest(L"POST");
        refreshRequest.set_request_uri(_tenantID + L"/oauth2/token");
        const auto body = L"client_id=" + AzureClientID + L"&resource=" + _wantedResource + L"&grant_type=refresh_token" + L"&refresh_token=" + _refreshToken;
        refreshRequest.set_body(body, L"application/x-www-form-urlencoded");
        refreshRequest.headers().add(L"User-Agent", L"Terminal");

        // Send the request and return the response as a json value
        return _requestHelper(refreshClient, refreshRequest);
    }

    // Method description:
    // - helper function to get the user's cloud shell settings
    // Return value:
    // - the user's cloud shell settings
    json::value AzureConnection::_getCloudShellUserSettings()
    {
        // Initialize client
        http_client settingsClient(_resourceUri);

        // Initialize request
        http_request settingsRequest(L"GET");
        settingsRequest.set_request_uri(L"providers/Microsoft.Portal/userSettings/cloudconsole?api-version=2018-10-01");
        _headerHelper(settingsRequest);

        return _requestHelper(settingsClient, settingsRequest);
    }

    // Method description:
    // - helper function to request for a cloud shell
    // Return value:
    // - the uri for the cloud shell
    utility::string_t AzureConnection::_getCloudShell()
    {
        // Initialize client
        http_client cloudShellClient(_resourceUri);

        // Initialize request
        http_request shellRequest(L"PUT");
        shellRequest.set_request_uri(L"providers/Microsoft.Portal/consoles/default?api-version=2018-10-01");
        _headerHelper(shellRequest);
        const auto innerBody = json::value::parse(U("{ \"osType\" : \"linux\" }"));
        json::value body;
        body[U("properties")] = innerBody;
        shellRequest.set_body(body);

        // Send the request and get the response as a json value
        const auto cloudShell = _requestHelper(cloudShellClient, shellRequest);

        // Return the uri
        return cloudShell.at(L"properties").at(L"uri").as_string() + L"/";
    }

    // Method description:
    // - helper function to request for a terminal
    // Return value:
    // - the uri for the terminal
    utility::string_t AzureConnection::_getTerminal(utility::string_t shellType)
    {
        // Initialize client
        http_client terminalClient(_cloudShellUri);

        // Initialize the request
        http_request terminalRequest(L"POST");
        terminalRequest.set_request_uri(L"terminals?cols=" + std::to_wstring(_initialCols) + L"&rows=" + std::to_wstring(_initialRows) + L"&version=2019-01-01&shell=" + shellType);
        _headerHelper(terminalRequest);
        terminalRequest.set_body(json::value(L""));

        // Send the request and get the response as a json value
        const auto terminalResponse = _requestHelper(terminalClient, terminalRequest);
        _terminalID = terminalResponse.at(L"id").as_string();

        // Return the uri
        return terminalResponse.at(L"socketUri").as_string();
    }

    // Method description:
    // - helper function to set the headers of a http_request
    // Arguments:
    // - the http_request
    void AzureConnection::_headerHelper(http_request theRequest)
    {
        theRequest.headers().add(L"Accept", L"application/json");
        theRequest.headers().add(L"Content-Type", L"application/json");
        theRequest.headers().add(L"Authorization", L"Bearer " + _accessToken);
        theRequest.headers().add(L"User-Agent", L"Terminal");
    }
}
