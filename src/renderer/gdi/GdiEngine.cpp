// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "GdiEngine.h"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;

GdiEngine::GdiEngine()
{
}

GdiEngine::~GdiEngine()
{
}

void GdiEngine::WaitUntilCanRender()
{
}

void GdiEngine::Render(RenderingPayload& payload)
{
    UNREFERENCED_PARAMETER(payload);
}
