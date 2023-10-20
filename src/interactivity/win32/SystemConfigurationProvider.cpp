// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "SystemConfigurationProvider.hpp"

#include "icon.hpp"
#include "../inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity::Win32;

UINT SystemConfigurationProvider::GetCaretBlinkTime()
{
    return ::GetCaretBlinkTime();
}

bool SystemConfigurationProvider::IsCaretBlinkingEnabled()
{
    return GetSystemMetrics(SM_CARETBLINKINGENABLED) ? true : false;
}

int SystemConfigurationProvider::GetNumberOfMouseButtons()
{
    return GetSystemMetrics(SM_CMOUSEBUTTONS);
}

ULONG SystemConfigurationProvider::GetCursorWidth()
{
    ULONG width;
    if (SystemParametersInfoW(SPI_GETCARETWIDTH, 0, &width, FALSE))
    {
        return width;
    }
    else
    {
        LOG_LAST_ERROR();
        return s_DefaultCursorWidth;
    }
}

ULONG SystemConfigurationProvider::GetNumberOfWheelScrollLines()
{
    ULONG lines;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, FALSE);

    return lines;
}

ULONG SystemConfigurationProvider::GetNumberOfWheelScrollCharacters()
{
    ULONG characters;
    SystemParametersInfoW(SPI_GETWHEELSCROLLCHARS, 0, &characters, FALSE);

    return characters;
}

