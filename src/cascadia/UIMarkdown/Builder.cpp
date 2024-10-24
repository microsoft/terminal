#include "pch.h"
#include "Builder.h"
#include "Builder.g.cpp"

#include "MarkdownToXaml.h"

namespace winrt::Microsoft::Terminal::UI::Markdown::implementation
{
    winrt::Windows::UI::Xaml::Controls::RichTextBlock Builder::Convert(const winrt::hstring& text,
                                                                       const winrt::hstring& baseUrl)
    {
        const auto u8String{ til::u16u8(text) };
        return MarkdownToXaml::Convert(u8String, baseUrl);
    }
}
