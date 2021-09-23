// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "RegistrySerialization.hpp"

#pragma hdrstop

#define SET_FIELD_AND_SIZE(x) FIELD_OFFSET(Settings, x), RTL_FIELD_SIZE(Settings, x)

#define NT_TESTNULL(var) (((var) == nullptr) ? STATUS_NO_MEMORY : STATUS_SUCCESS)

DWORD RegistrySerialization::ToWin32RegistryType(const _RegPropertyType type)
{
    switch (type)
    {
    case _RegPropertyType::Boolean:
    case _RegPropertyType::Dword:
    case _RegPropertyType::Word:
    case _RegPropertyType::Byte:
    case _RegPropertyType::Coordinate:
        return REG_DWORD;
    case _RegPropertyType::String:
        return REG_SZ;
    default:
        return REG_NONE;
    }
}

// clang-format off
// Registry settings to load (not all of them, some are special)
const RegistrySerialization::_RegPropertyMap RegistrySerialization::s_PropertyMappings[] =
{
    //+---------------------------------+-----------------------------------------------+-----------------------------------------------+
    //| Property type                   | Property Name                                 | Corresponding Settings field                  |
    { _RegPropertyType::Word,           CONSOLE_REGISTRY_POPUPATTR,                     SET_FIELD_AND_SIZE(_wPopupFillAttribute),        },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_INSERTMODE,                    SET_FIELD_AND_SIZE(_bInsertMode)                 },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_LINESELECTION,                 SET_FIELD_AND_SIZE(_bLineSelection)              },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_FILTERONPASTE,                 SET_FIELD_AND_SIZE(_fFilterOnPaste)              },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_LINEWRAP,                      SET_FIELD_AND_SIZE(_bWrapText)                   },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_CTRLKEYSHORTCUTS_DISABLED,     SET_FIELD_AND_SIZE(_fCtrlKeyShortcutsDisabled)   },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_QUICKEDIT,                     SET_FIELD_AND_SIZE(_bQuickEdit)                  },
    { _RegPropertyType::Byte,           CONSOLE_REGISTRY_WINDOWALPHA,                   SET_FIELD_AND_SIZE(_bWindowAlpha)                },
    { _RegPropertyType::Coordinate,     CONSOLE_REGISTRY_FONTSIZE,                      SET_FIELD_AND_SIZE(_dwFontSize)                  },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_FONTFAMILY,                    SET_FIELD_AND_SIZE(_uFontFamily)                 },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_FONTWEIGHT,                    SET_FIELD_AND_SIZE(_uFontWeight)                 },
    { _RegPropertyType::String,         CONSOLE_REGISTRY_FACENAME,                      SET_FIELD_AND_SIZE(_FaceName)                    },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_CURSORSIZE,                    SET_FIELD_AND_SIZE(_uCursorSize)                 },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_HISTORYSIZE,                   SET_FIELD_AND_SIZE(_uHistoryBufferSize)          },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_HISTORYBUFS,                   SET_FIELD_AND_SIZE(_uNumberOfHistoryBuffers)     },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_HISTORYNODUP,                  SET_FIELD_AND_SIZE(_bHistoryNoDup)               },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_SCROLLSCALE,                   SET_FIELD_AND_SIZE(_uScrollScale)                },
    { _RegPropertyType::Word,           CONSOLE_REGISTRY_FILLATTR,                      SET_FIELD_AND_SIZE(_wFillAttribute)              },
    { _RegPropertyType::Coordinate,     CONSOLE_REGISTRY_BUFFERSIZE,                    SET_FIELD_AND_SIZE(_dwScreenBufferSize)          },
    { _RegPropertyType::Coordinate,     CONSOLE_REGISTRY_WINDOWSIZE,                    SET_FIELD_AND_SIZE(_dwWindowSize)                },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_TRIMZEROHEADINGS,              SET_FIELD_AND_SIZE(_fTrimLeadingZeros)           },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_ENABLE_COLOR_SELECTION,        SET_FIELD_AND_SIZE(_fEnableColorSelection)       },
    { _RegPropertyType::Coordinate,     CONSOLE_REGISTRY_WINDOWPOS,                     SET_FIELD_AND_SIZE(_dwWindowOrigin)              },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_CURSORCOLOR,                   SET_FIELD_AND_SIZE(_CursorColor)                 },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_CURSORTYPE,                    SET_FIELD_AND_SIZE(_CursorType)                  },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_INTERCEPTCOPYPASTE,            SET_FIELD_AND_SIZE(_fInterceptCopyPaste)         },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_DEFAULTFOREGROUND,             SET_FIELD_AND_SIZE(_DefaultForeground)           },
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_DEFAULTBACKGROUND,             SET_FIELD_AND_SIZE(_DefaultBackground)           },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_TERMINALSCROLLING,             SET_FIELD_AND_SIZE(_TerminalScrolling)           },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_USEDX,                         SET_FIELD_AND_SIZE(_fUseDx)                      },
    { _RegPropertyType::Boolean,        CONSOLE_REGISTRY_COPYCOLOR,                     SET_FIELD_AND_SIZE(_fCopyColor)                  }

};
const size_t RegistrySerialization::s_PropertyMappingsSize = ARRAYSIZE(s_PropertyMappings);

