// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "../../terminal/adapter/termDispatch.hpp"
#include "ITerminalApi.hpp"

class TerminalDispatch : public Microsoft::Console::VirtualTerminal::TermDispatch
{
public:
    TerminalDispatch(::Microsoft::Terminal::Core::ITerminalApi& terminalApi);
    virtual ~TerminalDispatch(){};
    virtual void Execute(const wchar_t wchControl) override;
    virtual void Print(const wchar_t wchPrintable) override;
    virtual void PrintString(const wchar_t* const rgwch, const size_t cch) override;

    bool SetGraphicsRendition(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions* const rgOptions,
                              const size_t cOptions) override;

    virtual bool CursorPosition(const unsigned int uiLine,
                                const unsigned int uiColumn) override; // CUP

    bool CursorForward(const unsigned int uiDistance) override;
    bool CursorBackward(const unsigned int uiDistance) override;
    bool CursorUp(const unsigned int uiDistance) override;

    bool EraseCharacters(const unsigned int uiNumChars) override;
    bool SetWindowTitle(std::wstring_view title) override;

    bool SetColorTableEntry(const size_t tableIndex, const DWORD dwColor) override;
    bool SetCursorStyle(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::CursorStyle cursorStyle) override;

    bool SetDefaultForeground(const DWORD dwColor) override;
    bool SetDefaultBackground(const DWORD dwColor) override;
    bool EraseInLine(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) override; // ED
    bool DeleteCharacter(const unsigned int uiCount) override;
    bool InsertCharacter(const unsigned int uiCount) override;
    bool EraseInDisplay(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::EraseType eraseType) override;

private:
    ::Microsoft::Terminal::Core::ITerminalApi& _terminalApi;

    static bool s_IsRgbColorOption(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions opt) noexcept;
    static bool s_IsBoldColorOption(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions opt) noexcept;
    static bool s_IsDefaultColorOption(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions opt) noexcept;

    bool _SetRgbColorsHelper(_In_reads_(cOptions) const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions* const rgOptions,
                             const size_t cOptions,
                             _Out_ size_t* const pcOptionsConsumed);
    bool _SetBoldColorHelper(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions option);
    bool _SetDefaultColorHelper(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions option);
    void _SetGraphicsOptionHelper(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::GraphicsOptions opt);
};
