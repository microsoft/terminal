// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "FontSizeChangedArgs.g.h"
#include "TitleChangedEventArgs.g.h"
#include "ContextMenuRequestedEventArgs.g.h"
#include "WriteToClipboardEventArgs.g.h"
#include "PasteFromClipboardEventArgs.g.h"
#include "OpenHyperlinkEventArgs.g.h"
#include "NoticeEventArgs.g.h"
#include "ScrollPositionChangedArgs.g.h"
#include "RendererWarningArgs.g.h"
#include "TransparencyChangedEventArgs.g.h"
#include "ShowWindowArgs.g.h"
#include "UpdateSelectionMarkersEventArgs.g.h"
#include "CompletionsChangedEventArgs.g.h"
#include "KeySentEventArgs.g.h"
#include "CharSentEventArgs.g.h"
#include "StringSentEventArgs.g.h"
#include "SearchMissingCommandEventArgs.g.h"
#include "WindowSizeChangedEventArgs.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{

    struct FontSizeChangedArgs : public FontSizeChangedArgsT<FontSizeChangedArgs>
    {
    public:
        FontSizeChangedArgs(int32_t width,
                            int32_t height) :
            Width(width),
            Height(height)
        {
        }

        til::property<int32_t> Width;
        til::property<int32_t> Height;
    };

    struct TitleChangedEventArgs : public TitleChangedEventArgsT<TitleChangedEventArgs>
    {
    public:
        TitleChangedEventArgs(hstring title) :
            _Title(title) {}

        WINRT_PROPERTY(hstring, Title);
    };

    struct ContextMenuRequestedEventArgs : public ContextMenuRequestedEventArgsT<ContextMenuRequestedEventArgs>
    {
    public:
        ContextMenuRequestedEventArgs(winrt::Windows::Foundation::Point pos) :
            _Position(pos) {}

        WINRT_PROPERTY(winrt::Windows::Foundation::Point, Position);
    };

    struct WriteToClipboardEventArgs : WriteToClipboardEventArgsT<WriteToClipboardEventArgs>
    {
        WriteToClipboardEventArgs(winrt::hstring&& plain, std::string&& html, std::string&& rtf) :
            _plain(std::move(plain)),
            _html(std::move(html)),
            _rtf(std::move(rtf))
        {
        }

        winrt::hstring Plain() const noexcept { return _plain; }
        winrt::com_array<uint8_t> Html() noexcept { return _cast(_html); }
        winrt::com_array<uint8_t> Rtf() noexcept { return _cast(_rtf); }

    private:
        static winrt::com_array<uint8_t> _cast(const std::string& str)
        {
            const auto beg = reinterpret_cast<const uint8_t*>(str.data());
            const auto len = str.size();
            return { beg, beg + len };
        }

        winrt::hstring _plain;
        std::string _html;
        std::string _rtf;
    };

    struct PasteFromClipboardEventArgs : public PasteFromClipboardEventArgsT<PasteFromClipboardEventArgs>
    {
    public:
        PasteFromClipboardEventArgs(std::function<void(const hstring&)> clipboardDataHandler, bool bracketedPasteEnabled) :
            m_clipboardDataHandler(clipboardDataHandler),
            _BracketedPasteEnabled{ bracketedPasteEnabled } {}

        void HandleClipboardData(hstring value)
        {
            m_clipboardDataHandler(value);
        };

        WINRT_PROPERTY(bool, BracketedPasteEnabled, false);

    private:
        std::function<void(const hstring&)> m_clipboardDataHandler;
    };

    struct OpenHyperlinkEventArgs : public OpenHyperlinkEventArgsT<OpenHyperlinkEventArgs>
    {
    public:
        OpenHyperlinkEventArgs(hstring uri) :
            _uri(uri) {}

        hstring Uri() { return _uri; };

    private:
        hstring _uri;
    };

    struct NoticeEventArgs : public NoticeEventArgsT<NoticeEventArgs>
    {
    public:
        NoticeEventArgs(const NoticeLevel level, const hstring& message) :
            _level(level),
            _message(message) {}

        NoticeLevel Level() { return _level; };
        hstring Message() { return _message; };

    private:
        const NoticeLevel _level;
        const hstring _message;
    };

    struct ScrollPositionChangedArgs : public ScrollPositionChangedArgsT<ScrollPositionChangedArgs>
    {
    public:
        ScrollPositionChangedArgs(const int viewTop,
                                  const int viewHeight,
                                  const int bufferSize) :
            _ViewTop(viewTop),
            _ViewHeight(viewHeight),
            _BufferSize(bufferSize)
        {
        }

        WINRT_PROPERTY(int32_t, ViewTop);
        WINRT_PROPERTY(int32_t, ViewHeight);
        WINRT_PROPERTY(int32_t, BufferSize);
    };

    struct RendererWarningArgs : public RendererWarningArgsT<RendererWarningArgs>
    {
    public:
        RendererWarningArgs(const HRESULT hr, winrt::hstring parameter) :
            _Result{ hr },
            _Parameter{ std::move(parameter) }
        {
        }

        WINRT_PROPERTY(HRESULT, Result);
        WINRT_PROPERTY(winrt::hstring, Parameter);
    };

    struct TransparencyChangedEventArgs : public TransparencyChangedEventArgsT<TransparencyChangedEventArgs>
    {
    public:
        TransparencyChangedEventArgs(const float opacity) :
            _Opacity(opacity)
        {
        }

        WINRT_PROPERTY(float, Opacity);
    };

    struct ShowWindowArgs : public ShowWindowArgsT<ShowWindowArgs>
    {
    public:
        ShowWindowArgs(const bool showOrHide) :
            _ShowOrHide(showOrHide)
        {
        }

        WINRT_PROPERTY(bool, ShowOrHide);
    };

    struct UpdateSelectionMarkersEventArgs : public UpdateSelectionMarkersEventArgsT<UpdateSelectionMarkersEventArgs>
    {
    public:
        UpdateSelectionMarkersEventArgs(const bool clearMarkers) :
            _ClearMarkers(clearMarkers)
        {
        }

        WINRT_PROPERTY(bool, ClearMarkers, false);
    };

    struct CompletionsChangedEventArgs : public CompletionsChangedEventArgsT<CompletionsChangedEventArgs>
    {
    public:
        CompletionsChangedEventArgs(const winrt::hstring menuJson, const unsigned int replaceLength) :
            _MenuJson(menuJson),
            _ReplacementLength(replaceLength)
        {
        }

        WINRT_PROPERTY(winrt::hstring, MenuJson, L"");
        WINRT_PROPERTY(uint32_t, ReplacementLength, 0);
    };

    struct KeySentEventArgs : public KeySentEventArgsT<KeySentEventArgs>
    {
    public:
        KeySentEventArgs(const WORD vkey, const WORD scanCode, const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers, const bool keyDown) :
            _VKey(vkey),
            _ScanCode(scanCode),
            _Modifiers(modifiers),
            _KeyDown(keyDown) {}

        WINRT_PROPERTY(WORD, VKey);
        WINRT_PROPERTY(WORD, ScanCode);
        WINRT_PROPERTY(winrt::Microsoft::Terminal::Core::ControlKeyStates, Modifiers);
        WINRT_PROPERTY(bool, KeyDown, false);
    };

    struct CharSentEventArgs : public CharSentEventArgsT<CharSentEventArgs>
    {
    public:
        CharSentEventArgs(const wchar_t character, const WORD scanCode, const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers) :
            _Character(character),
            _ScanCode(scanCode),
            _Modifiers(modifiers) {}

        WINRT_PROPERTY(wchar_t, Character);
        WINRT_PROPERTY(WORD, ScanCode);
        WINRT_PROPERTY(winrt::Microsoft::Terminal::Core::ControlKeyStates, Modifiers);
    };

    struct StringSentEventArgs : public StringSentEventArgsT<StringSentEventArgs>
    {
    public:
        StringSentEventArgs(const winrt::hstring& text) :
            _Text(text) {}

        WINRT_PROPERTY(winrt::hstring, Text);
    };

    struct SearchMissingCommandEventArgs : public SearchMissingCommandEventArgsT<SearchMissingCommandEventArgs>
    {
    public:
        SearchMissingCommandEventArgs(const winrt::hstring& missingCommand, const til::CoordType& bufferRow) :
            MissingCommand(missingCommand),
            BufferRow(bufferRow) {}

        til::property<winrt::hstring> MissingCommand;
        til::property<til::CoordType> BufferRow;
    };

    struct WindowSizeChangedEventArgs : public WindowSizeChangedEventArgsT<WindowSizeChangedEventArgs>
    {
    public:
        WindowSizeChangedEventArgs(int32_t width,
                                   int32_t height) :
            _Width(width),
            _Height(height)
        {
        }

        WINRT_PROPERTY(int32_t, Width);
        WINRT_PROPERTY(int32_t, Height);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(OpenHyperlinkEventArgs);
}
