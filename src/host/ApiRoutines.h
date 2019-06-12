/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ApiRoutines.h

Abstract:
- This file defines the interface to respond to all API calls.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in srvinit.cpp, getset.cpp, directio.cpp, stream.cpp
--*/

#pragma once

#include "..\server\IApiRoutines.h"

class ApiRoutines : public IApiRoutines
{
public:
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

    [[nodiscard]] HRESULT PeekConsoleInputAImpl(IConsoleInputObject& context,
                                                std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                const size_t eventsToRead,
                                                INPUT_READ_HANDLE_DATA& readHandleState,
                                                std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

    [[nodiscard]] HRESULT PeekConsoleInputWImpl(IConsoleInputObject& context,
                                                std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                const size_t eventsToRead,
                                                INPUT_READ_HANDLE_DATA& readHandleState,
                                                std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleInputAImpl(IConsoleInputObject& context,
                                                std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                const size_t eventsToRead,
                                                INPUT_READ_HANDLE_DATA& readHandleState,
                                                std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleInputWImpl(IConsoleInputObject& context,
                                                std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                const size_t eventsToRead,
                                                INPUT_READ_HANDLE_DATA& readHandleState,
                                                std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleAImpl(IConsoleInputObject& context,
                                           gsl::span<char> buffer,
                                           size_t& written,
                                           std::unique_ptr<IWaitRoutine>& waiter,
                                           const std::string_view initialData,
                                           const std::wstring_view exeName,
                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                           const HANDLE clientHandle,
                                           const DWORD controlWakeupMask,
                                           DWORD& controlKeyState) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleWImpl(IConsoleInputObject& context,
                                           gsl::span<char> buffer,
                                           size_t& written,
                                           std::unique_ptr<IWaitRoutine>& waiter,
                                           const std::string_view initialData,
                                           const std::wstring_view exeName,
                                           INPUT_READ_HANDLE_DATA& readHandleState,
                                           const HANDLE clientHandle,
                                           const DWORD controlWakeupMask,
                                           DWORD& controlKeyState) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleAImpl(IConsoleOutputObject& context,
                                            const std::string_view buffer,
                                            size_t& read,
                                            std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleWImpl(IConsoleOutputObject& context,
                                            const std::wstring_view buffer,
                                            size_t& read,
                                            std::unique_ptr<IWaitRoutine>& waiter) noexcept override;

#pragma region ThreadCreationInfo
    [[nodiscard]] HRESULT GetConsoleLangIdImpl(LANGID& langId) noexcept override;
#pragma endregion

#pragma endregion

#pragma region L2

    [[nodiscard]] HRESULT FillConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                         const WORD attribute,
                                                         const size_t lengthToWrite,
                                                         const COORD startingCoordinate,
                                                         size_t& cellsModified) noexcept override;

    [[nodiscard]] HRESULT FillConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                          const char character,
                                                          const size_t lengthToWrite,
                                                          const COORD startingCoordinate,
                                                          size_t& cellsModified) noexcept override;

    [[nodiscard]] HRESULT FillConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                          const wchar_t character,
                                                          const size_t lengthToWrite,
                                                          const COORD startingCoordinate,
                                                          size_t& cellsModified) noexcept override;

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
                                                         const COORD size) noexcept override;

    [[nodiscard]] HRESULT SetConsoleCursorPositionImpl(SCREEN_INFORMATION& context,
                                                       const COORD position) noexcept override;

    void GetLargestConsoleWindowSizeImpl(const SCREEN_INFORMATION& context,
                                         COORD& size) noexcept override;

    [[nodiscard]] HRESULT ScrollConsoleScreenBufferAImpl(SCREEN_INFORMATION& context,
                                                         const SMALL_RECT& source,
                                                         const COORD target,
                                                         std::optional<SMALL_RECT> clip,
                                                         const char fillCharacter,
                                                         const WORD fillAttribute) noexcept override;

    [[nodiscard]] HRESULT ScrollConsoleScreenBufferWImpl(SCREEN_INFORMATION& context,
                                                         const SMALL_RECT& source,
                                                         const COORD target,
                                                         std::optional<SMALL_RECT> clip,
                                                         const wchar_t fillCharacter,
                                                         const WORD fillAttribute) noexcept override;

    [[nodiscard]] HRESULT SetConsoleTextAttributeImpl(SCREEN_INFORMATION& context,
                                                      const WORD attribute) noexcept override;

    [[nodiscard]] HRESULT SetConsoleWindowInfoImpl(SCREEN_INFORMATION& context,
                                                   const bool isAbsolute,
                                                   const SMALL_RECT& windowRect) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputAttributeImpl(const SCREEN_INFORMATION& context,
                                                         const COORD origin,
                                                         gsl::span<WORD> buffer,
                                                         size_t& written) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputCharacterAImpl(const SCREEN_INFORMATION& context,
                                                          const COORD origin,
                                                          gsl::span<char> buffer,
                                                          size_t& written) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputCharacterWImpl(const SCREEN_INFORMATION& context,
                                                          const COORD origin,
                                                          gsl::span<wchar_t> buffer,
                                                          size_t& written) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleInputAImpl(InputBuffer& context,
                                                 const std::basic_string_view<INPUT_RECORD> buffer,
                                                 size_t& written,
                                                 const bool append) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleInputWImpl(InputBuffer& context,
                                                 const std::basic_string_view<INPUT_RECORD> buffer,
                                                 size_t& written,
                                                 const bool append) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputAImpl(SCREEN_INFORMATION& context,
                                                  gsl::span<CHAR_INFO> buffer,
                                                  const Microsoft::Console::Types::Viewport& requestRectangle,
                                                  Microsoft::Console::Types::Viewport& writtenRectangle) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputWImpl(SCREEN_INFORMATION& context,
                                                  gsl::span<CHAR_INFO> buffer,
                                                  const Microsoft::Console::Types::Viewport& requestRectangle,
                                                  Microsoft::Console::Types::Viewport& writtenRectangle) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                          const std::basic_string_view<WORD> attrs,
                                                          const COORD target,
                                                          size_t& used) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                           const std::string_view text,
                                                           const COORD target,
                                                           size_t& used) noexcept override;

    [[nodiscard]] HRESULT WriteConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                           const std::wstring_view text,
                                                           const COORD target,
                                                           size_t& used) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputAImpl(const SCREEN_INFORMATION& context,
                                                 gsl::span<CHAR_INFO> buffer,
                                                 const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                 Microsoft::Console::Types::Viewport& readRectangle) noexcept override;

    [[nodiscard]] HRESULT ReadConsoleOutputWImpl(const SCREEN_INFORMATION& context,
                                                 gsl::span<CHAR_INFO> buffer,
                                                 const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                 Microsoft::Console::Types::Viewport& readRectangle) noexcept override;

    [[nodiscard]] HRESULT GetConsoleTitleAImpl(gsl::span<char> title,
                                               size_t& written,
                                               size_t& needed) noexcept override;

    [[nodiscard]] HRESULT GetConsoleTitleWImpl(gsl::span<wchar_t> title,
                                               size_t& written,
                                               size_t& needed) noexcept override;

    [[nodiscard]] HRESULT GetConsoleOriginalTitleAImpl(gsl::span<char> title,
                                                       size_t& written,
                                                       size_t& needed) noexcept override;

    [[nodiscard]] HRESULT GetConsoleOriginalTitleWImpl(gsl::span<wchar_t> title,
                                                       size_t& written,
                                                       size_t& needed) noexcept override;

    [[nodiscard]] HRESULT SetConsoleTitleAImpl(const std::string_view title) noexcept override;

    [[nodiscard]] HRESULT SetConsoleTitleWImpl(const std::wstring_view title) noexcept override;

