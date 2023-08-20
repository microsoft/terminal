/*++

Copyright (c) Microsoft Corporation.
Licensed under the MIT license.

Module Name:

    TfContext.h

Abstract:

    This file defines the CConsoleTSF Interface Class.

Author:

Revision History:

Notes:

--*/

#pragma once

class CConversionArea;

class CConsoleTSF final :
    public ITfContextOwner,
    public ITfContextOwnerCompositionSink,
    public ITfInputProcessorProfileActivationSink,
    public ITfUIElementSink,
    public ITfCleanupContextSink,
    public ITfTextEditSink
{
public:
    CConsoleTSF(HWND hwndConsole,
                GetSuggestionWindowPos pfnPosition,
                GetTextBoxAreaPos pfnTextArea) :
        _hwndConsole(hwndConsole),
        _pfnPosition(pfnPosition),
        _pfnTextArea(pfnTextArea),
        _cRef(1),
        _tid()
    {
    }

    virtual ~CConsoleTSF() = default;
    [[nodiscard]] HRESULT Initialize();
    void Uninitialize();

public:
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG)
    AddRef(void);
    STDMETHODIMP_(ULONG)
    Release(void);

    // ITfContextOwner
    STDMETHODIMP GetACPFromPoint(const POINT*, DWORD, LONG* pCP)
    {
        if (pCP)
        {
            *pCP = 0;
        }

        return S_OK;
    }

    // This returns Rectangle of the text box of whole console.
    // When a user taps inside the rectangle while hardware keyboard is not available,
    // touch keyboard is invoked.
    STDMETHODIMP GetScreenExt(RECT* pRect)
    {
        if (pRect)
        {
            *pRect = _pfnTextArea();
        }

        return S_OK;
    }

    // This returns rectangle of current command line edit area.
    // When a user types in East Asian language, candidate window is shown at this position.
    // Emoji and more panel (Win+.) is shown at the position, too.
    STDMETHODIMP GetTextExt(LONG, LONG, RECT* pRect, BOOL* pbClipped)
    {
        if (pRect)
        {
            *pRect = _pfnPosition();
        }

        if (pbClipped)
        {
            *pbClipped = FALSE;
        }

        return S_OK;
    }

    STDMETHODIMP GetStatus(TF_STATUS* pTfStatus)
    {
        if (pTfStatus)
        {
            pTfStatus->dwDynamicFlags = 0;
            pTfStatus->dwStaticFlags = TF_SS_TRANSITORY;
        }
        return pTfStatus ? S_OK : E_INVALIDARG;
    }
    STDMETHODIMP GetWnd(HWND* phwnd)
    {
        *phwnd = _hwndConsole;
        return S_OK;
    }
    STDMETHODIMP GetAttribute(REFGUID, VARIANT*)
    {
        return E_NOTIMPL;
    }

    // ITfContextOwnerCompositionSink methods
    STDMETHODIMP OnStartComposition(ITfCompositionView* pComposition, BOOL* pfOk);
    STDMETHODIMP OnUpdateComposition(ITfCompositionView* pComposition, ITfRange* pRangeNew);
    STDMETHODIMP OnEndComposition(ITfCompositionView* pComposition);

    // ITfInputProcessorProfileActivationSink
    STDMETHODIMP OnActivated(DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags);

    // ITfUIElementSink methods
    STDMETHODIMP BeginUIElement(DWORD dwUIElementId, BOOL* pbShow);
    STDMETHODIMP UpdateUIElement(DWORD dwUIElementId);
    STDMETHODIMP EndUIElement(DWORD dwUIElementId);

    // ITfCleanupContextSink methods
    STDMETHODIMP OnCleanupContext(TfEditCookie ecWrite, ITfContext* pic);

    // ITfTextEditSink methods
    STDMETHODIMP OnEndEdit(ITfContext* pInputContext, TfEditCookie ecReadOnly, ITfEditRecord* pEditRecord);

public:
    CConversionArea* CreateConversionArea();
    CConversionArea* GetConversionArea() { return _pConversionArea; }
    ITfContext* GetInputContext() { return _spITfInputContext.get(); }
    HWND GetConsoleHwnd() { return _hwndConsole; }
    TfClientId GetTfClientId() { return _tid; }
    BOOL IsInComposition() { return (_cCompositions > 0); }
    void OnEditSession() { _fEditSessionRequested = FALSE; }
    BOOL IsPendingCompositionCleanup() { return _fCleanupSessionRequested || _fCompositionCleanupSkipped; }
    void OnCompositionCleanup(BOOL bSucceeded)
    {
        _fCleanupSessionRequested = FALSE;
        _fCompositionCleanupSkipped = !bSucceeded;
    }
    void SetModifyingDocFlag(BOOL fSet) { _fModifyingDoc = fSet; }
    void SetFocus(BOOL fSet)
    {
        if (!fSet && _cCompositions)
        {
            // Close (terminate) any open compositions when losing the input focus.
            if (_spITfInputContext)
            {
                auto spCompositionServices = _spITfInputContext.try_query<ITfContextOwnerCompositionServices>();
                if (spCompositionServices)
                {
                    spCompositionServices->TerminateComposition(nullptr);
                }
            }
        }
    }

    // A workaround for a MS Korean IME scenario where the IME appends a whitespace
    // composition programmatically right after completing a keyboard input composition.
    // Since post-composition clean-up is an async operation, the programmatic whitespace
    // composition gets completed before the previous composition cleanup happened,
    // and this results in a double insertion of the first composition. To avoid that, we'll
    // store the length of the last completed composition here until it's cleaned up.
    // (for simplicity, this patch doesn't provide a generic solution for all possible
    // scenarios with subsequent synchronous compositions, only for the known 'append').
    long GetCompletedRangeLength() const { return _cchCompleted; }
    void SetCompletedRangeLength(long cch) { _cchCompleted = cch; }

private:
    [[nodiscard]] HRESULT _OnUpdateComposition();
    [[nodiscard]] HRESULT _OnCompleteComposition();
    BOOL _HasCompositionChanged(ITfContext* pInputContext, TfEditCookie ecReadOnly, ITfEditRecord* pEditRecord);

private:
    // ref count.
    DWORD _cRef;

    // Cicero stuff.
    TfClientId _tid;
    wil::com_ptr_nothrow<ITfThreadMgrEx> _spITfThreadMgr;
    wil::com_ptr_nothrow<ITfDocumentMgr> _spITfDocumentMgr;
    wil::com_ptr_nothrow<ITfContext> _spITfInputContext;

    // Event sink cookies.
    DWORD _dwContextOwnerCookie = 0;
    DWORD _dwUIElementSinkCookie = 0;
    DWORD _dwTextEditSinkCookie = 0;
    DWORD _dwActivationSinkCookie = 0;

    // Conversion area object for the languages.
    CConversionArea* _pConversionArea = nullptr;

    // Console info.
    HWND _hwndConsole;
    GetSuggestionWindowPos _pfnPosition;
    GetTextBoxAreaPos _pfnTextArea;

    // Miscellaneous flags
    BOOL _fModifyingDoc = FALSE; // Set TRUE, when calls ITfRange::SetText
    BOOL _fCoInitialized = FALSE;
    BOOL _fEditSessionRequested = FALSE;
    BOOL _fCleanupSessionRequested = FALSE;
    BOOL _fCompositionCleanupSkipped = FALSE;

    int _cCompositions = 0;
    long _cchCompleted = 0; // length of completed composition waiting for cleanup
};

extern CConsoleTSF* g_pConsoleTSF;
