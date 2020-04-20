// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "cpprest/json.h"

namespace Microsoft::Terminal::Azure
{
    class AzureException : public std::runtime_error
    {
        std::wstring _code;

    public:
        static bool IsErrorPayload(const web::json::value& errorObject)
        {
            return errorObject.has_string_field(L"error");
        }

        AzureException(const web::json::value& errorObject) :
            runtime_error(til::u16u8(errorObject.at(L"error_description").as_string())), // surface the human-readable description as .what()
            _code(errorObject.at(L"error").as_string())
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
