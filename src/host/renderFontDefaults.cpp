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

[[nodiscard]] HRESULT RenderFontDefaults::RetrieveDefaultFontNameForCodepage(const unsigned int uiCodePage,
                                                                             _Out_ std::wstring& outFaceName)
try
{
    wchar_t faceName[LF_FACESIZE]{};
    NTSTATUS status = TrueTypeFontList::s_SearchByCodePage(uiCodePage, faceName, ARRAYSIZE(faceName));
    outFaceName.assign(faceName);
    return HRESULT_FROM_NT(status);
}
CATCH_RETURN();
