/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- VtApiRoutines.h

Abstract:
- This file defines the interface to respond to all API calls by using VT on behalf of the client

Author:
- Michael Niksa (miniksa) 26-Jul-2021

Revision History:
- Adapted from original items in srvinit.cpp, getset.cpp, directio.cpp, stream.cpp
--*/

#pragma once

#include "../server/IApiRoutines.h"
#include "../renderer/vt/Xterm256Engine.hpp"

class VtApiRoutines : public IApiRoutines
{
public:
    VtApiRoutines();

#pragma region ObjectManagement
    /*HRESULT CreateInitialObjects(_Out_ InputBuffer** const ppInputObject,
    _Out_ SCREEN_INFORMATION** const ppOutputObject);
    */

#pragma endregion

#pragma region L1
    void GetConsoleInputCodePageImpl(ULONG& codepage) noexcept override;

    void GetConsoleOutputCodePageImpl(ULONG& codepage) noexcept override;

    void GetConsoleInputModeImpl(InputBuffer& context,
                                 ULONG& mode) noexcept override;

    void GetConsoleOutputModeImpl(SCREEN_INFORMATION& context,
                                  ULONG& mode) noexcept override;

    [[nodiscard]] HRESULT SetConsoleInputModeImpl(InputBuffer& context,
                                                  const ULONG mode) noexcept override;

    [[nodiscard]] HRESULT SetConsoleOutputModeImpl(SCREEN_INFORMATION& context,
                                                   const ULONG Mode) noexcept override;

    [[nodiscard]] HRESULT GetNumberOfConsoleInputEventsImpl(const InputBuffer& context,
                                                            ULONG& events) noexcept override;

    [[nodiscard]] HRESULT GetConsoleInputImpl(IConsoleInputObject& context,
                                              InputEventQueue& outEvents,
                                              const size_t eventReadCount,
                                              INPUT_READ_HANDLE_DATA& readHandleState,
                                              const bool IsUnicode,
                                              const bool IsPeek,
                                              std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleImpl(IConsoleInputObject& context,
                                          std::span<char> buffer,
                                          size_t& written,
                                          std::unique_ptr<IWaitRoutine>& waiter,
                                          const std::wstring_view initialData,
                                          const std::wstring_view exeName,
                                          INPUT_READ_HANDLE_DATA& readHandleState,
                                          const bool IsUnicode,
                                          const HANDLE clientHandle,
                                          const DWORD controlWakeupMask,
                                          DWORD& controlKeyState) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleAImpl(IConsoleOutputObject& context,
                                            const std::string_view buffer,
                                            size_t& read,
                                            bool requiresVtQuirk,
                                            std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleWImpl(IConsoleOutputObject& context,
                                            const std::wstring_view buffer,
                                            size_t& read,
                                            bool requiresVtQuirk,
                                            std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

#pragma region ThreadCreationInfo
    [[nodiscard]] HRESULT GetConsoleLangIdImpl(LANGID& langId) noexcept override;
#pragma endregion

#pragma endregion

#pragma region L2

    [[nodiscard]] HRESULT FillConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                         const WORD attribute,
                                                         const size_t lengthToWrite,
                                                         const til::point startingCoordinate,
                                                         size_t& cellsModified) noexcept override;

    [[nodiscard]] HRESULT FillConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                          const char character,
                                                          const size_t lengthToWrite,
                                                          const til::point startingCoordinate,
                                                          size_t& cellsModified) noexcept override;

    [[nodiscard]] HRESULT FillConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                          const wchar_t character,
                                                          const size_t lengthToWrite,
                                                          const til::point startingCoordinate,
                                                          size_t& cellsModified,
                                                          const bool enablePowershellShim = false) noexcept override;

    //// Process based. Restrict in protocol side?
    //HRESULT GenerateConsoleCtrlEventImpl(const ULONG ProcessGroupFilter,
    //                                             const ULONG ControlEvent);

    void SetConsoleActiveScreenBufferImpl(SCREEN_INFORMATION& newContext) noexcept override;

    void FlushConsoleInputBuffer(InputBuffer& context) noexcept override;

    [[nodiscard]] HRESULT SetConsoleInputCodePageImpl(const ULONG codepage) noexcept override;

    [[nodiscard]] HRESULT SetConsoleOutputCodePageImpl(const ULONG codepage) noexcept override;

    void GetConsoleCursorInfoImpl(const SCREEN_INFORMATION& context,
                                  ULONG& size,
                                  bool& isVisible) noexcept override;

    [[nodiscard]] HRESULT SetConsoleCursorInfoImpl(SCREEN_INFORMATION& context,
                                                   const ULONG size,
                                                   const bool isVisible) noexcept override;

