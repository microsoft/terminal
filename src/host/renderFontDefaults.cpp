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

[[nodiscard]] HRESULT RenderFontDefaults::RetrieveDefaultFontNameForCodepage(const unsigned int codePage,
                                                                             std::wstring& outFaceName)
try
{
    // GH#3123: Propagate font length changes up through Settings and propsheet
    wchar_t faceName[LF_FACESIZE]{ 0 };
    NTSTATUS status = TrueTypeFontList::s_SearchByCodePage(codePage, faceName, ARRAYSIZE(faceName));
    outFaceName.assign(faceName);
    return HRESULT_FROM_NT(status);
}
CATCH_RETURN();
