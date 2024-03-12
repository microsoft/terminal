// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "CodeBlock.g.h"
#include "RequestRunCommandsArgs.g.h"
#include "../../../src/cascadia/inc/cppwinrt_utils.h"
#include <til/hash.h>

namespace winrt::SampleApp::implementation
{
    struct CodeBlock : CodeBlockT<CodeBlock>
    {
        CodeBlock(const winrt::hstring& initialCommandlines);

        til::property<winrt::hstring> Commandlines;

        winrt::Microsoft::Terminal::Control::NotebookBlock OutputBlock();
        void OutputBlock(const winrt::Microsoft::Terminal::Control::NotebookBlock& block);

        til::property_changed_event PropertyChanged;
        til::typed_event<SampleApp::CodeBlock, RequestRunCommandsArgs> RequestRunCommands;

    private:
        friend struct CodeBlockT<CodeBlock>; // for Xaml to bind events

        winrt::hstring _providedCommandlines{};
        winrt::Microsoft::Terminal::Control::NotebookBlock _block{ nullptr };

        void _playPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);
    };

    struct RequestRunCommandsArgs : RequestRunCommandsArgsT<RequestRunCommandsArgs>
    {
        RequestRunCommandsArgs(const winrt::hstring& commandlines) :
            Commandlines{ commandlines } {};

        til::property<winrt::hstring> Commandlines;
    };
}

namespace winrt::SampleApp::factory_implementation
{
    BASIC_FACTORY(CodeBlock);
}
