# AtlasEngine

## General architecture overview

```mermaid
graph TD
    RenderThread["RenderThread (base/thread.cpp)\n<small>calls Renderer::PaintFrame() x times per sec</small>"]
    Renderer["Renderer (base/renderer.cpp)\n<small>breaks the text buffer down into GDI-oriented graphics\nprimitives (#quot;change brush to color X#quot;, #quot;draw string Y#quot;, ...)</small>"]
    RenderEngineBase[/"RenderEngineBase\n(base/RenderEngineBase.cpp)\n<small>abstracts 24 LOC 👻</small>"\]
    GdiEngine["GdiEngine (gdi/...)"]
    DxEngine["DxEngine (dx/...)"]

    subgraph AtlasEngine["AtlasEngine (atlas/...)"]
        AtlasEngine.cpp["AtlasEngine.cpp\n<small>Implements IRenderEngine text rendering API\nbreaks GDI graphics primitives down into DWRITE_GLYPH_RUNs</small>"]
        AtlasEngine.api.cpp["AtlasEngine.api.cpp\n<small>Implements the parts run inside the console\nlock (many IRenderEngine setters)<small>"]
        AtlasEngine.r.cpp["AtlasEngine.r.cpp\n<small>Implements the parts run\noutside of the console lock<small>"]
        Backend.cpp["Backend.cpp\n<small>Implements common functionality/helpers</small>"]
        BackendD2D.cpp["BackendD2D.cpp\n<small>Pure Direct2D text renderer (for low latency\nremote desktop and older/no GPUs)</small>"]
        BackendD3D.cpp["BackendD3D.cpp\n<small>Custom, performant text renderer\nwith our own glyph cache</small>"]
    end

    RenderThread --> Renderer
    Renderer -->|owns| RenderThread
    Renderer -.-> RenderEngineBase
    %% Mermaid.js has no support for backwards arrow at the moment
    RenderEngineBase <-.->|extends| GdiEngine
    RenderEngineBase <-.->|extends| DxEngine
    Renderer ----> AtlasEngine
    AtlasEngine.cpp <--> AtlasEngine.api.cpp
    AtlasEngine.cpp <--> AtlasEngine.r.cpp
    AtlasEngine.r.cpp --> BackendD2D.cpp
    AtlasEngine.r.cpp --> BackendD3D.cpp
    BackendD2D.cpp -.- Backend.cpp
    BackendD3D.cpp -.- Backend.cpp
```

As you can see, breaking the text buffer down into GDI-style primitives just to rebuild them into DirectWrite ones, is pretty wasteful. It's also incredibly bug prone. It would be beneficial if the TextBuffer and rendering settings were given directly to AtlasEngine so it can do its own bidding.

## BackendD3D

The primary entrypoint for rendering is `IBackend::Render` and `BackendD3D` implements it via the following functions, by calling them one by one in the order listed here.

### `_handleSettingsUpdate`

```mermaid
graph TD
    Render --> _handleSettingsUpdate
    _handleSettingsUpdate -->|font changes| _updateFontDependents --> _d2dRenderTargetUpdateFontSettings
    _handleSettingsUpdate -->|misc changes| _recreateCustomShader
    _handleSettingsUpdate --->|misc changes| _recreateCustomRenderTargetView
    _handleSettingsUpdate ---->|size changes| _recreateBackgroundColorBitmap
    _handleSettingsUpdate -----> _recreateConstBuffer
    _handleSettingsUpdate ------> _setupDeviceContextState
```

### `_drawBackground`

```mermaid
graph TD
    Render --> _drawBackground
    _drawBackground --> _uploadBackgroundBitmap
```

### `_drawCursorPart1` / `_drawCursorPart2`

```mermaid
graph TD
    Render --> _drawCursorPart1["_drawCursorPart1\nruns before _drawText\ndraws cursors that are behind the text"]
    Render --> _drawCursorPart2["_drawCursorPart2\nruns after _drawText\ndraws inverted cursors"]
    _drawCursorPart1 -.->|_cursorRects| _drawCursorPart2
```

### `_drawText`

```mermaid
graph TD
    Render --> _drawText

    _drawText --> foreachRow(("for each row"))
    foreachRow --> foreachRow
    foreachRow --> foreachFont(("for each font face"))
    foreachFont --> foreachFont
    foreachFont --> foreachGlyph(("for each glyph"))
    foreachGlyph --> foreachGlyph

    foreachGlyph --> _glyphAtlasMap[("font/glyph-pair lookup in\nglyph cache hashmap")]
    _glyphAtlasMap --> drawGlyph
    drawGlyph --> _appendQuad["_appendQuad\n<small>stages the glyph for later drawing</small>"]
    _glyphAtlasMap --> _appendQuad

    subgraph drawGlyph["if glyph is missing"]
        _drawGlyph["_drawGlyph\n<small>(defers to _drawSoftFontGlyph for soft fonts)</small>"]

        _drawGlyph -.->|if glpyh cache is full| _drawGlyphPrepareRetry
        _drawGlyphPrepareRetry --> _flushQuads["_flushQuads\n<small>draws the current state\ninto the render target</small>"]
        _flushQuads --> _recreateInstanceBuffers["_recreateInstanceBuffers\n<small>allocates a GPU buffer\nfor our glyph instances</small>"]
        _drawGlyphPrepareRetry --> _resetGlyphAtlas["_resetGlyphAtlas\n<small>clears the glyph texture</small>"]
        _resetGlyphAtlas --> _resizeGlyphAtlas["_resizeGlyphAtlas\n<small>resizes the glyph texture if it's still small</small>"]

        _drawGlyph -.->|if it's a DECDHL glyph| _splitDoubleHeightGlyph["_splitDoubleHeightGlyph\n<small>DECDHL glyphs are split up into their\ntop/bottom halves to emulate clip rects</small>"]
    end

    foreachGlyph -.-> _drawTextOverlapSplit["_drawTextOverlapSplit\n<small>splits overly wide glyphs up into smaller chunks to support\nforeground color changes within the ligature</small>"]

    foreachRow -.->|if gridlines exist| _drawGridlineRow["_drawGridlineRow\n<small>draws underlines, etc.</small>"]
```

### `_drawSelection`

```mermaid
graph TD
    Render --> _drawSelection
```

### `_handleSettingsUpdate`

```mermaid
graph TD
    Render --> _executeCustomShader
```
