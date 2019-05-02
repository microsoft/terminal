/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    TfDispAttr.h

Abstract:

    This file defines the CicDisplayAttributeMgr Class.

Author:

Revision History:

Notes:

--*/

#pragma once

#include "StructureArray.h"

class CicDisplayAttributeMgr
{
public:
    CicDisplayAttributeMgr();
    virtual ~CicDisplayAttributeMgr();

public:
    [[nodiscard]]
    HRESULT GetDisplayAttributeTrackPropertyRange(TfEditCookie ec,
                                                  ITfContext *pic,
                                                  ITfRange *pRange,
                                                  ITfReadOnlyProperty **ppProp,
                                                  IEnumTfRanges **ppEnum,
                                                  ULONG *pulNumProp);
    [[nodiscard]]
    HRESULT GetDisplayAttributeData(ITfCategoryMgr *pcat,
                                    TfEditCookie ec,
                                    ITfReadOnlyProperty *pProp,
                                    ITfRange *pRange,
                                    TF_DISPLAYATTRIBUTE *pda,
                                    TfGuidAtom *pguid,
                                    ULONG  ulNumProp);
    [[nodiscard]]
    HRESULT InitDisplayAttributeInstance(ITfCategoryMgr* pcat);

    inline ITfDisplayAttributeMgr* GetDisplayAttributeMgr() { return m_pDAM; }

private:
    CComQIPtr<ITfDisplayAttributeMgr>  m_pDAM;
    CStructureArray<GUID>              m_DispAttrProp;
};
