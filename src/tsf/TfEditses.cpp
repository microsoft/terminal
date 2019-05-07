/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    TfEditses.cpp

Abstract:

    This file implements the CEditSessionObject Class.

Author:

Revision History:

Notes:

--*/


#include "precomp.h"
#include "TfConvArea.h"
#include "TfCatUtil.h"
#include "TfDispAttr.h"
#include "TfEditses.h"

//+---------------------------------------------------------------------------
//
// CEditSessionObject::IUnknown::QueryInterface
// CEditSessionObject::IUnknown::AddRef
// CEditSessionObject::IUnknown::Release
//
//----------------------------------------------------------------------------

STDAPI CEditSessionObject::QueryInterface(REFIID riid, void** ppvObj)
{
    *ppvObj = NULL;

    if (IsEqualIID(riid, IID_ITfEditSession)) {
        *ppvObj = static_cast<ITfEditSession*>(this);
    }
    else if (IsEqualIID(riid, IID_IUnknown)) {
        *ppvObj = static_cast<IUnknown*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDAPI_(ULONG) CEditSessionObject::AddRef()
{
    return ++m_cRef;
}

STDAPI_(ULONG) CEditSessionObject::Release()
{
    long cr;

    cr = --m_cRef;
    FAIL_FAST_IF(!(cr >= 0));

    if (cr == 0) {
        delete this;
    }

    return cr;
}


//+---------------------------------------------------------------------------
//
// CEditSessionObject::GetAllTextRange
//
//----------------------------------------------------------------------------

// static
[[nodiscard]]
HRESULT CEditSessionObject::GetAllTextRange(TfEditCookie ec, ITfContext* ic, ITfRange** range, LONG* lpTextLength, TF_HALTCOND* lpHaltCond)
{
    HRESULT hr;

    //
    // init lpTextLength first.
    //
    *lpTextLength = 0;

    //
    // Create the range that covers all the text.
    //
    CComPtr<ITfRange> rangeFull;
    if (FAILED(hr = ic->GetStart(ec, &rangeFull))) {
        return hr;
    }

    LONG cch = 0;
    if (FAILED(hr = rangeFull->ShiftEnd(ec, LONG_MAX, &cch, lpHaltCond))) {
        return hr;
    }

    if (FAILED(hr = rangeFull->Clone(range))) {
        return hr;
    }

    *lpTextLength = cch;

    rangeFull.Release();

    return S_OK;
}


//+---------------------------------------------------------------------------
//
// CEditSessionObject::SetTextInRange
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionObject::SetTextInRange(TfEditCookie ec, ITfRange* range, __in_ecount_opt(len) LPWSTR psz, DWORD len)
{
    HRESULT hr = E_FAIL;
    if (g_pConsoleTSF)
    {
        g_pConsoleTSF->SetModifyingDocFlag(TRUE);
        hr = range->SetText(ec, 0, psz, len);
        g_pConsoleTSF->SetModifyingDocFlag(FALSE);
    }
    return hr;
}


//+---------------------------------------------------------------------------
//
// CEditSessionObject::ClearTextInRange
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionObject::ClearTextInRange(TfEditCookie ec, ITfRange* range)
{
    //
    // Clear the text in Cicero TOM
    //
    return SetTextInRange(ec, range, NULL, 0);
}

//+---------------------------------------------------------------------------
//
// CEditSessionObject::_GetCursorPosition
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionObject::_GetCursorPosition(TfEditCookie ec, CCompCursorPos& CompCursorPos)
{
    ITfContext* pic = g_pConsoleTSF ? g_pConsoleTSF->GetInputContext() : NULL;
    if (pic == NULL) {
        return E_FAIL;
    }

    HRESULT hr;
    ULONG cFetched;

    TF_SELECTION sel;
    sel.range = NULL;

    if (SUCCEEDED(hr = pic->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched))) {
        CComPtr<ITfRange> start;
        LONG ich;
        TF_HALTCOND hc;

        hc.pHaltRange = sel.range;
        hc.aHaltPos = (sel.style.ase == TF_AE_START) ? TF_ANCHOR_START : TF_ANCHOR_END;
        hc.dwFlags = 0;

        if (SUCCEEDED(hr = GetAllTextRange(ec, pic, &start, &ich, &hc))) {
            CompCursorPos.SetCursorPosition(ich);
        }

        SafeReleaseClear(sel.range);
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// CEditSessionObject::_GetTextAndAttribute
//
//----------------------------------------------------------------------------

//
// Get text and attribute in given range
//
//                                ITfRange::range
//   TF_ANCHOR_START
//    |======================================================================|
//                        +--------------------+          #+----------+
//                        |ITfRange::pPropRange|          #|pPropRange|
//                        +--------------------+          #+----------+
//                        |     GUID_ATOM      |          #
//                        +--------------------+          #
//    ^^^^^^^^^^^^^^^^^^^^                      ^^^^^^^^^^#
//    ITfRange::gap_range                       gap_range #
//                                                        #
//                                                        V
//                                                        ITfRange::no_display_attribute_range
//                                                   result_comp
//                                          +1   <-       0    ->     -1
//

[[nodiscard]]
HRESULT CEditSessionObject::_GetTextAndAttribute(TfEditCookie ec, ITfRange* rangeIn,
                                                 CCompString& CompStr, CCompTfGuidAtom& CompGuid, CCompString& ResultStr,
                                                 BOOL bInWriteSession,
                                                 CicCategoryMgr* pCicCatMgr, CicDisplayAttributeMgr* pCicDispAttr)
{
    HRESULT hr;

    ITfContext* pic = g_pConsoleTSF ? g_pConsoleTSF->GetInputContext() : NULL;
    if (pic == NULL) {
        return E_FAIL;
    }

    //
    // Get no display attribute range if there exist.
    // Otherwise, result range is the same to input range.
    //
    LONG result_comp;
    CComPtr<ITfRange> no_display_attribute_range;
    if (FAILED(hr = rangeIn->Clone(&no_display_attribute_range))) {
        return hr;
    }

    const GUID* guids[] = { &GUID_PROP_COMPOSING };
    const int   guid_size = sizeof(guids) / sizeof(GUID*);

    if (FAILED(hr = _GetNoDisplayAttributeRange(ec, rangeIn,
                                                guids, guid_size,
                                                no_display_attribute_range))) {
        return hr;
    }



    CComPtr<ITfReadOnlyProperty> propComp;
    if (FAILED(hr = pic->TrackProperties(guids, guid_size,       // system property
                                         NULL, 0,                // application property
                                         &propComp))) {
        return hr;
    }


    CComPtr<IEnumTfRanges> enumComp;
    if (FAILED(hr = propComp->EnumRanges(ec, &enumComp, rangeIn))) {
        return hr;
    }

    CComPtr<ITfRange>  range;
    while (enumComp->Next(1, &range, NULL) == S_OK) {
        VARIANT var;
        BOOL fCompExist = FALSE;

        hr = propComp->GetValue(ec, range, &var);
        if (S_OK == hr) {

            CComQIPtr<IEnumTfPropertyValue> EnumPropVal(var.punkVal);
            if (EnumPropVal) {
                TF_PROPERTYVAL tfPropertyVal;

                while (EnumPropVal->Next(1, &tfPropertyVal, NULL) == S_OK) {
                    for (int i = 0; i < guid_size; i++) {
                        if (IsEqualGUID(tfPropertyVal.guidId, *guids[i])) {
                            if ((V_VT(&tfPropertyVal.varValue) == VT_I4 && V_I4(&tfPropertyVal.varValue) != 0)) {
                                fCompExist = TRUE;
                                break;
                            }
                        }
                    }

                    VariantClear(&tfPropertyVal.varValue);

                    if (fCompExist) {
                        break;
                    }
                }
            }
        }

        VariantClear(&var);

        ULONG ulNumProp;

        CComPtr<IEnumTfRanges> enumProp;
        CComPtr<ITfReadOnlyProperty> prop;
        if (FAILED(hr = pCicDispAttr->GetDisplayAttributeTrackPropertyRange(ec, pic, range, &prop, &enumProp, &ulNumProp))) {
            return hr;
        }

        // use text range for get text
        CComPtr<ITfRange> textRange;
        if (FAILED(hr = range->Clone(&textRange))) {
            return hr;
        }

        // use text range for gap text (no property range).
        CComPtr<ITfRange> gap_range;
        if (FAILED(hr = range->Clone(&gap_range))) {
            return hr;
        }

        CComPtr<ITfRange> pPropRange;
        while (enumProp->Next(1, &pPropRange, NULL) == S_OK) {

            // pick up the gap up to the next property
            gap_range->ShiftEndToRange(ec, pPropRange, TF_ANCHOR_START);

            //
            // GAP range
            //
            no_display_attribute_range->CompareStart(ec, gap_range, TF_ANCHOR_START, &result_comp);
            LOG_IF_FAILED(_GetTextAndAttributeGapRange(ec,
                                                       gap_range,
                                                       result_comp,
                                                       CompStr, CompGuid,
                                                       ResultStr));

            //
            // Get display attribute data if some GUID_ATOM exist.
            //
            TF_DISPLAYATTRIBUTE da;
            TfGuidAtom guidatom = TF_INVALID_GUIDATOM;

            LOG_IF_FAILED(pCicDispAttr->GetDisplayAttributeData(pCicCatMgr->GetCategoryMgr(),
                                                                ec,
                                                                prop,
                                                                pPropRange,
                                                                &da,
                                                                &guidatom,
                                                                ulNumProp));

            //
            // Property range
            //
            no_display_attribute_range->CompareStart(ec, pPropRange, TF_ANCHOR_START, &result_comp);

            // Adjust GAP range's start anchor to the end of proprty range.
            gap_range->ShiftStartToRange(ec, pPropRange, TF_ANCHOR_END);

            //
            // Get property text
            //
            LOG_IF_FAILED(_GetTextAndAttributePropertyRange(ec,
                                                            pPropRange,
                                                            fCompExist,
                                                            result_comp,
                                                            bInWriteSession,
                                                            da,
                                                            guidatom,
                                                            CompStr,
                                                            CompGuid,
                                                            ResultStr));

            pPropRange.Release();

        } // while

        // the last non-attr
        textRange->ShiftStartToRange(ec, gap_range, TF_ANCHOR_START);
        textRange->ShiftEndToRange(ec, range, TF_ANCHOR_END);

        BOOL fEmpty;
        while (textRange->IsEmpty(ec, &fEmpty) == S_OK && !fEmpty) {
            WCHAR wstr0[256 + 1];
            ULONG ulcch0 = ARRAYSIZE(wstr0) - 1;
            textRange->GetText(ec, TF_TF_MOVESTART, wstr0, ulcch0, &ulcch0);

            TfGuidAtom guidatom;
            guidatom = TF_INVALID_GUIDATOM;

            TF_DISPLAYATTRIBUTE da;
            da.bAttr = TF_ATTR_INPUT;

            CompGuid.FillData(guidatom, ulcch0);
            CompStr.Append(wstr0, ulcch0);
        }

        textRange->Collapse(ec, TF_ANCHOR_END);

        range.Release();

    } // out-most while for GUID_PROP_COMPOSING


    //
    // set GUID_PROP_CONIME_TRACKCOMPOSITION
    //
    CComPtr<ITfProperty> PropertyTrackComposition;
    if (SUCCEEDED(hr = pic->GetProperty(GUID_PROP_CONIME_TRACKCOMPOSITION, &PropertyTrackComposition))) {
        VARIANT var;
        var.vt = VT_I4;
        var.lVal = 1;
        PropertyTrackComposition->SetValue(ec, rangeIn, &var);
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// CEditSessionObject::_GetTextAndAttributeGapRange
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionObject::_GetTextAndAttributeGapRange(TfEditCookie ec, ITfRange* gap_range, LONG result_comp,
                                                         CCompString& CompStr, CCompTfGuidAtom& CompGuid, CCompString& ResultStr)
{
    TfGuidAtom guidatom;
    guidatom = TF_INVALID_GUIDATOM;

    TF_DISPLAYATTRIBUTE da;
    da.bAttr = TF_ATTR_INPUT;

    BOOL fEmpty;
    WCHAR wstr0[256 + 1];
    ULONG ulcch0;

    while (gap_range->IsEmpty(ec, &fEmpty) == S_OK && !fEmpty) {
        CComPtr<ITfRange> backup_range;
        if (FAILED(gap_range->Clone(&backup_range))) {
            return E_FAIL;
        }

        //
        // Retrieve gap text if there exist.
        //
        ulcch0 = ARRAYSIZE(wstr0) - 1;
        if (FAILED(gap_range->GetText(ec,
                                      TF_TF_MOVESTART,    // Move range to next after get text.
                                      wstr0,
                                      ulcch0, &ulcch0))) {
            return E_FAIL;
        }

        if (result_comp <= 0) {
            CompGuid.FillData(guidatom, ulcch0);
            CompStr.Append(wstr0, ulcch0);
        }
        else {
            ResultStr.Append(wstr0, ulcch0);
            LOG_IF_FAILED(ClearTextInRange(ec, backup_range));
        }
    }


    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CEditSessionObject::_GetTextAndAttributePropertyRange
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionObject::_GetTextAndAttributePropertyRange(TfEditCookie ec,
                                                              ITfRange* pPropRange,
                                                              BOOL fCompExist,
                                                              LONG result_comp,
                                                              BOOL bInWriteSession,
                                                              TF_DISPLAYATTRIBUTE da,
                                                              TfGuidAtom guidatom,
                                                              CCompString& CompStr,
                                                              CCompTfGuidAtom& CompGuid,
                                                              CCompString& ResultStr)
{
    BOOL fEmpty;
    WCHAR wstr0[256 + 1];
    ULONG ulcch0;

    while (pPropRange->IsEmpty(ec, &fEmpty) == S_OK && !fEmpty) {
        CComPtr<ITfRange> backup_range;
        if (FAILED(pPropRange->Clone(&backup_range))) {
            return E_FAIL;
        }

        //
        // Retrieve property text if there exist.
        //
        ulcch0 = ARRAYSIZE(wstr0) - 1;
        if (FAILED(pPropRange->GetText(ec,
                                       TF_TF_MOVESTART,    // Move range to next after get text.
                                       wstr0,
                                       ulcch0, &ulcch0))) {
            return E_FAIL;
        }

        // see if there is a valid disp attribute
        if (fCompExist == TRUE && result_comp <= 0) {
            if (guidatom == TF_INVALID_GUIDATOM) {
                da.bAttr = TF_ATTR_INPUT;
            }
            CompGuid.FillData(guidatom, ulcch0);
            CompStr.Append(wstr0, ulcch0);
        }
        else if (bInWriteSession) {
            // if there's no disp attribute attached, it probably means
            // the part of string is finalized.
            //
            ResultStr.Append(wstr0, ulcch0);

            // it was a 'determined' string
            // so the doc has to shrink
            //
            LOG_IF_FAILED(ClearTextInRange(ec, backup_range));
        }
        else {
            //
            // Prevent infinite loop
            //
            break;
        }
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CEditSessionObject::_GetNoDisplayAttributeRange
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionObject::_GetNoDisplayAttributeRange(TfEditCookie ec, ITfRange* rangeIn, const GUID** guids, const int guid_size, ITfRange* no_display_attribute_range)
{
    ITfContext* pic = g_pConsoleTSF ? g_pConsoleTSF->GetInputContext() : NULL;
    if (pic == NULL) {
        return E_FAIL;
    }

    CComPtr<ITfReadOnlyProperty> propComp;
    HRESULT hr = pic->TrackProperties(guids, guid_size,       // system property
                                      NULL, 0,                // application property
                                      &propComp);
    if (FAILED(hr)) {
        return hr;
    }

    CComPtr<IEnumTfRanges> enumComp;
    hr = propComp->EnumRanges(ec, &enumComp, rangeIn);
    if (FAILED(hr)) {
        return hr;
    }

    CComPtr<ITfRange> pRange;

    while (enumComp->Next(1, &pRange, NULL) == S_OK) {
        VARIANT var;
        BOOL fCompExist = FALSE;

        hr = propComp->GetValue(ec, pRange, &var);
        if (S_OK == hr) {

            CComQIPtr<IEnumTfPropertyValue> EnumPropVal(var.punkVal);
            if (EnumPropVal) {
                TF_PROPERTYVAL tfPropertyVal;

                while (EnumPropVal->Next(1, &tfPropertyVal, NULL) == S_OK) {
                    for (int i = 0; i < guid_size; i++) {
                        if (IsEqualGUID(tfPropertyVal.guidId, *guids[i])) {
                            if ((V_VT(&tfPropertyVal.varValue) == VT_I4 && V_I4(&tfPropertyVal.varValue) != 0)) {
                                fCompExist = TRUE;
                                break;
                            }
                        }
                    }

                    VariantClear(&tfPropertyVal.varValue);

                    if (fCompExist) {
                        break;
                    }
                }
            }
        }

        if (!fCompExist) {

            // Adjust GAP range's start anchor to the end of proprty range.
            no_display_attribute_range->ShiftStartToRange(ec, pRange, TF_ANCHOR_START);
        }

        VariantClear(&var);

        pRange.Release();
    }

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CEditSessionCompositionComplete::CompComplete
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionCompositionComplete::CompComplete(TfEditCookie ec)
{
    ITfContext* pic = g_pConsoleTSF ? g_pConsoleTSF->GetInputContext() : NULL;
    RETURN_HR_IF_NULL(E_FAIL, pic);

    // Get the whole text, finalize it, and set empty string in TOM
    CComPtr<ITfRange> spRange;
    LONG cch;

    RETURN_IF_FAILED(GetAllTextRange(ec, pic, &spRange, &cch));

    // Check if a part of the range has already been finalized but not removed yet.
    // Adjust the range appropriately to avoid inserting the same text twice.
    long cchCompleted = g_pConsoleTSF->GetCompletedRangeLength();
    if ((cchCompleted > 0) &&
        (cchCompleted < cch) &&
        SUCCEEDED(spRange->ShiftStart(ec, cchCompleted, &cchCompleted, NULL)))
    {
        FAIL_FAST_IF(!((cchCompleted > 0) && (cchCompleted < cch)));
        cch -= cchCompleted;
    }
    else
    {
        cchCompleted = 0;
    }

    // Get conversion area service.
    CConversionArea* conv_area = g_pConsoleTSF->GetConversionArea();
    RETURN_HR_IF_NULL(E_FAIL, conv_area);

    // If there is no string in TextStore we don't have to do anything.
    if (!cch) {
        // Clear composition
        LOG_IF_FAILED(conv_area->ClearComposition());
        return S_OK;
    }

    HRESULT hr = S_OK;
    try
    {
        auto wstr = std::make_unique<WCHAR[]>(cch + 1);

        // Get the whole text, finalize it, and erase the whole text.
        if (SUCCEEDED(spRange->GetText(ec, TF_TF_IGNOREEND, wstr.get(), (ULONG)cch, (ULONG*)&cch)))
        {
            // Make Result String.
            CCompString ResultStr(wstr.get(), cch);
            hr = conv_area->DrawResult(ResultStr);
        }
    }
    CATCH_RETURN();

    // Update the stored length of the completed fragment.
    g_pConsoleTSF->SetCompletedRangeLength(cchCompleted + cch);

    return hr;
}


//+---------------------------------------------------------------------------
//
// CEditSessionCompositionCleanup::EmptyCompositionRange()
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionCompositionCleanup::EmptyCompositionRange(TfEditCookie ec)
{
    if (!g_pConsoleTSF)
    {
        return E_FAIL;
    }
    if (!g_pConsoleTSF->IsPendingCompositionCleanup())
    {
        return S_OK;
    }

    HRESULT hr = E_FAIL;
    ITfContext* pic = g_pConsoleTSF->GetInputContext();
    if (pic != NULL)
    {
        // Cleanup (empty the context range) after the last composition.

        hr = S_OK;
        long cchCompleted = g_pConsoleTSF->GetCompletedRangeLength();
        if (cchCompleted != 0)
        {
            CComPtr<ITfRange> spRange;
            LONG cch;
            hr = GetAllTextRange(ec, pic, &spRange, &cch);
            if (SUCCEEDED(hr))
            {
                // Clean up only the completed part (which start is expected to coincide with the start of the full range).
                if (cchCompleted < cch)
                {
                    spRange->ShiftEnd(ec, (cchCompleted - cch), &cch, NULL);
                }
                hr = ClearTextInRange(ec, spRange);
                g_pConsoleTSF->SetCompletedRangeLength(0);  // cleaned up all completed text
            }
        }
    }
    g_pConsoleTSF->OnCompositionCleanup(SUCCEEDED(hr));
    return hr;
}


//+---------------------------------------------------------------------------
//
// CEditSessionUpdateCompositionString::UpdateCompositionString
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionUpdateCompositionString::UpdateCompositionString(TfEditCookie ec)
{
    HRESULT hr;

    ITfContext* pic = g_pConsoleTSF ? g_pConsoleTSF->GetInputContext() : NULL;
    if (pic == NULL) {
        return E_FAIL;
    }

    // Reset the 'edit session requested' flag.
    g_pConsoleTSF->OnEditSession();

    // If the composition has been cancelled\finalized, no update necessary.
    if (!g_pConsoleTSF->IsInComposition())
    {
        return S_OK;
    }

    BOOL bInWriteSession;
    if (FAILED(hr = pic->InWriteSession(g_pConsoleTSF->GetTfClientId(), &bInWriteSession))) {
        return hr;
    }

    CComPtr<ITfRange> FullTextRange;
    LONG lTextLength;
    if (FAILED(hr = GetAllTextRange(ec, pic, &FullTextRange, &lTextLength))) {
        return hr;
    }

    CComPtr<ITfRange> InterimRange;
    BOOL fInterim = FALSE;
    if (FAILED(hr = _IsInterimSelection(ec, &InterimRange, &fInterim))) {
        return hr;
    }

    CicCategoryMgr* pCicCat = NULL;
    CicDisplayAttributeMgr* pDispAttr = NULL;

    //
    // Create Cicero Category Manager and Display Attribute Manager
    //
    hr = _CreateCategoryAndDisplayAttributeManager(&pCicCat, &pDispAttr);
    if (SUCCEEDED(hr)) {
        if (fInterim) {
            hr = _MakeInterimString(ec, FullTextRange, InterimRange, lTextLength, bInWriteSession, pCicCat, pDispAttr);
        }
        else {
            hr = _MakeCompositionString(ec, FullTextRange, bInWriteSession, pCicCat, pDispAttr);
        }
    }

    if (pCicCat) {
        delete pCicCat;
    }
    if (pDispAttr) {
        delete pDispAttr;
    }

    return hr;
}

//+---------------------------------------------------------------------------
//
// CEditSessionUpdateCompositionString::_IsInterimSelection
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionUpdateCompositionString::_IsInterimSelection(TfEditCookie ec, ITfRange** pInterimRange, BOOL *pfInterim)
{
    ITfContext* pic = g_pConsoleTSF ? g_pConsoleTSF->GetInputContext() : NULL;
    if (pic == NULL) {
        return E_FAIL;
    }

    ULONG cFetched;

    TF_SELECTION sel;
    sel.range = NULL;

    *pfInterim = FALSE;
    if (pic->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched) != S_OK) {
        // no selection. we can return S_OK.
        return S_OK;
    }

    if (sel.style.fInterimChar && sel.range) {
        HRESULT hr;
        if (FAILED(hr = sel.range->Clone(pInterimRange))) {
            SafeReleaseClear(sel.range);
            return hr;
        }

        *pfInterim = TRUE;
    }

    SafeReleaseClear(sel.range);

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CEditSessionUpdateCompositionString::_MakeCompositionString
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionUpdateCompositionString::_MakeCompositionString(TfEditCookie ec, ITfRange* FullTextRange, BOOL bInWriteSession,
                                                                    CicCategoryMgr* pCicCatMgr, CicDisplayAttributeMgr* pCicDispAttr)
{
    CCompString CompStr;
    CCompTfGuidAtom CompGuid;
    CCompCursorPos CompCursorPos;
    CCompString ResultStr;
    BOOL fIgnorePreviousCompositionResult = FALSE;

    RETURN_IF_FAILED(_GetTextAndAttribute(ec, FullTextRange,
                                          CompStr, CompGuid, ResultStr,
                                          bInWriteSession,
                                          pCicCatMgr, pCicDispAttr));

    if (g_pConsoleTSF && g_pConsoleTSF->IsPendingCompositionCleanup())
    {
        // Don't draw the previous composition result if there was a cleanup session requested for it.
        fIgnorePreviousCompositionResult = TRUE;
        // Cancel pending cleanup, since the ResultStr was cleared from the composition in _GetTextAndAttribute.
        g_pConsoleTSF->OnCompositionCleanup(TRUE);
    }

    RETURN_IF_FAILED(_GetCursorPosition(ec, CompCursorPos));

    // Get display attribute manager
    ITfDisplayAttributeMgr* dam = pCicDispAttr->GetDisplayAttributeMgr();
    RETURN_HR_IF_NULL(E_FAIL, dam);

    // Get category manager
    ITfCategoryMgr* cat = pCicCatMgr->GetCategoryMgr();
    RETURN_HR_IF_NULL(E_FAIL, cat);

    // Allocate and fill TF_DISPLAYATTRIBUTE
    try
    {
        // Get conversion area service.
        CConversionArea* conv_area = g_pConsoleTSF ? g_pConsoleTSF->GetConversionArea() : NULL;
        RETURN_HR_IF_NULL(E_FAIL, conv_area);

        if (ResultStr && !fIgnorePreviousCompositionResult) {
            return conv_area->DrawResult(ResultStr);
        }
        if (CompStr) {
            ULONG cchDisplayAttribute = (ULONG)CompGuid.Count();
            std::vector<TF_DISPLAYATTRIBUTE> DisplayAttributes;
            DisplayAttributes.reserve(cchDisplayAttribute);

            for (DWORD i = 0; i < cchDisplayAttribute; i++) {
                TF_DISPLAYATTRIBUTE da;
                ZeroMemory(&da, sizeof(da));
                da.bAttr = TF_ATTR_OTHER;

                GUID guid;
                if (SUCCEEDED(cat->GetGUID(*CompGuid.GetAt(i), &guid))) {
                    CLSID clsid;
                    CComPtr<ITfDisplayAttributeInfo> dai;
                    if (SUCCEEDED(dam->GetDisplayAttributeInfo(guid, &dai, &clsid))) {
                        dai->GetAttributeInfo(&da);
                    }
                }

                DisplayAttributes.emplace_back(da);
            }

            return conv_area->DrawComposition(CompStr, // composition string
                                              DisplayAttributes, // display attributes
                                              CompCursorPos.GetCursorPosition()); // cursor position
        }
    }
    CATCH_RETURN();

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CEditSessionUpdateCompositionString::_MakeInterimString
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionUpdateCompositionString::_MakeInterimString(TfEditCookie ec,
                                                                ITfRange* FullTextRange,
                                                                ITfRange* InterimRange,
                                                                LONG lTextLength,
                                                                BOOL bInWriteSession,
                                                                CicCategoryMgr* pCicCatMgr,
                                                                CicDisplayAttributeMgr* pCicDispAttr)
{
    LONG lStartResult;
    LONG lEndResult;

    FullTextRange->CompareStart(ec, InterimRange, TF_ANCHOR_START, &lStartResult);
    RETURN_HR_IF(E_FAIL, lStartResult > 0);

    FullTextRange->CompareEnd(ec, InterimRange, TF_ANCHOR_END, &lEndResult);
    RETURN_HR_IF(E_FAIL, lEndResult != 1);

    CCompString ResultStr;

    if (lStartResult < 0) {
        // Make result string.
        RETURN_IF_FAILED(FullTextRange->ShiftEndToRange(ec, InterimRange, TF_ANCHOR_START));

        // Interim char assume 1 char length.
        // Full text length - 1 means result string length.
        lTextLength--;

        FAIL_FAST_IF(!(lTextLength > 0));

        if (lTextLength > 0) {
            try
            {
                auto wstr = std::make_unique<WCHAR[]>(lTextLength + 1);

                // Get the result text, finalize it, and erase the result text.
                if (SUCCEEDED(FullTextRange->GetText(ec, TF_TF_IGNOREEND, wstr.get(), (ULONG)lTextLength, (ULONG*)&lTextLength))) {
                    // Clear the TOM
                    LOG_IF_FAILED(ClearTextInRange(ec, FullTextRange));
                }
            }
            CATCH_RETURN();
        }
    }

    // Make interim character
    CCompString CompStr;
    CCompTfGuidAtom CompGuid;
    CCompString _tempResultStr;

    RETURN_IF_FAILED(_GetTextAndAttribute(ec, InterimRange,
                                          CompStr, CompGuid, _tempResultStr,
                                          bInWriteSession,
                                          pCicCatMgr, pCicDispAttr));


    // Get display attribute manager
    ITfDisplayAttributeMgr* dam = pCicDispAttr->GetDisplayAttributeMgr();
    RETURN_HR_IF_NULL(E_FAIL, dam);

    // Get category manager
    ITfCategoryMgr* cat = pCicCatMgr->GetCategoryMgr();
    RETURN_HR_IF_NULL(E_FAIL, cat);

    // Allocate and fill TF_DISPLAYATTRIBUTE
    try
    {
        // Get conversion area service.
        CConversionArea* conv_area = g_pConsoleTSF ? g_pConsoleTSF->GetConversionArea() : NULL;
        RETURN_HR_IF_NULL(E_FAIL, conv_area);

        if (ResultStr) {
            return conv_area->DrawResult(ResultStr);
        }
        if (CompStr) {
            ULONG cchDisplayAttribute = (ULONG)CompGuid.Count();
            std::vector<TF_DISPLAYATTRIBUTE> DisplayAttributes;
            DisplayAttributes.reserve(cchDisplayAttribute);

            for (DWORD i = 0; i < cchDisplayAttribute; i++) {
                TF_DISPLAYATTRIBUTE da;
                ZeroMemory(&da, sizeof(da));
                da.bAttr = TF_ATTR_OTHER;
                GUID  guid;
                if (SUCCEEDED(cat->GetGUID(*CompGuid.GetAt(i), &guid))) {
                    CLSID clsid;
                    CComPtr<ITfDisplayAttributeInfo> dai;
                    if (SUCCEEDED(dam->GetDisplayAttributeInfo(guid, &dai, &clsid))) {
                        dai->GetAttributeInfo(&da);
                    }
                }

                DisplayAttributes.emplace_back(da);
            }

            return conv_area->DrawComposition(CompStr, // composition string (Interim string)
                                              DisplayAttributes); // display attributes
        }
    }
    CATCH_RETURN();


    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CEditSessionUpdateCompositionString::_CreateCategoryAndDisplayAttributeManager
//
//----------------------------------------------------------------------------

[[nodiscard]]
HRESULT CEditSessionUpdateCompositionString::_CreateCategoryAndDisplayAttributeManager(CicCategoryMgr** pCicCatMgr, CicDisplayAttributeMgr** pCicDispAttr)
{
    HRESULT hr = E_OUTOFMEMORY;

    CicCategoryMgr* pTmpCat = NULL;
    CicDisplayAttributeMgr* pTmpDispAttr = NULL;

    //
    // Create Cicero Category Manager
    //
    pTmpCat = new(std::nothrow) CicCategoryMgr;
    if (pTmpCat) {
        if (SUCCEEDED(hr = pTmpCat->InitCategoryInstance())) {

            ITfCategoryMgr* pcat = pTmpCat->GetCategoryMgr();
            if (pcat) {
                //
                // Create Cicero Display Attribute Manager
                //
                pTmpDispAttr = new(std::nothrow) CicDisplayAttributeMgr;
                if (pTmpDispAttr) {
                    if (SUCCEEDED(hr = pTmpDispAttr->InitDisplayAttributeInstance(pcat))) {
                        *pCicCatMgr = pTmpCat;
                        *pCicDispAttr = pTmpDispAttr;
                    }
                }
            }
        }
    }

    if (FAILED(hr)) {
        if (pTmpCat) {
            delete pTmpCat;
        }
        if (pTmpDispAttr) {
            delete pTmpDispAttr;
        }
    }

    return hr;
}
