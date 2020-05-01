// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "BoxDrawingEffect.h"

using namespace Microsoft::Console::Render;

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
