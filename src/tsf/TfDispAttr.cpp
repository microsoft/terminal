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
}

//+---------------------------------------------------------------------------
//
// CicDisplayAttributeMgr::GetDisplayAttributeTrackPropertyRange
//
//----------------------------------------------------------------------------

[[nodiscard]] HRESULT CicDisplayAttributeMgr::GetDisplayAttributeTrackPropertyRange(TfEditCookie ec,
                                                                                    ITfContext* pic,
                                                                                    ITfRange* pRange,
                                                                                    ITfReadOnlyProperty** ppProp,
                                                                                    IEnumTfRanges** ppEnum,
                                                                                    ULONG* pulNumProp)
{
    HRESULT hr = E_FAIL;
    try
    {
        ULONG ulNumProp = static_cast<ULONG>(m_DispAttrProp.size());
        if (ulNumProp)
        {
            // TrackProperties wants an array of GUID *'s
            auto ppguidProp = std::make_unique<const GUID*[]>(ulNumProp);
            for (ULONG i = 0; i < ulNumProp; i++)
            {
                ppguidProp[i] = &m_DispAttrProp.at(i);
            }

            wil::com_ptr<ITfReadOnlyProperty> pProp;
            if (SUCCEEDED(hr = pic->TrackProperties(ppguidProp.get(), ulNumProp, 0, NULL, &pProp)))
            {
                hr = pProp->EnumRanges(ec, ppEnum, pRange);
                if (SUCCEEDED(hr))
                {
                    *ppProp = pProp.detach();
                }
            }

            if (SUCCEEDED(hr))
            {
                *pulNumProp = ulNumProp;
            }
        }
    }
    CATCH_RETURN();
    return hr;
}

//+---------------------------------------------------------------------------
//
// CicDisplayAttributeMgr::GetDisplayAttributeData
//
//----------------------------------------------------------------------------

[[nodiscard]] HRESULT CicDisplayAttributeMgr::GetDisplayAttributeData(ITfCategoryMgr* pcat,
                                                                      TfEditCookie ec,
                                                                      ITfReadOnlyProperty* pProp,
                                                                      ITfRange* pRange,
                                                                      TF_DISPLAYATTRIBUTE* pda,
                                                                      TfGuidAtom* pguid,
                                                                      ULONG /*ulNumProp*/)
{
    VARIANT var;

    HRESULT hr = E_FAIL;

    if (SUCCEEDED(pProp->GetValue(ec, pRange, &var)))
    {
        FAIL_FAST_IF(!(var.vt == VT_UNKNOWN));

        wil::com_ptr_nothrow<IEnumTfPropertyValue> pEnumPropertyVal;
        if (wil::try_com_query_to(var.punkVal, &pEnumPropertyVal))
        {
            TF_PROPERTYVAL tfPropVal;
            while (pEnumPropertyVal->Next(1, &tfPropVal, NULL) == S_OK)
            {
                if (tfPropVal.varValue.vt == VT_EMPTY)
                {
                    continue; // prop has no value over this span
                }

                FAIL_FAST_IF(!(tfPropVal.varValue.vt == VT_I4)); // expecting GUIDATOMs

                TfGuidAtom gaVal = (TfGuidAtom)tfPropVal.varValue.lVal;

                GUID guid;
                pcat->GetGUID(gaVal, &guid);

                wil::com_ptr_nothrow<ITfDisplayAttributeInfo> pDAI;
                if (SUCCEEDED(m_pDAM->GetDisplayAttributeInfo(guid, &pDAI, NULL)))
                {
                    //
                    // Issue: for simple apps.
                    //
                    // Small apps can not show multi underline. So
                    // this helper function returns only one
                    // DISPLAYATTRIBUTE structure.
                    //
                    if (pda)
                    {
                        pDAI->GetAttributeInfo(pda);
                    }

                    if (pguid)
                    {
                        *pguid = gaVal;
                    }

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

[[nodiscard]] HRESULT CicDisplayAttributeMgr::InitDisplayAttributeInstance(ITfCategoryMgr* pcat)
{
    HRESULT hr;

    //
    // Create ITfDisplayAttributeMgr instance.
    //
    if (FAILED(hr = ::CoCreateInstance(CLSID_TF_DisplayAttributeMgr, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_pDAM))))
    {
        return hr;
    }

    wil::com_ptr_nothrow<IEnumGUID> pEnumProp;
    pcat->EnumItemsInCategory(GUID_TFCAT_DISPLAYATTRIBUTEPROPERTY, &pEnumProp);

    //
    // make a database for Display Attribute Properties.
    //
    if (pEnumProp)
    {
        GUID guidProp;

        try
        {
            //
            // add System Display Attribute first.
            // so no other Display Attribute property overwrite it.
            //
            m_DispAttrProp.emplace_back(GUID_PROP_ATTRIBUTE);

            while (pEnumProp->Next(1, &guidProp, NULL) == S_OK)
            {
                if (!IsEqualGUID(guidProp, GUID_PROP_ATTRIBUTE))
                {
                    m_DispAttrProp.emplace_back(guidProp);
                }
            }
        }
        CATCH_RETURN();
    }
    return hr;
}
