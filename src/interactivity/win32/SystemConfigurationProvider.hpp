/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SystemConfigurationProvider.hpp

Abstract:
- Win32 implementation of the ISystemConfigurationProvider interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

#pragma once

#include "precomp.h"

#include "..\inc\ISystemConfigurationProvider.hpp"

namespace Microsoft::Console::Interactivity::Win32
{
    class SystemConfigurationProvider final : public ISystemConfigurationProvider
    {
    public:
        ~SystemConfigurationProvider() = default;

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
        static const ULONG s_DefaultCursorWidth = 1;
    };
}
