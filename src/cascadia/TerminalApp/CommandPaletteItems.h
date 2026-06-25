// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "BasePaletteItem.h"
#include "TabPaletteItem.g.h"

#include "../inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct ActionPaletteItem :
        public winrt::implements<ActionPaletteItem, IPaletteItem, winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged>,
        BasePaletteItem<ActionPaletteItem, winrt::TerminalApp::PaletteItemType::Action>
    {
        ActionPaletteItem(const Microsoft::Terminal::Settings::Model::Command& command, const winrt::hstring keyChordText) :
            _Command{ command }, _name{ command.Name() }, _keyChordText{ keyChordText }
        {
            static bool shouldShowSubtitles = [] {
                try
                {
                    const auto context{ winrt::Windows::ApplicationModel::Resources::Core::ResourceContext::GetForViewIndependentUse() };
                    const auto qualifiers{ context.QualifierValues() };
                    if (const auto language{ qualifiers.TryLookup(L"language") })
                    {
                        return !til::starts_with_insensitive_ascii(*language, L"en-");
                    }
                }
                catch (...)
                {
                    LOG_CAUGHT_EXCEPTION();
                }
                return false;
            }();

            if (shouldShowSubtitles)
            {
                const auto subtitle = _Command.LanguageNeutralName();
                if (subtitle != _name)
                {
                    _subtitle = std::move(subtitle);
                }
            }
        }

        winrt::hstring Name()
        {
            return _name;
        }

        winrt::hstring Subtitle()
        {
            return _subtitle;
        }

        winrt::hstring KeyChordText()
        {
            return _keyChordText;
        }

        winrt::hstring Icon()
        {
            return _Command.Icon().Resolved();
        }

        WINRT_PROPERTY(Microsoft::Terminal::Settings::Model::Command, Command, nullptr);

    private:
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _commandChangedRevoker;
        winrt::hstring _name;
        winrt::hstring _subtitle;
        winrt::hstring _keyChordText;
    };

    struct CommandLinePaletteItem :
        public winrt::implements<CommandLinePaletteItem, IPaletteItem, winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged>,
        BasePaletteItem<CommandLinePaletteItem, winrt::TerminalApp::PaletteItemType::CommandLine>
    {
        CommandLinePaletteItem(const winrt::hstring& commandLine) :
            _CommandLine{ commandLine } {}

        winrt::hstring Name()
        {
            return _CommandLine;
        }

        winrt::hstring Subtitle()
        {
            return {};
        }

        winrt::hstring KeyChordText()
        {
            return {};
        }

        winrt::hstring Icon()
        {
            return {};
        }

        WINRT_PROPERTY(winrt::hstring, CommandLine);
    };

    struct TabPaletteItem :
        public TabPaletteItemT<TabPaletteItem>,
        BasePaletteItem<TabPaletteItem, winrt::TerminalApp::PaletteItemType::Tab>
    {
        TabPaletteItem(const winrt::TerminalApp::Tab& tab);

        winrt::TerminalApp::Tab Tab() const noexcept
        {
            return _tab.get();
        }

        winrt::hstring Name()
        {
            if (auto tab = _tab.get())
            {
                return tab.Title();
            }
            return {};
        }

        winrt::hstring Subtitle()
        {
            return {};
        }

        winrt::hstring KeyChordText()
        {
            return {};
        }

        winrt::hstring Icon()
        {
            if (auto tab = _tab.get())
            {
                return tab.Icon();
            }
            return {};
        }

        winrt::TerminalApp::TerminalTabStatus TabStatus()
        {
            if (auto tab = _tab.get())
            {
                return tab.TabStatus();
            }
            return { nullptr };
        }

    private:
        winrt::weak_ref<winrt::TerminalApp::Tab> _tab;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabChangedRevoker;
        Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker _tabStatusChangedRevoker;
    };
}
