/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IFontDefaultList.hpp

Abstract:
- This serves as an abstraction to retrieve a list of default preferred fonts that we should use if the user hasn't chosen one.

Author(s):
- Michael Niksa (MiNiksa) 14-Mar-2016
--*/
#pragma once

namespace Microsoft::Console::Render
{
    class IFontDefaultList
    {
    public:
        [[nodiscard]] virtual HRESULT RetrieveDefaultFontNameForCodepage(const unsigned int codepage,
                                                                         std::wstring& outFaceName) = 0;
    };
}
