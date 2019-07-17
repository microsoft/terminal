<?xml version="1.0" encoding="utf-8"?>
<Package xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities" xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10" xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10" IgnorableNamespaces="uap">

  <!-- This file is used as the Appxmanifest for tests that _need_ to run in a
  packaged environment. It will be copied to the test's OutDir as part of the
  PostBuid step. It's highly similar to the "PackagedCwaFullTrust" manifest that
  TAEF ships with, with the following modifications:

  1. All of our winrt types are included in this manifest, including types from
     MUX.dll. Should this list of types ever change, we'll need to manually
     update this file. The easiest way of doing that is deploying the app from
     VS, then copying the Extensions from the Appxmandifest.xml that's generated
     under `src/cascadia/CascadiaPackage/bin/$(platform)/$(configuration)/appx`.

  2. We also _NEED_ the two vclibs listed under the `PackageDependency` block.

  If your test fails for whatever reason, it's likely possible you're testing a
  type that's _not_ included in this file for some reason. So, here be dragons. -->

  <Identity Name="TerminalApp.Unit.Tests.Package"
          ProcessorArchitecture="neutral"
          Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US"
          Version="1.0.0.0"
          ResourceId="en-us" />
  <Properties>
    <DisplayName>TerminalApp.Unit.Tests.Package Host Process</DisplayName>
    <PublisherDisplayName>Microsoft Corp.</PublisherDisplayName>
    <Logo>taef.png</Logo>
    <Description>TAEF Packaged Cwa FullTrust Application Host Process</Description>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Universal" MinVersion="10.0.18362.0" MaxVersionTested="10.0.18362.0" />
    <PackageDependency Name="Microsoft.VCLibs.140.00.Debug" MinVersion="14.0.27023.1" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" />
    <PackageDependency Name="Microsoft.VCLibs.140.00.Debug.UWPDesktop" MinVersion="14.0.27027.1" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" />
  </Dependencies>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Applications>
    <Application Id="TE.ProcessHost" Executable="TE.ProcessHost.exe" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements DisplayName="TAEF Packaged Cwa FullTrust Application Host Process" Square150x150Logo="taef.png" Square44x44Logo="taef.png" Description="TAEF Packaged Cwa Application Host Process" BackgroundColor="#222222">
        <uap:SplashScreen Image="taef.png" />
      </uap:VisualElements>
    </Application>
  </Applications>
  <Capabilities>
    <rescap:Capability Name="runFullTrust"/>
  </Capabilities>

  <Extensions>
    <Extension Category="windows.activatableClass.inProcessServer">
      <InProcessServer>
        <Path>TerminalSettings.dll</Path>
        <ActivatableClass ActivatableClassId="Microsoft.Terminal.Settings.KeyChord" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.Terminal.Settings.TerminalSettings" ThreadingModel="both" />
      </InProcessServer>
    </Extension>
    <Extension Category="windows.activatableClass.inProcessServer">
      <InProcessServer>
        <Path>TerminalApp.dll</Path>
        <ActivatableClass ActivatableClassId="Microsoft.Terminal.TerminalApp.AppKeyBindings" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.Terminal.TerminalApp.TermApp" ThreadingModel="both" />
      </InProcessServer>
    </Extension>
    <Extension Category="windows.activatableClass.inProcessServer">
      <InProcessServer>
        <Path>TerminalConnection.dll</Path>
        <ActivatableClass ActivatableClassId="Microsoft.Terminal.TerminalConnection.EchoConnection" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.Terminal.TerminalConnection.ConhostConnection" ThreadingModel="both" />
      </InProcessServer>
    </Extension>
    <Extension Category="windows.activatableClass.inProcessServer">
      <InProcessServer>
        <Path>TerminalControl.dll</Path>
        <ActivatableClass ActivatableClassId="Microsoft.Terminal.TerminalControl.TermControl" ThreadingModel="both" />
      </InProcessServer>
    </Extension>
    <Extension Category="windows.activatableClass.inProcessServer">
      <InProcessServer>
        <Path>Microsoft.UI.Xaml.dll</Path>
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.NavigationViewItemAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.RepeaterAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.MenuBarItemAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.RatingControlAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.ColorSpectrumAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.TreeViewItemAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.AnimatedVisualPlayerAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.MenuBarAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.ScrollerAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.TeachingTipAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.DropDownButtonAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.PersonPictureAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.ToggleSplitButtonAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.ColorPickerSliderAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.TreeViewListAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.SplitButtonAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Automation.Peers.RadioButtonsListViewItemAutomationPeer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.XamlTypeInfo.XamlControlsXamlMetaDataProvider" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RecyclingElementFactory" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.CommandBarFlyout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ScrollAnchorProvider" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TreeViewItemTemplateSettings" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.UniformGridLayout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.IndexPath" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RadioMenuFlyoutItem" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.DropDownButton" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ItemsSourceView" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TreeViewList" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.VirtualizingLayoutContext" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RadioButtons" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.IconSource" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NavigationViewTemplateSettings" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ElementAnimator" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.UniformGridLayoutState" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RatingControl" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.SelectionModel" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ItemsRepeater" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ScrollViewer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ElementFactoryRecycleArgs" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.FlowLayoutState" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ElementFactoryGetArgs" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RatingItemImageInfo" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.BitmapIconSource" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.LayoutContext" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TextCommandBarFlyout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NavigationViewItemSeparator" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TeachingTipTemplateSettings" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TwoPaneView" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.MenuBarItemFlyout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.XamlControlsResources" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.StackLayout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NavigationViewItemHeader" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RefreshVisualizer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.PathIconSource" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NonVirtualizingLayout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.AnimatedVisualPlayer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RatingItemFontInfo" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.PersonPicture" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TabView" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.SwipeControl" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.MenuBarItem" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.VirtualizingLayout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.SymbolIconSource" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NavigationViewItemInvokedEventArgs" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Layout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RefreshContainer" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ZoomOptions" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ParallaxView" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NavigationViewItemBase" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TreeViewNode" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.SplitButton" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.MenuBar" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.FontIconSource" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ElementFactory" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.FlowLayout" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ScrollOptions" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TabViewItem" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NavigationViewList" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TreeViewItem" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ScrollerSnapPointBase" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.NavigationViewItemPresenter" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.RadioButtonsListViewItem" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ScrollControllerScrollFromRequestedEventArgs" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ColorSpectrum" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ColorPickerSlider" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.RadioButtonsListView" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ScrollerSnapPointIrregular" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ScrollControllerScrollToRequestedEventArgs" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.Scroller" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ScrollControllerInteractionRequestedEventArgs" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ScrollControllerScrollByRequestedEventArgs" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.CommandBarFlyoutCommandBar" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.Primitives.ScrollerSnapPointRegular" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TreeView" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ColorPicker" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RecyclePool" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RevealListViewItemPresenter" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.ToggleSplitButton" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.RatingItemInfo" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.TeachingTip" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.SwipeItem" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NavigationViewItem" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.LayoutPanel" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NonVirtualizingLayoutContext" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.NavigationView" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.SwipeItems" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Controls.StackLayoutState" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Media.RevealBackgroundBrush" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Media.RevealBorderBrush" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Media.RevealBrush" ThreadingModel="both" />
        <ActivatableClass ActivatableClassId="Microsoft.UI.Xaml.Media.AcrylicBrush" ThreadingModel="both" />
      </InProcessServer>
    </Extension>
  </Extensions>

</Package>
