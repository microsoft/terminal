// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "renderFontDefaults.hpp"

#pragma hdrstop

RenderFontDefaults::RenderFontDefaults()
{
    LOG_IF_FAILED(TrueTypeFontList::s_Initialize());
}

RenderFontDefaults::~RenderFontDefaults()
{
    LOG_IF_FAILED(TrueTypeFontList::s_Destroy());
}

[[nodiscard]] HRESULT RenderFontDefaults::RetrieveDefaultFontNameForCodepage(const unsigned int codePage,
                                                                             std::wstring& outFaceName)
{
    return TrueTypeFontList::s_SearchByCodePage(codePage, outFaceName);
}
