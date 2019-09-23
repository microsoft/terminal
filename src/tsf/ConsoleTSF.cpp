// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TfConvArea.h"
#include "TfEditSes.h"

/* 626761ad-78d2-44d2-be8b-752cf122acec */
const GUID GUID_APPLICATION = { 0x626761ad, 0x78d2, 0x44d2, { 0xbe, 0x8b, 0x75, 0x2c, 0xf1, 0x22, 0xac, 0xec } };

//+---------------------------------------------------------------------------
//
// CConsoleTSF::Initialize
//
//----------------------------------------------------------------------------

#define Init_CheckResult() \
    if (FAILED(hr))        \
    {                      \
        Uninitialize();    \
        return hr;         \
    }

[[nodiscard]] HRESULT CConsoleTSF::Initialize()
{
    HRESULT hr;

    if (_spITfThreadMgr)
    {
        return S_FALSE;
    }

    // Activate per-thread Cicero in custom UI mode (TF_TMAE_UIELEMENTENABLEDONLY).

    hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    Init_CheckResult();
    _fCoInitialized = TRUE;

    hr = ::CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&_spITfThreadMgr));
    Init_CheckResult();

    hr = _spITfThreadMgr->ActivateEx(&_tid, TF_TMAE_CONSOLE);
    Init_CheckResult();

    // Create Cicero document manager and input context.

    hr = _spITfThreadMgr->CreateDocumentMgr(&_spITfDocumentMgr);
    Init_CheckResult();

    TfEditCookie ecTmp;
    hr = _spITfDocumentMgr->CreateContext(_tid,
                                          0,
                                          static_cast<ITfContextOwnerCompositionSink*>(this),
                                          &_spITfInputContext,
                                          &ecTmp);
    Init_CheckResult();

    // Set the context owner before attaching the context to the doc.
    wil::com_ptr_nothrow<ITfSource> spSrcIC;
    hr = _spITfInputContext.query_to(&spSrcIC);
    Init_CheckResult();

    hr = spSrcIC->AdviseSink(IID_ITfContextOwner, static_cast<ITfContextOwner*>(this), &_dwContextOwnerCookie);
    Init_CheckResult();

    hr = _spITfDocumentMgr->Push(_spITfInputContext.get());
    Init_CheckResult();

    // Collect the active keyboard layout info.

    wil::com_ptr_nothrow<ITfInputProcessorProfileMgr> spITfProfilesMgr;
    hr = ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&spITfProfilesMgr));
    if (SUCCEEDED(hr))
    {
        TF_INPUTPROCESSORPROFILE ipp;
        hr = spITfProfilesMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &ipp);
        if (SUCCEEDED(hr))
        {
            OnActivated(ipp.dwProfileType, ipp.langid, ipp.clsid, ipp.catid, ipp.guidProfile, ipp.hkl, ipp.dwFlags);
        }
    }
    Init_CheckResult();

    // Setup some useful Cicero event sinks and callbacks.
    // _spITfThreadMgr && _spITfInputContext must be non-null for checks above to have succeeded, so
    // we're not going to check them again here. try_query will A/V if they're null.
    wil::com_ptr_nothrow<ITfSource> spSrcTIM(_spITfThreadMgr.try_query<ITfSource>());
    wil::com_ptr_nothrow<ITfSourceSingle> spSrcICS(_spITfInputContext.try_query<ITfSourceSingle>());

    hr = (spSrcTIM && spSrcIC && spSrcICS) ? S_OK : E_FAIL;
    Init_CheckResult();

    hr = spSrcTIM->AdviseSink(IID_ITfInputProcessorProfileActivationSink,
                              static_cast<ITfInputProcessorProfileActivationSink*>(this),
                              &_dwActivationSinkCookie);
    Init_CheckResult();

    hr = spSrcTIM->AdviseSink(IID_ITfUIElementSink, static_cast<ITfUIElementSink*>(this), &_dwUIElementSinkCookie);
    Init_CheckResult();

    hr = spSrcIC->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this), &_dwTextEditSinkCookie);
    Init_CheckResult();

    hr = spSrcICS->AdviseSingleSink(_tid, IID_ITfCleanupContextSink, static_cast<ITfCleanupContextSink*>(this));
    Init_CheckResult();

    return hr;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::Uninitialize()
