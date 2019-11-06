<UserControl x:Class="Microsoft.Terminal.Wpf.TerminalControl"
             xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
             xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
             xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006" 
             xmlns:d="http://schemas.microsoft.com/expression/blend/2008" 
             xmlns:local="clr-namespace:Microsoft.Terminal.Wpf"
             mc:Ignorable="d" 
             d:DesignHeight="450" d:DesignWidth="800"
             Focusable="True">
    <Grid>
        <Grid.ColumnDefinitions>
            <ColumnDefinition />
            <ColumnDefinition Width="Auto"/>
        </Grid.ColumnDefinitions>
        <local:TerminalContainer x:Name="termContainer"/>
        <ScrollBar x:Name="scrollbar" Scroll="Scrollbar_Scroll" SmallChange="1" Grid.Column="1" />
    </Grid>
</UserControl>
