#pragma once

namespace winrt::ObjectModel::implementation
{
    enum class CloseOnExitMode
    {
        graceful,
        always,
        never
    };

    enum class ScrollbarState
    {
        visible,
        hidden
    };

    enum class FontWeight
    {
        thin,
        extra_light,
        light,
        semi_light,
        normal,
        medium,
        semi_bold,
        bold,
        extra_bold,
        black,
        extra_black
    };

    enum class Stretch
    {
        none,
        fill,
        uniform,
        uniformToFill
    };

    enum class HorizontalAlignment
    {
        left,
        center,
        right
    };

    enum class VerticalAlignment
    {
        top,
        center,
        bottom
    };

    enum class TextAntialiasingMode
    {
        grayscale,
        cleartype,
        aliased
    };

    enum class CursorStyle
    {
        vintage,
        bar,
        underscore,
        filledBox,
        emptyBox
    };

    class Profile
    {
    public:
        Profile() = default;
        ~Profile() = default;

        std::optional<hstring> Guid;
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
