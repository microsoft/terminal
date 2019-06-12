// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "registry.hpp"

#include "dbcs.h"
#include "srvinit.h"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

#define SET_FIELD_AND_SIZE(x) FIELD_OFFSET(Settings, (x)), RTL_FIELD_SIZE(Settings, (x))

using Microsoft::Console::Interactivity::ServiceLocator;

Registry::Registry(_In_ Settings* const pSettings) :
    _pSettings(pSettings)
{
}

Registry::~Registry()
{
}

// Routine Description:
// - Reads extended edit keys and related registry information into the global state.
// Arguments:
// - hConsoleKey - The console subkey to use for querying.
// Return Value:
// - <none>
void Registry::GetEditKeys(_In_opt_ HKEY hConsoleKey) const
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    NTSTATUS Status;
    HKEY hCurrentUserKey = nullptr;
    if (hConsoleKey == nullptr)
    {
        Status = RegistrySerialization::s_OpenConsoleKey(&hCurrentUserKey, &hConsoleKey);
        if (!NT_SUCCESS(Status))
        {
            return;
        }
    }

    // determine whether the user wants to allow alt-f4 to close the console (global setting)
    DWORD dwValue;
    Status = RegistrySerialization::s_QueryValue(hConsoleKey,
                                                 CONSOLE_REGISTRY_ALLOW_ALTF4_CLOSE,
                                                 sizeof(dwValue),
                                                 REG_DWORD,
                                                 (PBYTE)&dwValue,
                                                 nullptr);
    if (NT_SUCCESS(Status) && dwValue <= 1)
    {
        gci.SetAltF4CloseAllowed(!!dwValue);
    }

    // --- START LOAD BEARING CODE ---
    // NOTE: Because of some accident of history (win2k time or before) the key type of
    // CONSOLE_REGISTRY_WORD_DELIM was set to REG_DWORD when it should have been REG_SZ. Registry key reads
    // didn't use to be type checked so the key was "read" and the buffer it was read into was used instead of
    // the default word delimiters. Meaning that it really just turned off the word delimiter feature entirely
    // because the buffer was still zero'd out (except for the space character which is handled differently
    // and not modifiable by this registry setting).
    //
    // In order to maintain compatibility with this long-standing behavior we need set the word delimiters
    // based on three scenarios:
    // 1. key type is REG_DWORD: have no word delimiters
    // 2. key type is REG_SZ: someone has specified custom word delimiters, use them
    // 3. key doesn't exist: use the original default word delimiters
    //
    // The space character is always considered a word delimiter, no matter the scenario.
    //
    // Read word delimiters from registry
    auto& delimiters = ServiceLocator::LocateGlobals().WordDelimiters;
    delimiters.clear();
    Status = RegistrySerialization::s_QueryValue(hConsoleKey,
                                                 CONSOLE_REGISTRY_WORD_DELIM,
                                                 sizeof(dwValue),
                                                 REG_DWORD,
                                                 reinterpret_cast<BYTE*>(&dwValue),
                                                 nullptr);
    if (!NT_SUCCESS(Status))
    {
        // the key isn't a REG_DWORD, try to read it as a REG_SZ
        const size_t bufferSize = 64;
        WCHAR awchBuffer[bufferSize];
        DWORD cbWritten = 0;
        Status = RegistrySerialization::s_QueryValue(hConsoleKey,
                                                     CONSOLE_REGISTRY_WORD_DELIM,
                                                     bufferSize * sizeof(WCHAR),
                                                     REG_SZ,
                                                     reinterpret_cast<BYTE*>(awchBuffer),
                                                     &cbWritten);
        if (NT_SUCCESS(Status))
        {
            // we read something, set it as the word delimiters
            const std::wstring regWordDelimiters{ awchBuffer, cbWritten / sizeof(wchar_t) };
            for (const wchar_t wch : regWordDelimiters)
            {
                if (wch == '\0')
                {
                    break;
                }
                delimiters.push_back(wch);
            }
        }
        else
        {
            // the key isn't a REG_DWORD or a REG_SZ, fall back to our default word delimiters
            delimiters = { '\\', '+', '!', ':', '=', '/', '.', '<', '>', ';', '|', '&' };
        }
    }
    // --- END LOAD BEARING CODE ---

    if (hCurrentUserKey)
    {
        RegCloseKey((HKEY)hConsoleKey);
        RegCloseKey((HKEY)hCurrentUserKey);
    }
}

