// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "renderFontDefaults.hpp"

#pragma hdrstop

RenderFontDefaults::RenderFontDefaults()
{
    LOG_IF_NTSTATUS_FAILED(TrueTypeFontList::s_Initialize());
}

RenderFontDefaults::~RenderFontDefaults()
{
    LOG_IF_FAILED(TrueTypeFontList::s_Destroy());
}

[[nodiscard]] HRESULT RenderFontDefaults::RetrieveDefaultFontNameForCodepage(const UINT uiCodePage,
                                                                             _Out_writes_(cchFaceName) PWSTR pwszFaceName,
                                                                             const size_t cchFaceName)
{
    NTSTATUS status = TrueTypeFontList::s_SearchByCodePage(uiCodePage, pwszFaceName, cchFaceName);
    return HRESULT_FROM_NT(status);
}
