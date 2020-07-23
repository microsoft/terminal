#pragma once

namespace winrt::TerminalSettings::implementation
{
    enum class CloseOnExitMode
    {
        TODO_FIX
    };

    enum class ScrollbarState
    {
        TODO_FIX
    };

    enum class FontWeight
    {
        TODO_FIX
    };

    enum class Stretch
    {
        TODO_FIX
    };

    enum class HorizontalAlignment
    {
        TODO_FIX
    };

    enum class VerticalAlignment
    {
        TODO_FIX
    };

    enum class TextAntialiasingMode
    {
        TODO_FIX
    };

    enum class CursorStyle
    {
        TODO_FIX
    };

    class Profile
    {
        Profile() = default;
        ~Profile() = default;

        std::optional<guid> Guid;
        hstring Name;
        std::optional<hstring> Source;
        std::optional<hstring> ConnectionType;
        std::optional<hstring> Icon;
        bool Hidden;
        CloseOnExitMode CloseOnExit;
        hstring TabTitle;

        // Terminal Control Settings
        bool UseAcrylic;
        double AcrylicOpacity;
        ScrollbarState ScrollState;
        hstring FontFace;
        int FontSize;
        FontWeight FontWeight;
        hstring Padding;
        bool CopyOnSelect;
        hstring Commandline;
        hstring StartingDirectory;
        hstring EnvironmentVariables;
        hstring BackgroundImage;
        double BackgroundImageOpacity;
        Stretch BackgroundImageStretchMode;
        // BackgroundImageAlignment is weird. It's 1 settings saved as 2 separate values
        HorizontalAlignment BackgroundImageHorizontalAlignment;
        VerticalAlignment BackgroundImageVerticalAlignment;
        unsigned int SelectionBackground;
        TextAntialiasingMode AntialiasingMode;
        bool RetroTerminalEffect;
        bool ForceFullRepaintRendering;
        bool SoftwareRendering;

        // Terminal Core Settings
        unsigned int DefaultForeground;
        unsigned int DefaultBackground;
        hstring ColorScheme ;
        int HistorySize ;
        int InitialRows;
        int InitialCols;
        bool SnapOnInput ;
        bool AltGrAliasing ;
        unsigned int CursorColor;
        CursorStyle CursorShape ;
        unsigned int CursorHeight ;
        hstring StartingTitle;
        bool SuppressApplicationTitle;
        bool ForceVTInput;
    };
}
