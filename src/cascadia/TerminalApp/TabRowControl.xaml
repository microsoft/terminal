<!-- Copyright (c) Microsoft Corporation. All rights reserved. Licensed under
the MIT License. See LICENSE in the project root for license information. -->
<ContentPresenter
    x:Class="TerminalApp.TabRowControl"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:local="using:TerminalApp"
    xmlns:mux="using:Microsoft.UI.Xaml.Controls"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
    mc:Ignorable="d">

    <mux:TabView x:Name="TabView"
                 VerticalAlignment="Bottom"
                 HorizontalContentAlignment="Stretch"
                 IsAddTabButtonVisible="false"
                 TabWidthMode="SizeToContent"
                 CanReorderTabs="True"
                 CanDragTabs="True"
                 AllowDropTabs="True">

        <mux:TabView.TabStripFooter>
            <mux:SplitButton
                x:Name="NewTabButton"
                Click="OnNewTabButtonClick"
                VerticalAlignment="Stretch"
                HorizontalAlignment="Left"
                Content="&#xE710;"
                UseLayoutRounding="true"
                FontFamily="Segoe MDL2 Assets"
                FontWeight="SemiLight"
                FontSize="12">
                <!-- U+E710 is the fancy plus icon. -->
                <mux:SplitButton.Resources>
		    <!-- Override the SplitButton* resources to match the tab view's button's styles. -->
                    <ResourceDictionary>
                        <ResourceDictionary.ThemeDictionaries>
                            <ResourceDictionary x:Key="Light">
                                <StaticResource x:Key="SplitButtonBackground" ResourceKey="TabViewItemHeaderBackground" />
                                <StaticResource x:Key="SplitButtonForeground" ResourceKey="SystemControlBackgroundBaseMediumBrush" />
                                <StaticResource x:Key="SplitButtonBackgroundPressed" ResourceKey="TabViewItemHeaderBackgroundPressed" />
                                <StaticResource x:Key="SplitButtonForegroundPressed" ResourceKey="SystemControlBackgroundBaseMediumHighBrush" />
                                <StaticResource x:Key="SplitButtonBackgroundPointerOver" ResourceKey="TabViewItemHeaderBackgroundPointerOver" />
                                <StaticResource x:Key="SplitButtonForegroundPointerOver" ResourceKey="SystemControlBackgroundBaseMediumHighBrush" />
                            </ResourceDictionary>
                            <ResourceDictionary x:Key="Dark">
                                <StaticResource x:Key="SplitButtonBackground" ResourceKey="TabViewItemHeaderBackground" />
                                <StaticResource x:Key="SplitButtonForeground" ResourceKey="SystemControlBackgroundBaseMediumBrush" />
                                <StaticResource x:Key="SplitButtonBackgroundPressed" ResourceKey="TabViewItemHeaderBackgroundPressed" />
                                <StaticResource x:Key="SplitButtonForegroundPressed" ResourceKey="SystemControlBackgroundBaseMediumHighBrush" />
                                <StaticResource x:Key="SplitButtonBackgroundPointerOver" ResourceKey="TabViewItemHeaderBackgroundPointerOver" />
                                <StaticResource x:Key="SplitButtonForegroundPointerOver" ResourceKey="SystemControlBackgroundBaseMediumHighBrush" />
                            </ResourceDictionary>
                            <ResourceDictionary x:Key="HighContrast">
                                <StaticResource x:Key="SplitButtonBackground" ResourceKey="TabViewItemHeaderBackground" />
                                <StaticResource x:Key="SplitButtonForeground" ResourceKey="SystemControlBackgroundBaseMediumBrush" />
                                <StaticResource x:Key="SplitButtonBackgroundPressed" ResourceKey="TabViewItemHeaderBackgroundPressed" />
                                <StaticResource x:Key="SplitButtonForegroundPressed" ResourceKey="SystemControlBackgroundBaseMediumHighBrush" />
                                <StaticResource x:Key="SplitButtonBackgroundPointerOver" ResourceKey="TabViewItemHeaderBackgroundPointerOver" />
                                <StaticResource x:Key="SplitButtonForegroundPointerOver" ResourceKey="SystemControlBackgroundBaseMediumHighBrush" />
                            </ResourceDictionary>
                        </ResourceDictionary.ThemeDictionaries>
                    </ResourceDictionary>
                </mux:SplitButton.Resources>
            </mux:SplitButton>
        </mux:TabView.TabStripFooter>

    </mux:TabView>

</ContentPresenter>
