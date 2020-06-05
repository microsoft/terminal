/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SystemConfigurationProvider.hpp

Abstract:
- OneCore implementation of the ISystemConfigurationProvider interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "..\inc\ISystemConfigurationProvider.hpp"

#pragma hdrstop

class InputTests;

namespace Microsoft::Console::Interactivity::OneCore
{
    class SystemConfigurationProvider sealed : public ISystemConfigurationProvider
    {
    public:
        bool IsCaretBlinkingEnabled();

        UINT GetCaretBlinkTime();
        int GetNumberOfMouseButtons();
        ULONG GetCursorWidth() override;
        ULONG GetNumberOfWheelScrollLines();
        ULONG GetNumberOfWheelScrollCharacters();

        void GetSettingsFromLink(_Inout_ Settings* pLinkSettings,
                                 _Inout_updates_bytes_(*pdwTitleLength) LPWSTR pwszTitle,
                                 _Inout_ PDWORD pdwTitleLength,
                                 _In_ PCWSTR pwszCurrDir,
                                 _In_ PCWSTR pwszAppName);

    private:
        static const UINT s_DefaultCaretBlinkTime = 530; // milliseconds
        static const bool s_DefaultIsCaretBlinkingEnabled = true;
        static const int s_DefaultNumberOfMouseButtons = 3;
        static const ULONG s_DefaultCursorWidth = 1;
        static const ULONG s_DefaultNumberOfWheelScrollLines = 3;
        static const ULONG s_DefaultNumberOfWheelScrollCharacters = 3;

        friend class ::InputTests;
    };
}