// Global registry settings to load
const RegistrySerialization::_RegPropertyMap RegistrySerialization::s_GlobalPropMappings[] =
{
    //+---------------------------------+-----------------------------------------------+-----------------------------------------------+
    //| Property type                   | Property Name                                 | Corresponding Settings field                  |
    { _RegPropertyType::Dword,          CONSOLE_REGISTRY_VIRTTERM_LEVEL,                SET_FIELD_AND_SIZE(_dwVirtTermLevel), }
};
const size_t RegistrySerialization::s_GlobalPropMappingsSize = ARRAYSIZE(s_GlobalPropMappings);

// clang-format off

// Routine Description:
// - Reads number from the registry and applies it to the given property if the value exists
//   Supports: Dword, Word, Byte, Boolean, and Coordinate
// Arguments:
// - hKey - Registry key to read from
// - pPropMap - Contains property information to use in looking up/storing value data
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_LoadRegDword(const HKEY hKey, const _RegPropertyMap* const pPropMap, _In_ Settings* const pSettings)
{
    // find offset into destination structure for this numerical value
    PBYTE const pbField = (PBYTE)pSettings + pPropMap->dwFieldOffset;

    // attempt to load number into this field
    // If we're not successful, it's ok. Just don't fill it.
    DWORD dwValue;
    NTSTATUS Status = s_QueryValue(hKey,
                                   pPropMap->pwszValueName,
                                   sizeof(dwValue),
                                   ToWin32RegistryType(pPropMap->propertyType),
                                   (PBYTE)& dwValue,
                                   nullptr);
    if (NT_SUCCESS(Status))
    {
        switch (pPropMap->propertyType)
        {
        case _RegPropertyType::Dword:
        {
            DWORD* const pdField = (DWORD*)pbField;
            *pdField = dwValue;
            break;
        }
        case _RegPropertyType::Word:
        {
            WORD* const pwField = (WORD*)pbField;
            *pwField = (WORD)dwValue;
            break;
        }
        case _RegPropertyType::Boolean:
        {
            *pbField = !!dwValue;
            break;
        }
        case _RegPropertyType::Byte:
        {
            *pbField = LOBYTE(dwValue);
            break;
        }
        case _RegPropertyType::Coordinate:
        {
            PCOORD pcoordField = (PCOORD)pbField;
            pcoordField->X = LOWORD(dwValue);
            pcoordField->Y = HIWORD(dwValue);
            break;
        }
        }
    }

    return Status;
}

// Routine Description:
// - Reads string from the registry and applies it to the given property if the value exists
// Arguments:
// - hKey - Registry key to read from
// - pPropMap - Contains property information to use in looking up/storing value data
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_LoadRegString(const HKEY hKey, const _RegPropertyMap* const pPropMap, _In_ Settings* const pSettings)
{
    // find offset into destination structure for this numerical value
    PBYTE const pbField = (PBYTE)pSettings + pPropMap->dwFieldOffset;

    // number of characters within the field
    size_t const cchField = pPropMap->cbFieldSize / sizeof(WCHAR);

    PWCHAR pwchString = new(std::nothrow) WCHAR[cchField];
    NTSTATUS Status = NT_TESTNULL(pwchString);
    if (NT_SUCCESS(Status))
    {
        Status = s_QueryValue(hKey,
                              pPropMap->pwszValueName,
                              (DWORD)(cchField) * sizeof(WCHAR),
                              ToWin32RegistryType(pPropMap->propertyType),
                              (PBYTE)pwchString,
                              nullptr);
        if (NT_SUCCESS(Status))
        {
            // ensure pwchString is null terminated
            pwchString[cchField - 1] = UNICODE_NULL;

            Status = StringCchCopyW((PWSTR)pbField, cchField, pwchString);
        }

        delete[] pwchString;
    }

    return Status;
}

#pragma region Helpers

