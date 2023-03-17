/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Class Name:
- ContentManager.h

Abstract:
- This is a helper class for tracking all of the terminal "content" instances of
  the Terminal. These are all the ControlInteractivity & ControlCore's of each
  of our TermControls. These are each assigned a GUID on creation, and stored in
  a map for later lookup.
- This is used to enable moving panes between windows. TermControl's are not
  thread-agile, so they cannot be reused on other threads. However, the content
  is. This helper, which exists as a singleton across all the threads in the
  Terminal app, allows each thread to create content, assign it to a
  TermControl, detach it from that control, and reattach to new controls on
  other threads.
- When you want to create a new TermControl, call CreateCore to instantiate a
  new content with a GUID for later reparenting.
- Detach can be used to temporarily remove a content from its hosted
  TermControl. After detaching, you can still use LookupCore &
  TermControl::AttachContent to re-attach to the content.
--*/
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
