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

        ////////////////////////////////////////////////////////////////////////
        // These members are new
        double _compositionScaleX;
        double _compositionScaleY;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // These methods are taken from TermControl
        bool InitializeTerminal(const double actualWidth,
                                const double actualHeight,
                                const double compositionScaleX,
                                const double compositionScaleY);

        void _UpdateFont(const bool initialUpdate);

        void _SendInputToConnection(const winrt::hstring& wstr);
        void _SendInputToConnection(std::wstring_view wstr);

        ////////////////////////////////////////////////////////////////////////
        // These methods are new
    };
}

namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    BASIC_FACTORY(ControlCore);
}