    //// driver will pare down for non-Ex method
    void GetConsoleScreenBufferInfoExImpl(const SCREEN_INFORMATION& context,
                                          CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept override;

    [[nodiscard]] HRESULT SetConsoleScreenBufferInfoExImpl(SCREEN_INFORMATION& context,
                                                           const CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept override;

    [[nodiscard]] HRESULT SetConsoleScreenBufferSizeImpl(SCREEN_INFORMATION& context,
                                                         const til::size size) noexcept override;

    [[nodiscard]] HRESULT SetConsoleCursorPositionImpl(SCREEN_INFORMATION& context,
                                                       const til::point position) noexcept override;

    void GetLargestConsoleWindowSizeImpl(const SCREEN_INFORMATION& context,
                                         til::size& size) noexcept override;

    [[nodiscard]] HRESULT ScrollConsoleScreenBufferAImpl(SCREEN_INFORMATION& context,
                                                         const til::inclusive_rect& source,
                                                         const til::point target,
                                                         std::optional<til::inclusive_rect> clip,
                                                         const char fillCharacter,
                                                         const WORD fillAttribute) noexcept override;

    [[nodiscard]] HRESULT ScrollConsoleScreenBufferWImpl(SCREEN_INFORMATION& context,
                                                         const til::inclusive_rect& source,
                                                         const til::point target,
                                                         std::optional<til::inclusive_rect> clip,
                                                         const wchar_t fillCharacter,
                                                         const WORD fillAttribute,
                                                         const bool enableCmdShim = false) noexcept override;

    [[nodiscard]] HRESULT SetConsoleTextAttributeImpl(SCREEN_INFORMATION& context,
                                                      const WORD attribute) noexcept override;

    [[nodiscard]] HRESULT SetConsoleWindowInfoImpl(SCREEN_INFORMATION& context,
                                                   const bool isAbsolute,
                                                   const til::inclusive_rect& windowRect) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputAttributeImpl(const SCREEN_INFORMATION& context,
                                                         const til::point origin,
                                                         std::span<WORD> buffer,
                                                         size_t& written) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputCharacterAImpl(const SCREEN_INFORMATION& context,
                                                          const til::point origin,
                                                          std::span<char> buffer,
                                                          size_t& written) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputCharacterWImpl(const SCREEN_INFORMATION& context,
                                                          const til::point origin,
                                                          std::span<wchar_t> buffer,
                                                          size_t& written) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleInputAImpl(InputBuffer& context,
                                                 const std::span<const INPUT_RECORD> buffer,
                                                 size_t& written,
                                                 const bool append) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleInputWImpl(InputBuffer& context,
                                                 const std::span<const INPUT_RECORD> buffer,
                                                 size_t& written,
                                                 const bool append) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputAImpl(SCREEN_INFORMATION& context,
                                                  std::span<CHAR_INFO> buffer,
                                                  const Microsoft::Console::Types::Viewport& requestRectangle,
                                                  Microsoft::Console::Types::Viewport& writtenRectangle) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputWImpl(SCREEN_INFORMATION& context,
                                                  std::span<CHAR_INFO> buffer,
                                                  const Microsoft::Console::Types::Viewport& requestRectangle,
                                                  Microsoft::Console::Types::Viewport& writtenRectangle) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                          const std::span<const WORD> attrs,
                                                          const til::point target,
                                                          size_t& used) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                           const std::string_view text,
                                                           const til::point target,
                                                           size_t& used) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                           const std::wstring_view text,
                                                           const til::point target,
                                                           size_t& used) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputAImpl(const SCREEN_INFORMATION& context,
                                                 std::span<CHAR_INFO> buffer,
                                                 const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                 Microsoft::Console::Types::Viewport& readRectangle) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputWImpl(const SCREEN_INFORMATION& context,
                                                 std::span<CHAR_INFO> buffer,
                                                 const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                 Microsoft::Console::Types::Viewport& readRectangle) noexcept override;

    [[nodiscard]] HRESULT GetConsoleTitleAImpl(std::span<char> title,
                                               size_t& written,
                                               size_t& needed) noexcept override;

    [[nodiscard]] HRESULT GetConsoleTitleWImpl(std::span<wchar_t> title,
                                               size_t& written,
                                               size_t& needed) noexcept override;

    [[nodiscard]] HRESULT GetConsoleOriginalTitleAImpl(std::span<char> title,
                                                       size_t& written,
                                                       size_t& needed) noexcept override;

    [[nodiscard]] HRESULT GetConsoleOriginalTitleWImpl(std::span<wchar_t> title,
                                                       size_t& written,
                                                       size_t& needed) noexcept override;

    [[nodiscard]] HRESULT SetConsoleTitleAImpl(const std::string_view title) noexcept override;

    [[nodiscard]] HRESULT SetConsoleTitleWImpl(const std::wstring_view title) noexcept override;

