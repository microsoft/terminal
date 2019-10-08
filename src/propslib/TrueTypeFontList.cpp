// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "TrueTypeFontList.hpp"

#include "RegistrySerialization.hpp"

#pragma hdrstop

#define DEFAULT_NON_DBCS_FONTFACE L"Consolas"

std::vector<TrueTypeFontList::Entry> TrueTypeFontList::s_ttFontList;

[[nodiscard]] HRESULT TrueTypeFontList::s_Initialize()
try
{
    wil::unique_hkey hkRegistry;

    // Prevent memory leak. Delete if it's already allocated before refilling it.
    LOG_IF_FAILED(s_Destroy());

    RETURN_IF_NTSTATUS_FAILED(RegistrySerialization::s_OpenKey(HKEY_LOCAL_MACHINE,
                                                               MACHINE_REGISTRY_CONSOLE_TTFONT_WIN32_PATH,
                                                               &hkRegistry));
    for (DWORD dwIndex = 0;; dwIndex++)
    {
        wchar_t awchValue[512];
        wchar_t awchData[512];
        NTSTATUS Status = RegistrySerialization::s_EnumValue(hkRegistry.get(),
                                                             dwIndex,
                                                             sizeof(awchValue),
                                                             (LPWSTR)awchValue,
                                                             sizeof(awchData),
                                                             (PBYTE)awchData);

        if (Status == ERROR_NO_MORE_ITEMS)
        {
            // This is fine.
            break;
        }

        RETURN_IF_NTSTATUS_FAILED(Status);

        const wchar_t* pwsz{ awchData };

        Entry fontEntry;
        fontEntry.CodePage = std::stoi(awchValue);
        if (*pwsz == BOLD_MARK)
        {
            fontEntry.DisableBold = TRUE;
            pwsz++;
        }
        else
        {
            fontEntry.DisableBold = FALSE;
        }

        fontEntry.FontNames.first = pwsz;

        // wcslen is only valid on non-null pointers.
        if (pwsz != nullptr)
        {
            pwsz += wcslen(pwsz) + 1;

            // Validate that pwsz must be pointing to a position in awchData array after the movement.
            if (pwsz >= awchData && pwsz < (awchData + ARRAYSIZE(awchData)))
            {
                if (*pwsz == BOLD_MARK)
                {
                    fontEntry.DisableBold = TRUE;
                    pwsz++;
                }

                fontEntry.FontNames.second = pwsz;
            }
        }

        s_ttFontList.emplace_back(std::move(fontEntry));
    }

    return S_OK;
}
CATCH_RETURN();

[[nodiscard]] HRESULT TrueTypeFontList::s_Destroy()
try
{
    s_ttFontList.clear();
    return S_OK;
}
CATCH_RETURN();

const TrueTypeFontList::Entry* TrueTypeFontList::s_SearchByName(const std::wstring_view name,
                                                                const std::optional<unsigned int> CodePage)
{
    if (!name.empty())
    {
        for (const auto& entry : s_ttFontList)
        {
            if (name != entry.FontNames.first &&
                name != entry.FontNames.second)
            {
                continue;
            }

            if (CodePage.has_value() && entry.CodePage != CodePage.value())
            {
                continue;
            }

            return &entry;
        }
    }

    return nullptr;
}

[[nodiscard]] HRESULT TrueTypeFontList::s_SearchByCodePage(const unsigned int codePage,
                                                           std::wstring& outFaceName)
{
    for (const auto& entry : s_ttFontList)
    {
        if (entry.CodePage == codePage)
        {
            outFaceName = entry.FontNames.first;
            return S_OK;
        }
    }

    // Fallthrough: we didn't find a font; presume it's non-DBCS.
    outFaceName = DEFAULT_NON_DBCS_FONTFACE;
    return S_OK;
}
