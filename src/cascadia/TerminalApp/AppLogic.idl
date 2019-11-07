// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

import "../TerminalPage.idl";

namespace TerminalApp
{
    enum LaunchMode
    {
        DefaultMode,
        MaximizedMode,
    };

    [default_interface] runtimeclass AppLogic
    {
        AppLogic();

        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like TermApp just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is TermApp not
        // registered?" when it definitely is.
        void Create();

        void LoadSettings();
        Windows.UI.Xaml.UIElement GetRoot();

        String Title { get; };

        Windows.Foundation.Point GetLaunchDimensions(UInt32 dpi);
        Windows.Foundation.Point GetLaunchInitialPositions(Int32 defaultInitialX, Int32 defaultInitialY);
        Windows.UI.Xaml.ElementTheme GetRequestedTheme();
        LaunchMode GetLaunchMode();
        Boolean GetShowTabsInTitlebar();
        void TitlebarClicked();
        void WindowCloseButtonClicked();

        event Windows.Foundation.TypedEventHandler<Object, Windows.UI.Xaml.UIElement> SetTitleBarContent;
        event Windows.Foundation.TypedEventHandler<Object, String> TitleChanged;
        event Windows.Foundation.TypedEventHandler<Object, LastTabClosedEventArgs> LastTabClosed;
        event Windows.Foundation.TypedEventHandler<Object, Windows.UI.Xaml.ElementTheme> RequestedThemeChanged;
        event Windows.Foundation.TypedEventHandler<Object, ToggleFullscreenEventArgs> ToggleFullscreen;
    }
}