void SystemConfigurationProvider::GetSettingsFromLink(
    _Inout_ Settings* pLinkSettings,
    _Inout_updates_bytes_(*pdwTitleLength) LPWSTR pwszTitle,
    _Inout_ PDWORD pdwTitleLength,
    _In_ PCWSTR pwszCurrDir,
    _In_ PCWSTR pwszAppName,
    _Inout_opt_ IconInfo* iconInfo)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    WCHAR wszLinkTarget[MAX_PATH] = { 0 };
    WCHAR wszIconLocation[MAX_PATH] = { 0 };
    auto iIconIndex = 0;

    pLinkSettings->SetCodePage(ServiceLocator::LocateGlobals().uiOEMCP);

    // Did we get started from a link?
    if (pLinkSettings->GetStartupFlags() & STARTF_TITLEISLINKNAME)
    {
        auto initializedCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        // GH#9458: If it's RPC_E_CHANGED_MODE, that's okay, we're doing a
        // defterm and have already started COM. We can continue on here.
        if (SUCCEEDED(initializedCom) || initializedCom == RPC_E_CHANGED_MODE)
        {
            // Don't CoUninitialize if we still need COM for defterm.
            auto unInitCom = wil::scope_exit([initializedCom]() { if (SUCCEEDED(initializedCom)){CoUninitialize();} });

            const auto cch = *pdwTitleLength / sizeof(wchar_t);

            gci.SetLinkTitle(std::wstring(pwszTitle, cch));

            const auto linkNameForCsi = new (std::nothrow) wchar_t[gci.GetLinkTitle().length() + 1]{ 0 };
            if (linkNameForCsi)
            {
                gci.GetLinkTitle().copy(linkNameForCsi, gci.GetLinkTitle().length());
            }

            auto csi = pLinkSettings->CreateConsoleStateInfo();
            csi.LinkTitle = linkNameForCsi;
            WCHAR wszShortcutTitle[MAX_PATH] = L"\0";
            auto fReadConsoleProperties = FALSE;
            auto wShowWindow = pLinkSettings->GetShowWindow();
            auto dwHotKey = pLinkSettings->GetHotKey();
            auto iShowWindow = 0;
            WORD wHotKey = 0;
            auto Status = ShortcutSerialization::s_GetLinkValues(&csi,
                                                                 &fReadConsoleProperties,
                                                                 wszShortcutTitle,
                                                                 ARRAYSIZE(wszShortcutTitle),
                                                                 wszLinkTarget,
                                                                 ARRAYSIZE(wszLinkTarget),
                                                                 wszIconLocation,
                                                                 ARRAYSIZE(wszIconLocation),
                                                                 &iIconIndex,
                                                                 &iShowWindow,
                                                                 &wHotKey);

            if (SUCCEEDED_NTSTATUS(Status))
            {
                // Convert results back to appropriate types and set.
                if (SUCCEEDED(IntToWord(iShowWindow, &wShowWindow)))
                {
                    pLinkSettings->SetShowWindow(wShowWindow);
                }

                dwHotKey = wHotKey;
                pLinkSettings->SetHotKey(dwHotKey);

                if (wszLinkTarget[0] != L'\0')
                {
                    // guarantee null termination to make OACR happy.
                    wszLinkTarget[ARRAYSIZE(wszLinkTarget) - 1] = L'\0';
                }
            }

            // if we got a title, use it. even on overall link value load failure, the title will be correct if
            // filled out.
            if (wszShortcutTitle[0] != L'\0')
            {
                // guarantee null termination to make OACR happy.
                wszShortcutTitle[ARRAYSIZE(wszShortcutTitle) - 1] = L'\0';
                StringCbCopyW(pwszTitle, *pdwTitleLength, wszShortcutTitle);

                // OACR complains about the use of a DWORD here, so roundtrip through a size_t
                size_t cbTitleLength;
                if (SUCCEEDED(StringCbLengthW(pwszTitle, *pdwTitleLength, &cbTitleLength)))
                {
                    // don't care about return result -- the buffer is guaranteed null terminated to at least
                    // the length of Title
                    (void)SizeTToDWord(cbTitleLength, pdwTitleLength);
                }
            }

            if (SUCCEEDED_NTSTATUS(Status) && fReadConsoleProperties)
            {
                // copy settings
                pLinkSettings->InitFromStateInfo(&csi);

                // since we were launched via shortcut, make sure we don't let the invoker's STARTUPINFO pollute the
                // shortcut's settings
                pLinkSettings->UnsetStartupFlag(STARTF_USESIZE | STARTF_USECOUNTCHARS);
            }
            else
            {
                // if we didn't find any console properties, or otherwise failed to load link properties, pretend
                // like we weren't launched from a shortcut -- this allows us to at least try to find registry
                // settings based on title.
                pLinkSettings->UnsetStartupFlag(STARTF_TITLEISLINKNAME);
            }
        }
    }

    // Go get the icon
    if (wszIconLocation[0] == L'\0')
    {
        if (PathFileExists(wszLinkTarget))
        {
            StringCchCopyW(wszIconLocation, ARRAYSIZE(wszIconLocation), wszLinkTarget);
        }
        else
        {
            // search for the application along the path so that we can load its icons (if we didn't find one explicitly in
            // the shortcut)
            const auto dwLinkLen = SearchPathW(pwszCurrDir, pwszAppName, nullptr, ARRAYSIZE(wszIconLocation), wszIconLocation, nullptr);

            // If we cannot find the application in the path, then try to fall back and see if the window title is a valid path and use that.
            if (dwLinkLen <= 0 || dwLinkLen > ARRAYSIZE(wszIconLocation))
            {
                if (PathFileExistsW(pwszTitle) && (wcslen(pwszTitle) < ARRAYSIZE(wszIconLocation)))
                {
                    StringCchCopyW(wszIconLocation, ARRAYSIZE(wszIconLocation), pwszTitle);
                }
                else
                {
                    // If all else fails, just stick the app name into the path and try to resolve just the app name.
                    StringCchCopyW(wszIconLocation, ARRAYSIZE(wszIconLocation), pwszAppName);
                }
            }
        }
    }

    if (wszIconLocation[0] != L'\0')
    {
        // GH#9458, GH#13111 - when this is executed during defterm startup,
        // we'll pass in an iconInfo pointer, which we should fill with the
        // selected icon path and index, rather than loading the icon with our
        // global Icon class.
        if (iconInfo)
        {
            iconInfo->path = std::wstring{ wszIconLocation };
            iconInfo->index = iIconIndex;
        }
        else
        {
            LOG_IF_FAILED(Icon::Instance().LoadIconsFromPath(wszIconLocation, iIconIndex));
        }
    }

    if (!IsValidCodePage(pLinkSettings->GetCodePage()))
    {
        // make sure we don't leave this function with an invalid codepage
        pLinkSettings->SetCodePage(ServiceLocator::LocateGlobals().uiOEMCP);
    }
}
