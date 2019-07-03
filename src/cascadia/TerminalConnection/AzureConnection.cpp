// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AzureConnection.h"
#include "AzureClientID.h"
#include <sstream>
#include <stdlib.h>

// STARTF_USESTDHANDLES is only defined in WINAPI_PARTITION_DESKTOP
// We're just gonna manually define it for this prototyping code
#ifndef STARTF_USESTDHANDLES
#define STARTF_USESTDHANDLES 0x00000100
#endif

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
        _initialCols{ initialCols },
        _loginUri{ U("https://login.microsoftonline.com/") },
        _resourceUri{ U("https://management.azure.com/") },
        _wantedResource{ U("https://management.core.windows.net/") }
    {
    }

    winrt::event_token AzureConnection::TerminalOutput(Microsoft::Terminal::TerminalConnection::TerminalOutputEventArgs const& handler)
    {
        return _outputHandlers.add(handler);
    }

    void AzureConnection::TerminalOutput(winrt::event_token const& token) noexcept
    {
        _outputHandlers.remove(token);
    }

    winrt::event_token AzureConnection::TerminalDisconnected(Microsoft::Terminal::TerminalConnection::TerminalDisconnectedEventArgs const& handler)
    {
        return _disconnectHandlers.add(handler);
    }

    void AzureConnection::TerminalDisconnected(winrt::event_token const& token) noexcept
    {
        _disconnectHandlers.remove(token);
    }

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

        _connected = true;
    }

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
                    _outputHandlers(winrt::to_hstring("Please enter a number.\r\n"));
                    return;
                }
                if (tenantNum >= _maxSize)
                {
                    _outputHandlers(winrt::to_hstring("Number out of bounds.\r\n"));
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
        // send the message over the websocket
        websocket_outgoing_message msg;
        std::string str = winrt::to_string(data);
        msg.set_utf8_message(str);

        pplx::task<void> msgSent = _cloudShellSocket.send(msg);
        msgSent.wait();
    }

    void AzureConnection::Resize(uint32_t rows, uint32_t columns)
    {
        if (!_connected || !_termConnected)
        {
            _initialRows = rows;
            _initialCols = columns;
        }
        else if (!_closing.load())
        {
            //Initialize client
            http_client terminalClient(_cloudShellUri);

            //Initialize the request
            http_request terminalRequest(L"POST");
            terminalRequest.set_request_uri(L"terminals/" + _terminalID + L"/size?cols=" + std::to_wstring(columns) + L"&rows=" + std::to_wstring(rows) + L"&version=2019-01-01");
            terminalRequest.headers().add(L"Accept", L"application/json");
            terminalRequest.headers().add(L"Content-Type", L"application/json");
            terminalRequest.headers().add(L"Authorization", L"Bearer " + _accessToken);
            terminalRequest.headers().add(L"User-Agent", L"Terminal");
            terminalRequest.set_body(json::value(L""));

            //Send the request
            _requestHelper(terminalClient, terminalRequest);
        }
    }

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
                // close the websocket connection
                pplx::task<void> closed = _cloudShellSocket.close();
                closed.wait();
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

    DWORD AzureConnection::_OutputThread()
    {
        //Azure authentication starts here:
        //step 1a: initiate device code flow
        json::value deviceCodeResponse = _getDeviceCode();

        //step 1b: print the message and store the device code, polling interval and expiry
        utility::string_t message = deviceCodeResponse.at(L"message").as_string();
        _outputHandlers(message + L" This code will expire in 15 minutes.\r\n");
        utility::string_t devCode = deviceCodeResponse.at(L"device_code").as_string();
        int pollInterval = std::stoi(deviceCodeResponse.at(L"interval").as_string());
        int expiresIn = std::stoi(deviceCodeResponse.at(L"expires_in").as_string());

        //step 2: wait for user authentication and obtain the access/refresh tokens
        json::value authenticatedResponse;
        try
        {
            authenticatedResponse = _waitForUser(devCode, pollInterval, expiresIn);
        }
        catch (...)
        {
            _outputHandlers(winrt::to_hstring("Exit."));
            return -1;
        }

        _accessToken = authenticatedResponse.at(L"access_token").as_string();
        _refreshToken = authenticatedResponse.at(L"refresh_token").as_string();

        //step 3: getting the tenants and the required tenant id
        json::value tenantsResponse = _getTenants();
        json::array tenantList = tenantsResponse.at(L"value").as_array();

        if (tenantList.size() == 0)
        {
            _outputHandlers(winrt::to_hstring("Could not find any tenants.\r\n"));
            return -1;
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
                utility::string_t str = L"Tenant " + std::to_wstring(i) + L": " + tenantList.at(i).at(L"displayName").as_string() + L" (" + tenantList.at(i).at(L"tenantId").as_string() + L")\r\n";
                _outputHandlers(str);
            }
            utility::string_t str = L"Please enter the desired tenant number (a number from 0 to " + std::to_wstring((_maxSize - 1)) + L" inclusive).\r\n";
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
                return -1;
            }
            _needInput = false;
            _tenantID = tenantList.at(_tenantNumber).at(L"tenantId").as_string();
        }

        //step 4: refresh token to get a new one specific to the desired tenant ID
        json::value refreshResponse = _refreshTokens();
        _accessToken = refreshResponse.at(L"access_token").as_string();
        _refreshToken = refreshResponse.at(L"refresh_token").as_string();

        //step 5: use new access token to get user's cloud shell settings
        json::value settingsResponse = _getCloudShellUserSettings();
        if (settingsResponse.has_field(L"error"))
        {
            _outputHandlers(winrt::to_hstring("You have not set up your cloud shell account yet. Please go to https://shell.azure.com to set it up.\r\n"));
            return -1;
        }

        //step 6: request for a cloud shell
        _outputHandlers(winrt::to_hstring("Requesting for a cloud shell..."));
        _cloudShellUri = _getCloudShell();
        _outputHandlers(winrt::to_hstring("Succeeded.\r\n"));

        //step 7: request for a terminal for said cloud shell
        //We only support bash for now, so don't bother with the user's preferred shell
        //fyi: we can't call powershell yet because it sends VT sequences we don't support yet
        //utility::string_t shellType = settingsResponse.at(L"properties").at(L"preferredShellType").as_string();
        utility::string_t shellType = L"bash";
        _outputHandlers(winrt::to_hstring("Requesting for a terminal (this might take a while)..."));
        utility::string_t socketUri = _getTerminal(shellType);
        _outputHandlers(winrt::to_hstring("\r\n"));

        //step 8: connecting to said terminal
        pplx::task<void> connReq = _cloudShellSocket.connect(socketUri);
        connReq.wait();

        _termConnected = true;

        while (true)
        {
            //READ FROM WEBSOCKET
            pplx::task<websocket_incoming_message> msgT;
            try
            {
                msgT = _cloudShellSocket.receive();
                msgT.wait();
            }
            catch (...)
            {
                //websocket has been closed
                _termConnected = false;
                if (!_closing.load())
                {
                    _disconnectHandlers();
                    return (DWORD)-1;
                }
                break;
            }
            websocket_incoming_message msg = msgT.get();
            pplx::task<std::string> msgStringT = msg.extract_string();
            std::string msgString = msgStringT.get();

            //Convert to hstring
            auto hstr = winrt::to_hstring(msgString);

            // Pass the output to our registered event handlers
            _outputHandlers(hstr);
        }
        return 0;
    }

    // Helper function to send requests and extract responses as json values
    json::value AzureConnection::_requestHelper(http_client theClient, http_request theRequest)
    {
        pplx::task<http_response> responseT = theClient.request(theRequest);
        responseT.wait();
        http_response response = responseT.get();
        pplx::task<json::value> responseJsonT = response.extract_json();
        responseJsonT.wait();
        return responseJsonT.get();
    }

    // Helper function to start the device code flow
    json::value AzureConnection::_getDeviceCode()
    {
        //Initialize the client
        http_client loginClient(_loginUri);

        //Initialize the request
        http_request commonRequest(L"POST");
        commonRequest.set_request_uri(L"common/oauth2/devicecode");
        utility::string_t body = L"client_id=" + AzureClientID + L"&resource=" + _wantedResource;
        commonRequest.set_body(body, L"application/x-www-form-urlencoded");

        //Send the request and receive the response as a json value
        return _requestHelper(loginClient, commonRequest);
    }

    // Helper function to wait for the user to authenticate using their web browser
    json::value AzureConnection::_waitForUser(const utility::string_t deviceCode, int pollInterval, int expiresIn)
    {
        //Initialize the client
        http_client pollingClient(_loginUri);

        //Continuously send a poll request until the user authenticates
        utility::string_t body = L"grant_type=device_code&resource=" + _wantedResource + L"&client_id=" + AzureClientID + L"&code=" + deviceCode;
        utility::string_t requestUri = L"common/oauth2/token";
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
                Sleep(pollInterval * 1000); //Sleep takes arguments in milliseconds
                continue; //Still waiting for authentication
            }
            else
            {
                _outputHandlers(winrt::to_hstring("Authenticated.\r\n"));
                break; //Authentication is done, break from loop
            }
        }
        if (responseJson.has_field(L"error"))
        {
            throw "Time out.";
        }
        return responseJson;
    }

    // Helper function to acquire the user's Azure tenants
    json::value AzureConnection::_getTenants()
    {
        //Initialize the client
        http_client tenantClient(_resourceUri);

        //Initialize the request
        http_request tenantRequest(L"GET");
        tenantRequest.set_request_uri(L"tenants?api-version=2018-01-01");
        tenantRequest.headers().add(L"Accept", L"application/json");
        tenantRequest.headers().add(L"Authorization", L"Bearer " + _accessToken);
        tenantRequest.headers().add(L"User-Agent", L"Terminal");

        //Send the request and return the response as a json value
        return _requestHelper(tenantClient, tenantRequest);
    }

    json::value AzureConnection::_refreshTokens()
    {
        //Initialize the client
        http_client refreshClient(_loginUri);

        //Initialize the request
        http_request refreshRequest(L"POST");
        refreshRequest.set_request_uri(_tenantID + L"/oauth2/token");
        utility::string_t body = L"client_id=" + AzureClientID + L"&resource=" + _wantedResource + L"&grant_type=refresh_token" + L"&refresh_token=" + _refreshToken;
        refreshRequest.set_body(body, L"application/x-www-form-urlencoded");
        refreshRequest.headers().add(L"User-Agent", L"Terminal");

        //Send the request and return the response as a json value
        return _requestHelper(refreshClient, refreshRequest);
    }

    // Helper function to get the user's cloud shell settings
    json::value AzureConnection::_getCloudShellUserSettings()
    {
        //Initialize client
        http_client settingsClient(_resourceUri);

        //Initialize request
        http_request settingsRequest(L"GET");
        settingsRequest.set_request_uri(L"providers/Microsoft.Portal/userSettings/cloudconsole?api-version=2018-10-01");
        settingsRequest.headers().add(L"Accept", L"application/json");
        settingsRequest.headers().add(L"Content-Type", L"application/json");
        settingsRequest.headers().add(L"Authorization", L"Bearer " + _accessToken);
        settingsRequest.headers().add(L"User-Agent", L"Terminal");

        return _requestHelper(settingsClient, settingsRequest);
    }

    utility::string_t AzureConnection::_getCloudShell()
    {
        //Initialize client
        http_client cloudShellClient(_resourceUri);

        //Initialize request
        http_request shellRequest(L"PUT");
        shellRequest.set_request_uri(L"providers/Microsoft.Portal/consoles/default?api-version=2018-10-01");
        shellRequest.headers().add(L"Accept", L"application/json");
        shellRequest.headers().add(L"Content-Type", L"application/json");
        shellRequest.headers().add(L"Authorization", L"Bearer " + _accessToken);
        shellRequest.headers().add(L"User-Agent", L"Terminal");
        json::value innerBody = json::value::parse(U("{ \"osType\" : \"linux\" }"));
        json::value body;
        body[U("properties")] = innerBody;
        shellRequest.set_body(body);

        //Send the request and get the response as a json value
        json::value cloudShell = _requestHelper(cloudShellClient, shellRequest);

        //Return the uri
        return cloudShell.at(L"properties").at(L"uri").as_string() + L"/";
    }

    utility::string_t AzureConnection::_getTerminal(utility::string_t shellType)
    {
        //Initialize client
        http_client terminalClient(_cloudShellUri);

        //Initialize the request
        http_request terminalRequest(L"POST");
        terminalRequest.set_request_uri(L"terminals?cols=" + std::to_wstring(_initialCols) + L"&rows=" + std::to_wstring(_initialRows) + L"&version=2019-01-01&shell=" + shellType);
        terminalRequest.headers().add(L"Accept", L"application/json");
        terminalRequest.headers().add(L"Content-Type", L"application/json");
        terminalRequest.headers().add(L"Authorization", L"Bearer " + _accessToken);
        terminalRequest.headers().add(L"User-Agent", L"Terminal");
        terminalRequest.set_body(json::value(L""));

        //Send the request and get the response as a json value
        json::value terminalResponse = _requestHelper(terminalClient, terminalRequest);
        _terminalID = terminalResponse.at(L"id").as_string();

        //Return the uri
        return terminalResponse.at(L"socketUri").as_string();
    }
}
