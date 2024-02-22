// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace Microsoft::Terminal::Azure
{
    class AzureException : public std::runtime_error
    {
        std::wstring _code;

    public:
        static bool IsErrorPayload(const winrt::Windows::Data::Json::JsonObject& errorObject)
        {
            if (!errorObject.HasKey(L"error"))
            {
                return false;
            }

            if (errorObject.GetNamedValue(L"error").ValueType() != winrt::Windows::Data::Json::JsonValueType::String)
            {
                return false;
            }

            return true;
        }

        AzureException(const winrt::Windows::Data::Json::JsonObject& errorObject) :
            runtime_error(til::u16u8(errorObject.GetNamedString(L"error_description"))), // surface the human-readable description as .what()
            _code(errorObject.GetNamedString(L"error"))
        {
        }

        std::wstring_view GetCode() const noexcept
        {
            return _code;
        }
    };

    namespace ErrorCodes
    {
        static constexpr std::wstring_view AuthorizationPending{ L"authorization_pending" };
        static constexpr std::wstring_view InvalidGrant{ L"invalid_grant" };
    }

    struct Tenant
    {
        std::wstring ID;
        std::optional<std::wstring> DisplayName;
        std::optional<std::wstring> DefaultDomain;
    };
}

#define THROW_IF_AZURE_ERROR(payload)                  \
    do                                                 \
    {                                                  \
        if (AzureException::IsErrorPayload((payload))) \
        {                                              \
            throw AzureException((payload));           \
        }                                              \
    } while (0)
