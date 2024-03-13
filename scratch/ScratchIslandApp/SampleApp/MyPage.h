// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MyPage.g.h"
#include "../../../src/cascadia/inc/cppwinrt_utils.h"

namespace winrt::SampleApp::implementation
{
    struct MyPage : MyPageT<MyPage>
    {
    public:
        MyPage();

        void Create();

        hstring Title();

        void _handleRunCommandRequest(const SampleApp::CodeBlock& sender,
                                      const SampleApp::RequestRunCommandsArgs& control);

    private:
        friend struct MyPageT<MyPage>; // for Xaml to bind events

        void _createNotebook();

        void _clearOldNotebook();
        void _loadMarkdown();

        winrt::Microsoft::Terminal::Control::Notebook _notebook{ nullptr };
        winrt::hstring _filePath{};

        void _loadTapped(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);
        void _reloadTapped(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);

        void _newBlockHandler(const winrt::Microsoft::Terminal::Control::Notebook& sender,
                              const winrt::Microsoft::Terminal::Control::NotebookBlock& control);
        void _addControl(const winrt::Microsoft::Terminal::Control::TermControl& control);
        void _scrollToElement(const Windows::UI::Xaml::UIElement& element,
                              bool isVerticalScrolling = true,
                              bool smoothScrolling = true);

        winrt::fire_and_forget _stupid(Windows::UI::Xaml::UIElement elem);
    };
}

namespace winrt::SampleApp::factory_implementation
{
    BASIC_FACTORY(MyPage);
}
