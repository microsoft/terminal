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
