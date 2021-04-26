// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "AzureConnection.h"
#include "AzureClientID.h"
#include <sstream>
#include <cstdlib>
#include <LibraryResources.h>
#include <unicode.hpp>

#include "AzureConnection.g.cpp"

#include "winrt/Windows.System.UserProfile.h"
#include "../../types/inc/Utils.hpp"

using namespace ::Microsoft::Console;
using namespace ::Microsoft::Terminal::Azure;

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::websockets::client;
using namespace winrt::Windows::Security::Credentials;

static constexpr int CurrentCredentialVersion = 1;
static constexpr auto PasswordVaultResourceName = L"Terminal";
static constexpr auto HttpUserAgent = L"Terminal/0.0";

static constexpr int USER_INPUT_COLOR = 93; // yellow - the color of something the user can type
static constexpr int USER_INFO_COLOR = 97; // white - the color of clarifying information

static constexpr winrt::guid AzureConnectionType = { 0xd9fcfdfa, 0xa479, 0x412c, { 0x83, 0xb7, 0xc5, 0x64, 0xe, 0x61, 0xcd, 0x62 } };

static inline std::wstring _colorize(const unsigned int colorCode, const std::wstring_view text)
{
    return fmt::format(L"\x1b[{0}m{1}\x1b[m", colorCode, text);
}

// Takes N resource names, loads the first one as a format string, and then
// loads all the remaining ones into the %s arguments in the first one after
// colorizing them in the USER_INPUT_COLOR.
// This is intended to be used to drop UserEntry resources into an existing string.
template<typename... Args>
static inline std::wstring _formatResWithColoredUserInputOptions(const std::wstring_view resourceKey, Args&&... args)
{
    return fmt::format(std::wstring_view{ GetLibraryResourceString(resourceKey) }, (_colorize(USER_INPUT_COLOR, GetLibraryResourceString(args)))...);
}

