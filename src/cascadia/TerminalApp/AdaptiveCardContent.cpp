// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AdaptiveCardContent.h"
#include "AdaptiveCardContent.g.cpp"

using namespace winrt::AdaptiveCards::Rendering::Uwp;
// using namespace winrt::AdaptiveCards::ObjectModel::Uwp;

namespace winrt::TerminalApp::implementation
{
    AdaptiveCardContent::AdaptiveCardContent() {}

    bool AdaptiveCardContent::InitFromString(const winrt::hstring& jsonString)
    {
        try
        {
            AdaptiveCardRenderer renderer{};
            // TODO!: double check if this VVV throws or just logs
            auto card{ winrt::AdaptiveCards::Rendering::Uwp::AdaptiveCard::FromJsonString(jsonString) };
            if (!card.AdaptiveCard())
            {
                return false;
            }
            _renderedCard = renderer.RenderAdaptiveCard(card.AdaptiveCard());
            _root = _renderedCard.FrameworkElement();

            _renderedCard.Action([](const auto& /*s*/, const AdaptiveActionEventArgs& args) {
                auto a = 0;
                a++;
                a;

                if (const auto& openUrlAction{ args.Action().try_as<AdaptiveOpenUrlAction>() })
                {
                    // await Launcher.LaunchUriAsync(openUrlAction.Url);
                }
                else if (const auto& showCardAction{ args.Action().try_as<AdaptiveShowCardAction>() })
                {
                    // This is only fired if, in HostConfig, you set the ShowCard
                    // ActionMode to Popup. Otherwise, the renderer will
                    // automatically display the card inline without firing this
                    // event.
                }
                else if (const auto& submitAction{ args.Action().try_as<AdaptiveSubmitAction>() })
                {
                    // Get the data and inputs
                    const auto data{ submitAction.DataJson().Stringify() };
                    const auto inputs{ args.Inputs().AsJson().Stringify() };
                    // Process them as desired
                    data;
                    inputs;
                    auto a = 0;
                    a++;
                    a;
                }
            });
            return true;
        }
        CATCH_LOG();
        return false;
    }

    winrt::Windows::UI::Xaml::FrameworkElement AdaptiveCardContent::GetRoot()
    {
        return _root;
    }
    winrt::Windows::Foundation::Size AdaptiveCardContent::MinSize()
    {
        return Windows::Foundation::Size{ 1, 1 };
    }
    void AdaptiveCardContent::Focus()
    {
        // TODO!?
        // UIElement.GetChildrenInTabFocusOrder maybe?
    }
    void AdaptiveCardContent::Close()
    {
    }
}
