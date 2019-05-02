/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    TfCatUtil.cpp

Abstract:

    This file implements the CicCategoryMgr Class.

Author:

Revision History:

Notes:

--*/


#include "precomp.h"
#include "TfCatUtil.h"

//+---------------------------------------------------------------------------
//
// CicCategoryMgr::ctor
// CicCategoryMgr::dtor
//
//----------------------------------------------------------------------------

CicCategoryMgr::CicCategoryMgr()
{
}

CicCategoryMgr::~CicCategoryMgr()
{
    if (m_pcat) {
        m_pcat.Release();
    }
}

//+---------------------------------------------------------------------------
//
// CicCategoryMgr::GetGUIDFromGUIDATOM
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CicCategoryMgr::GetGUIDFromGUIDATOM(TfGuidAtom guidatom, GUID *pguid)
{
    return m_pcat->GetGUID(guidatom, pguid);
}

//+---------------------------------------------------------------------------
//
// CicCategoryMgr::InitCategoryInstance
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CicCategoryMgr::InitCategoryInstance( )
{
    //
    // Create ITfCategoryMgr instance.
    //
    return m_pcat.CoCreateInstance(CLSID_TF_CategoryMgr);
}