static inline std::wstring _formatTenant(int tenantNumber, const Tenant& tenant)
{
    return fmt::format(std::wstring_view{ RS_(L"AzureIthTenant") },
                       _colorize(USER_INPUT_COLOR, std::to_wstring(tenantNumber)),
                       _colorize(USER_INFO_COLOR, tenant.DisplayName.value_or(std::wstring{ RS_(L"AzureUnknownTenantName") })),
                       tenant.DefaultDomain.value_or(tenant.ID)); // use the domain name if possible, ID if not.
}

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    winrt::guid AzureConnection::ConnectionType() noexcept
    {
        return AzureConnectionType;
    }

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
    // - helper that prints exception information to the output stream.
    // Arguments:
    // - [IMPLICIT] the current exception context
    void AzureConnection::_WriteCaughtExceptionRecord()
    {
        try
        {
            throw;
        }
        catch (const std::exception& runtimeException)
        {
            // This also catches the AzureException, which has a .what()
            _TerminalOutputHandlers(_colorize(91, til::u8u16(std::string{ runtimeException.what() })));
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    // Method description:
    // - ascribes to the ITerminalConnection interface
    // - creates the output thread (where we will do the authentication and actually connect to Azure)
    void AzureConnection::Start()
    {
        // Create our own output handling thread
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(
            nullptr,
            0,
            [](LPVOID lpParameter) noexcept {
                AzureConnection* const pInstance = static_cast<AzureConnection*>(lpParameter);
                if (pInstance)
                {
                    return pInstance->_OutputThread();
                }
                return gsl::narrow_cast<DWORD>(E_INVALIDARG);
            },
            this,
            0,
            nullptr));

        THROW_LAST_ERROR_IF_NULL(_hOutputThread);

        LOG_IF_FAILED(SetThreadDescription(_hOutputThread.get(), L"AzureConnection Output Thread"));

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

        _TerminalOutputHandlers(L"> \x1b[92m"); // Make prompted user input green

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
    try
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
            terminalRequest.set_request_uri(fmt::format(L"terminals/{}/size?cols={}&rows={}&version=2019-01-01", _terminalID, columns, rows));
            terminalRequest.set_body(json::value::null());

            // Send the request (don't care about the response)
            (void)_SendAuthenticatedRequestReturningJson(terminalClient, terminalRequest);
        }
    }
    CATCH_LOG();

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
    static Tenant _crackTenant(const json::value& jsonTenant)
    {
        Tenant tenant{};
        if (jsonTenant.has_string_field(L"tenantID"))
        {
            // for compatibility with version 1 credentials
            tenant.ID = jsonTenant.at(L"tenantID").as_string();
        }
        else
        {
            // This one comes in off the wire
            tenant.ID = jsonTenant.at(L"tenantId").as_string();
        }

        if (jsonTenant.has_string_field(L"displayName"))
        {
            tenant.DisplayName = jsonTenant.at(L"displayName").as_string();
        }

        if (jsonTenant.has_string_field(L"defaultDomain"))
        {
            tenant.DefaultDomain = jsonTenant.at(L"defaultDomain").as_string();
        }

        return tenant;
    }

    static void _packTenant(json::value& jsonTenant, const Tenant& tenant)
    {
        jsonTenant[L"tenantId"] = json::value::string(tenant.ID);
        if (tenant.DisplayName.has_value())
        {
            jsonTenant[L"displayName"] = json::value::string(*tenant.DisplayName);
        }

        if (tenant.DefaultDomain.has_value())
        {
            jsonTenant[L"defaultDomain"] = json::value::string(*tenant.DefaultDomain);
        }
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
                    _RunAccessState();
                    break;
                }
                // User has no saved connection settings or has opted to login with a different account
                // Azure authentication happens here
                case AzureState::DeviceFlow:
                {
                    _RunDeviceFlowState();
                    break;
                }
                // User has multiple tenants in their Azure account, they need to choose which one to connect to
                case AzureState::TenantChoice:
                {
                    _RunTenantChoiceState();
                    break;
                }
                // Ask the user if they want to save these connection settings for future logins
                case AzureState::StoreTokens:
                {
                    _RunStoreState();
                    break;
                }
                // Connect to Azure, we only get here once we have everything we need (tenantID, accessToken, refreshToken)
                case AzureState::TermConnecting:
                {
                    _RunConnectState();
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
                }
            }
            catch (...)
            {
                _WriteCaughtExceptionRecord();
                _transitionToState(ConnectionState::Failed);
                return E_FAIL;
            }
        }
    }

    // Method description:
    // - helper function to get the stored credentials (if any) and let the user choose what to do next
    void AzureConnection::_RunAccessState()
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
            return;
        }

        int numTenants{ 0 };
        _tenantList.clear();
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

            auto newTenant{ _tenantList.emplace_back(_crackTenant(nameJson)) };

            _WriteStringWithNewline(_formatTenant(numTenants, newTenant));
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
            return;
        }

        _WriteStringWithNewline(RS_(L"AzureEnterTenant"));
        _WriteStringWithNewline(_formatResWithColoredUserInputOptions(USES_RESOURCE(L"AzureNewLogin"), USES_RESOURCE(L"AzureUserEntry_NewLogin")));
        _WriteStringWithNewline(_formatResWithColoredUserInputOptions(USES_RESOURCE(L"AzureRemoveStored"), USES_RESOURCE(L"AzureUserEntry_RemoveStored")));

        int selectedTenant{ -1 };
        do
        {
            auto maybeTenantSelection = _ReadUserInput(InputMode::Line);
            if (!maybeTenantSelection.has_value())
            {
                return;
            }

            const auto& tenantSelection = maybeTenantSelection.value();
            if (tenantSelection == RS_(L"AzureUserEntry_RemoveStored"))
            {
                // User wants to remove the stored settings
                _RemoveCredentials();
                _state = AzureState::DeviceFlow;
                return;
            }
            else if (tenantSelection == RS_(L"AzureUserEntry_NewLogin"))
            {
                // User wants to login with a different account
                _state = AzureState::DeviceFlow;
                return;
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
        auto passWordJson = json::value::parse(desiredCredential.Password().c_str());
        _currentTenant = til::at(_tenantList, selectedTenant); // we already unpacked the name info, so we should just use it
        _accessToken = passWordJson.at(L"accessToken").as_string();
        _refreshToken = passWordJson.at(L"refreshToken").as_string();
        _expiry = std::stoi(passWordJson.at(L"expiry").as_string());

        const auto t1 = std::chrono::system_clock::now();
        const auto timeNow = std::chrono::duration_cast<std::chrono::seconds>(t1.time_since_epoch()).count();

        // Check if the token is close to expiring and refresh if so
        if (timeNow + _expireLimit > _expiry)
        {
            try
            {
                _RefreshTokens();
                // Store the updated tokens under the same username
                _StoreCredential();
            }
            catch (const AzureException& e)
            {
                if (e.GetCode() == ErrorCodes::InvalidGrant)
                {
                    _WriteCaughtExceptionRecord();
                    vault.Remove(desiredCredential);
                    // Delete this credential and try again.
                    _state = AzureState::AccessStored;
                    return;
                }
                throw; // rethrow. we couldn't handle this error.
            }
        }

        // We have everything we need, so go ahead and connect
        _state = AzureState::TermConnecting;
    }

    // Method description:
    // - helper function to start the device code flow (required for authentication to Azure)
    void AzureConnection::_RunDeviceFlowState()
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
        json::value authenticatedResponse = _WaitForUser(devCode, pollInterval, expiresIn);
        _accessToken = authenticatedResponse.at(L"access_token").as_string();
        _refreshToken = authenticatedResponse.at(L"refresh_token").as_string();

        // Get the tenants and the required tenant id
        _PopulateTenantList();
        if (_tenantList.size() == 0)
        {
            _WriteStringWithNewline(RS_(L"AzureNoTenants"));
            _transitionToState(ConnectionState::Failed);
            return;
        }
        else if (_tenantList.size() == 1)
        {
            _currentTenant = til::at(_tenantList, 0);

            // We have to refresh now that we have the tenantID
            _RefreshTokens();
            _state = AzureState::StoreTokens;
        }
        else
        {
            _state = AzureState::TenantChoice;
        }
    }

    // Method description:
    // - helper function to list the user's tenants and let them decide which tenant they wish to connect to
    void AzureConnection::_RunTenantChoiceState()
    {
        auto numTenants = gsl::narrow<int>(_tenantList.size());
        for (int i = 0; i < numTenants; i++)
        {
            _WriteStringWithNewline(_formatTenant(i, til::at(_tenantList, i)));
        }
        _WriteStringWithNewline(RS_(L"AzureEnterTenant"));

        int selectedTenant{ -1 };
        do
        {
            auto maybeTenantSelection = _ReadUserInput(InputMode::Line);
            if (!maybeTenantSelection.has_value())
            {
                return;
            }

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

        _currentTenant = til::at(_tenantList, selectedTenant);

        // We have to refresh now that we have the tenantID
        _RefreshTokens();
        _state = AzureState::StoreTokens;
    }

    // Method description:
    // - helper function to ask the user if they wish to store their credentials
    void AzureConnection::_RunStoreState()
    {
        _WriteStringWithNewline(_formatResWithColoredUserInputOptions(USES_RESOURCE(L"AzureStorePrompt"), USES_RESOURCE(L"AzureUserEntry_Yes"), USES_RESOURCE(L"AzureUserEntry_No")));
        // Wait for user input
        do
        {
            auto maybeStoreCredentials = _ReadUserInput(InputMode::Line);
            if (!maybeStoreCredentials.has_value())
            {
                return;
            }

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
    }

    // Method description:
    // - Helper function to parse the preferred shell type from user settings returned by cloud console API.
    // We need this function because the field might be missing in the settings
    // created with old versions of cloud console API.
    std::optional<utility::string_t> AzureConnection::_ParsePreferredShellType(const web::json::value& settingsResponse)
    {
        if (settingsResponse.has_object_field(L"properties"))
        {
            const auto userSettings = settingsResponse.at(L"properties");
            if (userSettings.has_string_field(L"preferredShellType"))
            {
                const auto preferredShellTypeValue = userSettings.at(L"preferredShellType");
                return preferredShellTypeValue.as_string();
            }
        }

        return std::nullopt;
    }

    // Method description:
    // - helper function to connect the user to the Azure cloud shell
    void AzureConnection::_RunConnectState()
    {
        // Get user's cloud shell settings
        const auto settingsResponse = _GetCloudShellUserSettings();
        if (settingsResponse.has_field(L"error"))
        {
            _WriteStringWithNewline(RS_(L"AzureNoCloudAccount"));
            _transitionToState(ConnectionState::Failed);
            return;
        }

        // Request for a cloud shell
        _WriteStringWithNewline(RS_(L"AzureRequestingCloud"));
        _cloudShellUri = _GetCloudShell();
        _WriteStringWithNewline(RS_(L"AzureSuccess"));

        // Request for a terminal for said cloud shell
        const auto shellType = _ParsePreferredShellType(settingsResponse);
        _WriteStringWithNewline(RS_(L"AzureRequestingTerminal"));
        const auto socketUri = _GetTerminal(shellType.value_or(L"pwsh"));
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
    }

    // Method description:
    // - helper function to send requests with default headers and extract responses as json values
    // Arguments:
    // - a http_client
    // - a http_request for the client to send
    // Return value:
    // - the response from the server as a json value
    json::value AzureConnection::_SendRequestReturningJson(http_client& theClient, http_request theRequest)
    {
        auto& headers{ theRequest.headers() };
        headers.add(L"User-Agent", HttpUserAgent);
        headers.add(L"Accept", L"application/json");

        json::value jsonResult;
        const auto responseTask = theClient.request(theRequest);
        responseTask.wait();
        const auto response = responseTask.get();
        const auto responseJsonTask = response.extract_json();
        responseJsonTask.wait();
        jsonResult = responseJsonTask.get();

        THROW_IF_AZURE_ERROR(jsonResult);
        return jsonResult;
    }

    // Method description:
    // - helper function to send _authenticated_ requests with json bodies whose responses are expected
    //   to be json. builds on _SendRequestReturningJson.
    // Arguments:
    // - the http_request
    json::value AzureConnection::_SendAuthenticatedRequestReturningJson(http_client& theClient, http_request theRequest)
    {
        auto& headers{ theRequest.headers() };
        headers.add(L"Authorization", L"Bearer " + _accessToken);

        return _SendRequestReturningJson(theClient, std::move(theRequest));
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
        const auto body{ fmt::format(L"client_id={}&resource={}", AzureClientID, _wantedResource) };
        commonRequest.set_body(body.c_str(), L"application/x-www-form-urlencoded");

        // Send the request and receive the response as a json value
        return _SendRequestReturningJson(loginClient, commonRequest);
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
        const auto body{ fmt::format(L"grant_type=device_code&resource={}&client_id={}&code={}", _wantedResource, AzureClientID, deviceCode) };
        const auto requestUri = L"common/oauth2/token";

        // use a steady clock here so it's not impacted by local time discontinuities
        const auto tokenExpiry{ std::chrono::steady_clock::now() + std::chrono::seconds(expiresIn) };
        while (std::chrono::steady_clock::now() < tokenExpiry)
        {
            std::this_thread::sleep_for(std::chrono::seconds(pollInterval));

            // User might close the tab while we wait for them to authenticate, this case handles that
            if (_isStateAtOrBeyond(ConnectionState::Closing))
            {
                // We're going down, there's no valid user for us to return
                break;
            }

            http_request pollRequest(L"POST");
            pollRequest.set_request_uri(requestUri);
            pollRequest.set_body(body.c_str(), L"application/x-www-form-urlencoded");

            try
            {
                auto response{ _SendRequestReturningJson(pollingClient, pollRequest) };
                _WriteStringWithNewline(RS_(L"AzureSuccessfullyAuthenticated"));
                // Got a valid response: we're done
                return response;
            }
            catch (const AzureException& e)
            {
                if (e.GetCode() == ErrorCodes::AuthorizationPending)
                {
                    // Handle the "auth pending" exception by retrying.
                    continue;
                }
                throw;
            } // uncaught exceptions bubble up to the caller
        }

        return json::value::null();
    }

    // Method description:
    // - helper function to acquire the user's Azure tenants
    // Return value:
    // - the response which contains a list of the user's Azure tenants
    void AzureConnection::_PopulateTenantList()
    {
        // Initialize the client
        http_client tenantClient(_resourceUri);

        // Initialize the request
        http_request tenantRequest(L"GET");
        tenantRequest.set_request_uri(L"tenants?api-version=2020-01-01");

        // Send the request and return the response as a json value
        auto tenantResponse{ _SendAuthenticatedRequestReturningJson(tenantClient, tenantRequest) };
        auto tenantList{ tenantResponse.at(L"value").as_array() };

        _tenantList.clear();
        std::transform(tenantList.begin(), tenantList.end(), std::back_inserter(_tenantList), _crackTenant);
    }

    // Method description:
    // - helper function to refresh the access/refresh tokens
    // Return value:
    // - the response with the new tokens
    void AzureConnection::_RefreshTokens()
    {
        // Initialize the client
        http_client refreshClient(_loginUri);

        // Initialize the request
        http_request refreshRequest(L"POST");
        refreshRequest.set_request_uri(_currentTenant->ID + L"/oauth2/token");
        const auto body{ fmt::format(L"client_id={}&resource={}&grant_type=refresh_token&refresh_token={}", AzureClientID, _wantedResource, _refreshToken) };
        refreshRequest.set_body(body.c_str(), L"application/x-www-form-urlencoded");

        // Send the request and return the response as a json value
        auto refreshResponse{ _SendRequestReturningJson(refreshClient, refreshRequest) };
        _accessToken = refreshResponse.at(L"access_token").as_string();
        _refreshToken = refreshResponse.at(L"refresh_token").as_string();
        _expiry = std::stoi(refreshResponse.at(L"expires_on").as_string());
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

        return _SendAuthenticatedRequestReturningJson(settingsClient, settingsRequest);
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
        // { "properties": { "osType": "linux" } }
        auto body = json::value::object({ { U("properties"), json::value::object({ { U("osType"), json::value::string(U("linux")) } }) } });
        shellRequest.set_body(body);

        // Send the request and get the response as a json value
        const auto cloudShell = _SendAuthenticatedRequestReturningJson(cloudShellClient, shellRequest);

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
        terminalRequest.set_request_uri(fmt::format(L"terminals?cols={}&rows={}&version=2019-01-01&shell={}", _initialCols, _initialRows, shellType));
        // LOAD-BEARING. the API returns "'content-type' should be 'application/json' or 'multipart/form-data'"
        terminalRequest.set_body(json::value::null());

        // Send the request and get the response as a json value
        const auto terminalResponse = _SendAuthenticatedRequestReturningJson(terminalClient, terminalRequest);
        _terminalID = terminalResponse.at(L"id").as_string();

        // Return the uri
        return terminalResponse.at(L"socketUri").as_string();
    }

    // Method description:
    // - helper function to store the credentials
    // - we store the display name, tenant ID, access/refresh tokens, and token expiry
    void AzureConnection::_StoreCredential()
    {
        json::value userName;
        userName[U("ver")] = CurrentCredentialVersion;
        _packTenant(userName, *_currentTenant);
        json::value passWord;
        passWord[U("accessToken")] = json::value::string(_accessToken);
        passWord[U("refreshToken")] = json::value::string(_refreshToken);
        passWord[U("expiry")] = json::value::string(std::to_wstring(_expiry));

        PasswordVault vault;
        PasswordCredential newCredential{ PasswordVaultResourceName, userName.serialize(), passWord.serialize() };
        vault.Add(newCredential);
    }

    // Method description:
    // - helper function to remove all stored credentials
    void AzureConnection::_RemoveCredentials()
    {
        PasswordVault vault;
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
