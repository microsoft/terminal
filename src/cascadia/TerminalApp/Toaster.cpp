// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Toaster.h"
#include "Toast.h"
#include <LibraryResources.h>
#include "Toaster.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::TerminalApp::implementation
{
    Toaster::Toaster(WUX::Controls::Panel root) :
        _root{ root }
    {
        _toasts = winrt::single_threaded_observable_vector<winrt::TerminalApp::Toast>();
    }

    void Toaster::MakeToast(const hstring& title,
                            const hstring& subtitle,
                            const WUX::FrameworkElement& target)
    {
        auto t = winrt::make_self<Toast>();
        t->Closed({ this, &Toaster::_onToastClosed });
        _toasts.Append(*t);

        if (auto tt = t->Root().try_as<MUX::Controls::TeachingTip>())
        {
            tt.Title(title);
            tt.Subtitle(subtitle);
            if (target)
            {
                tt.PreferredPlacement(MUX::Controls::TeachingTipPlacementMode::Bottom);
                tt.Target(target);
                tt.PlacementMargin(WUX::ThicknessHelper::FromUniformLength(-80));
                // tt.PlacementMargin(WUX::ThicknessHelper::FromLength(-80));
            }
            if (_root)
            {
                auto c = _root.Children();
                if (c)
                {
                    c.Append(tt);
                }
            }
            // _root.Children().Append(tt);
            t->Show();
        }
    }
    void Toaster::_onToastClosed(const IInspectable& /*sender*/,
                                 const IInspectable& /*eventArgs*/)
    {
        // TODO: find the toast in the list of toasts, and remove it.
    }
}
