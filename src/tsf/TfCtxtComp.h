/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    TfCtxtComp.h

Abstract:

    This file defines the Context of Composition Class.

Author:

Revision History:

Notes:

--*/

#pragma once

#include "StructureArray.h"

/////////////////////////////////////////////////////////////////////////////
// CCompString

class CCompString : public CComBSTR
{
public:
    CCompString() : CComBSTR() { };
    CCompString(__in_ecount(dwLen) LPWSTR lpsz, DWORD dwLen) : CComBSTR(dwLen, lpsz) { };
    virtual ~CCompString() { };
};

/////////////////////////////////////////////////////////////////////////////
// CCompAttribute

class CCompAttribute : public CStructureArray<BYTE>
{
public:
    CCompAttribute(BYTE* lpAttr=NULL, DWORD dwLen=0) : CStructureArray<BYTE>(dwLen)
    {
        if (! InsertAt(0, dwLen)) {
            return;
        }

        BYTE* pb = GetAt(0);
        if (pb == NULL) {
            return;
        }

        memcpy(pb, lpAttr, dwLen * sizeof(BYTE));
    }
    virtual ~CCompAttribute() { };
};

/////////////////////////////////////////////////////////////////////////////
// CCompCursorPos

class CCompCursorPos
{
public:
    CCompCursorPos()
    {
        m_CursorPosition = 0;
    }

    void SetCursorPosition(DWORD CursorPosition)
    {
        m_CursorPosition = CursorPosition;
    }

    DWORD GetCursorPosition() { return m_CursorPosition; }

private:
    DWORD m_CursorPosition;
};

/////////////////////////////////////////////////////////////////////////////
// CCompTfGuidAtom

class CCompTfGuidAtom : public CStructureArray<TfGuidAtom>
{
public:
    CCompTfGuidAtom() { };
    virtual ~CCompTfGuidAtom() { };

    operator TfGuidAtom* () { return GetAt(0); }

    DWORD FillData(const TfGuidAtom& data, DWORD dwLen)
    {
        TfGuidAtom *psTemp = Append(dwLen);
        if (psTemp == NULL) {
            return 0;
        }

        DWORD index = dwLen;
        while (index--) {
            *psTemp++ = data;
        }

        return dwLen;
    }
};
