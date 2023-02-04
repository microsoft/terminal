#pragma once
namespace winrt
{
    namespace Microsoft::UI::Xaml
    {
    }
    namespace Windows::Foundation
    {
    }
    namespace Microsoft::Terminal::TerminalConnection
    {
    }
    namespace Windows::UI::Xaml::Media
    {
    }
    namespace Windows::UI::Xaml::Markup
    {
    }
    namespace Microsoft::Terminal::Control
    {
    }
    namespace Windows::UI::Xaml
    {
    }
    namespace Windows::Storage::Streams
    {
    }
    namespace Microsoft::Terminal::Core
    {
    }
    namespace Microsoft::UI::Xaml::Controls
    {
    }
    namespace TerminalApp
    {
    }
    namespace Windows::Foundation::Collections
    {
    }
    namespace Windows::System
    {
    }
    namespace Windows::Data::Json
    {
    }
    namespace Windows::UI::Core
    {
    }
    namespace Windows::Web::Http
    {
    }
    namespace Microsoft::Terminal::Settings::Model
    {
    }
    namespace Windows::UI::Text
    {
    }
    namespace Windows::UI::Xaml::Controls
    {
    }
}
namespace MUX = ::winrt::Microsoft::UI::Xaml;
namespace WF = ::winrt::Windows::Foundation;
namespace MTConnection = ::winrt::Microsoft::Terminal::TerminalConnection;
namespace WUXMedia = ::winrt::Windows::UI::Xaml::Media;
namespace WUXMarkup = ::winrt::Windows::UI::Xaml::Markup;
namespace MTControl = ::winrt::Microsoft::Terminal::Control;
namespace WUX = ::winrt::Windows::UI::Xaml;
namespace WSS = ::winrt::Windows::Storage::Streams;
namespace MTCore = ::winrt::Microsoft::Terminal::Core;
namespace MUXC = ::winrt::Microsoft::UI::Xaml::Controls;
namespace MTApp = ::winrt::TerminalApp;
namespace WFC = ::winrt::Windows::Foundation::Collections;
namespace WS = ::winrt::Windows::System;
namespace WDJ = ::winrt::Windows::Data::Json;
namespace WUC = ::winrt::Windows::UI::Core;
namespace WWH = ::winrt::Windows::Web::Http;
namespace MTSM = ::winrt::Microsoft::Terminal::Settings::Model;
namespace WUT = ::winrt::Windows::UI::Text;
namespace WUXC = ::winrt::Windows::UI::Xaml::Controls;
// -------- end header file
// perl -p -i -e 'next if /^namespace /; s/((?<=\W)::)?winrt::Microsoft::Terminal::TerminalConnection(::|;)/MTConnection\2/g;s/((?<=\W)::)?winrt::Microsoft::Terminal::Settings::Model(::|;)/MTSM\2/g;s/((?<=\W)::)?winrt::Windows::Foundation::Collections(::|;)/WFC\2/g;s/((?<=\W)::)?Windows::Foundation::Collections(::|;)/WFC\2/g;s/((?<=\W)::)?winrt::Microsoft::UI::Xaml::Controls(::|;)/MUXC\2/g;s/((?<=\W)::)?Microsoft::UI::Xaml::Controls(::|;)/MUXC\2/g;s/((?<=\W)::)?winrt::Microsoft::Terminal::Control(::|;)/MTControl\2/g;s/((?<=\W)::)?winrt::Windows::UI::Xaml::Controls(::|;)/WUXC\2/g;s/((?<=\W)::)?Windows::UI::Xaml::Controls(::|;)/WUXC\2/g;s/((?<=\W)::)?winrt::Windows::Storage::Streams(::|;)/WSS\2/g;s/((?<=\W)::)?Windows::Storage::Streams(::|;)/WSS\2/g;s/((?<=\W)::)?winrt::Microsoft::Terminal::Core(::|;)/MTCore\2/g;s/((?<=\W)::)?winrt::Windows::UI::Xaml::Markup(::|;)/WUXMarkup\2/g;s/((?<=\W)::)?Windows::UI::Xaml::Markup(::|;)/WUXMarkup\2/g;s/((?<=\W)::)?winrt::Windows::UI::Xaml::Media(::|;)/WUXMedia\2/g;s/((?<=\W)::)?Windows::UI::Xaml::Media(::|;)/WUXMedia\2/g;s/((?<=\W)::)?winrt::Windows::Data::Json(::|;)/WDJ\2/g;s/((?<=\W)::)?Windows::Data::Json(::|;)/WDJ\2/g;s/((?<=\W)::)?winrt::Windows::Foundation(::|;)/WF\2/g;s/((?<=\W)::)?Windows::Foundation(::|;)/WF\2/g;s/((?<=\W)::)?winrt::Microsoft::UI::Xaml(::|;)/MUX\2/g;s/((?<=\W)::)?Microsoft::UI::Xaml(::|;)/MUX\2/g;s/((?<=\W)::)?winrt::Windows::Web::Http(::|;)/WWH\2/g;s/((?<=\W)::)?Windows::Web::Http(::|;)/WWH\2/g;s/((?<=\W)::)?winrt::Windows::UI::Xaml(::|;)/WUX\2/g;s/((?<=\W)::)?Windows::UI::Xaml(::|;)/WUX\2/g;s/((?<=\W)::)?winrt::Windows::UI::Text(::|;)/WUT\2/g;s/((?<=\W)::)?Windows::UI::Text(::|;)/WUT\2/g;s/((?<=\W)::)?winrt::Windows::UI::Core(::|;)/WUC\2/g;s/((?<=\W)::)?Windows::UI::Core(::|;)/WUC\2/g;s/((?<=\W)::)?winrt::Windows::System(::|;)/WS\2/g;s/((?<=\W)::)?Windows::System(::|;)/WS\2/g;s/((?<=\W)::)?winrt::TerminalApp(::|;)/MTApp\2/g;' **/*.cpp **/*.h
