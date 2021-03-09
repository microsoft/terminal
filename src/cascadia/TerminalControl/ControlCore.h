// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ControlCore.g.h"
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../buffer/out/search.h"
#include "cppwinrt_utils.h"
#include "ThrottledFunc.h"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct ControlCore : ControlCoreT<ControlCore>
    {
    public:
        ControlCore(IControlSettings settings, TerminalConnection::ITerminalConnection connection);

        ////////////////////////////////////////////////////////////////////////
        // These members are taken from TermControl

        bool _initializedTerminal;

        TerminalConnection::ITerminalConnection _connection;
        event_token _connectionOutputEventToken;
        TerminalConnection::ITerminalConnection::StateChanged_revoker _connectionStateChangedRevoker;

        std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;
        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;
        std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine;

        IControlSettings _settings; // ? Might be able to get away with only retrieving pieces

        FontInfoDesired _desiredFont;
        FontInfo _actualFont;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        bool _isReadOnly{ false }; // ?

        std::optional<COORD> _lastHoveredCell;
        // Track the last hyperlink ID we hovered over
        uint16_t _lastHoveredId;

        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval;

        ////////////////////////////////////////////////////////////////////////
        // These members are new
        double _panelWidth;
        double _panelHeight;
        double _compositionScaleX;
        double _compositionScaleY;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // These methods are taken from TermControl
        bool InitializeTerminal(const double actualWidth,
                                const double actualHeight,
                                const double compositionScaleX,
                                const double compositionScaleY);

        void _SetFontSize(int fontSize);
        void _UpdateFont(const bool initialUpdate = false);
        void AdjustFontSize(int fontSizeDelta);
        void ResetFontSize();
        void _RefreshSizeUnderLock();
        void _DoResizeUnderLock(const double newWidth,
                                const double newHeight);

        void _SendInputToConnection(const winrt::hstring& wstr);
        void _SendInputToConnection(std::wstring_view wstr);

        void SendInput(const winrt::hstring& wstr);
        void ToggleShaderEffects();
        void _UpdateHoveredCell(const std::optional<COORD>& terminalPosition);

        void _SetEndSelectionPointAtCursor(winrt::Windows::Foundation::Point const& cursorPosition);

        ////////////////////////////////////////////////////////////////////////
        // These methods are new
        void UpdateSettings(const IControlSettings& settings);

        void SizeChanged(const double width, const double height);
        void ScaleChanged(const double scaleX, const double scaleY);

        void _raiseHoveredHyperlinkChanged();
        winrt::hstring GetHoveredUriText();

        void PasteText(const winrt::hstring& hstr);

        TYPED_EVENT(HoveredHyperlinkChanged, IInspectable, IInspectable);
    };
}

namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    BASIC_FACTORY(ControlCore);
}