//
//----------------------------------------------------------------------------

void CConsoleTSF::Uninitialize()
{
    // Destroy the current conversion area object

    if (_pConversionArea)
    {
        delete _pConversionArea;
        _pConversionArea = NULL;
    }

    // Detach Cicero event sinks.
    if (_spITfInputContext)
    {
        wil::com_ptr_nothrow<ITfSourceSingle> spSrcICS(_spITfInputContext.try_query<ITfSourceSingle>());
        if (spSrcICS)
        {
            spSrcICS->UnadviseSingleSink(_tid, IID_ITfCleanupContextSink);
        }
    }

    // Associate the document\context with the console window.

    if (_spITfThreadMgr)
    {
        wil::com_ptr_nothrow<ITfSource> spSrcTIM(_spITfThreadMgr.try_query<ITfSource>());
        if (spSrcTIM)
        {
            if (_dwUIElementSinkCookie)
            {
                spSrcTIM->UnadviseSink(_dwUIElementSinkCookie);
            }
            if (_dwActivationSinkCookie)
            {
                spSrcTIM->UnadviseSink(_dwActivationSinkCookie);
            }
        }
    }

    _dwUIElementSinkCookie = 0;
    _dwActivationSinkCookie = 0;

    if (_spITfInputContext)
    {
        wil::com_ptr_nothrow<ITfSource> spSrcIC(_spITfInputContext.try_query<ITfSource>());
        if (spSrcIC)
        {
            if (_dwContextOwnerCookie)
            {
                spSrcIC->UnadviseSink(_dwContextOwnerCookie);
            }
            if (_dwTextEditSinkCookie)
            {
                spSrcIC->UnadviseSink(_dwTextEditSinkCookie);
            }
        }
    }
    _dwContextOwnerCookie = 0;
    _dwTextEditSinkCookie = 0;

    // Clear the Cicero reference to our document manager.

    if (_spITfThreadMgr && _spITfDocumentMgr)
    {
        wil::com_ptr_nothrow<ITfDocumentMgr> spDocMgr;
        _spITfThreadMgr->AssociateFocus(_hwndConsole, NULL, &spDocMgr);
    }

    // Dismiss the input context and document manager.

    if (_spITfDocumentMgr)
    {
        _spITfDocumentMgr->Pop(TF_POPF_ALL);
    }

    _spITfInputContext.reset();
    _spITfDocumentMgr.reset();

    // Deactivate per-thread Cicero and uninitialize COM.

    if (_spITfThreadMgr)
    {
        _spITfThreadMgr->Deactivate();
        _spITfThreadMgr.reset();
    }
    if (_fCoInitialized)
    {
        ::CoUninitialize();
        _fCoInitialized = FALSE;
    }
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::IUnknown::QueryInterface
// CConsoleTSF::IUnknown::AddRef
// CConsoleTSF::IUnknown::Release
//
//----------------------------------------------------------------------------

STDMETHODIMP CConsoleTSF::QueryInterface(REFIID riid, void** ppvObj)
{
    if (!ppvObj)
    {
        return E_FAIL;
    }
    *ppvObj = NULL;

    if (IsEqualIID(riid, IID_ITfCleanupContextSink))
    {
        *ppvObj = static_cast<ITfCleanupContextSink*>(this);
    }
    else if (IsEqualGUID(riid, IID_ITfContextOwnerCompositionSink))
    {
        *ppvObj = static_cast<ITfContextOwnerCompositionSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfUIElementSink))
    {
        *ppvObj = static_cast<ITfUIElementSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfContextOwner))
    {
        *ppvObj = static_cast<ITfContextOwner*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfInputProcessorProfileActivationSink))
    {
        *ppvObj = static_cast<ITfInputProcessorProfileActivationSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfTextEditSink))
    {
        *ppvObj = static_cast<ITfTextEditSink*>(this);
    }
    else if (IsEqualGUID(riid, IID_IUnknown))
    {
        *ppvObj = this;
    }
    if (*ppvObj)
    {
        AddRef();
    }
    return (*ppvObj) ? S_OK : E_NOINTERFACE;
}

STDAPI_(ULONG)
CConsoleTSF::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

