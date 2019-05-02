/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    TfDispAttr.cpp

Abstract:

    This file implements the CicDisplayAttributeMgr Class.

Author:

Revision History:

Notes:

--*/


#include "precomp.h"
#include "TfDispAttr.h"

//+---------------------------------------------------------------------------
//
// CicDisplayAttributeMgr::ctor
// CicDisplayAttributeMgr::dtor
//
//----------------------------------------------------------------------------

CicDisplayAttributeMgr::CicDisplayAttributeMgr()
{
}

CicDisplayAttributeMgr::~CicDisplayAttributeMgr()
{
    if (m_pDAM) {
        m_pDAM.Release();
    }
}

//+---------------------------------------------------------------------------
//
// CicDisplayAttributeMgr::GetDisplayAttributeTrackPropertyRange
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CicDisplayAttributeMgr::GetDisplayAttributeTrackPropertyRange(TfEditCookie ec, ITfContext *pic, ITfRange *pRange,
                                                                      ITfReadOnlyProperty **ppProp, IEnumTfRanges **ppEnum, ULONG *pulNumProp)
{
    HRESULT hr = E_FAIL;

    ULONG ulNumProp;
    ulNumProp = (ULONG) m_DispAttrProp.Count();
    if (ulNumProp) {
        const GUID **ppguidProp;
        //
        // TrackProperties wants an array of GUID *'s
        //
        ppguidProp = (const GUID **) new(std::nothrow) GUID* [ulNumProp];
        if (ppguidProp == NULL) {
            hr = E_OUTOFMEMORY;
        }
        else {
            for (ULONG i=0; i<ulNumProp; i++) {
                ppguidProp[i] = m_DispAttrProp.GetAt(i);
            }

            CComPtr<ITfReadOnlyProperty> pProp;
            if (SUCCEEDED(hr = pic->TrackProperties(ppguidProp, ulNumProp, 0, NULL, &pProp))) {
                hr = pProp->EnumRanges(ec, ppEnum, pRange);
                if (SUCCEEDED(hr)) {
                    *ppProp = pProp;
                    (*ppProp)->AddRef();
                }
                pProp.Release();
            }

            delete [] ppguidProp;

            if (SUCCEEDED(hr)) {
                *pulNumProp = ulNumProp;
            }
        }
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// CicDisplayAttributeMgr::GetDisplayAttributeData
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CicDisplayAttributeMgr::GetDisplayAttributeData(ITfCategoryMgr *pcat, TfEditCookie ec, ITfReadOnlyProperty *pProp, ITfRange *pRange, TF_DISPLAYATTRIBUTE *pda, TfGuidAtom *pguid, ULONG /*ulNumProp*/)
{
    VARIANT var;

    HRESULT hr = E_FAIL;

    if (SUCCEEDED(pProp->GetValue(ec, pRange, &var))) {
        FAIL_FAST_IF(!(var.vt == VT_UNKNOWN));

        CComQIPtr<IEnumTfPropertyValue> pEnumPropertyVal(var.punkVal);
        if (pEnumPropertyVal) {
            TF_PROPERTYVAL tfPropVal;
            while (pEnumPropertyVal->Next(1, &tfPropVal, NULL) == S_OK) {
                if (tfPropVal.varValue.vt == VT_EMPTY) {
                    continue; // prop has no value over this span
                }

                FAIL_FAST_IF(!(tfPropVal.varValue.vt == VT_I4)); // expecting GUIDATOMs

                TfGuidAtom gaVal = (TfGuidAtom)tfPropVal.varValue.lVal;

                GUID guid;
                pcat->GetGUID(gaVal, &guid);

                CComPtr<ITfDisplayAttributeInfo> pDAI;
                if (SUCCEEDED(m_pDAM->GetDisplayAttributeInfo(guid, &pDAI, NULL))) {
                    //
                    // Issue: for simple apps.
                    //
                    // Small apps can not show multi underline. So
                    // this helper function returns only one
                    // DISPLAYATTRIBUTE structure.
                    //
                    if (pda) {
                        pDAI->GetAttributeInfo(pda);
                    }

                    if (pguid) {
                        *pguid = gaVal;
                    }

                    pDAI.Release();
                    hr = S_OK;
                    break;
                }
            }
        }
        VariantClear(&var);
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// CicDisplayAttributeMgr::InitCategoryInstance
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CicDisplayAttributeMgr::InitDisplayAttributeInstance(ITfCategoryMgr* pcat)
{
    HRESULT hr;

    //
    // Create ITfDisplayAttributeMgr instance.
    //
    if (FAILED(hr = m_pDAM.CoCreateInstance(CLSID_TF_DisplayAttributeMgr))) {
        return hr;
    }

    CComPtr<IEnumGUID> pEnumProp;
    pcat->EnumItemsInCategory(GUID_TFCAT_DISPLAYATTRIBUTEPROPERTY, &pEnumProp);

    //
    // make a database for Display Attribute Properties.
    //
    if (pEnumProp) {
         GUID guidProp;

         //
         // add System Display Attribute first.
         // so no other Display Attribute property overwrite it.
         //
         GUID *pguid;

         pguid = m_DispAttrProp.Append();
         if (pguid != NULL) {
             *pguid = GUID_PROP_ATTRIBUTE;
         }

         while(pEnumProp->Next(1, &guidProp, NULL) == S_OK) {
             if (!IsEqualGUID(guidProp, GUID_PROP_ATTRIBUTE)) {
                 pguid = m_DispAttrProp.Append();
                 if (pguid != NULL) {
                     *pguid = guidProp;
                 }
             }
         }
    }
    return hr;
}