void Registry::_LoadMappedProperties(_In_reads_(cPropertyMappings) const RegistrySerialization::RegPropertyMap* const rgPropertyMappings,
                                     const size_t cPropertyMappings,
                                     const HKEY hKey)
{
    // Iterate through properties table and load each setting for common property types
    for (UINT iMapping = 0; iMapping < cPropertyMappings; iMapping++)
    {
        const RegistrySerialization::RegPropertyMap* const pPropMap = &(rgPropertyMappings[iMapping]);

        NTSTATUS Status = STATUS_SUCCESS;

        switch (pPropMap->propertyType)
        {
        case RegistrySerialization::_RegPropertyType::Boolean:
        case RegistrySerialization::_RegPropertyType::Dword:
        case RegistrySerialization::_RegPropertyType::Word:
        case RegistrySerialization::_RegPropertyType::Byte:
        case RegistrySerialization::_RegPropertyType::Coordinate:
        {
            Status = RegistrySerialization::s_LoadRegDword(hKey, pPropMap, _pSettings);
            break;
        }
        case RegistrySerialization::_RegPropertyType::String:
        {
            Status = RegistrySerialization::s_LoadRegString(hKey, pPropMap, _pSettings);
            break;
        }
        }

        // Don't log "file not found" messages. It's fine to not find a registry key. Log other types.
        if (!NT_SUCCESS(Status) && NTSTATUS_FROM_WIN32(ERROR_FILE_NOT_FOUND) != Status)
        {
            LOG_NTSTATUS(Status);
        }
    }
}

// Routine Description:
// - Read settings that apply to all console instances from the registry.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Registry::LoadGlobalsFromRegistry()
{
    HKEY hCurrentUserKey;
    HKEY hConsoleKey;
    NTSTATUS status = RegistrySerialization::s_OpenConsoleKey(&hCurrentUserKey, &hConsoleKey);

    if (NT_SUCCESS(status))
    {
        _LoadMappedProperties(RegistrySerialization::s_GlobalPropMappings, RegistrySerialization::s_GlobalPropMappingsSize, hConsoleKey);

        RegCloseKey((HKEY)hConsoleKey);
        RegCloseKey((HKEY)hCurrentUserKey);
    }
}

// Routine Description:
// - Reads default settings from the registry into the current console state.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Registry::LoadDefaultFromRegistry()
{
    LoadFromRegistry(L"");
}

