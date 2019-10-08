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
    struct Entry
    {
        unsigned int CodePage;
        bool DisableBold;
        std::pair<std::wstring, std::wstring> FontNames;
    };

    static std::vector<Entry> s_ttFontList;

    [[nodiscard]] static HRESULT s_Initialize();
    [[nodiscard]] static HRESULT s_Destroy();

    static const Entry* s_SearchByName(const std::wstring_view name,
                                       const std::optional<unsigned int> CodePage);

    [[nodiscard]] static HRESULT s_SearchByCodePage(const unsigned int codePage,
                                                    std::wstring& outFaceName);
};
