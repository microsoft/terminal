// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ContentManager.g.h"

#include <inc/cppwinrt_utils.h>
namespace winrt::TerminalApp::implementation
{
    struct ContentManager : ContentManagerT<ContentManager>
    {
    public:
        ContentManager() = default;
        Microsoft::Terminal::Control::ControlInteractivity CreateCore(Microsoft::Terminal::Control::IControlSettings settings,
                                                                      Microsoft::Terminal::Control::IControlAppearance unfocusedAppearance,
                                                                      Microsoft::Terminal::TerminalConnection::ITerminalConnection connection);
        Microsoft::Terminal::Control::ControlInteractivity LookupCore(winrt::guid id);

        void Detach(const Microsoft::Terminal::Control::TermControl& control);

    private:
        Windows::Foundation::Collections::IMap<winrt::guid, Microsoft::Terminal::Control::ControlInteractivity> _content{
            winrt::multi_threaded_map<winrt::guid, Microsoft::Terminal::Control::ControlInteractivity>()
        };

        Windows::Foundation::Collections::IMap<winrt::guid, Microsoft::Terminal::Control::ControlInteractivity> _recentlyDetachedContent{
            winrt::multi_threaded_map<winrt::guid, Microsoft::Terminal::Control::ControlInteractivity>()
        };

        void _finalizeDetach(winrt::Windows::Foundation::IInspectable sender,
                             winrt::Windows::Foundation::IInspectable e);

        void _closedHandler(winrt::Windows::Foundation::IInspectable sender,
                            winrt::Windows::Foundation::IInspectable e);
    };
}