STDAPI_(ULONG)
CConsoleTSF::Release()
{
    ULONG cr = InterlockedDecrement(&_cRef);
    if (cr == 0)
    {
        if (g_pConsoleTSF == this)
        {
            g_pConsoleTSF = NULL;
        }
        delete this;
    }
    return cr;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::ITfCleanupContextSink::OnCleanupContext
//
//----------------------------------------------------------------------------

STDMETHODIMP CConsoleTSF::OnCleanupContext(TfEditCookie ecWrite, ITfContext* pic)
{
    //
    // Remove GUID_PROP_COMPOSING
    //
    wil::com_ptr_nothrow<ITfProperty> prop;
    if (SUCCEEDED(pic->GetProperty(GUID_PROP_COMPOSING, &prop)))
    {
        wil::com_ptr_nothrow<IEnumTfRanges> enumranges;
        if (SUCCEEDED(prop->EnumRanges(ecWrite, &enumranges, nullptr)))
        {
            wil::com_ptr_nothrow<ITfRange> rangeTmp;
            while (enumranges->Next(1, &rangeTmp, NULL) == S_OK)
            {
                VARIANT var;
                VariantInit(&var);
                prop->GetValue(ecWrite, rangeTmp.get(), &var);
                if ((var.vt == VT_I4) && (var.lVal != 0))
                {
                    prop->Clear(ecWrite, rangeTmp.get());
                }
            }
        }
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::ITfContextOwnerCompositionSink::OnStartComposition
// CConsoleTSF::ITfContextOwnerCompositionSink::OnUpdateComposition
// CConsoleTSF::ITfContextOwnerCompositionSink::OnEndComposition
//
//----------------------------------------------------------------------------

STDMETHODIMP CConsoleTSF::OnStartComposition(ITfCompositionView* pCompView, BOOL* pfOk)
{
    if (!_pConversionArea || (_cCompositions > 0 && (!_fModifyingDoc)))
    {
        *pfOk = FALSE;
    }
    else
    {
        *pfOk = TRUE;
        // Ignore compositions triggered by our own edit sessions
        // (i.e. when the application is the composition owner)
        CLSID clsidCompositionOwner = GUID_APPLICATION;
        pCompView->GetOwnerClsid(&clsidCompositionOwner);
        if (!IsEqualGUID(clsidCompositionOwner, GUID_APPLICATION))
        {
            _cCompositions++;
            if (_cCompositions == 1)
            {
                LOG_IF_FAILED(ImeStartComposition());
            }
        }
    }
    return S_OK;
}

STDMETHODIMP CConsoleTSF::OnUpdateComposition(ITfCompositionView* /*pComp*/, ITfRange*)
{
    return S_OK;
}

STDMETHODIMP CConsoleTSF::OnEndComposition(ITfCompositionView* pCompView)
{
    if (!_cCompositions || !_pConversionArea)
    {
        return E_FAIL;
    }
    // Ignore compositions triggered by our own edit sessions
    // (i.e. when the application is the composition owner)
    CLSID clsidCompositionOwner = GUID_APPLICATION;
    pCompView->GetOwnerClsid(&clsidCompositionOwner);
    if (!IsEqualGUID(clsidCompositionOwner, GUID_APPLICATION))
    {
        _cCompositions--;
        if (!_cCompositions)
        {
            LOG_IF_FAILED(_OnCompleteComposition());
            LOG_IF_FAILED(ImeEndComposition());
        }
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::ITfTextEditSink::OnEndEdit
//
//----------------------------------------------------------------------------

STDMETHODIMP CConsoleTSF::OnEndEdit(ITfContext* pInputContext, TfEditCookie ecReadOnly, ITfEditRecord* pEditRecord)
{
    if (_cCompositions && _pConversionArea && _HasCompositionChanged(pInputContext, ecReadOnly, pEditRecord))
    {
        LOG_IF_FAILED(_OnUpdateComposition());
    }
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::ITfInputProcessorProfileActivationSink::OnActivated
//
//----------------------------------------------------------------------------

STDMETHODIMP CConsoleTSF::OnActivated(DWORD /*dwProfileType*/,
                                      LANGID /*langid*/,
                                      REFCLSID /*clsid*/,
                                      REFGUID catid,
                                      REFGUID /*guidProfile*/,
                                      HKL /*hkl*/,
                                      DWORD dwFlags)
{
    if (!(dwFlags & TF_IPSINK_FLAG_ACTIVE))
    {
        return S_OK;
    }
    if (!IsEqualGUID(catid, GUID_TFCAT_TIP_KEYBOARD))
    {
        // Don't care for non-keyboard profiles.
        return S_OK;
    }

    try
    {
        CreateConversionArea();
    }
    CATCH_RETURN();

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::ITfUIElementSink::BeginUIElement
//
//----------------------------------------------------------------------------

STDMETHODIMP CConsoleTSF::BeginUIElement(DWORD /*dwUIElementId*/, BOOL* pbShow)
{
    *pbShow = TRUE;
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::ITfUIElementSink::UpdateUIElement
//
//----------------------------------------------------------------------------

STDMETHODIMP CConsoleTSF::UpdateUIElement(DWORD /*dwUIElementId*/)
{
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::ITfUIElementSink::EndUIElement
//
//----------------------------------------------------------------------------

STDMETHODIMP CConsoleTSF::EndUIElement(DWORD /*dwUIElementId*/)
{
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::CreateConversionAreaService
//
//----------------------------------------------------------------------------

CConversionArea* CConsoleTSF::CreateConversionArea()
{
    BOOL fHadConvArea = (_pConversionArea != NULL);

    if (!_pConversionArea)
    {
        _pConversionArea = new CConversionArea();
    }

    // Associate the document\context with the console window.
    if (!fHadConvArea)
    {
        wil::com_ptr_nothrow<ITfDocumentMgr> spPrevDocMgr;
        _spITfThreadMgr->AssociateFocus(_hwndConsole, _pConversionArea ? _spITfDocumentMgr.get() : NULL, &spPrevDocMgr);
    }

    return _pConversionArea;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::OnUpdateComposition()
//
//----------------------------------------------------------------------------

[[nodiscard]] HRESULT CConsoleTSF::_OnUpdateComposition()
{
    if (_fEditSessionRequested)
    {
        return S_FALSE;
    }

    HRESULT hr = E_OUTOFMEMORY;
    CEditSessionUpdateCompositionString* pEditSession = new (std::nothrow) CEditSessionUpdateCompositionString();
    if (pEditSession)
    {
        // Can't use TF_ES_SYNC because called from OnEndEdit.
        _fEditSessionRequested = TRUE;
        _spITfInputContext->RequestEditSession(_tid, pEditSession, TF_ES_READWRITE, &hr);
        if (FAILED(hr))
        {
            pEditSession->Release();
            _fEditSessionRequested = FALSE;
        }
    }
    return hr;
}

//+---------------------------------------------------------------------------
//
// CConsoleTSF::OnCompleteComposition()
//
//----------------------------------------------------------------------------

[[nodiscard]] HRESULT CConsoleTSF::_OnCompleteComposition()
{
    // Update the composition area.

    HRESULT hr = E_OUTOFMEMORY;
    CEditSessionCompositionComplete* pEditSession = new (std::nothrow) CEditSessionCompositionComplete();
    if (pEditSession)
    {
        // The composition could have been finalized because of a caret move, therefore it must be
        // inserted synchronously while at the orignal caret position.(TF_ES_SYNC is ok for a nested RO session).
        _spITfInputContext->RequestEditSession(_tid, pEditSession, TF_ES_READ | TF_ES_SYNC, &hr);
        if (FAILED(hr))
        {
            pEditSession->Release();
        }
    }

    // Cleanup (empty the context range) after the last composition, unless a new one has started.
    if (!_fCleanupSessionRequested)
    {
        _fCleanupSessionRequested = TRUE;
        CEditSessionCompositionCleanup* pEditSessionCleanup = new (std::nothrow) CEditSessionCompositionCleanup();
        if (pEditSessionCleanup)
        {
            // Can't use TF_ES_SYNC because requesting RW while called within another session.
            // For the same reason, must use explicit TF_ES_ASYNC, or the request will be rejected otherwise.
            _spITfInputContext->RequestEditSession(_tid, pEditSessionCleanup, TF_ES_READWRITE | TF_ES_ASYNC, &hr);
            if (FAILED(hr))
            {
                pEditSessionCleanup->Release();
                _fCleanupSessionRequested = FALSE;
            }
        }
    }
    return hr;
}