// Routine Description:
// - Opens the root console key from HKCU
// Arguments:
// - phCurrentUserKey - Returns a handle to the HKCU root
// - phConsoleKey - Returns a handle to the Console subkey
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_OpenConsoleKey(_Out_ HKEY* phCurrentUserKey, _Out_ HKEY* phConsoleKey)
{
    // Always set an output value. It will be made valid before the end if everything succeeds.
    *phCurrentUserKey = static_cast<HKEY>(INVALID_HANDLE_VALUE);
    *phConsoleKey = static_cast<HKEY>(INVALID_HANDLE_VALUE);

    wil::unique_hkey currentUserKey;
    wil::unique_hkey consoleKey;

    // Open the current user registry key.
    NTSTATUS Status = NTSTATUS_FROM_WIN32(RegOpenCurrentUser(KEY_READ | KEY_WRITE, &currentUserKey));

    if (NT_SUCCESS(Status))
    {
        // Open the console registry key.
        Status = s_OpenKey(currentUserKey.get(), CONSOLE_REGISTRY_STRING, &consoleKey);

        // If we can't open the console registry key, create one and open it.
        if (NTSTATUS_FROM_WIN32(ERROR_FILE_NOT_FOUND) == Status)
        {
            Status = s_CreateKey(currentUserKey.get(), CONSOLE_REGISTRY_STRING, &consoleKey);
        }

        // If we're successful, give the keys back.
        if (NT_SUCCESS(Status))
        {
            *phCurrentUserKey = currentUserKey.release();
            *phConsoleKey = consoleKey.release();
        }
    }

    return Status;
}

// Routine Description:
// - Opens a subkey of the given key. Fails if it doesn't exist.
// - NOTE: To create if it doesn't exist and open otherwise, try `s_CreateKey`.
// Arguments:
// - hKey - Handle to a registry key
// - pwszSubKey - String name of sub key
// - phResult - Filled with handle to the sub key if available. Check return status.
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_OpenKey(_In_opt_ HKEY const hKey, _In_ PCWSTR const pwszSubKey, _Out_ HKEY* const phResult)
{
    return NTSTATUS_FROM_WIN32(RegOpenKeyW(hKey, pwszSubKey, phResult));
}

// Routine Description:
// - Deletes the value under a given key
// Arguments:
// - hKey - Handle to a registry key
// - pwszValueName - String name of value to delete under that key
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_DeleteValue(const HKEY hKey, _In_ PCWSTR const pwszValueName)
{
    return NTSTATUS_FROM_WIN32(RegDeleteKeyValueW(hKey, nullptr, pwszValueName));
}

// Routine Description:
// - Creates a subkey of the given key
//   This function creates keys with read/write access.
// - If key already exists, opens existing.
// Arguments:
// - hKey - Handle to a registry key
// - pwszSubKey - String name of sub key
// - phResult - Filled with handle to the sub key if created/opened successfully. Check return status.
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_CreateKey(const HKEY hKey, _In_ PCWSTR const pwszSubKey, _Out_ HKEY* const phResult)
{
    return NTSTATUS_FROM_WIN32(RegCreateKeyW(hKey, pwszSubKey, phResult));
}

// Routine Description:
// - Sets a value for the given key
// Arguments:
// - hKey - Handle to a registry key
// - pwszValueName - Name of the value to set
// - dwType - Type of the value being set (see: http://msdn.microsoft.com/en-us/library/windows/desktop/ms724884(v=vs.85).aspx)
// - pbData - Pointer to byte stream of data to set into this value
// - cbDataLength - The length in bytes of the data provided
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_SetValue(const HKEY hKey,
                                           _In_ PCWSTR const pValueName,
                                           const DWORD dwType,
                                           _In_reads_bytes_(cbDataLength) BYTE* const pbData,
                                           const DWORD cbDataLength)
{
    return NTSTATUS_FROM_WIN32(RegSetKeyValueW(hKey,
                                               nullptr,
                                               pValueName,
                                               dwType,
                                               pbData,
                                               cbDataLength));
}

