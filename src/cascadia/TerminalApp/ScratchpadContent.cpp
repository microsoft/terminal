// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ScratchpadContent.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    ScratchpadContent::ScratchpadContent()
    {
        _root = winrt::Windows::UI::Xaml::Controls::Grid{};
        // Vertical and HorizontalAlignment are Stretch by default

        auto res = Windows::UI::Xaml::Application::Current().Resources();
        auto bg = res.Lookup(winrt::box_value(L"UnfocusedBorderBrush"));
        _root.Background(bg.try_as<Media::Brush>());

        _box = winrt::Windows::UI::Xaml::Controls::TextBox{};
        _box.ContextFlyout(winrt::Microsoft::Terminal::UI::TextMenuFlyout{});
        _box.Margin({ 10, 10, 10, 10 });
        _box.AcceptsReturn(true);
        _box.TextWrapping(TextWrapping::Wrap);
        _root.Children().Append(_box);
    }

    void ScratchpadContent::UpdateSettings(const CascadiaSettings& /*settings*/)
    {
        // Nothing to do.
    }

    winrt::Windows::UI::Xaml::FrameworkElement ScratchpadContent::GetRoot()
    {
        return _root;
    }
    winrt::Windows::Foundation::Size ScratchpadContent::MinimumSize()
    {
        return { 1, 1 };
    }
    void ScratchpadContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        _box.Focus(reason);
    }
    void ScratchpadContent::Close()
    {
    }

    INewContentArgs ScratchpadContent::GetNewTerminalArgs(const BuildStartupKind /* kind */) const
    {
        return BaseContentArgs(L"scratchpad");
    }

    winrt::hstring ScratchpadContent::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xe70b" }; // QuickNote
        return winrt::hstring{ glyph };
    }

    winrt::Windows::UI::Xaml::Media::Brush ScratchpadContent::BackgroundBrush()
    {
        return _root.Background();
    }
}
