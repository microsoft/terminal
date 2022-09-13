/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ISystemConfigurationProvider.hpp

Abstract:
- Defines methods that fetch user settings that can customize the console's
  behavior.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

class Settings;

namespace Microsoft::Console::Interactivity
{
    struct IconInfo
    {
        std::wstring path;
        int index = 0;
    };

    class ISystemConfigurationProvider
    {
    public:
        virtual ~ISystemConfigurationProvider() = default;

        virtual bool IsCaretBlinkingEnabled() = 0;

        virtual UINT GetCaretBlinkTime() = 0;
        virtual int GetNumberOfMouseButtons() = 0;
        virtual ULONG GetCursorWidth() = 0;
        virtual ULONG GetNumberOfWheelScrollLines() = 0;
        virtual ULONG GetNumberOfWheelScrollCharacters() = 0;

        virtual void GetSettingsFromLink(_Inout_ Settings* pLinkSettings,
                                         _Inout_updates_bytes_(*pdwTitleLength) LPWSTR pwszTitle,
                                         _Inout_ PDWORD pdwTitleLength,
                                         _In_ PCWSTR pwszCurrDir,
                                         _In_ PCWSTR pwszAppName,
                                         _Inout_opt_ IconInfo* iconInfo) = 0;
    };
}