// Routine Description:
// - Queries a value for the given key
// Arguments:
// - hKey - Handle to a registry key
// - pwszValueName - Name of the value to query
// - cbValueLength - Length of the provided data buffer.
// - regType - the type of the registry key.
// - pbData - Pointer to byte stream of data to fill with the registry value data.
// - pcbDataLength - Number of bytes filled in the given data buffer
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_QueryValue(const HKEY hKey,
                                             _In_ PCWSTR const pwszValueName,
                                             const DWORD cbValueLength,
                                             const DWORD regType,
                                             _Out_writes_bytes_(cbValueLength) BYTE* const pbData,
                                             _Out_opt_ _Out_range_(0, cbValueLength) DWORD* const pcbDataLength)
{
    DWORD cbData = cbValueLength;

    DWORD actualRegType = 0;
    LONG const Result = RegQueryValueExW(hKey,
                                         pwszValueName,
                                         nullptr,
                                         &actualRegType,
                                         pbData,
                                         &cbData);
    if (ERROR_FILE_NOT_FOUND != Result &&
        actualRegType != regType)
    {
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    if (nullptr != pcbDataLength)
    {
        *pcbDataLength = cbData;
    }

    return NTSTATUS_FROM_WIN32(Result);
}

// Routine Description:
// - Enumerates the values for the given key
// Arguments:
// - hKey - Handle to a registry key
// - dwIndex - Index of value within this key to return
// - cbValueLength - Length of the provided value name buffer.
// - pwszValueName - Value name buffer to be filled with null terminated string name of value.
// - cbDataLength - Length of the provided value data buffer.
// - pbData - Value data buffer to be filled based on data type. Will be null terminated for string types. (REG_SZ, REG_MULTI_SZ, REG_EXPAND_SZ)
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_EnumValue(const HKEY hKey,
                                            const DWORD dwIndex,
                                            const DWORD cbValueLength,
                                            _Out_writes_bytes_(cbValueLength) PWSTR const pwszValueName,
                                            const DWORD cbDataLength,
                                            _Out_writes_bytes_(cbDataLength) BYTE* const pbData)
{
    DWORD cchValueName = cbValueLength / sizeof(WCHAR);
    DWORD cbData = cbDataLength;

#pragma prefast(suppress: 26015, "prefast doesn't realize that cbData == cbDataLength and cchValueName == cbValueLength/2")
    return NTSTATUS_FROM_WIN32(RegEnumValueW(hKey,
                                             dwIndex,
                                             pwszValueName,
                                             &cchValueName,
                                             nullptr,
                                             nullptr,
                                             pbData,
                                             &cbData));
}

// Routine Description:
// - Updates the value in a given key
// - NOTE: For the console registry, if we're filling a console subkey and the default matches, the subkey copy will be deleted.
//         We only store settings in subkeys if they differ from the defaults.
// Arguments:
// - hConsoleKey - Handle to the default console key
// - hKey - Handle to the console subkey for this particular console
// - pwszValueName - Name of the value within the default and subkeys to check/update.
// - dwType - Type of the value being set (see: http://msdn.microsoft.com/en-us/library/windows/desktop/ms724884(v=vs.85).aspx)
// - pbData - Value data buffer to be stored into the registry.
// - cbDataLength - Length of the provided value data buffer.
// Return Value:
// - STATUS_SUCCESSFUL or appropriate NTSTATUS reply for registry operations.
[[nodiscard]]
NTSTATUS RegistrySerialization::s_UpdateValue(const HKEY hConsoleKey,
                                              const HKEY hKey,
                                              _In_ PCWSTR const pwszValueName,
                                              const DWORD dwType,
                                              _In_reads_bytes_(cbDataLength) BYTE* pbData,
                                              const DWORD cbDataLength)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL; // This value won't be used, added to avoid compiler warnings.
    BYTE* Data = new(std::nothrow) BYTE[cbDataLength];
    if (Data != nullptr)
    {
        // If this is not the main console key but the value is the same,
        // delete it. Otherwise, set it.
        bool fDeleteKey = false;
        if (hConsoleKey != hKey)
        {
            Status = s_QueryValue(hConsoleKey, pwszValueName, cbDataLength, dwType, Data, nullptr);
            if (NT_SUCCESS(Status))
            {
                fDeleteKey = (memcmp(pbData, Data, cbDataLength) == 0);
            }
        }

        if (fDeleteKey)
        {
            Status = s_DeleteValue(hKey, pwszValueName);
        }
        else
        {
            Status = s_SetValue(hKey, pwszValueName, dwType, pbData, cbDataLength);
        }
        delete[] Data;
    }

    return Status;
}

[[nodiscard]]
NTSTATUS RegistrySerialization::s_OpenCurrentUserConsoleTitleKey(_In_ PCWSTR const title,
                                                                 _Out_ HKEY* phCurrentUserKey,
                                                                 _Out_ HKEY* phConsoleKey,
                                                                 _Out_ HKEY* phTitleKey)
{
    NTSTATUS Status = NTSTATUS_FROM_WIN32(RegOpenKeyW(HKEY_CURRENT_USER,
                                                      nullptr,
                                                      phCurrentUserKey));
    if (NT_SUCCESS(Status))
    {
        Status = RegistrySerialization::s_CreateKey(*phCurrentUserKey,
                                                    CONSOLE_REGISTRY_STRING,
                                                    phConsoleKey);
        if (NT_SUCCESS(Status))
        {
            Status = RegistrySerialization::s_CreateKey(*phConsoleKey, title, phTitleKey);
            if (!NT_SUCCESS(Status))
            {
                RegCloseKey(*phConsoleKey);
                RegCloseKey(*phCurrentUserKey);
            }
            //else all keys were created/opened successfully, and we'll return success
        }
        else
        {
            RegCloseKey(*phCurrentUserKey);
        }
    }
    return Status;
}



#pragma endregion
