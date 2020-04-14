// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

// We have to define GSL here, not PCH
// because TelnetConnection has a conflicting GSL implementation.
#include <gsl/gsl>

#include "AzureConnection.h"
#include "AzureClientID.h"
#include <sstream>
#include <stdlib.h>
#include <LibraryResources.h>
#include <unicode.hpp>

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
using namespace winrt::Windows::Security::Credentials;

static constexpr int CurrentCredentialVersion = 1;
static constexpr auto PasswordVaultResourceName = L"Terminal";
static constexpr auto HttpUserAgent = L"Terminal/0.0";

#define FAILOUT_IF_OPTIONAL_EMPTY(optional) \
    do                                      \
    {                                       \
        if (!((optional).has_value()))      \
        {                                   \
            return E_FAIL;                  \
        }                                   \
    } while (0, 0)

static constexpr int USER_INPUT_COLOR = 93; // yellow - the color of something the user can type
static constexpr int USER_INFO_COLOR = 97; // white - the color of clarifying information

static inline std::wstring _colorize(const unsigned int colorCode, const std::wstring_view text)
{
    return wil::str_printf<std::wstring>(L"\x1b[%um%.*s\x1b[m", colorCode, gsl::narrow_cast<size_t>(text.size()), text.data());
}

// Takes N resource names, loads the first one as a format string, and then
// loads all the remaining ones into the %s arguments in the first one after
// colorizing them in the USER_INPUT_COLOR.
// This is intended to be used to drop UserEntry resources into an existing string.
template<typename... Args>
static inline std::wstring _formatResWithColoredUserInputOptions(const std::wstring_view resourceKey, Args&&... args)
{
    return wil::str_printf<std::wstring>(GetLibraryResourceString(resourceKey).data(), (_colorize(USER_INPUT_COLOR, GetLibraryResourceString(args)).data())...);
}

