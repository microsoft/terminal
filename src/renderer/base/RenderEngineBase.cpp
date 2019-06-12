// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/RenderEngineBase.hpp"
#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;

RenderEngineBase::RenderEngineBase() :
    _titleChanged(false),
    _lastFrameTitle(L"")
{
}

HRESULT RenderEngineBase::InvalidateTitle(const std::wstring& proposedTitle) noexcept
{
    if (proposedTitle != _lastFrameTitle)
    {
        _titleChanged = true;
    }

    return S_OK;
}

HRESULT RenderEngineBase::UpdateTitle(const std::wstring& newTitle) noexcept
{
    HRESULT hr = S_FALSE;
    if (newTitle != _lastFrameTitle)
    {
        RETURN_IF_FAILED(_DoUpdateTitle(newTitle));
        _lastFrameTitle = newTitle;
        _titleChanged = false;
        hr = S_OK;
    }
    return hr;
}