// Routine Description:
// - Reads settings from the registry into the current console state.
// Arguments:
// - pwszConsoleTitle - Name of the console subkey to open. Empty string for the default console settings.
// Return Value:
// - <none>
void Registry::LoadFromRegistry(_In_ PCWSTR const pwszConsoleTitle)
{
    HKEY hCurrentUserKey;
    HKEY hConsoleKey;
    NTSTATUS Status = RegistrySerialization::s_OpenConsoleKey(&hCurrentUserKey, &hConsoleKey);
    if (!NT_SUCCESS(Status))
    {
        return;
    }

    // Open the console title subkey.
    LPWSTR TranslatedConsoleTitle = TranslateConsoleTitle(pwszConsoleTitle, TRUE, TRUE);
    if (TranslatedConsoleTitle == nullptr)
    {
        RegCloseKey(hConsoleKey);
        RegCloseKey(hCurrentUserKey);
        return;
    }

    HKEY hTitleKey;
    Status = RegistrySerialization::s_OpenKey(hConsoleKey, TranslatedConsoleTitle, &hTitleKey);
    delete[] TranslatedConsoleTitle;
    TranslatedConsoleTitle = nullptr;

    if (!NT_SUCCESS(Status))
    {
        TranslatedConsoleTitle = TranslateConsoleTitle(pwszConsoleTitle, TRUE, FALSE);

        if (TranslatedConsoleTitle == nullptr)
        {
            RegCloseKey(hConsoleKey);
            RegCloseKey(hCurrentUserKey);
            return;
        }

        Status = RegistrySerialization::s_OpenKey(hConsoleKey, TranslatedConsoleTitle, &hTitleKey);
        delete[] TranslatedConsoleTitle;
        TranslatedConsoleTitle = nullptr;
    }

    if (!NT_SUCCESS(Status))
    {
        RegCloseKey(hConsoleKey);
        RegCloseKey(hCurrentUserKey);
        return;
    }

    // Iterate through properties table and load each setting for common property types
    _LoadMappedProperties(RegistrySerialization::s_PropertyMappings, RegistrySerialization::s_PropertyMappingsSize, hTitleKey);

    // Now load complex properties
    // Some properties shouldn't be filled by the registry if a copy already exists from the process start information.
    DWORD dwValue;

    // Window Origin Autopositioning Setting
    Status = RegistrySerialization::s_QueryValue(hTitleKey,
                                                 CONSOLE_REGISTRY_WINDOWPOS,
                                                 sizeof(dwValue),
                                                 REG_DWORD,
                                                 (PBYTE)&dwValue,
                                                 nullptr);

    if (NT_SUCCESS(Status))
    {
        // The presence of a position key means autopositioning is false.
        _pSettings->SetAutoPosition(FALSE);
    }
    //  The absence of the window position key means that autopositioning is true,
    //      HOWEVER, the defaults might not have been auto-pos, so don't assume that they are.

    // Code Page
    Status = RegistrySerialization::s_QueryValue(hTitleKey,
                                                 CONSOLE_REGISTRY_CODEPAGE,
                                                 sizeof(dwValue),
                                                 REG_DWORD,
                                                 (PBYTE)&dwValue,
                                                 nullptr);
    if (NT_SUCCESS(Status))
    {
        _pSettings->SetCodePage(dwValue);

        // If this routine specified default settings for console property,
        // then make sure code page value when East Asian environment.
        // If code page value does not the same to OEMCP and any EA's code page then
        // we are override code page value to OEMCP on default console property.
        // Because, East Asian environment has limitation that doesn not switch to
        // another EA's code page by the SetConsoleCP/SetConsoleOutputCP.
        //
        // Compare of pwszConsoleTitle and L"" has limit to default property of console.
        // It means, this code doesn't care user defined property.
        // Content of user defined property has responsibility to themselves.
        if (wcscmp(pwszConsoleTitle, L"") == 0 &&
            IsAvailableEastAsianCodePage(_pSettings->GetCodePage()) &&
            ServiceLocator::LocateGlobals().uiOEMCP != _pSettings->GetCodePage())
        {
            _pSettings->SetCodePage(ServiceLocator::LocateGlobals().uiOEMCP);
        }
    }

    // Color table
    for (DWORD i = 0; i < COLOR_TABLE_SIZE; i++)
    {
        WCHAR awchBuffer[64];
        StringCchPrintfW(awchBuffer, ARRAYSIZE(awchBuffer), CONSOLE_REGISTRY_COLORTABLE, i);
        Status = RegistrySerialization::s_QueryValue(hTitleKey,
                                                     awchBuffer,
                                                     sizeof(dwValue),
                                                     REG_DWORD,
                                                     (PBYTE)&dwValue,
                                                     nullptr);
        if (NT_SUCCESS(Status))
        {
            _pSettings->SetColorTableEntry(i, dwValue);
        }
    }

    GetEditKeys(hConsoleKey);

    // Close the registry keys
    RegCloseKey(hTitleKey);

    // These could be equal if there was no title. Don't try to double close.
    if (hTitleKey != hConsoleKey)
    {
        RegCloseKey(hConsoleKey);
    }

    RegCloseKey(hCurrentUserKey);
}