#pragma endregion

#pragma region L3
    void GetNumberOfConsoleMouseButtonsImpl(ULONG& buttons) noexcept override;

    [[nodiscard]] HRESULT GetConsoleFontSizeImpl(const SCREEN_INFORMATION& context,
                                                 const DWORD index,
                                                 COORD& size) noexcept override;

    //// driver will pare down for non-Ex method
    [[nodiscard]] HRESULT GetCurrentConsoleFontExImpl(const SCREEN_INFORMATION& context,
                                                      const bool isForMaximumWindowSize,
                                                      CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept override;

    [[nodiscard]] HRESULT SetConsoleDisplayModeImpl(SCREEN_INFORMATION& context,
                                                    const ULONG flags,
                                                    COORD& newSize) noexcept override;

    void GetConsoleDisplayModeImpl(ULONG& flags) noexcept override;

    [[nodiscard]] HRESULT AddConsoleAliasAImpl(const std::string_view source,
                                               const std::string_view target,
                                               const std::string_view exeName) noexcept override;

    [[nodiscard]] HRESULT AddConsoleAliasWImpl(const std::wstring_view source,
                                               const std::wstring_view target,
                                               const std::wstring_view exeName) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasAImpl(const std::string_view source,
                                               gsl::span<char> target,
                                               size_t& written,
                                               const std::string_view exeName) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasWImpl(const std::wstring_view source,
                                               gsl::span<wchar_t> target,
                                               size_t& written,
                                               const std::wstring_view exeName) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasesLengthAImpl(const std::string_view exeName,
                                                       size_t& bufferRequired) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasesLengthWImpl(const std::wstring_view exeName,
                                                       size_t& bufferRequired) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasExesLengthAImpl(size_t& bufferRequired) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasExesLengthWImpl(size_t& bufferRequired) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasesAImpl(const std::string_view exeName,
                                                 gsl::span<char> alias,
                                                 size_t& written) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasesWImpl(const std::wstring_view exeName,
                                                 gsl::span<wchar_t> alias,
                                                 size_t& written) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasExesAImpl(gsl::span<char> aliasExes,
                                                   size_t& written) noexcept override;

    [[nodiscard]] HRESULT GetConsoleAliasExesWImpl(gsl::span<wchar_t> aliasExes,
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
                                                        gsl::span<char> commandHistory,
                                                        size_t& written) noexcept override;

    [[nodiscard]] HRESULT GetConsoleCommandHistoryWImpl(const std::wstring_view exeName,
                                                        gsl::span<wchar_t> commandHistory,
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
};
