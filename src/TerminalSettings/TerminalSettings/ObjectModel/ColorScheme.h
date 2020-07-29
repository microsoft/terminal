#pragma once

namespace winrt::ObjectModel::implementation
{
    class ColorScheme
    {
    public:
        ColorScheme() = default;
        ~ColorScheme() = default;

        hstring Name;
        unsigned int Foreground;
        unsigned int Background;
        unsigned int SelectionBackground;
        unsigned int CursorColor;

        std::array<unsigned int, 16> Table;
    };
}
