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

#include "winrt/Windows.Web.Http.Filters.h"

using namespace ::Microsoft::Console;
using namespace ::Microsoft::Terminal::Azure;
using namespace winrt::Windows::Security::Credentials;

static constexpr int CurrentCredentialVersion = 2;
static constexpr std::wstring_view PasswordVaultResourceName = L"Terminal";
static constexpr std::wstring_view HttpUserAgent = L"Mozilla/5.0 (Windows NT 10.0) Terminal/1.0";

static constexpr int USER_INPUT_COLOR = 93; // yellow - the color of something the user can type
static constexpr int USER_INFO_COLOR = 97; // white - the color of clarifying information

using namespace winrt::Windows::Foundation;
namespace WDJ = ::winrt::Windows::Data::Json;
namespace WSS = ::winrt::Windows::Storage::Streams;
namespace WWH = ::winrt::Windows::Web::Http;

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

    void AzureConnection::Initialize(const Windows::Foundation::Collections::ValueSet& settings)
    {
        if (settings)
        {
            _initialRows = gsl::narrow<til::CoordType>(winrt::unbox_value_or<uint32_t>(settings.TryLookup(L"initialRows").try_as<Windows::Foundation::IPropertyValue>(), _initialRows));
            _initialCols = gsl::narrow<til::CoordType>(winrt::unbox_value_or<uint32_t>(settings.TryLookup(L"initialCols").try_as<Windows::Foundation::IPropertyValue>(), _initialCols));
        }
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
        _httpClient = winrt::Windows::Web::Http::HttpClient{};
        _httpClient.DefaultRequestHeaders().UserAgent().TryParseAdd(HttpUserAgent);
        // Create our own output handling thread
        // Each connection needs to make sure to drain the output from its backing host.
        _hOutputThread.reset(CreateThread(
            nullptr,
            0,
            [](LPVOID lpParameter) noexcept {
                const auto pInstance = static_cast<AzureConnection*>(lpParameter);
                if (pInstance)
                {
                    return pInstance->_OutputThread();
                }
                return gsl::narrow<DWORD>(E_INVALIDARG);
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
    void AzureConnection::WriteInput(const hstring& data)
    {
        // We read input while connected AND connecting.
        if (!_isStateOneOf(ConnectionState::Connected, ConnectionState::Connecting))
        {
            return;
        }

        if (_state == AzureState::TermConnected)
        {
            auto buff{ winrt::to_string(data) };
            WinHttpWebSocketSend(_webSocket.get(), WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, buff.data(), gsl::narrow<DWORD>(buff.size()));
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
            auto uri{ fmt::format(L"{}terminals/{}/size?cols={}&rows={}&version=2019-01-01", _cloudShellUri, _terminalID, columns, rows) };

            WWH::HttpStringContent content{
                L"",
                WSS::UnicodeEncoding::Utf8,
                // LOAD-BEARING. the API returns "'content-type' should be 'application/json' or 'multipart/form-data'"
                L"application/json"
            };

            // Send the request (don't care about the response)
            std::ignore = _SendRequestReturningJson(uri, content);
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
                std::ignore = WinHttpWebSocketClose(_webSocket.get(), WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0); // throw away the error
                _webSocket.reset();
                _socketConnectionHandle.reset();
                _socketSessionHandle.reset();
            }

            if (_hOutputThread)
            {
                // Waiting for the output thread to exit ensures that all pending _TerminalOutputHandlers()
                // calls have returned and won't notify our caller (ControlCore) anymore. This ensures that
                // we don't call a destroyed event handler asynchronously from a background thread (GH#13880).
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
    static Tenant _crackTenant(const WDJ::IJsonValue& value)
    {
        auto jsonTenant{ value.GetObjectW() };

        Tenant tenant{};
        if (jsonTenant.HasKey(L"tenantID"))
        {
            // for compatibility with version 1 credentials
            tenant.ID = jsonTenant.GetNamedString(L"tenantID");
        }
        else
        {
            // This one comes in off the wire
            tenant.ID = jsonTenant.GetNamedString(L"tenantId");
        }

        if (jsonTenant.HasKey(L"displayName"))
        {
            tenant.DisplayName = jsonTenant.GetNamedString(L"displayName");
        }

        if (jsonTenant.HasKey(L"defaultDomain"))
        {
            tenant.DefaultDomain = jsonTenant.GetNamedString(L"defaultDomain");
        }

        return tenant;
    }

    static void _packTenant(const WDJ::JsonObject& jsonTenant, const Tenant& tenant)
    {
        jsonTenant.SetNamedValue(L"tenantId", WDJ::JsonValue::CreateStringValue(tenant.ID));
        if (tenant.DisplayName.has_value())
        {
            jsonTenant.SetNamedValue(L"displayName", WDJ::JsonValue::CreateStringValue(*tenant.DisplayName));
        }

        if (tenant.DefaultDomain.has_value())
        {
            jsonTenant.SetNamedValue(L"defaultDomain", WDJ::JsonValue::CreateStringValue(*tenant.DefaultDomain));
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
                        WINHTTP_WEB_SOCKET_BUFFER_TYPE bufferType{};
                        DWORD read{};
                        THROW_IF_WIN32_ERROR(WinHttpWebSocketReceive(_webSocket.get(), _buffer.data(), gsl::narrow<DWORD>(_buffer.size()), &read, &bufferType));

                        switch (bufferType)
                        {
                        case WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
                        case WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE:
                        {
                            const auto result{ til::u8u16(std::string_view{ _buffer.data(), read }, _u16Str, _u8State) };
                            if (FAILED(result))
                            {
                                // EXIT POINT
                                _transitionToState(ConnectionState::Failed);
                                return gsl::narrow<DWORD>(result);
                            }

                            if (_u16Str.empty())
                            {
                                continue;
                            }

                            // Pass the output to our registered event handlers
                            _TerminalOutputHandlers(_u16Str);
                            break;
                        }
                        case WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE:
                            // EXIT POINT
                            if (_transitionToState(ConnectionState::Closed))
                            {
                                return S_FALSE;
                            }
                        }
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
        auto oldVersionEncountered = false;
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

        auto numTenants{ 0 };
        _tenantList.clear();
        for (const auto& entry : credList)
        {
            try
            {
                auto nameJson = WDJ::JsonObject::Parse(entry.UserName());
                std::optional<int> credentialVersion;
                if (nameJson.HasKey(L"ver"))
                {
                    credentialVersion = static_cast<int>(nameJson.GetNamedNumber(L"ver"));
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
            CATCH_LOG();
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

        auto selectedTenant{ -1 };
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
        auto passWordJson = WDJ::JsonObject::Parse(desiredCredential.Password());
        _currentTenant = til::at(_tenantList, selectedTenant); // we already unpacked the name info, so we should just use it
        _setAccessToken(passWordJson.GetNamedString(L"accessToken"));
        _refreshToken = passWordJson.GetNamedString(L"refreshToken");
        _expiry = std::stoi(winrt::to_string(passWordJson.GetNamedString(L"expiry")));

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
        const auto message{ deviceCodeResponse.GetNamedString(L"message") };
        _WriteStringWithNewline(message);
        _WriteStringWithNewline(RS_(L"AzureCodeExpiry"));
        const auto devCode = deviceCodeResponse.GetNamedString(L"device_code");
        const auto pollInterval = std::stoi(winrt::to_string(deviceCodeResponse.GetNamedString(L"interval")));
        const auto expiresIn = std::stoi(winrt::to_string(deviceCodeResponse.GetNamedString(L"expires_in")));

        // Wait for user authentication and obtain the access/refresh tokens
        auto authenticatedResponse = _WaitForUser(devCode, pollInterval, expiresIn);
        _setAccessToken(authenticatedResponse.GetNamedString(L"access_token"));
        _refreshToken = authenticatedResponse.GetNamedString(L"refresh_token");

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
        for (auto i = 0; i < numTenants; i++)
        {
            _WriteStringWithNewline(_formatTenant(i, til::at(_tenantList, i)));
        }
        _WriteStringWithNewline(RS_(L"AzureEnterTenant"));

        auto selectedTenant{ -1 };
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
    winrt::hstring AzureConnection::_ParsePreferredShellType(const WDJ::JsonObject& settingsResponse)
    {
        if (settingsResponse.HasKey(L"properties"))
        {
            const auto userSettings = settingsResponse.GetNamedObject(L"properties");
            if (userSettings.HasKey(L"preferredShellType"))
            {
                return userSettings.GetNamedString(L"preferredShellType");
            }
        }

        return L"pwsh";
    }

    // Method description:
    // - helper function to connect the user to the Azure cloud shell
    void AzureConnection::_RunConnectState()
    {
        // Get user's cloud shell settings
        const auto settingsResponse = _GetCloudShellUserSettings();
        if (settingsResponse.HasKey(L"error"))
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
        const auto socketUri = _GetTerminal(shellType);
        _TerminalOutputHandlers(L"\r\n");

        //// Step 8: connecting to said terminal
        {
            wil::unique_winhttp_hinternet sessionHandle, connectionHandle, requestHandle, socketHandle;
            Uri parsedUri{ socketUri };
            sessionHandle.reset(WinHttpOpen(HttpUserAgent.data(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
            THROW_LAST_ERROR_IF(!sessionHandle);

            connectionHandle.reset(WinHttpConnect(sessionHandle.get(), parsedUri.Host().c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
            THROW_LAST_ERROR_IF(!connectionHandle);

            requestHandle.reset(WinHttpOpenRequest(connectionHandle.get(), L"GET", parsedUri.Path().c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE));
            THROW_LAST_ERROR_IF(!requestHandle);

            THROW_IF_WIN32_BOOL_FALSE(WinHttpSetOption(requestHandle.get(), WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0));
#pragma warning(suppress : 26477) // WINHTTP_NO_ADDITIONAL_HEADERS expands to NULL rather than nullptr (who would have thought?)
            THROW_IF_WIN32_BOOL_FALSE(WinHttpSendRequest(requestHandle.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0));
            THROW_IF_WIN32_BOOL_FALSE(WinHttpReceiveResponse(requestHandle.get(), nullptr));

            socketHandle.reset(WinHttpWebSocketCompleteUpgrade(requestHandle.get(), 0));
            THROW_LAST_ERROR_IF(!socketHandle);

            requestHandle.reset(); // We no longer need the request once we've upgraded it.
            // We have to keep the socket session and connection handles.
            _socketSessionHandle = std::move(sessionHandle);
            _socketConnectionHandle = std::move(connectionHandle);
            _webSocket = std::move(socketHandle);
        }

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
    // - the URI
    // - optional body content
    // - an optional HTTP method (defaults to POST if content is present, GET otherwise)
    // Return value:
    // - the response from the server as a json value
    WDJ::JsonObject AzureConnection::_SendRequestReturningJson(std::wstring_view uri, const WWH::IHttpContent& content, WWH::HttpMethod method)
    {
        if (!method)
        {
            method = content == nullptr ? WWH::HttpMethod::Get() : WWH::HttpMethod::Post();
        }

        WWH::HttpRequestMessage request{ method, Uri{ uri } };
        request.Content(content);

        auto headers{ request.Headers() };
        headers.Accept().TryParseAdd(L"application/json");

        const auto response{ _httpClient.SendRequestAsync(request).get() };
        const auto string{ response.Content().ReadAsStringAsync().get() };
        const auto jsonResult{ WDJ::JsonObject::Parse(string) };

        THROW_IF_AZURE_ERROR(jsonResult);
        return jsonResult;
    }

    void AzureConnection::_setAccessToken(std::wstring_view accessToken)
    {
        _accessToken = accessToken;
        _httpClient.DefaultRequestHeaders().Authorization(WWH::Headers::HttpCredentialsHeaderValue{ L"Bearer", _accessToken });
    }

    // Method description:
    // - helper function to start the device code flow
    // Return value:
    // - the response to the device code flow initiation
    WDJ::JsonObject AzureConnection::_GetDeviceCode()
    {
        auto uri{ fmt::format(L"{}common/oauth2/devicecode", _loginUri) };
        WWH::HttpFormUrlEncodedContent content{
            std::unordered_map<winrt::hstring, winrt::hstring>{
                { winrt::hstring{ L"client_id" }, winrt::hstring{ AzureClientID } },
                { winrt::hstring{ L"resource" }, winrt::hstring{ _wantedResource } },
            }
        };
        return _SendRequestReturningJson(uri, content);
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
    WDJ::JsonObject AzureConnection::_WaitForUser(const winrt::hstring& deviceCode, int pollInterval, int expiresIn)
    {
        auto uri{ fmt::format(L"{}common/oauth2/token", _loginUri) };
        WWH::HttpFormUrlEncodedContent content{
            std::unordered_map<winrt::hstring, winrt::hstring>{
                { winrt::hstring{ L"grant_type" }, winrt::hstring{ L"device_code" } },
                { winrt::hstring{ L"client_id" }, winrt::hstring{ AzureClientID } },
                { winrt::hstring{ L"resource" }, winrt::hstring{ _wantedResource } },
                { winrt::hstring{ L"code" }, deviceCode },
            }
        };

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

            try
            {
                auto response = _SendRequestReturningJson(uri, content);
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

        return nullptr;
    }

    // Method description:
    // - helper function to acquire the user's Azure tenants
    // Return value:
    // - the response which contains a list of the user's Azure tenants
    void AzureConnection::_PopulateTenantList()
    {
        auto uri{ fmt::format(L"{}tenants?api-version=2020-01-01", _resourceUri) };

        // Send the request and return the response as a json value
        auto tenantResponse{ _SendRequestReturningJson(uri, nullptr) };
        auto tenantList{ tenantResponse.GetNamedArray(L"value") };

        _tenantList.clear();
        std::transform(tenantList.begin(), tenantList.end(), std::back_inserter(_tenantList), _crackTenant);
    }

    // Method description:
    // - helper function to refresh the access/refresh tokens
    // Return value:
    // - the response with the new tokens
    void AzureConnection::_RefreshTokens()
    {
        auto uri{ fmt::format(L"{}{}/oauth2/token", _loginUri, _currentTenant->ID) };
        WWH::HttpFormUrlEncodedContent content{
            std::unordered_map<winrt::hstring, winrt::hstring>{
                { winrt::hstring{ L"grant_type" }, winrt::hstring{ L"refresh_token" } },
                { winrt::hstring{ L"client_id" }, winrt::hstring{ AzureClientID } },
                { winrt::hstring{ L"resource" }, winrt::hstring{ _wantedResource } },
                { winrt::hstring{ L"refresh_token" }, winrt::hstring{ _refreshToken } },
            }
        };

        // Send the request and return the response as a json value
        auto refreshResponse{ _SendRequestReturningJson(uri, content) };
        _setAccessToken(refreshResponse.GetNamedString(L"access_token"));
        _refreshToken = refreshResponse.GetNamedString(L"refresh_token");
        _expiry = std::stoi(winrt::to_string(refreshResponse.GetNamedString(L"expires_on")));
    }

    // Method description:
    // - helper function to get the user's cloud shell settings
    // Return value:
    // - the user's cloud shell settings
    WDJ::JsonObject AzureConnection::_GetCloudShellUserSettings()
    {
        auto uri{ fmt::format(L"{}providers/Microsoft.Portal/userSettings/cloudconsole?api-version=2020-04-01-preview", _resourceUri) };
        return _SendRequestReturningJson(uri, nullptr);
    }

    // Method description:
    // - helper function to request for a cloud shell
    // Return value:
    // - the uri for the cloud shell
    winrt::hstring AzureConnection::_GetCloudShell()
    {
        auto uri{ fmt::format(L"{}providers/Microsoft.Portal/consoles/default?api-version=2020-04-01-preview", _resourceUri) };

        WWH::HttpStringContent content{
            LR"-({"properties": {"osType": "linux"}})-",
            WSS::UnicodeEncoding::Utf8,
            L"application/json"
        };

        const auto cloudShell = _SendRequestReturningJson(uri, content, WWH::HttpMethod::Put());

        // Return the uri
        return winrt::hstring{ std::wstring{ cloudShell.GetNamedObject(L"properties").GetNamedString(L"uri") } + L"/" };
    }

    // Method description:
    // - helper function to request for a terminal
    // Return value:
    // - the uri for the terminal
    winrt::hstring AzureConnection::_GetTerminal(const winrt::hstring& shellType)
    {
        auto uri{ fmt::format(L"{}terminals?cols={}&rows={}&version=2019-01-01&shell={}", _cloudShellUri, _initialCols, _initialRows, shellType) };

        WWH::HttpStringContent content{
            L"",
            WSS::UnicodeEncoding::Utf8,
            // LOAD-BEARING. the API returns "'content-type' should be 'application/json' or 'multipart/form-data'"
            L"application/json"
        };

        const auto terminalResponse = _SendRequestReturningJson(uri, content);
        _terminalID = terminalResponse.GetNamedString(L"id");

        // Return the uri
        return terminalResponse.GetNamedString(L"socketUri");
    }

    // Method description:
    // - helper function to store the credentials
    // - we store the display name, tenant ID, access/refresh tokens, and token expiry
    void AzureConnection::_StoreCredential()
    {
        WDJ::JsonObject userName;
        userName.SetNamedValue(L"ver", WDJ::JsonValue::CreateNumberValue(CurrentCredentialVersion));
        _packTenant(userName, *_currentTenant);

        WDJ::JsonObject passWord;
        passWord.SetNamedValue(L"accessToken", WDJ::JsonValue::CreateStringValue(_accessToken));
        passWord.SetNamedValue(L"refreshToken", WDJ::JsonValue::CreateStringValue(_refreshToken));
        passWord.SetNamedValue(L"expiry", WDJ::JsonValue::CreateStringValue(std::to_wstring(_expiry)));

        PasswordVault vault;
        PasswordCredential newCredential{ PasswordVaultResourceName, userName.Stringify(), passWord.Stringify() };
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
