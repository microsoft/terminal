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
    // TODO! These all probably need to be weak_refs, don't they, so that the
    // contentmanager doesn't just keep every control around indefinitely.

    ControlInteractivity ContentManager::CreateCore(Microsoft::Terminal::Control::IControlSettings settings,
                                                    IControlAppearance unfocusedAppearance,
                                                    TerminalConnection::ITerminalConnection connection)
    {
        auto content = ControlInteractivity{ settings, unfocusedAppearance, connection };
        // winrt::guid g{ ::Microsoft::Console::Utils::CreateGuid() };
        _content.Insert(content.Id(), content);
        return content;
    }

    ControlInteractivity ContentManager::LookupCore(winrt::guid id)
    {
        return _content.TryLookup(id);
    }

    void ContentManager::Detach(const winrt::guid& contentGuid)
    {
        if (const auto& content{ LookupCore(contentGuid) })
        {
            content.Attached({ get_weak(), &ContentManager::_finalizeDetach });
            _recentlyDetachedContent.Insert(contentGuid, content);
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

}
