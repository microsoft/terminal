#pragma once

#include "Model.ColorScheme.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::Model::implementation
{
    struct ColorScheme : ColorSchemeT<ColorScheme>
    {
        ColorScheme();

        // Background
        Windows::UI::Color Background();
        Windows::UI::Xaml::Media::Brush BackgroundBrush();
        winrt::hstring BackgroundHexValue();
        void BackgroundHexValue(winrt::hstring);
        void Background(Windows::UI::Xaml::Media::Brush const& brush);
        void Background(Windows::UI::Color const& color);

        //Foreground
        Windows::UI::Color Foreground();
        Windows::UI::Xaml::Media::Brush ForegroundBrush();
        winrt::hstring ForegroundHexValue();
        void ForegroundHexValue(winrt::hstring);
        void Foreground(Windows::UI::Xaml::Media::Brush const& brush);
        void Foreground(Windows::UI::Color const& color);

        // Black
        Windows::UI::Color Black();
        Windows::UI::Xaml::Media::Brush BlackBrush();
        winrt::hstring BlackHexValue();
        void BlackHexValue(winrt::hstring);
        void Black(Windows::UI::Xaml::Media::Brush const& brush);
        void Black(Windows::UI::Color const& color);

        // BrightBlack
        Windows::UI::Color BrightBlack();
        Windows::UI::Xaml::Media::Brush BrightBlackBrush();
        winrt::hstring BrightBlackHexValue();
        void BrightBlackHexValue(winrt::hstring);
        void BrightBlack(Windows::UI::Xaml::Media::Brush const& brush);
        void BrightBlack(Windows::UI::Color const& color);

        // Blue
        Windows::UI::Color Blue();
        Windows::UI::Xaml::Media::Brush BlueBrush();
        winrt::hstring BlueHexValue();
        void BlueHexValue(winrt::hstring);
        void Blue(Windows::UI::Xaml::Media::Brush const& brush);
        void Blue(Windows::UI::Color const& color);

        // BrightBlue
        Windows::UI::Color BrightBlue();
        Windows::UI::Xaml::Media::Brush BrightBlueBrush();
        winrt::hstring BrightBlueHexValue();
        void BrightBlueHexValue(winrt::hstring);
        void BrightBlue(Windows::UI::Xaml::Media::Brush const& brush);
        void BrightBlue(Windows::UI::Color const& color);

        // Cyan
        Windows::UI::Color Cyan();
        Windows::UI::Xaml::Media::Brush CyanBrush();
        winrt::hstring CyanHexValue();
        void CyanHexValue(winrt::hstring);
        void Cyan(Windows::UI::Xaml::Media::Brush const& brush);
        void Cyan(Windows::UI::Color const& color);

        // BrightCyan
        Windows::UI::Color BrightCyan();
        Windows::UI::Xaml::Media::Brush BrightCyanBrush();
        winrt::hstring BrightCyanHexValue();
        void BrightCyanHexValue(winrt::hstring);
        void BrightCyan(Windows::UI::Xaml::Media::Brush const& brush);
        void BrightCyan(Windows::UI::Color const& color);

        // Green
        Windows::UI::Color Green();
        Windows::UI::Xaml::Media::Brush GreenBrush();
        winrt::hstring GreenHexValue();
        void GreenHexValue(winrt::hstring);
        void Green(Windows::UI::Xaml::Media::Brush const& brush);
        void Green(Windows::UI::Color const& color);

        // BrightGreen
        Windows::UI::Color BrightGreen();
        Windows::UI::Xaml::Media::Brush BrightGreenBrush();
        winrt::hstring BrightGreenHexValue();
        void BrightGreenHexValue(winrt::hstring);
        void BrightGreen(Windows::UI::Xaml::Media::Brush const& brush);
        void BrightGreen(Windows::UI::Color const& color);

        // Purple
        Windows::UI::Color Purple();
        Windows::UI::Xaml::Media::Brush PurpleBrush();
        winrt::hstring PurpleHexValue();
        void PurpleHexValue(winrt::hstring);
        void Purple(Windows::UI::Xaml::Media::Brush const& brush);
        void Purple(Windows::UI::Color const& color);

        // BrightPurple
        Windows::UI::Color BrightPurple();
        Windows::UI::Xaml::Media::Brush BrightPurpleBrush();
        winrt::hstring BrightPurpleHexValue();
        void BrightPurpleHexValue(winrt::hstring);
        void BrightPurple(Windows::UI::Xaml::Media::Brush const& brush);
        void BrightPurple(Windows::UI::Color const& color);

        // Red
        Windows::UI::Color Red();
        Windows::UI::Xaml::Media::Brush RedBrush();
        winrt::hstring RedHexValue();
        void RedHexValue(winrt::hstring);
        void Red(Windows::UI::Xaml::Media::Brush const& brush);
        void Red(Windows::UI::Color const& color);

        // BrightRed
        Windows::UI::Color BrightRed();
        Windows::UI::Xaml::Media::Brush BrightRedBrush();
        winrt::hstring BrightRedHexValue();
        void BrightRedHexValue(winrt::hstring);
        void BrightRed(Windows::UI::Xaml::Media::Brush const& brush);
        void BrightRed(Windows::UI::Color const& color);

        // White
        Windows::UI::Color White();
        Windows::UI::Xaml::Media::Brush WhiteBrush();
        winrt::hstring WhiteHexValue();
        void WhiteHexValue(winrt::hstring);
        void White(Windows::UI::Xaml::Media::Brush const& brush);
        void White(Windows::UI::Color const& color);

        // BrightWhite
        Windows::UI::Color BrightWhite();
        Windows::UI::Xaml::Media::Brush BrightWhiteBrush();
        winrt::hstring BrightWhiteHexValue();
        void BrightWhiteHexValue(winrt::hstring);
        void BrightWhite(Windows::UI::Xaml::Media::Brush const& brush);
        void BrightWhite(Windows::UI::Color const& color);

        // Yellow
        Windows::UI::Color Yellow();
        Windows::UI::Xaml::Media::Brush YellowBrush();
        winrt::hstring YellowHexValue();
        void YellowHexValue(winrt::hstring);
        void Yellow(Windows::UI::Xaml::Media::Brush const& brush);
        void Yellow(Windows::UI::Color const& color);

        // BrightYellow
        Windows::UI::Color BrightYellow();
        Windows::UI::Xaml::Media::Brush BrightYellowBrush();
        winrt::hstring BrightYellowHexValue();
        void BrightYellowHexValue(winrt::hstring);
        void BrightYellow(Windows::UI::Xaml::Media::Brush const& brush);
        void BrightYellow(Windows::UI::Color const& color);

        winrt::event_token PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& value);
        void PropertyChanged(winrt::event_token const& token);

    private:
        static bool cmpColor(Windows::UI::Color a, Windows::UI::Color b);
        static winrt::hstring colorToHex(Windows::UI::Color);
        static Windows::UI::Color hexToColor(winrt::hstring);
        static Windows::UI::Xaml::Media::Brush colorToBrush(Windows::UI::Color);
        static Windows::UI::Color brushToColor(Windows::UI::Xaml::Media::Brush);

        Windows::UI::Color _Background;
        Windows::UI::Color _Foreground;
        Windows::UI::Color _Black;
        Windows::UI::Color _BrightBlack;
        Windows::UI::Color _Blue;
        Windows::UI::Color _BrightBlue;
        Windows::UI::Color _Cyan;
        Windows::UI::Color _BrightCyan;
        Windows::UI::Color _Green;
        Windows::UI::Color _BrightGreen;
        Windows::UI::Color _Purple;
        Windows::UI::Color _BrightPurple;
        Windows::UI::Color _Red;
        Windows::UI::Color _BrightRed;
        Windows::UI::Color _White;
        Windows::UI::Color _BrightWhite;
        Windows::UI::Color _Yellow;
        Windows::UI::Color _BrightYellow;

        winrt::event<Windows::UI::Xaml::Data::PropertyChangedEventHandler> _PropertyChanged;
    };
}
