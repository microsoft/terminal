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

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}
namespace winrt::TerminalApp::implementation
{
    ControlInteractivity ContentManager::CreateCore(Microsoft::Terminal::Control::IControlSettings settings,
                                                    IControlAppearance unfocusedAppearance,
                                                    TerminalConnection::ITerminalConnection connection)
    {
        auto content = ControlInteractivity{ settings, unfocusedAppearance, connection };
        content.Closed({ get_weak(), &ContentManager::_closedHandler });
        _content.insert(std::make_pair(content.Id(), content));
        return content;
    }

    ControlInteractivity ContentManager::LookupCore(uint64_t id)
    {
        return _content.at(id);
    }

    void ContentManager::Detach(const Microsoft::Terminal::Control::TermControl& control)
    {
        const auto contentId{ control.ContentId() };
        if (const auto& content{ LookupCore(contentId) })
        {
            control.Detach();
            content.Attached({ get_weak(), &ContentManager::_finalizeDetach });
            _recentlyDetachedContent.Insert(contentId, content);
        }
    }

    void ContentManager::_finalizeDetach(winrt::Windows::Foundation::IInspectable sender,
                                         winrt::Windows::Foundation::IInspectable e)
    {
        if (const auto& content{ sender.try_as<winrt::Microsoft::Terminal::Control::ControlInteractivity>() })
        {
            _recentlyDetachedContent.Remove(content.Id());
        }
    }

    void ContentManager::_closedHandler(winrt::Windows::Foundation::IInspectable sender,
                                        winrt::Windows::Foundation::IInspectable e)
    {
        if (const auto& content{ sender.try_as<winrt::Microsoft::Terminal::Control::ControlInteractivity>() })
        {
            const auto& contentId{ content.Id() };
            _content.erase(contentId);
            _recentlyDetachedContent.TryRemove(contentId);
        }
    }
}
