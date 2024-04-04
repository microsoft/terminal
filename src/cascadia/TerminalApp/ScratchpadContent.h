// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "winrt/TerminalApp.h"
#include "../TerminalSettingsModel/HashUtils.h"

namespace winrt::TerminalApp::implementation
{
    class ScratchpadContent : public winrt::implements<ScratchpadContent, IPaneContent>
    {
    public:
        ScratchpadContent();

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::Foundation::Size MinimumSize();

        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs GetNewTerminalArgs(BuildStartupKind kind) const;

        winrt::hstring Title() { return L"Scratchpad"; }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        til::typed_event<> ConnectionStateChanged;
        til::typed_event<IPaneContent> CloseRequested;
        til::typed_event<IPaneContent, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<IPaneContent> TitleChanged;
        til::typed_event<IPaneContent> TabColorChanged;
        til::typed_event<IPaneContent> TaskbarProgressChanged;
        til::typed_event<IPaneContent> ReadOnlyChanged;
        til::typed_event<IPaneContent> FocusRequested;

    private:
        winrt::Windows::UI::Xaml::Controls::Grid _root{ nullptr };
        winrt::Windows::UI::Xaml::Controls::TextBox _box{ nullptr };
    };

    struct BaseContentArgs : public winrt::implements<BaseContentArgs, winrt::Microsoft::Terminal::Settings::Model::INewContentArgs>
    {
        BaseContentArgs(winrt::hstring type) :
            Type{ type } {}
        BaseContentArgs() :
            BaseContentArgs(L"") {}
        til::property<winrt::hstring> Type;

        static constexpr std::string_view TypeKey{ "type" };

    public:
        bool Equals(winrt::Microsoft::Terminal::Settings::Model::INewContentArgs other) const
        {
            return other.Type() == Type();
        }
        size_t Hash() const
        {
            til::hasher h;
            Hash(h);
            return h.finalize();
        }
        void Hash(til::hasher& h) const
        {
            h.write(Type());
        }
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs Copy() const
        {
            auto copy{ winrt::make_self<BaseContentArgs>() };
            copy->Type(Type());
            return *copy;
        }
        winrt::hstring GenerateName() const
        {
            return winrt::hstring{ L"type: " } + Type();
        }
        // static Json::Value ToJson(const winrt::com_ptr<BaseContentArgs> args)
        // {
        //     if (!val)
        //     {
        //         return {};
        //     }
        //     // auto args{ get_self<BaseContentArgs>(val) };
        //     Json::Value json{ Json::ValueType::objectValue };
        //     JsonUtils::SetValueForKey(json, TypeKey, args.Type());
        //     return json;
        // }
    };

}
