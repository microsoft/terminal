#include "pch.h"
#include "ColorScheme.h"
#include "Model.ColorScheme.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
{
    ColorScheme::ColorScheme()
    {
        _Background = Colors::Black();
        _Foreground = Colors::White();
        _Black = Colors::Black();
        _BrightBlack = Colors::DimGray();
        _Blue = Colors::Blue();
        _BrightBlue = Colors::LightBlue();
        _Cyan = Colors::Cyan();
        _BrightCyan = Colors::LightCyan();
        _Green = Colors::Green();
        _BrightGreen = Colors::LightGreen();
        _Purple = Colors::Purple();
        _BrightPurple = Colors::MediumPurple();
        _Red = Colors::Red();
        _BrightRed = Colors::IndianRed();
        _White = Colors::WhiteSmoke();
        _BrightWhite = Colors::White();
        _Yellow = Colors::Yellow();
        _BrightYellow = Colors::LightYellow();
    }

    // Background Handlers
    Color ColorScheme::Background()
    {
        return _Background;
    }

    void ColorScheme::Background(Brush const& brush)
    {
        ColorScheme::Background(brushToColor(brush));
    }

    void ColorScheme::Background(Color const& color)
    {
        if (cmpColor(color, _Background))
        {
            _Background = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BackgroundBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BackgroundHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Background" });
        }
    }

    Brush ColorScheme::BackgroundBrush()
    {
        return ColorScheme::colorToBrush(_Background);
    }

    winrt::hstring ColorScheme::BackgroundHexValue()
    {
        return colorToHex(_Background);
    }

    void ColorScheme::BackgroundHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Background(ColorScheme::hexToColor(hex));
        }
    }

    // Foreground Handlers
    Color ColorScheme::Foreground()
    {
        return _Foreground;
    }

    void ColorScheme::Foreground(Brush const& brush)
    {
        ColorScheme::Foreground(brushToColor(brush));
    }

    void ColorScheme::Foreground(Color const& color)
    {
        if (cmpColor(color, _Foreground))
        {
            _Foreground = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"ForegroundBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"ForegroundHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Foreground" });
        }
    }

    Brush ColorScheme::ForegroundBrush()
    {
        return ColorScheme::colorToBrush(_Foreground);
    }

    winrt::hstring ColorScheme::ForegroundHexValue()
    {
        return colorToHex(_Foreground);
    }

    void ColorScheme::ForegroundHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Foreground(ColorScheme::hexToColor(hex));
        }
    }

    // Black Handlers
    Color ColorScheme::Black()
    {
        return _Black;
    }

    void ColorScheme::Black(Brush const& brush)
    {
        ColorScheme::Black(brushToColor(brush));
    }

    void ColorScheme::Black(Color const& color)
    {
        if (cmpColor(color, _Black))
        {
            _Black = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BlackBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BlackHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Black" });
        }
    }

    Brush ColorScheme::BlackBrush()
    {
        return ColorScheme::colorToBrush(_Black);
    }

    winrt::hstring ColorScheme::BlackHexValue()
    {
        return colorToHex(_Black);
    }

    void ColorScheme::BlackHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Black(ColorScheme::hexToColor(hex));
        }
    }

    // BrightBlack Handlers
    Color ColorScheme::BrightBlack()
    {
        return _BrightBlack;
    }

    void ColorScheme::BrightBlack(Brush const& brush)
    {
        ColorScheme::BrightBlack(brushToColor(brush));
    }

    void ColorScheme::BrightBlack(Color const& color)
    {
        if (cmpColor(color, _BrightBlack))
        {
            _BrightBlack = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlackBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlackHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlack" });
        }
    }

    Brush ColorScheme::BrightBlackBrush()
    {
        return ColorScheme::colorToBrush(_BrightBlack);
    }

    winrt::hstring ColorScheme::BrightBlackHexValue()
    {
        return colorToHex(_BrightBlack);
    }

    void ColorScheme::BrightBlackHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::BrightBlack(ColorScheme::hexToColor(hex));
        }
    }

    // Blue Handlers
    Color ColorScheme::Blue()
    {
        return _Blue;
    }

    void ColorScheme::Blue(Brush const& brush)
    {
        ColorScheme::Blue(brushToColor(brush));
    }

    void ColorScheme::Blue(Color const& color)
    {
        if (cmpColor(color, _Blue))
        {
            _Blue = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BlueBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BlueHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Blue" });
        }
    }

    Brush ColorScheme::BlueBrush()
    {
        return ColorScheme::colorToBrush(_Blue);
    }

    winrt::hstring ColorScheme::BlueHexValue()
    {
        return colorToHex(_Blue);
    }

    void ColorScheme::BlueHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Blue(ColorScheme::hexToColor(hex));
        }
    }

    // BrightBlue Handlers
    Color ColorScheme::BrightBlue()
    {
        return _BrightBlue;
    }

    void ColorScheme::BrightBlue(Brush const& brush)
    {
        ColorScheme::BrightBlue(brushToColor(brush));
    }

    void ColorScheme::BrightBlue(Color const& color)
    {
        if (cmpColor(color, _BrightBlue))
        {
            _BrightBlue = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlueBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlueHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlue" });
        }
    }

    Brush ColorScheme::BrightBlueBrush()
    {
        return ColorScheme::colorToBrush(_BrightBlue);
    }

    winrt::hstring ColorScheme::BrightBlueHexValue()
    {
        return colorToHex(_BrightBlue);
    }

    void ColorScheme::BrightBlueHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::BrightBlue(ColorScheme::hexToColor(hex));
        }
    }

    // Cyan Handlers
    Color ColorScheme::Cyan()
    {
        return _Cyan;
    }

    void ColorScheme::Cyan(Brush const& brush)
    {
        ColorScheme::Cyan(brushToColor(brush));
    }

    void ColorScheme::Cyan(Color const& color)
    {
        if (cmpColor(color, _Cyan))
        {
            _Cyan = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"CyanBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"CyanHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Cyan" });
        }
    }

    Brush ColorScheme::CyanBrush()
    {
        return ColorScheme::colorToBrush(_Cyan);
    }

    winrt::hstring ColorScheme::CyanHexValue()
    {
        return colorToHex(_Cyan);
    }

    void ColorScheme::CyanHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Cyan(ColorScheme::hexToColor(hex));
        }
    }

    // BrightCyan Handlers
    Color ColorScheme::BrightCyan()
    {
        return _BrightCyan;
    }

    void ColorScheme::BrightCyan(Brush const& brush)
    {
        ColorScheme::BrightCyan(brushToColor(brush));
    }

    void ColorScheme::BrightCyan(Color const& color)
    {
        if (cmpColor(color, _BrightCyan))
        {
            _BrightCyan = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightCyanBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightCyanHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightCyan" });
        }
    }

    Brush ColorScheme::BrightCyanBrush()
    {
        return ColorScheme::colorToBrush(_BrightCyan);
    }

    winrt::hstring ColorScheme::BrightCyanHexValue()
    {
        return colorToHex(_BrightCyan);
    }

    void ColorScheme::BrightCyanHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::BrightCyan(ColorScheme::hexToColor(hex));
        }
    }

    // Green Handlers
    Color ColorScheme::Green()
    {
        return _Green;
    }

    void ColorScheme::Green(Brush const& brush)
    {
        ColorScheme::Green(brushToColor(brush));
    }

    void ColorScheme::Green(Color const& color)
    {
        if (cmpColor(color, _Green))
        {
            _Green = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"GreenBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"GreenHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Green" });
        }
    }

    Brush ColorScheme::GreenBrush()
    {
        return ColorScheme::colorToBrush(_Green);
    }

    winrt::hstring ColorScheme::GreenHexValue()
    {
        return colorToHex(_Green);
    }

    void ColorScheme::GreenHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Green(ColorScheme::hexToColor(hex));
        }
    }

    // BrightGreen Handlers
    Color ColorScheme::BrightGreen()
    {
        return _BrightGreen;
    }

    void ColorScheme::BrightGreen(Brush const& brush)
    {
        ColorScheme::BrightGreen(brushToColor(brush));
    }

    void ColorScheme::BrightGreen(Color const& color)
    {
        if (cmpColor(color, _BrightGreen))
        {
            _BrightGreen = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightGreenBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightGreenHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightGreen" });
        }
    }

    Brush ColorScheme::BrightGreenBrush()
    {
        return ColorScheme::colorToBrush(_BrightGreen);
    }

    winrt::hstring ColorScheme::BrightGreenHexValue()
    {
        return colorToHex(_BrightGreen);
    }

    void ColorScheme::BrightGreenHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::BrightGreen(ColorScheme::hexToColor(hex));
        }
    }

    // Purple Handlers
    Color ColorScheme::Purple()
    {
        return _Purple;
    }

    void ColorScheme::Purple(Brush const& brush)
    {
        ColorScheme::Purple(brushToColor(brush));
    }

    void ColorScheme::Purple(Color const& color)
    {
        if (cmpColor(color, _Purple))
        {
            _Purple = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"PurpleBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"PurpleHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Purple" });
        }
    }

    Brush ColorScheme::PurpleBrush()
    {
        return ColorScheme::colorToBrush(_Purple);
    }

    winrt::hstring ColorScheme::PurpleHexValue()
    {
        return colorToHex(_Purple);
    }

    void ColorScheme::PurpleHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Purple(ColorScheme::hexToColor(hex));
        }
    }

    // BrightPurple Handlers
    Color ColorScheme::BrightPurple()
    {
        return _BrightPurple;
    }

    void ColorScheme::BrightPurple(Brush const& brush)
    {
        ColorScheme::BrightPurple(brushToColor(brush));
    }

    void ColorScheme::BrightPurple(Color const& color)
    {
        if (cmpColor(color, _BrightPurple))
        {
            _BrightPurple = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightPurpleBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightPurpleHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightPurple" });
        }
    }

    Brush ColorScheme::BrightPurpleBrush()
    {
        return ColorScheme::colorToBrush(_BrightPurple);
    }

    winrt::hstring ColorScheme::BrightPurpleHexValue()
    {
        return colorToHex(_BrightPurple);
    }

    void ColorScheme::BrightPurpleHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::BrightPurple(ColorScheme::hexToColor(hex));
        }
    }

    // Red Handlers
    Color ColorScheme::Red()
    {
        return _Red;
    }

    void ColorScheme::Red(Brush const& brush)
    {
        ColorScheme::Red(brushToColor(brush));
    }

    void ColorScheme::Red(Color const& color)
    {
        if (cmpColor(color, _Red))
        {
            _Red = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"RedBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"RedHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Red" });
        }
    }

    Brush ColorScheme::RedBrush()
    {
        return ColorScheme::colorToBrush(_Red);
    }

    winrt::hstring ColorScheme::RedHexValue()
    {
        return colorToHex(_Red);
    }

    void ColorScheme::RedHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Red(ColorScheme::hexToColor(hex));
        }
    }

    // BrightRed Handlers
    Color ColorScheme::BrightRed()
    {
        return _BrightRed;
    }

    void ColorScheme::BrightRed(Brush const& brush)
    {
        ColorScheme::BrightRed(brushToColor(brush));
    }

    void ColorScheme::BrightRed(Color const& color)
    {
        if (cmpColor(color, _BrightRed))
        {
            _BrightRed = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightRedBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightRedHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightRed" });
        }
    }

    Brush ColorScheme::BrightRedBrush()
    {
        return ColorScheme::colorToBrush(_BrightRed);
    }

    winrt::hstring ColorScheme::BrightRedHexValue()
    {
        return colorToHex(_BrightRed);
    }

    void ColorScheme::BrightRedHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::BrightRed(ColorScheme::hexToColor(hex));
        }
    }

    // White Handlers
    Color ColorScheme::White()
    {
        return _White;
    }

    void ColorScheme::White(Brush const& brush)
    {
        ColorScheme::White(brushToColor(brush));
    }

    void ColorScheme::White(Color const& color)
    {
        if (cmpColor(color, _White))
        {
            _White = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"WhiteBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"WhiteHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"White" });
        }
    }

    Brush ColorScheme::WhiteBrush()
    {
        return ColorScheme::colorToBrush(_White);
    }

    winrt::hstring ColorScheme::WhiteHexValue()
    {
        return colorToHex(_White);
    }

    void ColorScheme::WhiteHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::White(ColorScheme::hexToColor(hex));
        }
    }

    // BrightWhite Handlers
    Color ColorScheme::BrightWhite()
    {
        return _BrightWhite;
    }

    void ColorScheme::BrightWhite(Brush const& brush)
    {
        ColorScheme::BrightWhite(brushToColor(brush));
    }

    void ColorScheme::BrightWhite(Color const& color)
    {
        if (cmpColor(color, _BrightWhite))
        {
            _BrightWhite = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightWhiteBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightWhiteHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightWhite" });
        }
    }

    Brush ColorScheme::BrightWhiteBrush()
    {
        return ColorScheme::colorToBrush(_BrightWhite);
    }

    winrt::hstring ColorScheme::BrightWhiteHexValue()
    {
        return colorToHex(_BrightWhite);
    }

    void ColorScheme::BrightWhiteHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::BrightWhite(ColorScheme::hexToColor(hex));
        }
    }

    // Yellow Handlers
    Color ColorScheme::Yellow()
    {
        return _Yellow;
    }

    void ColorScheme::Yellow(Brush const& brush)
    {
        ColorScheme::Yellow(brushToColor(brush));
    }

    void ColorScheme::Yellow(Color const& color)
    {
        if (cmpColor(color, _Yellow))
        {
            _Yellow = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"YellowBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"YellowHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"Yellow" });
        }
    }

    Brush ColorScheme::YellowBrush()
    {
        return ColorScheme::colorToBrush(_Yellow);
    }

    winrt::hstring ColorScheme::YellowHexValue()
    {
        return colorToHex(_Yellow);
    }

    void ColorScheme::YellowHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::Yellow(ColorScheme::hexToColor(hex));
        }
    }

    // BrightYellow Handlers
    Color ColorScheme::BrightYellow()
    {
        return _BrightYellow;
    }

    void ColorScheme::BrightYellow(Brush const& brush)
    {
        ColorScheme::BrightYellow(brushToColor(brush));
    }

    void ColorScheme::BrightYellow(Color const& color)
    {
        if (cmpColor(color, _BrightYellow))
        {
            _BrightYellow = color;
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightYellowBrush" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightYellowHexValue" });
            _PropertyChanged(*this, PropertyChangedEventArgs{ L"BrightYellow" });
        }
    }

    Brush ColorScheme::BrightYellowBrush()
    {
        return ColorScheme::colorToBrush(_BrightYellow);
    }

    winrt::hstring ColorScheme::BrightYellowHexValue()
    {
        return colorToHex(_BrightYellow);
    }

    void ColorScheme::BrightYellowHexValue(winrt::hstring hex)
    {
        // TODO: Dirty dirty hack - We should try to parse this string a little harder than checking that its 9 long
        if (hex.size() == 9)
        {
            ColorScheme::BrightYellow(ColorScheme::hexToColor(hex));
        }
    }

    // event handlers
    event_token ColorScheme::PropertyChanged(PropertyChangedEventHandler const& handler)
    {
        return _PropertyChanged.add(handler);
    }

    void ColorScheme::PropertyChanged(event_token const& token)
    {
        _PropertyChanged.remove(token);
    }

    // helpers

    Color ColorScheme::hexToColor(winrt::hstring hex)
    {
        std::string string = winrt::to_string(hex);
        Color newColor = Color();
        newColor.A = base::checked_cast<uint8_t>(std::stoi(string.substr(1, 2), nullptr, 16));
        newColor.R = base::checked_cast<uint8_t>(std::stoi(string.substr(3, 2), nullptr, 16));
        newColor.G = base::checked_cast<uint8_t>(std::stoi(string.substr(5, 2), nullptr, 16));
        newColor.B = base::checked_cast<uint8_t>(std::stoi(string.substr(7, 2), nullptr, 16));
        return newColor;
    }

    winrt::hstring ColorScheme::colorToHex(Windows::UI::Color color)
    {
        char hex[10];
        snprintf(hex, sizeof hex, "#%02x%02x%02x%02x", color.A, color.R, color.G, color.B);

        return winrt::to_hstring(hex);
    }

    bool ColorScheme::cmpColor(Color a, Color b)
    {
        if (a.A == b.A && a.R == b.R && a.G == b.G && a.B == b.B)
        {
            return false;
        }
        return true;
    }

    Brush ColorScheme::colorToBrush(Color color)
    {
        return SolidColorBrush(color);
    }

    Color ColorScheme::brushToColor(Brush brush)
    {
        return brush.as<SolidColorBrush>().Color();
    }
}
