/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TrueTypeFontList.hpp

Abstract:
- This module is used for managing the list of preferred TrueType fonts in the registry

Author(s):
- Michael Niksa (MiNiksa) 14-Mar-2016

--*/

#pragma once

class TrueTypeFontList
{
public:
    static SINGLE_LIST_ENTRY s_ttFontList;

    [[nodiscard]] static NTSTATUS s_Initialize();
    [[nodiscard]] static NTSTATUS s_Destroy();

    static LPTTFONTLIST s_SearchByName(_In_opt_ LPCWSTR pwszFace,
                                       _In_ BOOL fCodePage,
                                       _In_ UINT CodePage);

    [[nodiscard]] static NTSTATUS s_SearchByCodePage(const UINT uiCodePage,
                                                     _Out_writes_(cchFaceName) PWSTR pwszFaceName,
                                                     const size_t cchFaceName);
};
