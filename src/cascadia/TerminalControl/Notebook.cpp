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
        // _renderData = std::make_unique<::Microsoft::Terminal::Core::BlockRenderData>(*_terminal);

        _terminal->NewPrompt([this](const auto& mark) {
            if (_gotFirstMark)
            {
                _fork(mark.start.y);
            }
            else
            {
                _gotFirstMark = true;
            }
        });

        _fork(0);
        // _fork();
        // _fork();
    }

    Windows::Foundation::Collections::IVector<Microsoft::Terminal::Control::TermControl> Notebook::Controls() const
    {
        return nullptr;
    }
    Control::TermControl Notebook::ActiveControl() const
    {
        if (_blocks.empty())
        {
            return nullptr;
        }
        return _blocks.rbegin()->control;
        // return _active;
    }

    winrt::fire_and_forget Notebook::_fork(const til::CoordType start)
    {
        auto active = ActiveControl();

        if (active && !active.Dispatcher().HasThreadAccess())
        {
            co_await wil::resume_foreground(active.Dispatcher());
        }

        active = ActiveControl();
        if (active)
        {
            til::rect r{ 0, 0, 0, -128 };
            // const auto p = r.to_core_padding();
            auto t{ WUX::ThicknessHelper::FromLengths(r.left,
                                                      r.top,
                                                      r.right,
                                                      r.bottom) };
            active.Margin(t);
            // WUX::Media::TranslateTransform transform{};
            // transform.X(-15);
            // transform.Y(-64);

            // WUX::Media::RectangleGeometry clipRect{};
            // clipRect.Rect(r.to_winrt_rect());
            // clipRect.Transform(transform);

            // active.Clip(clipRect);

            _blocks.rbegin()->renderData->SetBottom(start - 1);
            active.Connection(nullptr);
        }

        NotebookBlock newBlock{
            .renderData = std::make_unique<::Microsoft::Terminal::Core::BlockRenderData>(*_terminal, start),
            .control = nullptr
        };

        ControlData data{
            .terminal = _terminal,
            .renderData = newBlock.renderData.get(),
            .connection = _connection,
        };

        auto coreOne = winrt::make_self<implementation::ControlCore>(_settings, _unfocusedAppearance, data);
        auto interactivityOne = winrt::make_self<implementation::ControlInteractivity>(_settings, _unfocusedAppearance, coreOne);
        newBlock.control = winrt::make<implementation::TermControl>(*interactivityOne);

        _blocks.push_back(std::move(newBlock));

        // _controls.Append(newBlock.control);

        // _active = controlOne;

        NewBlock.raise(*this, ActiveControl());
    }

}
