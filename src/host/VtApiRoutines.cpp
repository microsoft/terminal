// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "VtApiRoutines.h"
#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/convert.hpp"

using namespace Microsoft::Console::Interactivity;

VtApiRoutines::VtApiRoutines() :
    m_inputCodepage(ServiceLocator::LocateGlobals().getConsoleInformation().CP),
    m_outputCodepage(ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP),
    m_inputMode(),
    m_outputMode(),
    m_pUsualRoutines(),
    m_pVtEngine()
{
}

#pragma warning(push)
#pragma warning(disable : 4100) // unreferenced param

void VtApiRoutines::GetConsoleInputCodePageImpl(ULONG& codepage) noexcept
{
    codepage = m_inputCodepage;
    return;
}

void VtApiRoutines::GetConsoleOutputCodePageImpl(ULONG& codepage) noexcept
{
    codepage = m_outputCodepage;
    return;
}

void VtApiRoutines::GetConsoleInputModeImpl(InputBuffer& context,
                                            ULONG& mode) noexcept
{
    mode = m_inputMode;
    return;
}

void VtApiRoutines::GetConsoleOutputModeImpl(SCREEN_INFORMATION& context,
                                             ULONG& mode) noexcept
{
    mode = m_outputMode;
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleInputModeImpl(InputBuffer& context,
                                                             const ULONG mode) noexcept
{
    m_inputMode = mode;
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleOutputModeImpl(SCREEN_INFORMATION& context,
                                                              const ULONG Mode) noexcept
{
    m_outputMode = Mode;
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetNumberOfConsoleInputEventsImpl(const InputBuffer& context,
                                                                       ULONG& events) noexcept
{
    events = 0; // one per character that we have ready to return... or 0.
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::PeekConsoleInputAImpl(IConsoleInputObject& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                           const size_t eventsToRead,
                                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                                           std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::PeekConsoleInputWImpl(IConsoleInputObject& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                           const size_t eventsToRead,
                                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                                           std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleInputAImpl(IConsoleInputObject& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                           const size_t eventsToRead,
                                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                                           std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleInputWImpl(IConsoleInputObject& context,
                                                           std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                           const size_t eventsToRead,
                                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                                           std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleAImpl(IConsoleInputObject& context,
                                                      gsl::span<char> buffer,
                                                      size_t& written,
                                                      std::unique_ptr<IWaitRoutine>& waiter,
                                                      const std::string_view initialData,
                                                      const std::wstring_view exeName,
                                                      INPUT_READ_HANDLE_DATA& readHandleState,
                                                      const HANDLE clientHandle,
                                                      const DWORD controlWakeupMask,
                                                      DWORD& controlKeyState) noexcept
{
    return m_pUsualRoutines->ReadConsoleAImpl(context, buffer, written, waiter, initialData, exeName, readHandleState, clientHandle, controlWakeupMask, controlKeyState);
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleWImpl(IConsoleInputObject& context,
                                                      gsl::span<char> buffer,
                                                      size_t& written,
                                                      std::unique_ptr<IWaitRoutine>& waiter,
                                                      const std::string_view initialData,
                                                      const std::wstring_view exeName,
                                                      INPUT_READ_HANDLE_DATA& readHandleState,
                                                      const HANDLE clientHandle,
                                                      const DWORD controlWakeupMask,
                                                      DWORD& controlKeyState) noexcept
{
    return m_pUsualRoutines->ReadConsoleWImpl(context, buffer, written, waiter, initialData, exeName, readHandleState, clientHandle, controlWakeupMask, controlKeyState);
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleAImpl(IConsoleOutputObject& context,
                                                       const std::string_view buffer,
                                                       size_t& read,
                                                       bool requiresVtQuirk,
                                                       std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    if (CP_UTF8 == m_outputCodepage)
    {
        (void)m_pVtEngine->WriteTerminalUtf8(buffer);

    }
    else
    {
        (void)m_pVtEngine->WriteTerminalW(ConvertToW(m_outputCodepage, buffer));
    }

    (void)m_pVtEngine->_Flush();
    read = buffer.size();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleWImpl(IConsoleOutputObject& context,
                                                       const std::wstring_view buffer,
                                                       size_t& read,
                                                       bool requiresVtQuirk,
                                                       std::unique_ptr<IWaitRoutine>& waiter) noexcept
{
    (void)m_pVtEngine->WriteTerminalW(buffer);
    (void)m_pVtEngine->_Flush();
    read = buffer.size();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleLangIdImpl(LANGID& langId) noexcept
{
    return m_pUsualRoutines->GetConsoleLangIdImpl(langId);
}

[[nodiscard]] HRESULT VtApiRoutines::FillConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                                    const WORD attribute,
                                                                    const size_t lengthToWrite,
                                                                    const COORD startingCoordinate,
                                                                    size_t& cellsModified) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::FillConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                                     const char character,
                                                                     const size_t lengthToWrite,
                                                                     const COORD startingCoordinate,
                                                                     size_t& cellsModified) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::FillConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                                     const wchar_t character,
                                                                     const size_t lengthToWrite,
                                                                     const COORD startingCoordinate,
                                                                     size_t& cellsModified,
                                                                     const bool enablePowershellShim) noexcept
{
    return S_OK;
}

//// Process based. Restrict in protocol side?
//HRESULT GenerateConsoleCtrlEventImpl(const ULONG ProcessGroupFilter,
//                                             const ULONG ControlEvent);

void VtApiRoutines::SetConsoleActiveScreenBufferImpl(SCREEN_INFORMATION& newContext) noexcept
{
    return;
}

void VtApiRoutines::FlushConsoleInputBuffer(InputBuffer& context) noexcept
{
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleInputCodePageImpl(const ULONG codepage) noexcept
{
    m_inputCodepage = codepage;
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleOutputCodePageImpl(const ULONG codepage) noexcept
{
    m_outputCodepage = codepage;
    return S_OK;
}

void VtApiRoutines::GetConsoleCursorInfoImpl(const SCREEN_INFORMATION& context,
                                             ULONG& size,
                                             bool& isVisible) noexcept
{
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleCursorInfoImpl(SCREEN_INFORMATION& context,
                                                              const ULONG size,
                                                              const bool isVisible) noexcept
{
    return S_OK;
}

//// driver will pare down for non-Ex method
void VtApiRoutines::GetConsoleScreenBufferInfoExImpl(const SCREEN_INFORMATION& context,
                                                     CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept
{
    return m_pUsualRoutines->GetConsoleScreenBufferInfoExImpl(context, data);
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleScreenBufferInfoExImpl(SCREEN_INFORMATION& context,
                                                                      const CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleScreenBufferSizeImpl(SCREEN_INFORMATION& context,
                                                                    const COORD size) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleCursorPositionImpl(SCREEN_INFORMATION& context,
                                                                  const COORD position) noexcept
{
    return S_OK;
}

void VtApiRoutines::GetLargestConsoleWindowSizeImpl(const SCREEN_INFORMATION& context,
                                                    COORD& size) noexcept
{
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::ScrollConsoleScreenBufferAImpl(SCREEN_INFORMATION& context,
                                                                    const SMALL_RECT& source,
                                                                    const COORD target,
                                                                    std::optional<SMALL_RECT> clip,
                                                                    const char fillCharacter,
                                                                    const WORD fillAttribute) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ScrollConsoleScreenBufferWImpl(SCREEN_INFORMATION& context,
                                                                    const SMALL_RECT& source,
                                                                    const COORD target,
                                                                    std::optional<SMALL_RECT> clip,
                                                                    const wchar_t fillCharacter,
                                                                    const WORD fillAttribute,
                                                                    const bool enableCmdShim) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleTextAttributeImpl(SCREEN_INFORMATION& context,
                                                                 const WORD attribute) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleWindowInfoImpl(SCREEN_INFORMATION& context,
                                                              const bool isAbsolute,
                                                              const SMALL_RECT& windowRect) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputAttributeImpl(const SCREEN_INFORMATION& context,
                                                                    const COORD origin,
                                                                    gsl::span<WORD> buffer,
                                                                    size_t& written) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputCharacterAImpl(const SCREEN_INFORMATION& context,
                                                                     const COORD origin,
                                                                     gsl::span<char> buffer,
                                                                     size_t& written) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputCharacterWImpl(const SCREEN_INFORMATION& context,
                                                                     const COORD origin,
                                                                     gsl::span<wchar_t> buffer,
                                                                     size_t& written) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleInputAImpl(InputBuffer& context,
                                                            const gsl::span<const INPUT_RECORD> buffer,
                                                            size_t& written,
                                                            const bool append) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleInputWImpl(InputBuffer& context,
                                                            const gsl::span<const INPUT_RECORD> buffer,
                                                            size_t& written,
                                                            const bool append) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputAImpl(SCREEN_INFORMATION& context,
                                                             gsl::span<CHAR_INFO> buffer,
                                                             const Microsoft::Console::Types::Viewport& requestRectangle,
                                                             Microsoft::Console::Types::Viewport& writtenRectangle) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputWImpl(SCREEN_INFORMATION& context,
                                                             gsl::span<CHAR_INFO> buffer,
                                                             const Microsoft::Console::Types::Viewport& requestRectangle,
                                                             Microsoft::Console::Types::Viewport& writtenRectangle) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                                     const gsl::span<const WORD> attrs,
                                                                     const COORD target,
                                                                     size_t& used) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                                      const std::string_view text,
                                                                      const COORD target,
                                                                      size_t& used) noexcept
{
    if (m_outputCodepage == CP_UTF8)
    {
        (void)m_pVtEngine->_CursorPosition(target);
        (void)m_pVtEngine->WriteTerminalUtf8(text);
        (void)m_pVtEngine->_Flush();
        return S_OK;
    }
    else
    {
        return WriteConsoleOutputCharacterWImpl(OutContext, ConvertToW(m_outputCodepage, text), target, used);
    }
}

[[nodiscard]] HRESULT VtApiRoutines::WriteConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                                      const std::wstring_view text,
                                                                      const COORD target,
                                                                      size_t& used) noexcept
{
    (void)m_pVtEngine->_CursorPosition(target);
    (void)m_pVtEngine->WriteTerminalW(text);
    (void)m_pVtEngine->_Flush();
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputAImpl(const SCREEN_INFORMATION& context,
                                                            gsl::span<CHAR_INFO> buffer,
                                                            const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                            Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    CHAR_INFO fill;
    fill.Attributes = FOREGROUND_INTENSITY | FOREGROUND_RED | BACKGROUND_GREEN;
    fill.Char.AsciiChar = '?';

    std::fill(buffer.begin(), buffer.end(), fill);

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::ReadConsoleOutputWImpl(const SCREEN_INFORMATION& context,
                                                            gsl::span<CHAR_INFO> buffer,
                                                            const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                            Microsoft::Console::Types::Viewport& readRectangle) noexcept
{
    CHAR_INFO fill;
    fill.Attributes = FOREGROUND_INTENSITY | FOREGROUND_RED | BACKGROUND_GREEN;
    fill.Char.UnicodeChar = UNICODE_REPLACEMENT;

    std::fill(buffer.begin(), buffer.end(), fill);

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleTitleAImpl(gsl::span<char> title,
                                                          size_t& written,
                                                          size_t& needed) noexcept
{
    written = 0;
    needed = 0;

    if (!title.empty())
    {
        title.front() = ANSI_NULL;
    }

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleTitleWImpl(gsl::span<wchar_t> title,
                                                          size_t& written,
                                                          size_t& needed) noexcept
{
    written = 0;
    needed = 0;

    if (!title.empty())
    {
        title.front() = UNICODE_NULL;
    }

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleOriginalTitleAImpl(gsl::span<char> title,
                                                                  size_t& written,
                                                                  size_t& needed) noexcept
{
    written = 0;
    needed = 0;

    if (!title.empty())
    {
        title.front() = ANSI_NULL;
    }

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleOriginalTitleWImpl(gsl::span<wchar_t> title,
                                                                  size_t& written,
                                                                  size_t& needed) noexcept
{
    written = 0;
    needed = 0;

    if (!title.empty())
    {
        title.front() = UNICODE_NULL;
    }

    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleTitleAImpl(const std::string_view title) noexcept
{
    return SetConsoleTitleWImpl(ConvertToW(m_inputCodepage, title));
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleTitleWImpl(const std::wstring_view title) noexcept
{
    (void)m_pVtEngine->UpdateTitle(title);
    (void)m_pVtEngine->_Flush();
    return S_OK;
}

void VtApiRoutines::GetNumberOfConsoleMouseButtonsImpl(ULONG& buttons) noexcept
{
    buttons = 2;
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleFontSizeImpl(const SCREEN_INFORMATION& context,
                                                            const DWORD index,
                                                            COORD& size) noexcept
{
    size.X = 8;
    size.Y = 12;
    return S_OK;
}

//// driver will pare down for non-Ex method
[[nodiscard]] HRESULT VtApiRoutines::GetCurrentConsoleFontExImpl(const SCREEN_INFORMATION& context,
                                                                 const bool isForMaximumWindowSize,
                                                                 CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleDisplayModeImpl(SCREEN_INFORMATION& context,
                                                               const ULONG flags,
                                                               COORD& newSize) noexcept
{
    return S_OK;
}

void VtApiRoutines::GetConsoleDisplayModeImpl(ULONG& flags) noexcept
{
    flags = 0;
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::AddConsoleAliasAImpl(const std::string_view source,
                                                          const std::string_view target,
                                                          const std::string_view exeName) noexcept
{
    return m_pUsualRoutines->AddConsoleAliasAImpl(source, target, exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::AddConsoleAliasWImpl(const std::wstring_view source,
                                                          const std::wstring_view target,
                                                          const std::wstring_view exeName) noexcept
{
    return m_pUsualRoutines->AddConsoleAliasWImpl(source, target, exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasAImpl(const std::string_view source,
                                                          gsl::span<char> target,
                                                          size_t& written,
                                                          const std::string_view exeName) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasAImpl(source, target, written, exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasWImpl(const std::wstring_view source,
                                                          gsl::span<wchar_t> target,
                                                          size_t& written,
                                                          const std::wstring_view exeName) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasWImpl(source, target, written, exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasesLengthAImpl(const std::string_view exeName,
                                                                  size_t& bufferRequired) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasesLengthAImpl(exeName, bufferRequired);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasesLengthWImpl(const std::wstring_view exeName,
                                                                  size_t& bufferRequired) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasesLengthWImpl(exeName, bufferRequired);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasExesLengthAImpl(size_t& bufferRequired) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasExesLengthAImpl(bufferRequired);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasExesLengthWImpl(size_t& bufferRequired) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasExesLengthWImpl(bufferRequired);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasesAImpl(const std::string_view exeName,
                                                            gsl::span<char> alias,
                                                            size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasesAImpl(exeName, alias, written);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasesWImpl(const std::wstring_view exeName,
                                                            gsl::span<wchar_t> alias,
                                                            size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasesWImpl(exeName, alias, written);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasExesAImpl(gsl::span<char> aliasExes,
                                                              size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasExesAImpl(aliasExes, written);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleAliasExesWImpl(gsl::span<wchar_t> aliasExes,
                                                              size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleAliasExesWImpl(aliasExes, written);
}

[[nodiscard]] HRESULT VtApiRoutines::ExpungeConsoleCommandHistoryAImpl(const std::string_view exeName) noexcept
{
    return m_pUsualRoutines->ExpungeConsoleCommandHistoryAImpl(exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::ExpungeConsoleCommandHistoryWImpl(const std::wstring_view exeName) noexcept
{
    return m_pUsualRoutines->ExpungeConsoleCommandHistoryWImpl(exeName);
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleNumberOfCommandsAImpl(const std::string_view exeName,
                                                                     const size_t numberOfCommands) noexcept
{
    return m_pUsualRoutines->SetConsoleNumberOfCommandsAImpl(exeName, numberOfCommands);
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleNumberOfCommandsWImpl(const std::wstring_view exeName,
                                                                     const size_t numberOfCommands) noexcept
{
    return m_pUsualRoutines->SetConsoleNumberOfCommandsWImpl(exeName, numberOfCommands);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleCommandHistoryLengthAImpl(const std::string_view exeName,
                                                                         size_t& length) noexcept
{
    return m_pUsualRoutines->GetConsoleCommandHistoryLengthAImpl(exeName, length);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleCommandHistoryLengthWImpl(const std::wstring_view exeName,
                                                                         size_t& length) noexcept
{
    return m_pUsualRoutines->GetConsoleCommandHistoryLengthWImpl(exeName, length);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleCommandHistoryAImpl(const std::string_view exeName,
                                                                   gsl::span<char> commandHistory,
                                                                   size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleCommandHistoryAImpl(exeName, commandHistory, written);
}

[[nodiscard]] HRESULT VtApiRoutines::GetConsoleCommandHistoryWImpl(const std::wstring_view exeName,
                                                                   gsl::span<wchar_t> commandHistory,
                                                                   size_t& written) noexcept
{
    return m_pUsualRoutines->GetConsoleCommandHistoryWImpl(exeName, commandHistory, written);
}

void VtApiRoutines::GetConsoleWindowImpl(HWND& hwnd) noexcept
{
    hwnd = ServiceLocator::LocatePseudoWindow();
    return;
}

void VtApiRoutines::GetConsoleSelectionInfoImpl(CONSOLE_SELECTION_INFO& consoleSelectionInfo) noexcept
{
    consoleSelectionInfo = { 0 };
    return;
}

void VtApiRoutines::GetConsoleHistoryInfoImpl(CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept
{
    m_pUsualRoutines->GetConsoleHistoryInfoImpl(consoleHistoryInfo);
    return;
}

[[nodiscard]] HRESULT VtApiRoutines::SetConsoleHistoryInfoImpl(const CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept
{
    return m_pUsualRoutines->SetConsoleHistoryInfoImpl(consoleHistoryInfo);
}

[[nodiscard]] HRESULT VtApiRoutines::SetCurrentConsoleFontExImpl(IConsoleOutputObject& context,
                                                                 const bool isForMaximumWindowSize,
                                                                 const CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept
{
    return S_OK;
}

#pragma warning(pop)
