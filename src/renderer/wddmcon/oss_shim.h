// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../host/conddkrefs.h"
#include <condrv.h>

// This is a fake interface, not to be confused with the actual private one.
MIDL_INTERFACE("e962a0bf-ba8c-4150-9939-4297b11329b6")
IDXGISwapChainDWM : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) = 0;
};

// This is a fake interface, not to be confused with the actual private one.
MIDL_INTERFACE("599628c0-c2c6-4720-8885-17abe0fd43f2")
IDXGIFactoryDWM : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown * pDevice, DXGI_SWAP_CHAIN_DESC * pDesc, IDXGIOutput * pTarget, IDXGISwapChainDWM * *ppSwapChain) = 0;
};
