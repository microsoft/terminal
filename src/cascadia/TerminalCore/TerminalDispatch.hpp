// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../../terminal/adapter/termDispatch.hpp"
#include "ITerminalApi.hpp"

class TerminalDispatch : public Microsoft::Console::VirtualTerminal::TermDispatch
{
public:
    TerminalDispatch(::Microsoft::Terminal::Core::ITerminalApi& terminalApi) noexcept;

    void Execute(const wchar_t wchControl) noexcept override;
    void Print(const wchar_t wchPrintable) noexcept override;
    void PrintString(const std::wstring_view string) noexcept override;

    bool SetGraphicsRendition(const std::basic_string_view<::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions> options) noexcept override;

    bool CursorPosition(const size_t line,
                        const size_t column) noexcept override; // CUP

    bool EnableWin32InputMode(const bool win32InputMode) noexcept override; // win32-input-mode

    bool CursorVisibility(const bool isVisible) noexcept override; // DECTCEM
    bool EnableCursorBlinking(const bool enable) noexcept override; // ATT610

    bool CursorForward(const size_t distance) noexcept override;
    bool CursorBackward(const size_t distance) noexcept override;
    bool CursorUp(const size_t distance) noexcept override;

    bool LineFeed(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::LineFeedType lineFeedType) noexcept override;

    bool EraseCharacters(const size_t numChars) noexcept override;
    bool CarriageReturn() noexcept override;
    bool SetWindowTitle(std::wstring_view title) noexcept override;

    bool SetColorTableEntry(const size_t tableIndex, const DWORD color) noexcept override;
    bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) noexcept override;

    bool SetDefaultForeground(const DWORD color) noexcept override;
    bool SetDefaultBackground(const DWORD color) noexcept override;
    bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept override; // ED
    bool DeleteCharacter(const size_t count) noexcept override;
    bool InsertCharacter(const size_t count) noexcept override;
    bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) noexcept override;

    bool SetCursorKeysMode(const bool applicationMode) noexcept override; // DECCKM
    bool SetKeypadMode(const bool applicationMode) noexcept override; // DECKPAM, DECKPNM

    bool SoftReset() noexcept override; // DECSTR
    bool HardReset() noexcept override; // RIS

    bool EnableVT200MouseMode(const bool enabled) noexcept override; // ?1000
    bool EnableUTF8ExtendedMouseMode(const bool enabled) noexcept override; // ?1005
    bool EnableSGRExtendedMouseMode(const bool enabled) noexcept override; // ?1006
    bool EnableButtonEventMouseMode(const bool enabled) noexcept override; // ?1002
    bool EnableAnyEventMouseMode(const bool enabled) noexcept override; // ?1003
    bool EnableAlternateScroll(const bool enabled) noexcept override; // ?1007

    bool SetPrivateModes(const std::basic_string_view<::Microsoft::Console::VirtualTerminal::DispatchTypes::PrivateModeParams> /*params*/) noexcept override; // DECSET
    bool ResetPrivateModes(const std::basic_string_view<::Microsoft::Console::VirtualTerminal::DispatchTypes::PrivateModeParams> /*params*/) noexcept override; // DECRST

private:
    ::Microsoft::Terminal::Core::ITerminalApi& _terminalApi;

    bool _SetRgbColorsHelper(const std::basic_string_view<::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions> options,
                             size_t& optionsConsumed) noexcept;
    bool _SetBoldColorHelper(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions opt) noexcept;
    bool _SetDefaultColorHelper(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions opt) noexcept;
    void _SetGraphicsOptionHelper(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions opt) noexcept;

    bool _SetResetPrivateModes(const std::basic_string_view<::Microsoft::Console::VirtualTerminal::DispatchTypes::PrivateModeParams> params, const bool enable) noexcept;
    bool _PrivateModeParamsHelper(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::PrivateModeParams param, const bool enable) noexcept;
};
