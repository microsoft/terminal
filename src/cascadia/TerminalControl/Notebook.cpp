// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Notebook.h"
#include "TermControl.h"
#include "Notebook.g.cpp"
using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Console::VirtualTerminal;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Storage::Streams;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::Microsoft::Terminal::Control::implementation
{
    Notebook::Notebook(Control::IControlSettings settings,
                       Control::IControlAppearance unfocusedAppearance,
                       TerminalConnection::ITerminalConnection connection) :
        _settings{ settings },
        _unfocusedAppearance{ unfocusedAppearance },
        _connection{ connection }
    {
        _terminal = std::make_shared<::Microsoft::Terminal::Core::Terminal>();
        _renderData = std::make_unique<::Microsoft::Terminal::Core::BlockRenderData>(*_terminal);

        _fork();
        _fork();
        _fork();
        // ControlData data{
        //     .terminal = _terminal,
        //     .renderData = _renderData.get(),
        //     .connection = _connection,
        // };

        // auto coreOne = winrt::make_self<implementation::ControlCore>(_settings, _unfocusedAppearance, data);
        // auto coreTwo = winrt::make_self<implementation::ControlCore>(_settings, _unfocusedAppearance, data);

        // auto interactivityOne = winrt::make_self<implementation::ControlInteractivity>(settings, _unfocusedAppearance, coreOne);
        // auto interactivityTwo = winrt::make_self<implementation::ControlInteractivity>(settings, _unfocusedAppearance, coreTwo);

        // auto controlOne = winrt::make<implementation::TermControl>(*interactivityOne);
        // auto controlTwo = winrt::make<implementation::TermControl>(*interactivityTwo);
        // _controls.Append(controlOne);
        // _controls.Append(controlTwo);

        // // controlOne.Connection(nullptr);
        // _active = _controls.GetAt(_controls.Size() - 1);
    }

    Windows::Foundation::Collections::IVector<Microsoft::Terminal::Control::TermControl> Notebook::Controls() const
    {
        return _controls;
    }
    Control::TermControl Notebook::ActiveControl() const
    {
        return _active;
    }

    void Notebook::_fork()
    {
        if (_active)
        {
            _active.Connection(nullptr);
        }

        ControlData data{
            .terminal = _terminal,
            .renderData = _renderData.get(),
            .connection = _connection,
        };

        auto coreOne = winrt::make_self<implementation::ControlCore>(_settings, _unfocusedAppearance, data);
        auto interactivityOne = winrt::make_self<implementation::ControlInteractivity>(_settings, _unfocusedAppearance, coreOne);
        auto controlOne = winrt::make<implementation::TermControl>(*interactivityOne);
        _controls.Append(controlOne);

        _active = controlOne;
    }

}
