<!-- Copyright (c) Microsoft Corporation. All rights reserved. Licensed under
the MIT License. See LICENSE in the project root for license information. -->
<StackPanel
    x:Class="TerminalApp.MinMaxCloseControl"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:local="using:TerminalApp"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
    mc:Ignorable="d"
    Background="Transparent"
    HorizontalAlignment="Left"
    VerticalAlignment="Top"
    Orientation="Horizontal"
    d:DesignHeight="36"
    d:DesignWidth="400">

    <StackPanel.Resources>
        <ResourceDictionary>
            <ResourceDictionary.ThemeDictionaries>
                <ResourceDictionary x:Key="Light">
                    <x:Double x:Key="CaptionButtonStrokeWidth">1.0</x:Double>
                    <StaticResource x:Key="CaptionButtonBackgroundPointerOver" ResourceKey="SystemControlBackgroundBaseLowBrush"/>
                    <StaticResource x:Key="CaptionButtonBackgroundPressed" ResourceKey="SystemControlBackgroundBaseMediumLowBrush"/>
                    <StaticResource x:Key="CaptionButtonStroke" ResourceKey="SystemControlForegroundBaseHighBrush"/>
                    <StaticResource x:Key="CaptionButtonStrokePointerOver" ResourceKey="SystemControlForegroundBaseHighBrush"/>
                    <StaticResource x:Key="CaptionButtonStrokePressed" ResourceKey="SystemControlForegroundBaseHighBrush"/>
                    <SolidColorBrush x:Key="CaptionButtonBackground" Color="Transparent" />
                    <SolidColorBrush x:Key="CloseButtonBackgroundPointerOver" Color="Red"/>
                    <SolidColorBrush x:Key="CloseButtonStrokePointerOver" Color="White"/>
                    <SolidColorBrush x:Key="CloseButtonBackgroundPressed" Color="#f1707a"/>
                    <SolidColorBrush x:Key="CloseButtonStrokePressed" Color="Black"/>
                </ResourceDictionary>
                <ResourceDictionary x:Key="Dark">
                    <x:Double x:Key="CaptionButtonStrokeWidth">1.0</x:Double>
                    <StaticResource x:Key="CaptionButtonBackgroundPointerOver" ResourceKey="SystemControlBackgroundBaseLowBrush"/>
                    <StaticResource x:Key="CaptionButtonBackgroundPressed" ResourceKey="SystemControlBackgroundBaseMediumLowBrush"/>
                    <StaticResource x:Key="CaptionButtonStroke" ResourceKey="SystemControlForegroundBaseHighBrush"/>
                    <StaticResource x:Key="CaptionButtonStrokePointerOver" ResourceKey="SystemControlForegroundBaseHighBrush"/>
                    <StaticResource x:Key="CaptionButtonStrokePressed" ResourceKey="SystemControlForegroundBaseHighBrush"/>
                    <SolidColorBrush x:Key="CaptionButtonBackground" Color="Transparent" />
                    <SolidColorBrush x:Key="CloseButtonBackgroundPointerOver" Color="Red"/>
                    <SolidColorBrush x:Key="CloseButtonStrokePointerOver" Color="White"/>
                    <SolidColorBrush x:Key="CloseButtonBackgroundPressed" Color="#f1707a"/>
                    <SolidColorBrush x:Key="CloseButtonStrokePressed" Color="Black"/>
                </ResourceDictionary>
                <ResourceDictionary x:Key="HighContrast">
                    <x:Double x:Key="CaptionButtonStrokeWidth">3.0</x:Double>
                    <SolidColorBrush x:Key="CaptionButtonBackground" Color="{ThemeResource SystemColorButtonFaceColor}"/>
                    <SolidColorBrush x:Key="CaptionButtonBackgroundPointerOver" Color="{ThemeResource SystemColorHighlightColor}"/>
                    <SolidColorBrush x:Key="CaptionButtonBackgroundPressed" Color="{ThemeResource SystemColorHighlightColor}"/>
                    <SolidColorBrush x:Key="CaptionButtonStroke" Color="{ThemeResource SystemColorButtonTextColor}"/>
                    <SolidColorBrush x:Key="CaptionButtonStrokePointerOver" Color="{ThemeResource SystemColorHighlightTextColor}"/>
                    <SolidColorBrush x:Key="CaptionButtonStrokePressed" Color="{ThemeResource SystemColorHighlightTextColor}"/>
                    <SolidColorBrush x:Key="CloseButtonBackgroundPointerOver" Color="{ThemeResource SystemColorHighlightColor}"/>
                    <SolidColorBrush x:Key="CloseButtonStrokePointerOver" Color="{ThemeResource SystemColorHighlightTextColor}"/>
                    <SolidColorBrush x:Key="CloseButtonBackgroundPressed" Color="{ThemeResource SystemColorHighlightColor}"/>
                    <SolidColorBrush x:Key="CloseButtonStrokePressed" Color="{ThemeResource SystemColorHighlightTextColor}"/>
                </ResourceDictionary>
            </ResourceDictionary.ThemeDictionaries>

            <x:String x:Key="CaptionButtonPath"></x:String>
            <x:String x:Key="CaptionButtonPathWindowMaximized"></x:String>

            <Style x:Key="CaptionButton" TargetType="Button">
                <Setter Property="BorderThickness" Value="0"/>
                <Setter Property="Background" Value="{ThemeResource CaptionButtonBackground}" />
                <Setter Property="IsTabStop" Value="False" />
                <Setter Property="Template">
                    <Setter.Value>
                        <ControlTemplate TargetType="Button">
                            <Border x:Name="ButtonBaseElement"
                                Background="{TemplateBinding Background}"
                                BackgroundSizing="{TemplateBinding BackgroundSizing}"
                                BorderBrush="{TemplateBinding BorderBrush}"
                                BorderThickness="{TemplateBinding BorderThickness}"
                                CornerRadius="{TemplateBinding CornerRadius}"
                                Padding="{TemplateBinding Padding}"
                                AutomationProperties.AccessibilityView="Raw">

                                <VisualStateManager.VisualStateGroups>
                                    <VisualStateGroup x:Name="CommonStates">
                                        <VisualState x:Name="Normal" />

                                        <VisualState x:Name="PointerOver">
                                            <VisualState.Setters>
                                                <Setter Target="ButtonBaseElement.Background" Value="{ThemeResource CaptionButtonBackgroundPointerOver}" />
                                                <Setter Target="Path.Stroke" Value="{ThemeResource CaptionButtonStrokePointerOver}" />
                                            </VisualState.Setters>
                                        </VisualState>

                                        <VisualState x:Name="Pressed">
                                            <VisualState.Setters>
                                                <Setter Target="ButtonBaseElement.Background" Value="{ThemeResource CaptionButtonBackgroundPressed}" />
                                                <Setter Target="Path.Stroke" Value="{ThemeResource CaptionButtonStrokePressed}" />
                                            </VisualState.Setters>
                                        </VisualState>

                                        <VisualState x:Name="Disabled" />
                                    </VisualStateGroup>

                                    <VisualStateGroup x:Name="MinMaxStates">
                                        <VisualState x:Name="WindowStateNormal" />

                                        <VisualState x:Name="WindowStateMaximized">
                                            <VisualState.Setters>
                                                <Setter Target="Path.Data" Value="{ThemeResource CaptionButtonPathWindowMaximized}" />
                                            </VisualState.Setters>
                                        </VisualState>
                                    </VisualStateGroup>

                                </VisualStateManager.VisualStateGroups>

                                <Path
                                    x:Name="Path"
                                    StrokeThickness="{ThemeResource CaptionButtonStrokeWidth}"
                                    Stroke="{ThemeResource CaptionButtonStroke}"
                                    Data="{ThemeResource CaptionButtonPath}"
                                    Stretch="Fill"
                                    UseLayoutRounding="True"
                                    Width="10"
                                    Height="10"
                                    StrokeEndLineCap="Square"
                                    StrokeStartLineCap="Square" />
                            </Border>
                        </ControlTemplate>
                    </Setter.Value>
                </Setter>
            </Style>

        </ResourceDictionary>
    </StackPanel.Resources>

    <Button Height="36.0" MinWidth="45.0" Width="45.0" x:Name="MinimizeButton" Style="{StaticResource CaptionButton}" Click="_MinimizeClick"
            AutomationProperties.Name="Minimize">
        <Button.Resources>
            <ResourceDictionary>
                <x:String x:Key="CaptionButtonPath">M 0 0 H 10</x:String>
            </ResourceDictionary>
        </Button.Resources>
    </Button>
    <Button Height="36.0" MinWidth="45.0" Width="45.0" x:Name="MaximizeButton" Style="{StaticResource CaptionButton}" Click="_MaximizeClick"
            AutomationProperties.Name="Maximize">
        <Button.Resources>
            <ResourceDictionary>
                <x:String x:Key="CaptionButtonPath">M 0 0 H 10 V 10 H 0 V 0</x:String>
                <x:String x:Key="CaptionButtonPathWindowMaximized">M 0 2 h 8 v 8 h -8 v -8 M 2 2 v -2 h 8 v 8 h -2</x:String>
            </ResourceDictionary>
        </Button.Resources>
    </Button>
    <Button Height="36.0" MinWidth="45.0" Width="45.0" x:Name="CloseButton" Style="{StaticResource CaptionButton}" Click="_CloseClick"
            AutomationProperties.Name="Close">
        <Button.Resources>
            <ResourceDictionary>
                <ResourceDictionary.ThemeDictionaries>
                    <ResourceDictionary x:Key="Light">
                        <StaticResource x:Key="CaptionButtonBackgroundPointerOver" ResourceKey="CloseButtonBackgroundPointerOver"/>
                        <StaticResource x:Key="CaptionButtonBackgroundPressed" ResourceKey="CloseButtonBackgroundPressed"/>
                        <StaticResource x:Key="CaptionButtonStrokePointerOver" ResourceKey="CloseButtonStrokePointerOver"/>
                        <StaticResource x:Key="CaptionButtonStrokePressed" ResourceKey="CloseButtonStrokePressed"/>
                    </ResourceDictionary>
                    <ResourceDictionary x:Key="Dark">
                        <StaticResource x:Key="CaptionButtonBackgroundPointerOver" ResourceKey="CloseButtonBackgroundPointerOver"/>
                        <StaticResource x:Key="CaptionButtonBackgroundPressed" ResourceKey="CloseButtonBackgroundPressed"/>
                        <StaticResource x:Key="CaptionButtonStrokePointerOver" ResourceKey="CloseButtonStrokePointerOver"/>
                        <StaticResource x:Key="CaptionButtonStrokePressed" ResourceKey="CloseButtonStrokePressed"/>
                    </ResourceDictionary>
                    <ResourceDictionary x:Key="HighContrast">
                        <StaticResource x:Key="CaptionButtonBackgroundPointerOver" ResourceKey="CloseButtonBackgroundPointerOver"/>
                        <StaticResource x:Key="CaptionButtonBackgroundPressed" ResourceKey="CloseButtonBackgroundPressed"/>
                        <StaticResource x:Key="CaptionButtonStrokePointerOver" ResourceKey="CloseButtonStrokePointerOver"/>
                        <StaticResource x:Key="CaptionButtonStrokePressed" ResourceKey="CloseButtonStrokePressed"/>
                    </ResourceDictionary>
                </ResourceDictionary.ThemeDictionaries>

                <x:String x:Key="CaptionButtonPath">M 0 0 L 10 10 M 10 0 L 0 10</x:String>
            </ResourceDictionary>
        </Button.Resources>
    </Button>
</StackPanel>
