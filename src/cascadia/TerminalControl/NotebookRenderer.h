// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Notebook.g.h"
#include "EventArgs.h"
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../cascadia/TerminalCore/BlockRenderData.hpp"

class NotebookRenderer : ::Microsoft::Console::Render::Renderer
{
public:
    NotebookRenderer() = default;

private:
};
