// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ContentManager.h"
#include "ContentManager.g.cpp"

#include <wil/token_helpers.h>

#include "../../types/inc/utils.hpp"

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    ControlInteractivity ContentManager::CreateCore(const Microsoft::Terminal::Control::IControlSettings& settings,
                                                    const IControlAppearance& unfocusedAppearance,
                                                    const TerminalConnection::ITerminalConnection& connection)
    {
        ControlInteractivity content{ settings, unfocusedAppearance, connection };
        content.Closed({ get_weak(), &ContentManager::_closedHandler });

        _content.emplace(content.Id(), content);

        return content;
    }

    ControlInteractivity ContentManager::TryLookupCore(uint64_t id)
    {
        const auto it = _content.find(id);
        return it != _content.end() ? it->second : ControlInteractivity{ nullptr };
    }

    void ContentManager::Detach(const Microsoft::Terminal::Control::TermControl& control)
    {
        const auto contentId{ control.ContentId() };
        if (const auto& content{ LookupCore(contentId) })
        {
            control.Detach();
            content.Attached({ get_weak(), &ContentManager::_finalizeDetach });
            _recentlyDetachedContent.emplace(contentId, content);
        }
    }

    void ContentManager::_finalizeDetach(winrt::Windows::Foundation::IInspectable sender,
                                         winrt::Windows::Foundation::IInspectable e)
    {
        if (const auto& content{ sender.try_as<winrt::Microsoft::Terminal::Control::ControlInteractivity>() })
        {
            _recentlyDetachedContent.erase(content.Id());
        }
    }

    void ContentManager::_closedHandler(winrt::Windows::Foundation::IInspectable sender,
                                        winrt::Windows::Foundation::IInspectable e)
    {
        if (const auto& content{ sender.try_as<winrt::Microsoft::Terminal::Control::ControlInteractivity>() })
        {
            const auto& contentId{ content.Id() };
            _content.erase(contentId);
            _recentlyDetachedContent.erase(contentId);
        }
    }
}
