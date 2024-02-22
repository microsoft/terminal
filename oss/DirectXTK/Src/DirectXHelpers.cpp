//--------------------------------------------------------------------------------------
// File: DirectXHelpers.cpp
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "DirectXHelpers.h"
#include "Effects.h"
#include "PlatformHelpers.h"


using namespace DirectX;

_Use_decl_annotations_
HRESULT DirectX::CreateInputLayoutFromEffect(
    ID3D11Device* device,
    IEffect* effect,
    const D3D11_INPUT_ELEMENT_DESC* desc,
    size_t count,
    ID3D11InputLayout** pInputLayout) noexcept
{
    if (!pInputLayout)
        return E_INVALIDARG;

    *pInputLayout = nullptr;

    if (!device || !effect || !desc || !count)
        return E_INVALIDARG;

    void const* shaderByteCode;
    size_t byteCodeLength;

    try
    {
        effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);
    }
    catch (com_exception e)
    {
        return e.get_result();
    }
    catch (...)
    {
        return E_FAIL;
    }

    return device->CreateInputLayout(
        desc, static_cast<UINT>(count),
        shaderByteCode, byteCodeLength,
        pInputLayout);
}