#pragma endregion

#pragma region L3
    void GetNumberOfConsoleMouseButtonsImpl(ULONG& buttons) noexcept override;

    [[nodiscard]] HRESULT GetConsoleFontSizeImpl(const SCREEN_INFORMATION& context,
                                                 const DWORD index,
                                                 til::size& size) noexcept override;

    //// driver will pare down for non-Ex method
    [[nodiscard]] HRESULT GetCurrentConsoleFontExImpl(const SCREEN_INFORMATION& context,
                                                      const bool isForMaximumWindowSize,
                                                      CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept override;

    [[nodiscard]] HRESULT SetConsoleDisplayModeImpl(SCREEN_INFORMATION& context,
                                                    const ULONG flags,
                                                    til::size& newSize) noexcept override;

    void GetConsoleDisplayModeImpl(ULONG& flags) noexcept override;

    [[nodiscard]] HRESULT AddConsoleAliasAImpl(const std::string_view source,
                                               const std::string_view target,
                                               const std::string_view exeName) noexcept override;

    [[nodiscard]] HRESULT AddConsoleAliasWImpl(const std::wstring_view source,
                                               const std::wstring_view target,
                                               const std::wstring_view exeName) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasAImpl(const std::string_view source,
                                               std::span<char> target,
                                               size_t& written,
                                               const std::string_view exeName) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasWImpl(const std::wstring_view source,
                                               std::span<wchar_t> target,
                                               size_t& written,
                                               const std::wstring_view exeName) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasesLengthAImpl(const std::string_view exeName,
                                                       size_t& bufferRequired) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasesLengthWImpl(const std::wstring_view exeName,
                                                       size_t& bufferRequired) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasExesLengthAImpl(size_t& bufferRequired) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasExesLengthWImpl(size_t& bufferRequired) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasesAImpl(const std::string_view exeName,
                                                 std::span<char> alias,
                                                 size_t& written) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasesWImpl(const std::wstring_view exeName,
                                                 std::span<wchar_t> alias,
                                                 size_t& written) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasExesAImpl(std::span<char> aliasExes,
                                                   size_t& written) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasExesWImpl(std::span<wchar_t> aliasExes,
                                                   size_t& written) noexcept override;

#pragma region CMDext Private API

    [[nodiscard]] HRESULT ExpungeConsoleCommandHistoryAImpl(const std::string_view exeName) noexcept override;

    [[nodiscard]] HRESULT ExpungeConsoleCommandHistoryWImpl(const std::wstring_view exeName) noexcept override;

    [[nodiscard]] HRESULT SetConsoleNumberOfCommandsAImpl(const std::string_view exeName,
                                                          const size_t numberOfCommands) noexcept override;

    [[nodiscard]] HRESULT SetConsoleNumberOfCommandsWImpl(const std::wstring_view exeName,
                                                          const size_t numberOfCommands) noexcept override;

    [[nodiscard]] HRESULT GetConsoleCommandHistoryLengthAImpl(const std::string_view exeName,
                                                              size_t& length) noexcept override;

    [[nodiscard]] HRESULT GetConsoleCommandHistoryLengthWImpl(const std::wstring_view exeName,
                                                              size_t& length) noexcept override;

    [[nodiscard]] HRESULT GetConsoleCommandHistoryAImpl(const std::string_view exeName,
                                                        std::span<char> commandHistory,
                                                        size_t& written) noexcept override;

    [[nodiscard]] HRESULT GetConsoleCommandHistoryWImpl(const std::wstring_view exeName,
                                                        std::span<wchar_t> commandHistory,
                                                        size_t& written) noexcept override;

#pragma endregion

    void GetConsoleWindowImpl(HWND& hwnd) noexcept override;

    void GetConsoleSelectionInfoImpl(CONSOLE_SELECTION_INFO& consoleSelectionInfo) noexcept override;

    void GetConsoleHistoryInfoImpl(CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept override;

    [[nodiscard]] HRESULT SetConsoleHistoryInfoImpl(const CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept override;

    [[nodiscard]] HRESULT SetCurrentConsoleFontExImpl(IConsoleOutputObject& context,
                                                      const bool isForMaximumWindowSize,
                                                      const CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept override;

#pragma endregion

    IApiRoutines* m_pUsualRoutines;
    UINT& m_inputCodepage;
    UINT& m_outputCodepage;
    ULONG m_inputMode;
    ULONG m_outputMode;
    bool m_listeningForDSR;
    Microsoft::Console::Render::Xterm256Engine* m_pVtEngine;

private:
    void _SynchronizeCursor(std::unique_ptr<IWaitRoutine>& waiter) noexcept;
};
