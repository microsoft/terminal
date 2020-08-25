#include "pch.h"
#include "ColorScheme.h"
#include "Microsoft.Terminal.Settings.Model.ColorScheme.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Media;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    ColorScheme::ColorScheme()
    {
        m_background = Colors::Black();
        m_foreground = Colors::White();
        m_black = Colors::Black();
        m_brightblack = Colors::DimGray();
        m_blue = Colors::Blue();
        m_brightblue = Colors::LightBlue();
        m_cyan = Colors::Cyan();
        m_brightcyan = Colors::LightCyan();
        m_green = Colors::Green();
        m_brightgreen = Colors::LightGreen();
        m_purple = Colors::Purple();
        m_brightpurple = Colors::MediumPurple();
        m_red = Colors::Red();
        m_brightred = Colors::IndianRed();
        m_white = Colors::WhiteSmoke();
        m_brightwhite = Colors::White();
        m_yellow = Colors::Yellow();
        m_brightyellow = Colors::LightYellow();
    }

    // Background Handlers
    Color ColorScheme::Background()
    {
        return m_background;
    }

    void ColorScheme::Background(Brush const& brush)
    {
        ColorScheme::Background(brushToColor(brush));
    }

    void ColorScheme::Background(Color const& color)
    {
        if (cmpColor(color, m_background))
        {
            m_background = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BackgroundBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BackgroundHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Background" });
        }
    }

    Brush ColorScheme::BackgroundBrush()
    {
        return ColorScheme::colorToBrush(m_background);
    }

    winrt::hstring ColorScheme::BackgroundHexValue()
    {
        return colorToHex(m_background);
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
        return m_foreground;
    }

    void ColorScheme::Foreground(Brush const& brush)
    {
        ColorScheme::Foreground(brushToColor(brush));
    }

    void ColorScheme::Foreground(Color const& color)
    {
        if (cmpColor(color, m_foreground))
        {
            m_foreground = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"ForegroundBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"ForegroundHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Foreground" });
        }
    }

    Brush ColorScheme::ForegroundBrush()
    {
        return ColorScheme::colorToBrush(m_foreground);
    }

    winrt::hstring ColorScheme::ForegroundHexValue()
    {
        return colorToHex(m_foreground);
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
        return m_black;
    }

    void ColorScheme::Black(Brush const& brush)
    {
        ColorScheme::Black(brushToColor(brush));
    }

    void ColorScheme::Black(Color const& color)
    {
        if (cmpColor(color, m_black))
        {
            m_black = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BlackBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BlackHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Black" });
        }
    }

    Brush ColorScheme::BlackBrush()
    {
        return ColorScheme::colorToBrush(m_black);
    }

    winrt::hstring ColorScheme::BlackHexValue()
    {
        return colorToHex(m_black);
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
        return m_brightblack;
    }

    void ColorScheme::BrightBlack(Brush const& brush)
    {
        ColorScheme::BrightBlack(brushToColor(brush));
    }

    void ColorScheme::BrightBlack(Color const& color)
    {
        if (cmpColor(color, m_brightblack))
        {
            m_brightblack = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlackBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlackHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlack" });
        }
    }

    Brush ColorScheme::BrightBlackBrush()
    {
        return ColorScheme::colorToBrush(m_brightblack);
    }

    winrt::hstring ColorScheme::BrightBlackHexValue()
    {
        return colorToHex(m_brightblack);
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
        return m_blue;
    }

    void ColorScheme::Blue(Brush const& brush)
    {
        ColorScheme::Blue(brushToColor(brush));
    }

    void ColorScheme::Blue(Color const& color)
    {
        if (cmpColor(color, m_blue))
        {
            m_blue = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BlueBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BlueHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Blue" });
        }
    }

    Brush ColorScheme::BlueBrush()
    {
        return ColorScheme::colorToBrush(m_blue);
    }

    winrt::hstring ColorScheme::BlueHexValue()
    {
        return colorToHex(m_blue);
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
        return m_brightblue;
    }

    void ColorScheme::BrightBlue(Brush const& brush)
    {
        ColorScheme::BrightBlue(brushToColor(brush));
    }

    void ColorScheme::BrightBlue(Color const& color)
    {
        if (cmpColor(color, m_brightblue))
        {
            m_brightblue = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlueBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlueHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightBlue" });
        }
    }

    Brush ColorScheme::BrightBlueBrush()
    {
        return ColorScheme::colorToBrush(m_brightblue);
    }

    winrt::hstring ColorScheme::BrightBlueHexValue()
    {
        return colorToHex(m_brightblue);
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
        return m_cyan;
    }

    void ColorScheme::Cyan(Brush const& brush)
    {
        ColorScheme::Cyan(brushToColor(brush));
    }

    void ColorScheme::Cyan(Color const& color)
    {
        if (cmpColor(color, m_cyan))
        {
            m_cyan = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"CyanBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"CyanHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Cyan" });
        }
    }

    Brush ColorScheme::CyanBrush()
    {
        return ColorScheme::colorToBrush(m_cyan);
    }

    winrt::hstring ColorScheme::CyanHexValue()
    {
        return colorToHex(m_cyan);
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
        return m_brightcyan;
    }

    void ColorScheme::BrightCyan(Brush const& brush)
    {
        ColorScheme::BrightCyan(brushToColor(brush));
    }

    void ColorScheme::BrightCyan(Color const& color)
    {
        if (cmpColor(color, m_brightcyan))
        {
            m_brightcyan = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightCyanBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightCyanHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightCyan" });
        }
    }

    Brush ColorScheme::BrightCyanBrush()
    {
        return ColorScheme::colorToBrush(m_brightcyan);
    }

    winrt::hstring ColorScheme::BrightCyanHexValue()
    {
        return colorToHex(m_brightcyan);
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
        return m_green;
    }

    void ColorScheme::Green(Brush const& brush)
    {
        ColorScheme::Green(brushToColor(brush));
    }

    void ColorScheme::Green(Color const& color)
    {
        if (cmpColor(color, m_green))
        {
            m_green = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"GreenBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"GreenHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Green" });
        }
    }

    Brush ColorScheme::GreenBrush()
    {
        return ColorScheme::colorToBrush(m_green);
    }

    winrt::hstring ColorScheme::GreenHexValue()
    {
        return colorToHex(m_green);
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
        return m_brightgreen;
    }

    void ColorScheme::BrightGreen(Brush const& brush)
    {
        ColorScheme::BrightGreen(brushToColor(brush));
    }

    void ColorScheme::BrightGreen(Color const& color)
    {
        if (cmpColor(color, m_brightgreen))
        {
            m_brightgreen = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightGreenBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightGreenHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightGreen" });
        }
    }

    Brush ColorScheme::BrightGreenBrush()
    {
        return ColorScheme::colorToBrush(m_brightgreen);
    }

    winrt::hstring ColorScheme::BrightGreenHexValue()
    {
        return colorToHex(m_brightgreen);
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
        return m_purple;
    }

    void ColorScheme::Purple(Brush const& brush)
    {
        ColorScheme::Purple(brushToColor(brush));
    }

    void ColorScheme::Purple(Color const& color)
    {
        if (cmpColor(color, m_purple))
        {
            m_purple = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"PurpleBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"PurpleHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Purple" });
        }
    }

    Brush ColorScheme::PurpleBrush()
    {
        return ColorScheme::colorToBrush(m_purple);
    }

    winrt::hstring ColorScheme::PurpleHexValue()
    {
        return colorToHex(m_purple);
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
        return m_brightpurple;
    }

    void ColorScheme::BrightPurple(Brush const& brush)
    {
        ColorScheme::BrightPurple(brushToColor(brush));
    }

    void ColorScheme::BrightPurple(Color const& color)
    {
        if (cmpColor(color, m_brightpurple))
        {
            m_brightpurple = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightPurpleBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightPurpleHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightPurple" });
        }
    }

    Brush ColorScheme::BrightPurpleBrush()
    {
        return ColorScheme::colorToBrush(m_brightpurple);
    }

    winrt::hstring ColorScheme::BrightPurpleHexValue()
    {
        return colorToHex(m_brightpurple);
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
        return m_red;
    }

    void ColorScheme::Red(Brush const& brush)
    {
        ColorScheme::Red(brushToColor(brush));
    }

    void ColorScheme::Red(Color const& color)
    {
        if (cmpColor(color, m_red))
        {
            m_red = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"RedBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"RedHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Red" });
        }
    }

    Brush ColorScheme::RedBrush()
    {
        return ColorScheme::colorToBrush(m_red);
    }

    winrt::hstring ColorScheme::RedHexValue()
    {
        return colorToHex(m_red);
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
        return m_brightred;
    }

    void ColorScheme::BrightRed(Brush const& brush)
    {
        ColorScheme::BrightRed(brushToColor(brush));
    }

    void ColorScheme::BrightRed(Color const& color)
    {
        if (cmpColor(color, m_brightred))
        {
            m_brightred = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightRedBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightRedHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightRed" });
        }
    }

    Brush ColorScheme::BrightRedBrush()
    {
        return ColorScheme::colorToBrush(m_brightred);
    }

    winrt::hstring ColorScheme::BrightRedHexValue()
    {
        return colorToHex(m_brightred);
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
        return m_white;
    }

    void ColorScheme::White(Brush const& brush)
    {
        ColorScheme::White(brushToColor(brush));
    }

    void ColorScheme::White(Color const& color)
    {
        if (cmpColor(color, m_white))
        {
            m_white = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"WhiteBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"WhiteHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"White" });
        }
    }

    Brush ColorScheme::WhiteBrush()
    {
        return ColorScheme::colorToBrush(m_white);
    }

    winrt::hstring ColorScheme::WhiteHexValue()
    {
        return colorToHex(m_white);
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
        return m_brightwhite;
    }

    void ColorScheme::BrightWhite(Brush const& brush)
    {
        ColorScheme::BrightWhite(brushToColor(brush));
    }

    void ColorScheme::BrightWhite(Color const& color)
    {
        if (cmpColor(color, m_brightwhite))
        {
            m_brightwhite = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightWhiteBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightWhiteHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightWhite" });
        }
    }

    Brush ColorScheme::BrightWhiteBrush()
    {
        return ColorScheme::colorToBrush(m_brightwhite);
    }

    winrt::hstring ColorScheme::BrightWhiteHexValue()
    {
        return colorToHex(m_brightwhite);
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
        return m_yellow;
    }

    void ColorScheme::Yellow(Brush const& brush)
    {
        ColorScheme::Yellow(brushToColor(brush));
    }

    void ColorScheme::Yellow(Color const& color)
    {
        if (cmpColor(color, m_yellow))
        {
            m_yellow = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"YellowBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"YellowHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"Yellow" });
        }
    }

    Brush ColorScheme::YellowBrush()
    {
        return ColorScheme::colorToBrush(m_yellow);
    }

    winrt::hstring ColorScheme::YellowHexValue()
    {
        return colorToHex(m_yellow);
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
        return m_brightyellow;
    }

    void ColorScheme::BrightYellow(Brush const& brush)
    {
        ColorScheme::BrightYellow(brushToColor(brush));
    }

    void ColorScheme::BrightYellow(Color const& color)
    {
        if (cmpColor(color, m_brightyellow))
        {
            m_brightyellow = color;
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightYellowBrush" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightYellowHexValue" });
            m_propertyChanged(*this, PropertyChangedEventArgs{ L"BrightYellow" });
        }
    }

    Brush ColorScheme::BrightYellowBrush()
    {
        return ColorScheme::colorToBrush(m_brightyellow);
    }

    winrt::hstring ColorScheme::BrightYellowHexValue()
    {
        return colorToHex(m_brightyellow);
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
        return m_propertyChanged.add(handler);
    }

    void ColorScheme::PropertyChanged(event_token const& token)
    {
        m_propertyChanged.remove(token);
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
