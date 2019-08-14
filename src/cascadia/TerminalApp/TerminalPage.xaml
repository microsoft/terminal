<!-- Copyright (c) Microsoft Corporation. All rights reserved. Licensed under
the MIT License. See LICENSE in the project root for license information. -->
<Page
    x:Class="TerminalApp.TerminalPage"
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
    xmlns:local="using:TerminalApp"
    xmlns:mux="using:Microsoft.UI.Xaml.Controls"
    xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
    xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
    mc:Ignorable="d">

    <Grid x:Name="Root" Background="{ThemeResource ApplicationPageBackgroundThemeBrush}">
        <Grid.RowDefinitions>
            <RowDefinition Height="Auto" />
            <RowDefinition Height="*" />
        </Grid.RowDefinitions>

        <local:TabRowControl x:Name="TabRow" Grid.Row="0" />

        <Grid x:Name="TabContent" Grid.Row="1" VerticalAlignment="Stretch" HorizontalAlignment="Stretch" />
    </Grid>
</Page>
