<Window x:Class="GUIConsole.Wpf.MainWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
        xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
        mc:Ignorable="d"
        
        Title="MainWindow" Height="450" Width="800"
        Background="#C7000000"
        AllowsTransparency="True"
        WindowStyle="None"
        MouseDown="Window_MouseDown"
        BorderThickness="1"
        BorderBrush="LightSlateGray"
        
        KeyDown="Window_KeyDown"
        Loaded="Window_Loaded">
    <Window.Resources>
        <Style x:Key="TitleBarButtonStyle" TargetType="{x:Type Button}">
            <Setter Property="Background" Value="Transparent"/>
            <Setter Property="FontFamily" Value="Segoe MDL2 Assets"/>
            <Setter Property="FontSize" Value="14"/>
            <Setter Property="Height" Value="32"/>
            <Setter Property="Width" Value="46"/>
            <Setter Property="Background" Value="Transparent"/>
            <Setter Property="Foreground" Value="White"/>
            <Setter Property="BorderThickness" Value="0"/>
        </Style>

        <Style x:Key="CloseButtonStyle" TargetType="{x:Type Button}" BasedOn="{StaticResource TitleBarButtonStyle}">
            <Setter Property="Content" Value="&#xE10A;"/>
            <!--Remove the default Button template's Triggers, otherwise they'll override our trigger below.-->
            <Setter Property="Template">
                <Setter.Value>
                    <ControlTemplate TargetType="{x:Type Button}">
                        <Border Background="{TemplateBinding Background}">
                            <ContentPresenter HorizontalAlignment="Center" VerticalAlignment="Center"/>
                        </Border>
                    </ControlTemplate>
                </Setter.Value>
            </Setter>
            <Style.Triggers>
                <Trigger Property="IsMouseOver" Value="True">
                    <Setter Property="Button.Background" Value="Red"/>
                </Trigger>
            </Style.Triggers>
        </Style>
    </Window.Resources>
    
    <Grid>
        <Grid.RowDefinitions>
            <RowDefinition Height="Auto"/>
            <RowDefinition Height="*"/>
        </Grid.RowDefinitions>

        <Grid Grid.Row="0">
            <Grid.ColumnDefinitions>
                <ColumnDefinition Width="*"/>
                <ColumnDefinition Width="Auto"/>
            </Grid.ColumnDefinitions>
            <TextBlock x:Name="TitleBarTitle" Grid.Column="0" VerticalAlignment="Center" Padding="10 0" Foreground="White">
                GUIConsole
            </TextBlock>
            <StackPanel x:Name="TitleBarButtons" Grid.Column="1" Orientation="Horizontal" VerticalAlignment="Top">
                <Button x:Name="MinimizeButton"
                        Click="MinimizeButton_Click"
                        Content="&#xE108;"
                        Style="{StaticResource TitleBarButtonStyle}"/>
                <Button x:Name="MaximizeRestoreButton"
                        Click="MaximizeRestoreButton_Click"
                        Content="&#xE922;"
                        FontSize="12"
                        Style="{StaticResource TitleBarButtonStyle}"/>
                <Button x:Name="CloseButton"
                        Click="CloseButton_Click" 
                        FontSize="12"
                        Style="{StaticResource CloseButtonStyle}"/>
            </StackPanel>
        </Grid>
        <ScrollViewer x:Name="TerminalHistoryViewer" Grid.Row="1" ScrollChanged="ScrollViewer_ScrollChanged">
            <TextBlock x:Name="TerminalHistoryBlock" FontFamily="Consolas" TextWrapping="Wrap" Foreground="White"/>
        </ScrollViewer>
    </Grid>
</Window>
