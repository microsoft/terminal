// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "screenInfoUiaProvider.hpp"
#include "..\types\WindowUiaProviderBase.hpp"

namespace Types
{
    using namespace Microsoft::Console::Types;
}

using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Interactivity::Win32;

ScreenInfoUiaProvider::ScreenInfoUiaProvider(_In_ Microsoft::Console::Render::IRenderData* pData,
                                             _In_ Types::WindowUiaProviderBase* const pUiaParent) :
    _pUiaParent(THROW_HR_IF_NULL(E_INVALIDARG, pUiaParent)),
    ScreenInfoUiaProviderBase(THROW_HR_IF_NULL(E_INVALIDARG, pData))
{
}

IFACEMETHODIMP ScreenInfoUiaProvider::Navigate(_In_ NavigateDirection direction,
                                               _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    // TODO GitHub 2120: _pUiaParent should not be allowed to be null
    RETURN_HR_IF(E_NOTIMPL, _pUiaParent == nullptr);

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    /*ApiMsgNavigate apiMsg;
    apiMsg.Direction = direction;
    Tracing::s_TraceUia(this, ApiCall::Navigate, &apiMsg);*/
    *ppProvider = nullptr;

    if (direction == NavigateDirection_Parent)
    {
        try
        {
            _pUiaParent->QueryInterface(IID_PPV_ARGS(ppProvider));
        }
        catch (...)
        {
            *ppProvider = nullptr;
            return wil::ResultFromCaughtException();
        }
        RETURN_IF_NULL_ALLOC(*ppProvider);
    }

    // For the other directions the default of nullptr is correct
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_BoundingRectangle(_Out_ UiaRect* pRect)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetBoundingRectangle, nullptr);

    RECT rc = _pUiaParent->GetWindowRect();

    pRect->left = rc.left;
    pRect->top = rc.top;
    pRect->width = rc.right - rc.left;
    pRect->height = rc.bottom - rc.top;

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider)
{
    // TODO GitHub 2120: _pUiaParent should not be allowed to be null
    RETURN_HR_IF(E_NOTIMPL, _pUiaParent == nullptr);

    //Tracing::s_TraceUia(this, ApiCall::GetFragmentRoot, nullptr);
    try
    {
        _pUiaParent->QueryInterface(IID_PPV_ARGS(ppProvider));
    }
    catch (...)
    {
        *ppProvider = nullptr;
        return wil::ResultFromCaughtException();
    }
    RETURN_IF_NULL_ALLOC(*ppProvider);
    return S_OK;
}

HWND ScreenInfoUiaProvider::GetWindowHandle() const
{
    return _pUiaParent->GetWindowHandle();
}

void ScreenInfoUiaProvider::ChangeViewport(const SMALL_RECT NewWindow)
{
    _pUiaParent->ChangeViewport(NewWindow);
}

std::deque<Microsoft::Console::Types::UiaTextRangeBase*> ScreenInfoUiaProvider::GetSelectionRangeUTRs(_In_ Microsoft::Console::Render::IRenderData* pData,
                                                                                                      _In_ IRawElementProviderSimple* pProvider)
{
    std::deque<Microsoft::Console::Types::UiaTextRangeBase*> result;

    auto ranges = UiaTextRange::GetSelectionRanges(pData, pProvider);
    while (!ranges.empty())
    {
        result.emplace_back(ranges.back());
        ranges.pop_back();
    }

    return result;
}

Microsoft::Console::Types::UiaTextRangeBase* ScreenInfoUiaProvider::CreateUTR(_In_ Microsoft::Console::Render::IRenderData* pData,
                                                                              _In_ IRawElementProviderSimple* const pProvider)
{
    return UiaTextRange::Create(pData, pProvider);
}

Microsoft::Console::Types::UiaTextRangeBase* ScreenInfoUiaProvider::CreateUTR(_In_ Microsoft::Console::Render::IRenderData* pData,
                                                                              _In_ IRawElementProviderSimple* const pProvider,
                                                                              const Cursor& cursor)
{
    return UiaTextRange::Create(pData, pProvider, cursor);
}

Microsoft::Console::Types::UiaTextRangeBase* ScreenInfoUiaProvider::CreateUTR(_In_ Microsoft::Console::Render::IRenderData* pData,
                                                                              _In_ IRawElementProviderSimple* const pProvider,
                                                                              const Endpoint start,
                                                                              const Endpoint end,
                                                                              const bool degenerate)
{
    return UiaTextRange::Create(pData, pProvider, start, end, degenerate);
}

Microsoft::Console::Types::UiaTextRangeBase* ScreenInfoUiaProvider::CreateUTR(_In_ Microsoft::Console::Render::IRenderData* pData,
                                                                              _In_ IRawElementProviderSimple* const pProvider,
                                                                              const UiaPoint point)
{
    return UiaTextRange::Create(pData, pProvider, point);
}
