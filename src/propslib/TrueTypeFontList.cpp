// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "TrueTypeFontList.hpp"

#include "RegistrySerialization.hpp"

#pragma hdrstop

#define DEFAULT_NON_DBCS_FONTFACE L"Consolas"

SINGLE_LIST_ENTRY TrueTypeFontList::s_ttFontList;

WORD ConvertStringToDec(
    __in LPTSTR lpch)
{
    TCHAR ch;
    WORD val = 0;

    while ((ch = *lpch) != TEXT('\0'))
    {
        if (TEXT('0') <= ch && ch <= TEXT('9'))
            val = (val * 10) + (ch - TEXT('0'));
        else
            break;

        lpch++;
    }

    return val;
}

[[nodiscard]] NTSTATUS TrueTypeFontList::s_Initialize()
{
    HKEY hkRegistry;
    WCHAR awchValue[512];
    WCHAR awchData[512];
    DWORD dwIndex;
    LPWSTR pwsz;

    // Prevent memory leak. Delete if it's already allocated before refilling it.
    LOG_IF_FAILED(s_Destroy());

    NTSTATUS Status = RegistrySerialization::s_OpenKey(HKEY_LOCAL_MACHINE,
                                                       MACHINE_REGISTRY_CONSOLE_TTFONT_WIN32_PATH,
                                                       &hkRegistry);
    if (NT_SUCCESS(Status))
    {
        LPTTFONTLIST pTTFontList;

        for (dwIndex = 0;; dwIndex++)
        {
            Status = RegistrySerialization::s_EnumValue(hkRegistry,
                                                        dwIndex,
                                                        sizeof(awchValue),
                                                        (LPWSTR)awchValue,
                                                        sizeof(awchData),
                                                        (PBYTE)awchData);

            if (Status == ERROR_NO_MORE_ITEMS)
            {
                Status = STATUS_SUCCESS;
                break;
            }

            if (!NT_SUCCESS(Status))
            {
                break;
            }

            pTTFontList = (TTFONTLIST*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TTFONTLIST));
            if (pTTFontList == nullptr)
            {
                break;
            }

            pTTFontList->List.Next = nullptr;
            pTTFontList->CodePage = ConvertStringToDec(awchValue);
            pwsz = awchData;
            if (*pwsz == BOLD_MARK)
            {
                pTTFontList->fDisableBold = TRUE;
                pwsz++;
            }
            else
            {
                pTTFontList->fDisableBold = FALSE;
            }

            StringCchCopyW(pTTFontList->FaceName1,
                           ARRAYSIZE(pTTFontList->FaceName1),
                           pwsz);

            // wcslen is only valid on non-null pointers.
            if (pwsz != nullptr)
            {
                pwsz += wcslen(pwsz) + 1;

                // Validate that pwsz must be pointing to a position in awchData array after the movement.
                if (pwsz >= awchData && pwsz < (awchData + ARRAYSIZE(awchData)))
                {
                    if (*pwsz == BOLD_MARK)
                    {
                        pTTFontList->fDisableBold = TRUE;
                        pwsz++;
                    }
                    StringCchCopyW(pTTFontList->FaceName2,
                                   ARRAYSIZE(pTTFontList->FaceName2),
                                   pwsz);
                }
            }

            PushEntryList(&s_ttFontList, &(pTTFontList->List));
        }

        RegCloseKey(hkRegistry);
    }

    return STATUS_SUCCESS;
}

[[nodiscard]] NTSTATUS TrueTypeFontList::s_Destroy()
{
    while (s_ttFontList.Next != nullptr)
    {
        LPTTFONTLIST pTTFontList = (LPTTFONTLIST)PopEntryList(&s_ttFontList);

        if (pTTFontList != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, pTTFontList);
        }
    }

    s_ttFontList.Next = nullptr;

    return STATUS_SUCCESS;
}

LPTTFONTLIST TrueTypeFontList::s_SearchByName(_In_opt_ LPCWSTR pwszFace,
                                              _In_ BOOL fCodePage,
                                              _In_ UINT CodePage)
{
    PSINGLE_LIST_ENTRY pTemp = s_ttFontList.Next;

    if (pwszFace)
    {
        while (pTemp != nullptr)
        {
            LPTTFONTLIST pTTFontList = (LPTTFONTLIST)pTemp;

            if (wcscmp(pwszFace, pTTFontList->FaceName1) == 0 ||
                wcscmp(pwszFace, pTTFontList->FaceName2) == 0)
            {
                if (fCodePage)
                    if (pTTFontList->CodePage == CodePage)
                        return pTTFontList;
                    else
                        return nullptr;
                else
                    return pTTFontList;
            }

            pTemp = pTemp->Next;
        }
    }

    return nullptr;
}

[[nodiscard]] NTSTATUS TrueTypeFontList::s_SearchByCodePage(const UINT uiCodePage,
                                                            _Out_writes_(cchFaceName) PWSTR pwszFaceName,
                                                            const size_t cchFaceName)
{
    NTSTATUS status = STATUS_SUCCESS;
    BOOL fFontFound = FALSE;

    // Look through our list entries to see if we can find a corresponding truetype font for this codepage
    for (PSINGLE_LIST_ENTRY pListEntry = s_ttFontList.Next; pListEntry != nullptr && !fFontFound; pListEntry = pListEntry->Next)
    {
        LPTTFONTLIST pTTFontEntry = (LPTTFONTLIST)pListEntry;
        if (pTTFontEntry->CodePage == uiCodePage)
        {
            // found a match, use this font's primary facename
            status = StringCchCopyW(pwszFaceName, cchFaceName, pTTFontEntry->FaceName1);
            fFontFound = TRUE;
        }
    }

    if (!fFontFound)
    {
        status = StringCchCopyW(pwszFaceName, cchFaceName, DEFAULT_NON_DBCS_FONTFACE);
    }

    return status;
}
