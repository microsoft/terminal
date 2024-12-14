// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

class TextAttribute;

namespace Microsoft::Console::Render
{
    class Renderer;
}

namespace Microsoft::Console::TSF
{
    struct IDataProvider;

    struct Implementation : ITfContextOwner, ITfContextOwnerCompositionSink, ITfTextEditSink
    {
        static void SetDefaultScopeAlphanumericHalfWidth(bool enable) noexcept;

        virtual ~Implementation() = default;

        void Initialize();
        void Uninitialize() noexcept;
        HWND FindWindowOfActiveTSF() noexcept;
        void AssociateFocus(IDataProvider* provider);
        void Focus(IDataProvider* provider);
        void Unfocus(IDataProvider* provider);
        bool HasActiveComposition() const noexcept;

        // IUnknown methods
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) noexcept override;
        ULONG STDMETHODCALLTYPE AddRef() noexcept override;
        ULONG STDMETHODCALLTYPE Release() noexcept override;

        // ITfContextOwner
        STDMETHODIMP GetACPFromPoint(const POINT* ptScreen, DWORD dwFlags, LONG* pacp) noexcept override;
        STDMETHODIMP GetTextExt(LONG acpStart, LONG acpEnd, RECT* prc, BOOL* pfClipped) noexcept override;
        STDMETHODIMP GetScreenExt(RECT* prc) noexcept override;
        STDMETHODIMP GetStatus(TF_STATUS* pdcs) noexcept override;
        STDMETHODIMP GetWnd(HWND* phwnd) noexcept override;
        STDMETHODIMP GetAttribute(REFGUID rguidAttribute, VARIANT* pvarValue) noexcept override;

        // ITfContextOwnerCompositionSink methods
        STDMETHODIMP OnStartComposition(ITfCompositionView* pComposition, BOOL* pfOk) noexcept override;
        STDMETHODIMP OnUpdateComposition(ITfCompositionView* pComposition, ITfRange* pRangeNew) noexcept override;
        STDMETHODIMP OnEndComposition(ITfCompositionView* pComposition) noexcept override;

        // ITfTextEditSink methods
        STDMETHODIMP OnEndEdit(ITfContext* pic, TfEditCookie ecReadOnly, ITfEditRecord* pEditRecord) noexcept override;

    private:
        struct EditSessionProxyBase : ITfEditSession
        {
            explicit EditSessionProxyBase(Implementation* self) noexcept;
            virtual ~EditSessionProxyBase() = default;

            // IUnknown methods
            STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) noexcept override;
            ULONG STDMETHODCALLTYPE AddRef() noexcept override;
            ULONG STDMETHODCALLTYPE Release() noexcept override;

            ULONG referenceCount = 0;
            Implementation* self = nullptr;
        };

        // In the past we had 3 different `ITfEditSession`s (update, finish, cleanup).
        // Due to refactoring only 1 is left now, but this code remains in case we need more in the future.
        // It allows you to statically bind a callback function to a `ITfEditSession` proxy type.
        template<void (Implementation::*Callback)(TfEditCookie)>
        struct EditSessionProxy : EditSessionProxyBase
        {
            using EditSessionProxyBase::EditSessionProxyBase;

            // ITfEditSession method
            STDMETHODIMP DoEditSession(TfEditCookie ec) noexcept override
            {
                try
                {
                    (self->*Callback)(ec);
                    return S_OK;
                }
                CATCH_RETURN();
            }
        };

        struct AnsiInputScope : ITfInputScope
        {
            explicit AnsiInputScope(Implementation* self) noexcept;
            virtual ~AnsiInputScope() = default;

            // IUnknown methods
            STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) noexcept override;
            ULONG STDMETHODCALLTYPE AddRef() noexcept override;
            ULONG STDMETHODCALLTYPE Release() noexcept override;

            // ITfInputScope methods
            STDMETHODIMP GetInputScopes(InputScope** pprgInputScopes, UINT* pcCount) noexcept override;
            STDMETHODIMP GetPhrase(BSTR** ppbstrPhrases, UINT* pcCount) noexcept override;
            STDMETHODIMP GetRegularExpression(BSTR* pbstrRegExp) noexcept override;
            STDMETHODIMP GetSRGS(BSTR* pbstrSRGS) noexcept override;
            STDMETHODIMP GetXML(BSTR* pbstrXML) noexcept override;

            Implementation* self = nullptr;
        };

        [[nodiscard]] HRESULT _request(EditSessionProxyBase& session, DWORD flags) const;
        void _doCompositionUpdate(TfEditCookie ec);
        TextAttribute _textAttributeFromAtom(TfGuidAtom atom) const;
        static COLORREF _colorFromDisplayAttribute(TF_DA_COLOR color);

        ULONG _referenceCount = 1;

        wil::com_ptr<IDataProvider> _provider;
        HWND _associatedHwnd = nullptr;

        wil::com_ptr_t<ITfCategoryMgr> _categoryMgr;
        wil::com_ptr<ITfDisplayAttributeMgr> _displayAttributeMgr;
        wil::com_ptr<ITfThreadMgrEx> _threadMgrEx;
        wil::com_ptr<ITfDocumentMgr> _documentMgr;
        wil::com_ptr<ITfContext> _context;
        wil::com_ptr<ITfContextOwnerCompositionServices> _ownerCompositionServices;
        wil::com_ptr<ITfSource> _contextSource;
        DWORD _cookieContextOwner = TF_INVALID_COOKIE;
        DWORD _cookieTextEditSink = TF_INVALID_COOKIE;
        TfClientId _clientId = TF_CLIENTID_NULL;

        EditSessionProxy<&Implementation::_doCompositionUpdate> _editSessionCompositionUpdate{ this };
        int _compositions = 0;

        AnsiInputScope _ansiInputScope{ this };
        inline static std::atomic<bool> _wantsAnsiInputScope{ false };
    };
}
