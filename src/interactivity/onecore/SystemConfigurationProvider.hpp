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

#include "../inc/ISystemConfigurationProvider.hpp"

#pragma hdrstop

class InputTests;

namespace Microsoft::Console::Interactivity::OneCore
{
    class SystemConfigurationProvider : public ISystemConfigurationProvider
    {
    public:
        bool IsCaretBlinkingEnabled() noexcept override;

        UINT GetCaretBlinkTime() noexcept override;
        int GetNumberOfMouseButtons() noexcept override;
        ULONG GetCursorWidth() noexcept override;
        ULONG GetNumberOfWheelScrollLines() noexcept override;
        ULONG GetNumberOfWheelScrollCharacters() noexcept override;

        void GetSettingsFromLink(_Inout_ Settings* pLinkSettings,
                                 _Inout_updates_bytes_(*pdwTitleLength) LPWSTR pwszTitle,
                                 _Inout_ PDWORD pdwTitleLength,
                                 _In_ PCWSTR pwszCurrDir,
                                 _In_ PCWSTR pwszAppName,
                                 _Inout_opt_ IconInfo* iconInfo) override;

    private:
        friend class ::InputTests;
    };
}
