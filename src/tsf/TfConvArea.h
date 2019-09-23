/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    TfConvArea.h

Abstract:

    This file defines the CConversionAreaJapanese Interface Class.

Author:

Revision History:

Notes:

--*/

#pragma once

//+---------------------------------------------------------------------------
//
// CConversionArea::Pure virtual class
//
//----------------------------------------------------------------------------

class CConversionArea
{
public:
    [[nodiscard]] HRESULT DrawComposition(const std::wstring_view CompStr,
                                          const std::vector<TF_DISPLAYATTRIBUTE>& DisplayAttributes,
                                          const DWORD CompCursorPos = -1);

    [[nodiscard]] HRESULT ClearComposition();

    [[nodiscard]] HRESULT DrawResult(const std::wstring_view ResultStr);

private:
    [[nodiscard]] std::vector<BYTE> _DisplayAttributesToEncodedAttributes(const std::vector<TF_DISPLAYATTRIBUTE>& DisplayAttributes,
                                                                          const DWORD CompCursorPos);
};