static inline std::wstring _formatTenantLine(int tenantNumber, const std::wstring_view tenantName, const std::wstring_view tenantID)
{
    return wil::str_printf<std::wstring>(RS_(L"AzureIthTenant").data(), _colorize(USER_INPUT_COLOR, std::to_wstring(tenantNumber)).data(), _colorize(USER_INFO_COLOR, tenantName).data(), tenantID.data());
}

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    // This function exists because the clientID only gets added by the release pipelines
    // and is not available on local builds, so we want to be able to make sure we don't
    // try to make an Azure connection if its a local build
    bool AzureConnection::IsAzureConnectionAvailable() noexcept
    {
        return (AzureClientID != L"0");
    }

    AzureConnection::AzureConnection(const uint32_t initialRows, const uint32_t initialCols) :
        _initialRows{ initialRows },
        _initialCols{ initialCols },
        _expiry{}
    {
    }

    // Method description:
    // - helper that will write an unterminated string (generally, from a resource) to the output stream.
    // Arguments:
    // - str: the string to write.
    void AzureConnection::_WriteStringWithNewline(const std::wstring_view str)
    {
        _TerminalOutputHandlers(str + L"\r\n");
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

        _transitionToState(ConnectionState::Connecting);
    }

    std::optional<std::wstring> AzureConnection::_ReadUserInput(InputMode mode)
    {
        std::unique_lock<std::mutex> inputLock{ _inputMutex };

        if (_isStateAtOrBeyond(ConnectionState::Closing))
        {
            return std::nullopt;
        }

        _currentInputMode = mode;

        _TerminalOutputHandlers(L"\x1b[92m"); // Make prompted user input green

        _inputEvent.wait(inputLock, [this, mode]() {
            return _currentInputMode != mode || _isStateAtOrBeyond(ConnectionState::Closing);
        });

        _TerminalOutputHandlers(L"\x1b[m");

        if (_isStateAtOrBeyond(ConnectionState::Closing))
        {
            return std::nullopt;
        }

        std::wstring readInput{};
        _userInput.swap(readInput);
        return readInput;
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - handles the different possible inputs in the different states
    // Arguments:
    // the user's input
    void AzureConnection::WriteInput(hstring const& data)
    {
        // We read input while connected AND connecting.
        if (!_isStateOneOf(ConnectionState::Connected, ConnectionState::Connecting))
        {
            return;
        }

        if (_state == AzureState::TermConnected)
        {
            // If we're connected, we don't need to do any fun input shenanigans.
            websocket_outgoing_message msg;
            const auto str = winrt::to_string(data);
            msg.set_utf8_message(str);

            _cloudShellSocket.send(msg).get();
            return;
        }

        std::lock_guard<std::mutex> lock{ _inputMutex };
        if (data.size() > 0 && (gsl::at(data, 0) == UNICODE_BACKSPACE || gsl::at(data, 0) == UNICODE_DEL)) // BS or DEL
        {
            if (_userInput.size() > 0)
            {
                _userInput.pop_back();
                _TerminalOutputHandlers(L"\x08 \x08"); // overstrike the character with a space
            }
        }
        else
        {
            _TerminalOutputHandlers(data); // echo back

            switch (_currentInputMode)
            {
            case InputMode::Line:
                if (data.size() > 0 && gsl::at(data, 0) == UNICODE_CARRIAGERETURN)
                {
                    _TerminalOutputHandlers(L"\r\n"); // we probably got a \r, so we need to advance to the next line.
                    _currentInputMode = InputMode::None; // toggling the mode indicates completion
                    _inputEvent.notify_one();
                    break;
                }
                [[fallthrough]];
            default:
                std::copy(data.cbegin(), data.cend(), std::back_inserter(_userInput));
                break;
            }
        }
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - resizes the terminal
    // Arguments:
    // - the new rows/cols values
    void AzureConnection::Resize(uint32_t rows, uint32_t columns)
    {
        if (!_isConnected())
        {
            _initialRows = rows;
            _initialCols = columns;
        }
        else // We only transition to Connected when we've established the websocket.
        {
            // Initialize client
            http_client terminalClient(_cloudShellUri);

            // Initialize the request
            http_request terminalRequest(L"POST");
            terminalRequest.set_request_uri(L"terminals/" + _terminalID + L"/size?cols=" + std::to_wstring(columns) + L"&rows=" + std::to_wstring(rows) + L"&version=2019-01-01");
            _HeaderHelper(terminalRequest);
            terminalRequest.set_body(json::value(L""));

            // Send the request
            const auto response = _RequestHelper(terminalClient, terminalRequest);
        }
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - closes the websocket connection and the output thread
    void AzureConnection::Close()
    {
        if (_transitionToState(ConnectionState::Closing))
        {
            _inputEvent.notify_all();

            if (_state == AzureState::TermConnected)
            {
                // Close the websocket connection
                auto closedTask = _cloudShellSocket.close();
                closedTask.wait();
            }

            if (_hOutputThread)
            {
                // Tear down our output thread
                WaitForSingleObject(_hOutputThread.get(), INFINITE);
                _hOutputThread.reset();
            }

            _transitionToState(ConnectionState::Closed);
        }
    }

    // Method description:
    // - This method returns a tenant's ID and display name (if one is available).
    //   If there is no display name, a placeholder is returned in its stead.
    // Arguments:
    // - tenant - the unparsed tenant
    // Return value:
    // - a tuple containing the ID and display name of the tenant.
    static std::tuple<utility::string_t, utility::string_t> _crackTenant(const json::value& tenant)
    {
        auto tenantId{ tenant.at(L"tenantId").as_string() };
        std::wstring displayName{ tenant.has_string_field(L"displayName") ? tenant.at(L"displayName").as_string() : static_cast<std::wstring>(RS_(L"AzureUnknownTenantName")) };
        return { tenantId, displayName };
    }

    // Method description:
    // - this method bridges the thread to the Azure connection instance
    // Arguments:
    // - lpParameter: the Azure connection parameter
    // Return value:
    // - the exit code of the thread
    DWORD WINAPI AzureConnection::StaticOutputThreadProc(LPVOID lpParameter)
    {
        AzureConnection* const pInstance = static_cast<AzureConnection*>(lpParameter);
        if (pInstance)
        {
            return pInstance->_OutputThread();
        }
        return gsl::narrow_cast<DWORD>(E_INVALIDARG);
    }

    // Method description:
    // - this is the output thread, where we initiate the connection to Azure
    //  and establish a websocket connection
    // Return value:
    // - return status
    DWORD AzureConnection::_OutputThread()
    {
        while (true)
        {
            if (_isStateAtOrBeyond(ConnectionState::Closing))
            {
                // If we enter a new state while closing, just bail.
                return S_FALSE;
            }

            try
            {
                switch (_state)
                {
                // Initial state, check if the user has any stored connection settings and allow them to login with those
                // or allow them to login with a different account or allow them to remove the saved settings
                case AzureState::AccessStored:
                {
                    RETURN_IF_FAILED(_AccessHelper());
                    break;
                }
                // User has no saved connection settings or has opted to login with a different account
                // Azure authentication happens here
                case AzureState::DeviceFlow:
                {
                    RETURN_IF_FAILED(_DeviceFlowHelper());
                    break;
                }
                // User has multiple tenants in their Azure account, they need to choose which one to connect to
                case AzureState::TenantChoice:
                {
                    RETURN_IF_FAILED(_TenantChoiceHelper());
                    break;
                }
                // Ask the user if they want to save these connection settings for future logins
                case AzureState::StoreTokens:
                {
                    RETURN_IF_FAILED(_StoreHelper());
                    break;
                }
                // Connect to Azure, we only get here once we have everything we need (tenantID, accessToken, refreshToken)
                case AzureState::TermConnecting:
                {
                    RETURN_IF_FAILED(_ConnectHelper());
                    break;
                }
                // We are connected, continuously read from the websocket until its closed
                case AzureState::TermConnected:
                {
                    _transitionToState(ConnectionState::Connected);
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
                            // Websocket has been closed; consider it a graceful exit?
                            // This should result in our termination.
                            if (_transitionToState(ConnectionState::Closed))
                            {
                                // End the output thread.
                                return S_FALSE;
                            }
                        }

                        auto msg = msgT.get();
                        auto msgStringTask = msg.extract_string();
                        auto msgString = msgStringTask.get();

                        // Convert to hstring
                        const auto hstr = winrt::to_hstring(msgString);

                        // Pass the output to our registered event handlers
                        _TerminalOutputHandlers(hstr);
                    }
                    return S_OK;
                }
                case AzureState::NoConnect:
                {
                    _WriteStringWithNewline(RS_(L"AzureInternetOrServerIssue"));
                    _transitionToState(ConnectionState::Failed);
                    return E_FAIL;
                }
                }
            }
            catch (...)
            {
                _state = AzureState::NoConnect;
            }
        }
    }

    // Method description:
    // - helper function to get the stored credentials (if any) and let the user choose what to do next
    // Return value:
    // - S_FALSE if there are no stored credentials
    // - S_OK if the user opts to login with a stored set of credentials or login with a different account
    // - E_FAIL if the user closes the tab
    HRESULT AzureConnection::_AccessHelper()
    {
        bool oldVersionEncountered = false;
        auto vault = PasswordVault();
        winrt::Windows::Foundation::Collections::IVectorView<PasswordCredential> credList;
        // FindAllByResource throws an exception if there are no credentials stored under the given resource so we wrap it in a try-catch block
        try
        {
            credList = vault.FindAllByResource(PasswordVaultResourceName);
        }
        catch (...)
        {
            // No credentials are stored, so start the device flow
            _state = AzureState::DeviceFlow;
            return S_FALSE;
        }

        int numTenants{ 0 };
        for (const auto& entry : credList)
        {
            auto nameJson = json::value::parse(entry.UserName().c_str());
            std::optional<int> credentialVersion;
            if (nameJson.has_integer_field(U("ver")))
            {
                credentialVersion = nameJson.at(U("ver")).as_integer();
            }

            if (!credentialVersion.has_value() || credentialVersion.value() != CurrentCredentialVersion)
            {
                // ignore credentials that aren't from the latest credential revision
                vault.Remove(entry);
                oldVersionEncountered = true;
                continue;
            }

            _WriteStringWithNewline(_formatTenantLine(numTenants, nameJson.at(L"displayName").as_string(), nameJson.at(L"tenantID").as_string()));
            numTenants++;
        }

        if (!numTenants)
        {
            if (oldVersionEncountered)
            {
                _WriteStringWithNewline(RS_(L"AzureOldCredentialsFlushedMessage"));
            }
            // No valid up-to-date credentials were found, so start the device flow
            _state = AzureState::DeviceFlow;
            return S_FALSE;
        }

        _WriteStringWithNewline(RS_(L"AzureEnterTenant"));
        _WriteStringWithNewline(_formatResWithColoredUserInputOptions(USES_RESOURCE(L"AzureNewLogin"), USES_RESOURCE(L"AzureUserEntry_NewLogin")));
        _WriteStringWithNewline(_formatResWithColoredUserInputOptions(USES_RESOURCE(L"AzureRemoveStored"), USES_RESOURCE(L"AzureUserEntry_RemoveStored")));

        int selectedTenant{ -1 };
        do
        {
            auto maybeTenantSelection = _ReadUserInput(InputMode::Line);
            FAILOUT_IF_OPTIONAL_EMPTY(maybeTenantSelection);

            const auto& tenantSelection = maybeTenantSelection.value();
            if (tenantSelection == RS_(L"AzureUserEntry_RemoveStored"))
            {
                // User wants to remove the stored settings
                _RemoveCredentials();
                _state = AzureState::DeviceFlow;
                return S_OK;
            }
            else if (tenantSelection == RS_(L"AzureUserEntry_NewLogin"))
            {
                // User wants to login with a different account
                _state = AzureState::DeviceFlow;
                return S_OK;
            }
            else
            {
                try
                {
                    selectedTenant = std::stoi(tenantSelection);
                    if (selectedTenant < 0 || selectedTenant >= numTenants)
                    {
                        _WriteStringWithNewline(RS_(L"AzureNumOutOfBoundsError"));
                        continue; // go 'round again
                    }
                    break;
                }
                catch (...)
                {
                    // suppress exceptions in conversion
                }
            }

            // if we got here, we didn't break out of the loop early and need to go 'round again
            _WriteStringWithNewline(_formatResWithColoredUserInputOptions(USES_RESOURCE(L"AzureInvalidAccessInput"), USES_RESOURCE(L"AzureUserEntry_NewLogin"), USES_RESOURCE(L"AzureUserEntry_RemoveStored")));
        } while (true);

        // User wants to login with one of the saved connection settings
        auto desiredCredential = credList.GetAt(selectedTenant);
        desiredCredential.RetrievePassword();
        auto nameJson = json::value::parse(desiredCredential.UserName().c_str());
        auto passWordJson = json::value::parse(desiredCredential.Password().c_str());
        _displayName = nameJson.at(L"displayName").as_string();
        _tenantID = nameJson.at(L"tenantID").as_string();
        _accessToken = passWordJson.at(L"accessToken").as_string();
        _refreshToken = passWordJson.at(L"refreshToken").as_string();
        _expiry = std::stoi(passWordJson.at(L"expiry").as_string());

        const auto t1 = std::chrono::system_clock::now();
        const auto timeNow = std::chrono::duration_cast<std::chrono::seconds>(t1.time_since_epoch()).count();

        // Check if the token is close to expiring and refresh if so
        if (timeNow + _expireLimit > _expiry)
        {
            const auto refreshResponse = _RefreshTokens();
            _accessToken = refreshResponse.at(L"access_token").as_string();
            _refreshToken = refreshResponse.at(L"refresh_token").as_string();
            _expiry = std::stoi(refreshResponse.at(L"expires_on").as_string());
            // Store the updated tokens under the same username
            _StoreCredential();
        }

        // We have everything we need, so go ahead and connect
        _state = AzureState::TermConnecting;
        return S_OK;
    }

    // Method description:
    // - helper function to start the device code flow (required for authentication to Azure)
    // Return value:
    // - E_FAIL if the user closes the tab, does not authenticate in time or has no tenants in their Azure account
    // - S_OK otherwise
    HRESULT AzureConnection::_DeviceFlowHelper()
    {
        // Initiate device code flow
        const auto deviceCodeResponse = _GetDeviceCode();

        // Print the message and store the device code, polling interval and expiry
        const auto message = winrt::to_hstring(deviceCodeResponse.at(L"message").as_string().c_str());
        _WriteStringWithNewline(message);
        _WriteStringWithNewline(RS_(L"AzureCodeExpiry"));
        const auto devCode = deviceCodeResponse.at(L"device_code").as_string();
        const auto pollInterval = std::stoi(deviceCodeResponse.at(L"interval").as_string());
        const auto expiresIn = std::stoi(deviceCodeResponse.at(L"expires_in").as_string());

        // Wait for user authentication and obtain the access/refresh tokens
        json::value authenticatedResponse;
        try
        {
            authenticatedResponse = _WaitForUser(devCode, pollInterval, expiresIn);
        }
        catch (...)
        {
            _WriteStringWithNewline(RS_(L"AzureExitStr"));
            return E_FAIL;
        }

        _accessToken = authenticatedResponse.at(L"access_token").as_string();
        _refreshToken = authenticatedResponse.at(L"refresh_token").as_string();

        // Get the tenants and the required tenant id
        const auto tenantsResponse = _GetTenants();
        _tenantList = tenantsResponse.at(L"value");
        const auto tenantListAsArray = _tenantList.as_array();
        if (tenantListAsArray.size() == 0)
        {
            _WriteStringWithNewline(RS_(L"AzureNoTenants"));
            _transitionToState(ConnectionState::Failed);
            return E_FAIL;
        }
        else if (_tenantList.size() == 1)
        {
            const auto& chosenTenant = tenantListAsArray.at(0);
            std::tie(_tenantID, _displayName) = _crackTenant(chosenTenant);

            // We have to refresh now that we have the tenantID
            const auto refreshResponse = _RefreshTokens();
            _accessToken = refreshResponse.at(L"access_token").as_string();
            _refreshToken = refreshResponse.at(L"refresh_token").as_string();
            _expiry = std::stoi(refreshResponse.at(L"expires_on").as_string());

            _state = AzureState::StoreTokens;
        }
        else
        {
            _state = AzureState::TenantChoice;
        }
        return S_OK;
    }

    // Method description:
    // - helper function to list the user's tenants and let them decide which tenant they wish to connect to
    // Return value:
    // - E_FAIL if the user closes the tab
    // - S_OK otherwise
    HRESULT AzureConnection::_TenantChoiceHelper()
    {
        try
        {
            const auto tenantListAsArray = _tenantList.as_array();
            auto numTenants = gsl::narrow<int>(tenantListAsArray.size());
            for (int i = 0; i < numTenants; i++)
            {
                const auto& tenant = tenantListAsArray.at(i);
                const auto [tenantId, tenantDisplayName] = _crackTenant(tenant);
                _WriteStringWithNewline(_formatTenantLine(i, tenantDisplayName, tenantId));
            }
            _WriteStringWithNewline(RS_(L"AzureEnterTenant"));

            int selectedTenant{ -1 };
            do
            {
                auto maybeTenantSelection = _ReadUserInput(InputMode::Line);
                FAILOUT_IF_OPTIONAL_EMPTY(maybeTenantSelection);

                const auto& tenantSelection = maybeTenantSelection.value();
                try
                {
                    selectedTenant = std::stoi(tenantSelection);

                    if (selectedTenant < 0 || selectedTenant >= numTenants)
                    {
                        _WriteStringWithNewline(RS_(L"AzureNumOutOfBoundsError"));
                        continue;
                    }
                    break;
                }
                catch (...)
                {
                    // suppress exceptions in conversion
                }

                // if we got here, we didn't break out of the loop early and need to go 'round again
                _WriteStringWithNewline(RS_(L"AzureNonNumberError"));
            } while (true);

            const auto& chosenTenant = tenantListAsArray.at(selectedTenant);
            std::tie(_tenantID, _displayName) = _crackTenant(chosenTenant);

            // We have to refresh now that we have the tenantID
            const auto refreshResponse = _RefreshTokens();
            _accessToken = refreshResponse.at(L"access_token").as_string();
            _refreshToken = refreshResponse.at(L"refresh_token").as_string();
            _expiry = std::stoi(refreshResponse.at(L"expires_on").as_string());

            _state = AzureState::StoreTokens;
            return S_OK;
        }
        CATCH_RETURN();
    }

    // Method description:
    // - helper function to ask the user if they wish to store their credentials
    // Return value:
    // - E_FAIL if the user closes the tab
    // - S_OK otherwise
    HRESULT AzureConnection::_StoreHelper()
    {
        _WriteStringWithNewline(_formatResWithColoredUserInputOptions(USES_RESOURCE(L"AzureStorePrompt"), USES_RESOURCE(L"AzureUserEntry_Yes"), USES_RESOURCE(L"AzureUserEntry_No")));
        // Wait for user input
        do
        {
            auto maybeStoreCredentials = _ReadUserInput(InputMode::Line);
            FAILOUT_IF_OPTIONAL_EMPTY(maybeStoreCredentials);

            const auto& storeCredentials = maybeStoreCredentials.value();
            if (storeCredentials == RS_(L"AzureUserEntry_Yes"))
            {
                _StoreCredential();
                _WriteStringWithNewline(RS_(L"AzureTokensStored"));
                break;
            }
            else if (storeCredentials == RS_(L"AzureUserEntry_No"))
            {
                break; // we're done, but the user wants nothing.
            }

            // if we got here, we didn't break out of the loop early and need to go 'round again
            _WriteStringWithNewline(_formatResWithColoredUserInputOptions(USES_RESOURCE(L"AzureInvalidStoreInput"), USES_RESOURCE(L"AzureUserEntry_Yes"), USES_RESOURCE(L"AzureUserEntry_No")));
        } while (true);

        _state = AzureState::TermConnecting;
        return S_OK;
    }

    // Method description:
    // - helper function to connect the user to the Azure cloud shell
    // Return value:
    // - E_FAIL if the user has not set up their cloud shell yet
    // - S_OK after successful connection
    HRESULT AzureConnection::_ConnectHelper()
    {
        // Get user's cloud shell settings
        const auto settingsResponse = _GetCloudShellUserSettings();
        if (settingsResponse.has_field(L"error"))
        {
            _WriteStringWithNewline(RS_(L"AzureNoCloudAccount"));
            _transitionToState(ConnectionState::Failed);
            return E_FAIL;
        }

        // Request for a cloud shell
        _WriteStringWithNewline(RS_(L"AzureRequestingCloud"));
        _cloudShellUri = _GetCloudShell();
        _WriteStringWithNewline(RS_(L"AzureSuccess"));

        // Request for a terminal for said cloud shell
        const auto shellType = settingsResponse.at(L"properties").at(L"preferredShellType").as_string();
        _WriteStringWithNewline(RS_(L"AzureRequestingTerminal"));
        const auto socketUri = _GetTerminal(shellType);
        _TerminalOutputHandlers(L"\r\n");

        // Step 8: connecting to said terminal
        const auto connReqTask = _cloudShellSocket.connect(socketUri);
        connReqTask.wait();

        _state = AzureState::TermConnected;

        std::wstring queuedUserInput{};
        std::swap(_userInput, queuedUserInput);
        if (queuedUserInput.size() > 0)
        {
            WriteInput(static_cast<winrt::hstring>(queuedUserInput)); // send the user's queued up input back through
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
    json::value AzureConnection::_RequestHelper(http_client theClient, http_request theRequest)
    {
        json::value jsonResult;
        try
        {
            const auto responseTask = theClient.request(theRequest);
            responseTask.wait();
            const auto response = responseTask.get();
            const auto responseJsonTask = response.extract_json();
            responseJsonTask.wait();
            jsonResult = responseJsonTask.get();
        }
        catch (...)
        {
            _WriteStringWithNewline(RS_(L"AzureInternetOrServerIssue"));
        }
        return jsonResult;
    }

    // Method description:
    // - helper function to start the device code flow
    // Return value:
    // - the response to the device code flow initiation
    json::value AzureConnection::_GetDeviceCode()
    {
        // Initialize the client
        http_client loginClient(_loginUri);

        // Initialize the request
        http_request commonRequest(L"POST");
        commonRequest.set_request_uri(L"common/oauth2/devicecode");
        const auto body = L"client_id=" + AzureClientID + L"&resource=" + _wantedResource;
        commonRequest.set_body(body.c_str(), L"application/x-www-form-urlencoded");

        // Send the request and receive the response as a json value
        return _RequestHelper(loginClient, commonRequest);
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
    json::value AzureConnection::_WaitForUser(const utility::string_t deviceCode, int pollInterval, int expiresIn)
    {
        // Initialize the client
        http_client pollingClient(_loginUri);

        // Continuously send a poll request until the user authenticates
        const auto body = hstring() + L"grant_type=device_code&resource=" + _wantedResource + L"&client_id=" + AzureClientID + L"&code=" + deviceCode;
        const auto requestUri = L"common/oauth2/token";
        json::value responseJson;
        for (int count = 0; count < expiresIn / pollInterval; count++)
        {
            // User might close the tab while we wait for them to authenticate, this case handles that
            if (_isStateAtOrBeyond(ConnectionState::Closing))
            {
                throw "Tab closed.";
            }
            http_request pollRequest(L"POST");
            pollRequest.set_request_uri(requestUri);
            pollRequest.set_body(body.c_str(), L"application/x-www-form-urlencoded");

            responseJson = _RequestHelper(pollingClient, pollRequest);

            if (responseJson.has_field(L"error"))
            {
                Sleep(pollInterval * 1000); // Sleep takes arguments in milliseconds
                continue; // Still waiting for authentication
            }
            else
            {
                _WriteStringWithNewline(RS_(L"AzureSuccessfullyAuthenticated"));
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
    json::value AzureConnection::_GetTenants()
    {
        // Initialize the client
        http_client tenantClient(_resourceUri);

        // Initialize the request
        http_request tenantRequest(L"GET");
        tenantRequest.set_request_uri(L"tenants?api-version=2018-01-01");
        _HeaderHelper(tenantRequest);

        // Send the request and return the response as a json value
        return _RequestHelper(tenantClient, tenantRequest);
    }

    // Method description:
    // - helper function to refresh the access/refresh tokens
    // Return value:
    // - the response with the new tokens
    json::value AzureConnection::_RefreshTokens()
    {
        // Initialize the client
        http_client refreshClient(_loginUri);

        // Initialize the request
        http_request refreshRequest(L"POST");
        refreshRequest.set_request_uri(_tenantID + L"/oauth2/token");
        const auto body = L"client_id=" + AzureClientID + L"&resource=" + _wantedResource + L"&grant_type=refresh_token" + L"&refresh_token=" + _refreshToken;
        refreshRequest.set_body(body.c_str(), L"application/x-www-form-urlencoded");
        refreshRequest.headers().add(L"User-Agent", HttpUserAgent);

        // Send the request and return the response as a json value
        return _RequestHelper(refreshClient, refreshRequest);
    }

    // Method description:
    // - helper function to get the user's cloud shell settings
    // Return value:
    // - the user's cloud shell settings
    json::value AzureConnection::_GetCloudShellUserSettings()
    {
        // Initialize client
        http_client settingsClient(_resourceUri);

        // Initialize request
        http_request settingsRequest(L"GET");
        settingsRequest.set_request_uri(L"providers/Microsoft.Portal/userSettings/cloudconsole?api-version=2018-10-01");
        _HeaderHelper(settingsRequest);

        return _RequestHelper(settingsClient, settingsRequest);
    }

    // Method description:
    // - helper function to request for a cloud shell
    // Return value:
    // - the uri for the cloud shell
    utility::string_t AzureConnection::_GetCloudShell()
    {
        // Initialize client
        http_client cloudShellClient(_resourceUri);

        // Initialize request
        http_request shellRequest(L"PUT");
        shellRequest.set_request_uri(L"providers/Microsoft.Portal/consoles/default?api-version=2018-10-01");
        _HeaderHelper(shellRequest);
        // { "properties": { "osType": "linux" } }
        auto body = json::value::object({ { U("properties"), json::value::object({ { U("osType"), json::value::string(U("linux")) } }) } });
        shellRequest.set_body(body);

        // Send the request and get the response as a json value
        const auto cloudShell = _RequestHelper(cloudShellClient, shellRequest);

        // Return the uri
        return cloudShell.at(L"properties").at(L"uri").as_string() + L"/";
    }

    // Method description:
    // - helper function to request for a terminal
    // Return value:
    // - the uri for the terminal
    utility::string_t AzureConnection::_GetTerminal(utility::string_t shellType)
    {
        // Initialize client
        http_client terminalClient(_cloudShellUri);

        // Initialize the request
        http_request terminalRequest(L"POST");
        terminalRequest.set_request_uri(L"terminals?cols=" + std::to_wstring(_initialCols) + L"&rows=" + std::to_wstring(_initialRows) + L"&version=2019-01-01&shell=" + shellType);
        _HeaderHelper(terminalRequest);

        // Send the request and get the response as a json value
        const auto terminalResponse = _RequestHelper(terminalClient, terminalRequest);
        _terminalID = terminalResponse.at(L"id").as_string();

        // Return the uri
        return terminalResponse.at(L"socketUri").as_string();
    }

    // Method description:
    // - helper function to set the headers of a http_request
    // Arguments:
    // - the http_request
    void AzureConnection::_HeaderHelper(http_request theRequest)
    {
        theRequest.headers().add(L"Accept", L"application/json");
        theRequest.headers().add(L"Content-Type", L"application/json");
        theRequest.headers().add(L"Authorization", L"Bearer " + _accessToken);
        theRequest.headers().add(L"User-Agent", HttpUserAgent);
    }

    // Method description:
    // - helper function to store the credentials
    // - we store the display name, tenant ID, access/refresh tokens, and token expiry
    void AzureConnection::_StoreCredential()
    {
        auto vault = PasswordVault();
        json::value userName;
        userName[U("ver")] = CurrentCredentialVersion;
        userName[U("displayName")] = json::value::string(_displayName);
        userName[U("tenantID")] = json::value::string(_tenantID);
        json::value passWord;
        passWord[U("accessToken")] = json::value::string(_accessToken);
        passWord[U("refreshToken")] = json::value::string(_refreshToken);
        passWord[U("expiry")] = json::value::string(std::to_wstring(_expiry));
        auto newCredential = PasswordCredential(PasswordVaultResourceName, userName.serialize(), passWord.serialize());
        vault.Add(newCredential);
    }

    // Method description:
    // - helper function to remove all stored credentials
    void AzureConnection::_RemoveCredentials()
    {
        auto vault = PasswordVault();
        winrt::Windows::Foundation::Collections::IVectorView<PasswordCredential> credList;
        // FindAllByResource throws an exception if there are no credentials stored under the given resource so we wrap it in a try-catch block
        try
        {
            credList = vault.FindAllByResource(PasswordVaultResourceName);
        }
        catch (...)
        {
            // No credentials are stored, so just return
            _WriteStringWithNewline(RS_(L"AzureNoTokens"));
            return;
        }

        for (const auto& cred : credList)
        {
            try
            {
                vault.Remove(cred);
            }
            CATCH_LOG();
        }
        _WriteStringWithNewline(RS_(L"AzureTokensRemoved"));
    }
}
