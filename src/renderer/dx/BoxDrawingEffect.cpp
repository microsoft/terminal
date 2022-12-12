// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "BoxDrawingEffect.h"

using namespace Microsoft::Console::Render;

BoxDrawingEffect::BoxDrawingEffect() noexcept :
    _scale{ 1.0f, 0.0f, 1.0f, 0.0f }
{
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT BoxDrawingEffect::RuntimeClassInitialize(float verticalScale, float verticalTranslate, float horizontalScale, float horizontalTranslate) noexcept
{
    _scale.VerticalScale = verticalScale;
    _scale.VerticalTranslation = verticalTranslate;
    _scale.HorizontalScale = horizontalScale;
    _scale.HorizontalTranslation = horizontalTranslate;
    return S_OK;
}

[[nodiscard]] HRESULT STDMETHODCALLTYPE BoxDrawingEffect::GetScale(BoxScale* scale) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, scale);
    *scale = _scale;
    return S_OK;
}
