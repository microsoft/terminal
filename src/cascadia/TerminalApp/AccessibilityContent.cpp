// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AccessibilityContent.h"
#include "PaneArgs.h"
#include "AccessibilityContent.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    AccessibilityContent::AccessibilityContent()
    {
        _root = winrt::Windows::UI::Xaml::Controls::Grid{};
        _root.VerticalAlignment(VerticalAlignment::Stretch);
        _root.HorizontalAlignment(HorizontalAlignment::Stretch);

        auto res = Windows::UI::Xaml::Application::Current().Resources();
        auto bg = res.Lookup(winrt::box_value(L"UnfocusedBorderBrush"));
        _root.Background(bg.try_as<Media::Brush>());

        _box = winrt::Windows::UI::Xaml::Controls::TextBox{};
        _box.Margin({ 10, 10, 10, 10 });
        _box.AcceptsReturn(true);
        _box.TextWrapping(TextWrapping::Wrap);
        _root.Children().Append(_box);
    }

    void AccessibilityContent::UpdateSettings(const CascadiaSettings& /*settings*/)
    {
        // Nothing to do.
    }

    winrt::Windows::UI::Xaml::FrameworkElement AccessibilityContent::GetRoot()
    {
        return _root;
    }
    winrt::Windows::Foundation::Size AccessibilityContent::MinSize()
    {
        return { 1, 1 };
    }
    void AccessibilityContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        _box.Focus(reason);
    }
    void AccessibilityContent::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    NewTerminalArgs AccessibilityContent::GetNewTerminalArgs(const bool /* asContent */) const
    {
        return nullptr;
    }

    winrt::hstring AccessibilityContent::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xe70b" }; // QuickNote
        return winrt::hstring{ glyph };
    }

    winrt::Windows::UI::Xaml::Media::Brush AccessibilityContent::BackgroundBrush()
    {
        return _root.Background();
    }

    void AccessibilityContent::Write(winrt::hstring content)
    {
        _box.Text(content);
    }

}
