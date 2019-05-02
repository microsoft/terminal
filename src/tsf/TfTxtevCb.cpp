/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    TfTxtevCb.cpp

Abstract:

    This file implements the CTextEventSinkCallBack Class.

Author:

Revision History:

Notes:

--*/


#include "precomp.h"
#include "ConsoleTSF.h"
#include "TfEditses.h"

//+---------------------------------------------------------------------------
//
// CConsoleTSF::HasCompositionChanged
//
//----------------------------------------------------------------------------

BOOL CConsoleTSF::_HasCompositionChanged(ITfContext *pInputContext, TfEditCookie ecReadOnly, ITfEditRecord *pEditRecord)
{
    BOOL fChanged;
    if (SUCCEEDED(pEditRecord->GetSelectionStatus(&fChanged)))
    {
        if (fChanged) {
            return TRUE;
        }
    }

    //
    // Find GUID_PROP_CONIME_TRACKCOMPOSITION property.
    //

    CComPtr<ITfProperty> Property;
    CComPtr<ITfRange>    FoundRange;
    CComPtr<ITfProperty> PropertyTrackComposition;

    BOOL bFound = FALSE;

    if (SUCCEEDED(pInputContext->GetProperty(GUID_PROP_CONIME_TRACKCOMPOSITION, &Property))) {

        CComPtr<IEnumTfRanges> EnumFindFirstTrackCompRange;

        if (SUCCEEDED(Property->EnumRanges(ecReadOnly, &EnumFindFirstTrackCompRange, NULL))) {

            HRESULT hr;
            CComPtr<ITfRange> range;

            while ((hr = EnumFindFirstTrackCompRange->Next(1, &range, NULL)) == S_OK) {

                VARIANT var;
                VariantInit(&var);

                hr = Property->GetValue(ecReadOnly, range, &var);
                if (SUCCEEDED(hr)) {
                    if ((V_VT(&var) == VT_I4 && V_I4(&var) != 0)) {
                        range->Clone(&FoundRange);
                        bFound = TRUE; // FOUND!!
                        break;
                    }
                }

                VariantClear(&var);

                range.Release();

                if (bFound) {
                    break; // FOUND!!
                }
            }
        }
    }

    //
    // if there is no track composition property,
    // the composition has been changed since we put it.
    //
    if (! bFound) {
        return TRUE;
    }

    if (FoundRange == NULL) {
        return FALSE;
    }


    bFound = FALSE;   // RESET bFound flag...

    CComPtr<ITfRange> rangeTrackComposition;
    if (SUCCEEDED(FoundRange->Clone(&rangeTrackComposition))) {

        //
        // get the text range that does not include read only area for
        // reconversion.
        //
        CComPtr<ITfRange> rangeAllText;
        LONG cch;
        if (SUCCEEDED(CEditSessionObject::GetAllTextRange(ecReadOnly, pInputContext, &rangeAllText, &cch))) {

            LONG lResult;
            if (SUCCEEDED(rangeTrackComposition->CompareStart(ecReadOnly, rangeAllText, TF_ANCHOR_START, &lResult))) {

                //
                // if the start position of the track composition range is not
                // the beggining of IC,
                // the composition has been changed since we put it.
                //
                if (lResult != 0) {
                    bFound = TRUE;  // FOUND!!
                }
                else if (SUCCEEDED(rangeTrackComposition->CompareEnd(ecReadOnly, rangeAllText, TF_ANCHOR_END, &lResult))) {

                    //
                    // if the start position of the track composition range is not
                    // the beggining of IC,
                    // the composition has been changed since we put it.
                    //
                    //
                    // If we find the changes in these property, we need to update hIMC.
                    //
                    const GUID *guids[] = {&GUID_PROP_COMPOSING,
                                           &GUID_PROP_ATTRIBUTE};
                    const int guid_size = sizeof(guids) / sizeof(GUID*);

                    CComPtr<IEnumTfRanges> EnumPropertyChanged;

                    if (lResult != 0) {
                        bFound = TRUE;  // FOUND!!
                    }
                    else if (SUCCEEDED(pEditRecord->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, guids, guid_size, &EnumPropertyChanged))) {

                        HRESULT hr;
                        CComPtr<ITfRange> range;

                        while ((hr = EnumPropertyChanged->Next(1, &range, NULL)) == S_OK) {
                            BOOL empty;
                            if (range->IsEmpty(ecReadOnly, &empty) == S_OK && empty) {
                                range.Release();
                                continue;
                            }

                            range.Release();

                            bFound = TRUE;  // FOUND!!
                            break;
                        }
                    }
                }
            }
        }
    }
    return bFound;
}
