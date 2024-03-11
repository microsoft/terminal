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
            auto core{ _blocks.rbegin()->core };
            auto* renderData{ _blocks.rbegin()->renderData.get() };

            renderData->LockConsole();
            auto blockRenderViewport = renderData->GetViewport();
            renderData->UnlockConsole();

            // auto size = TermControl::GetProposedDimensions(_core.Settings(), dpi, minSize);
            auto pixels = core->ViewInPixels(blockRenderViewport.ToExclusive());

            const auto scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
            // auto viewDips = pixels * scaleFactor;

            const auto controlHeightDips = active.ActualHeight();
            const auto viewHeightDips = pixels.height() * scaleFactor;

            auto r = pixels;

            // til::rect r{ 0, 0, 0, -128 };
            // const auto p = r.to_core_padding();
            auto t{ WUX::ThicknessHelper::FromLengths(0 /*r.left*/,
                                                      0 /*r.top*/,
                                                      0 /*r.right*/,
                                                      -(controlHeightDips - viewHeightDips)/*r.bottom*/) };
            active.Margin(t);
            // WUX::Media::TranslateTransform transform{};
            // transform.X(-15);
            // transform.Y(-64);

            // WUX::Media::RectangleGeometry clipRect{};
            // clipRect.Rect(r.to_winrt_rect());
            // clipRect.Transform(transform);

            // active.Clip(clipRect);

            renderData->SetBottom(start - 1);
            active.Connection(nullptr);
        }

        NotebookBlock newBlock{
            .renderData = std::make_unique<::Microsoft::Terminal::Core::BlockRenderData>(*_terminal, start),
            .core = nullptr,
            .control = nullptr
        };

        ControlData data{
            .terminal = _terminal,
            .renderData = newBlock.renderData.get(),
            .connection = _connection,
        };

        newBlock.core = winrt::make_self<implementation::ControlCore>(_settings, _unfocusedAppearance, data);
        auto interactivityOne = winrt::make_self<implementation::ControlInteractivity>(_settings, _unfocusedAppearance, newBlock.core);
        newBlock.control = winrt::make<implementation::TermControl>(*interactivityOne);

        _blocks.push_back(std::move(newBlock));

        // _controls.Append(newBlock.control);

        // _active = controlOne;

        NewBlock.raise(*this, ActiveControl());
    }

}
