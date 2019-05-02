/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- renderFontDefaults.hpp

Abstract:
- This provides the implementation of the interface that abstracts the lookup of default fonts from the actual rendering engine.

Author(s):
- Michael Niksa (miniksa) Mar 2016
--*/

#pragma once

#include "..\renderer\inc\IFontDefaultList.hpp"

using namespace Microsoft::Console::Render;

class RenderFontDefaults sealed : public IFontDefaultList
{
public:
    RenderFontDefaults();
    ~RenderFontDefaults();

    [[nodiscard]]
    HRESULT RetrieveDefaultFontNameForCodepage(const UINT uiCodePage,
                                               _Out_writes_(cchFaceName) PWSTR pwszFaceName,
                                               const size_t cchFaceName);
};
