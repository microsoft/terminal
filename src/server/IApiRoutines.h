/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IApiRoutines.h

Abstract:
- This file specifies the interface that must be defined by a server application to respond to all API calls.

Author:
- Michael Niksa (miniksa) 12-Oct-2016

Revision History:
- Adapted from original items in srvinit.cpp, getset.cpp, directio.cpp, stream.cpp
--*/

#pragma once

// TODO: 9115192 - Temporarily forward declare the real objects until I create an interface representing a console object
// This will be required so the server doesn't actually need to understand the implementation of a console object, just the few methods it needs to call.
class SCREEN_INFORMATION;
typedef SCREEN_INFORMATION IConsoleOutputObject;

class InputBuffer;
typedef InputBuffer IConsoleInputObject;

class INPUT_READ_HANDLE_DATA;

#include "IWaitRoutine.h"
#include <deque>
#include <memory>
#include "../types/inc/IInputEvent.hpp"
#include "../types/inc/viewport.hpp"

class IApiRoutines
{
public:
#pragma region ObjectManagement
    // TODO: 9115192 - We will need to make the objects via an interface eventually. This represents that idea.
    /*virtual HRESULT CreateInitialObjects(_Out_ IConsoleInputObject** const ppInputObject,
                                          _Out_ IConsoleOutputObject** const ppOutputObject);
*/

#pragma endregion

#pragma region L1
    virtual void GetConsoleInputCodePageImpl(ULONG& codepage) noexcept = 0;

    virtual void GetConsoleOutputCodePageImpl(ULONG& codepage) noexcept = 0;

    virtual void GetConsoleInputModeImpl(InputBuffer& context,
                                         ULONG& mode) noexcept = 0;

    virtual void GetConsoleOutputModeImpl(SCREEN_INFORMATION& context,
                                          ULONG& mode) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleInputModeImpl(IConsoleInputObject& context,
                                                          const ULONG mode) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleOutputModeImpl(IConsoleOutputObject& context,
                                                           const ULONG mode) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetNumberOfConsoleInputEventsImpl(const IConsoleInputObject& context,
                                                                    ULONG& events) noexcept = 0;

    [[nodiscard]] virtual HRESULT PeekConsoleInputAImpl(IConsoleInputObject& context,
                                                        std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                        const size_t eventsToRead,
                                                        INPUT_READ_HANDLE_DATA& readHandleState,
                                                        std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

    [[nodiscard]] virtual HRESULT PeekConsoleInputWImpl(IConsoleInputObject& context,
                                                        std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                        const size_t eventsToRead,
                                                        INPUT_READ_HANDLE_DATA& readHandleState,
                                                        std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleInputAImpl(IConsoleInputObject& context,
                                                        std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                        const size_t eventsToRead,
                                                        INPUT_READ_HANDLE_DATA& readHandleState,
                                                        std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleInputWImpl(IConsoleInputObject& context,
                                                        std::deque<std::unique_ptr<IInputEvent>>& outEvents,
                                                        const size_t eventsToRead,
                                                        INPUT_READ_HANDLE_DATA& readHandleState,
                                                        std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleAImpl(IConsoleInputObject& context,
                                                   gsl::span<char> buffer,
                                                   size_t& written,
                                                   std::unique_ptr<IWaitRoutine>& waiter,
                                                   const std::string_view initialData,
                                                   const std::wstring_view exeName,
                                                   INPUT_READ_HANDLE_DATA& readHandleState,
                                                   const HANDLE clientHandle,
                                                   const DWORD controlWakeupMask,
                                                   DWORD& controlKeyState) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleWImpl(IConsoleInputObject& context,
                                                   gsl::span<char> buffer,
                                                   size_t& written,
                                                   std::unique_ptr<IWaitRoutine>& waiter,
                                                   const std::string_view initialData,
                                                   const std::wstring_view exeName,
                                                   INPUT_READ_HANDLE_DATA& readHandleState,
                                                   const HANDLE clientHandle,
                                                   const DWORD controlWakeupMask,
                                                   DWORD& controlKeyState) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleAImpl(IConsoleOutputObject& context,
                                                    const std::string_view buffer,
                                                    size_t& read,
                                                    std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleWImpl(IConsoleOutputObject& context,
                                                    const std::wstring_view buffer,
                                                    size_t& read,
                                                    std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

#pragma region Thread Creation Info
    [[nodiscard]] virtual HRESULT GetConsoleLangIdImpl(LANGID& langId) noexcept = 0;
#pragma endregion

#pragma endregion

#pragma region L2

    [[nodiscard]] virtual HRESULT FillConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                                 const WORD attribute,
                                                                 const size_t lengthToWrite,
                                                                 const COORD startingCoordinate,
                                                                 size_t& cellsModified) noexcept = 0;

    [[nodiscard]] virtual HRESULT FillConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                                  const char character,
                                                                  const size_t lengthToWrite,
                                                                  const COORD startingCoordinate,
                                                                  size_t& cellsModified) noexcept = 0;

    [[nodiscard]] virtual HRESULT FillConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                                  const wchar_t character,
                                                                  const size_t lengthToWrite,
                                                                  const COORD startingCoordinate,
                                                                  size_t& cellsModified) noexcept = 0;

    virtual void SetConsoleActiveScreenBufferImpl(IConsoleOutputObject& newContext) noexcept = 0;

    virtual void FlushConsoleInputBuffer(IConsoleInputObject& context) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleInputCodePageImpl(const ULONG codepage) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleOutputCodePageImpl(const ULONG codepage) noexcept = 0;

    virtual void GetConsoleCursorInfoImpl(const SCREEN_INFORMATION& context,
                                          ULONG& size,
                                          bool& isVisible) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleCursorInfoImpl(IConsoleOutputObject& context,
                                                           const ULONG size,
                                                           const bool isVisible) noexcept = 0;

    // driver will pare down for non-Ex method
    virtual void GetConsoleScreenBufferInfoExImpl(const IConsoleOutputObject& context,
                                                  CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleScreenBufferInfoExImpl(IConsoleOutputObject& OutContext,
                                                                   const CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleScreenBufferSizeImpl(IConsoleOutputObject& context,
                                                                 const COORD size) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleCursorPositionImpl(IConsoleOutputObject& context,
                                                               const COORD position) noexcept = 0;

    virtual void GetLargestConsoleWindowSizeImpl(const IConsoleOutputObject& context,
                                                 COORD& size) noexcept = 0;

    [[nodiscard]] virtual HRESULT ScrollConsoleScreenBufferAImpl(IConsoleOutputObject& context,
                                                                 const SMALL_RECT& source,
                                                                 const COORD target,
                                                                 std::optional<SMALL_RECT> clip,
                                                                 const char fillCharacter,
                                                                 const WORD fillAttribute) noexcept = 0;

    [[nodiscard]] virtual HRESULT ScrollConsoleScreenBufferWImpl(IConsoleOutputObject& context,
                                                                 const SMALL_RECT& source,
                                                                 const COORD target,
                                                                 std::optional<SMALL_RECT> clip,
                                                                 const wchar_t fillCharacter,
                                                                 const WORD fillAttribute) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleTextAttributeImpl(IConsoleOutputObject& context,
                                                              const WORD attribute) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleWindowInfoImpl(IConsoleOutputObject& context,
                                                           const bool isAbsolute,
                                                           const SMALL_RECT& windowRect) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputAttributeImpl(const IConsoleOutputObject& context,
                                                                 const COORD origin,
                                                                 gsl::span<WORD> buffer,
                                                                 size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputCharacterAImpl(const IConsoleOutputObject& context,
                                                                  const COORD origin,
                                                                  gsl::span<char> buffer,
                                                                  size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputCharacterWImpl(const IConsoleOutputObject& context,
                                                                  const COORD origin,
                                                                  gsl::span<wchar_t> buffer,
                                                                  size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleInputAImpl(IConsoleInputObject& context,
                                                         const std::basic_string_view<INPUT_RECORD> buffer,
                                                         size_t& written,
                                                         const bool append) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleInputWImpl(IConsoleInputObject& context,
                                                         const std::basic_string_view<INPUT_RECORD> buffer,
                                                         size_t& written,
                                                         const bool append) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputAImpl(IConsoleOutputObject& context,
                                                          gsl::span<CHAR_INFO> buffer,
                                                          const Microsoft::Console::Types::Viewport& requestRectangle,
                                                          Microsoft::Console::Types::Viewport& writtenRectangle) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputWImpl(IConsoleOutputObject& context,
                                                          gsl::span<CHAR_INFO> buffer,
                                                          const Microsoft::Console::Types::Viewport& requestRectangle,
                                                          Microsoft::Console::Types::Viewport& writtenRectangle) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputAttributeImpl(IConsoleOutputObject& OutContext,
                                                                  const std::basic_string_view<WORD> attrs,
                                                                  const COORD target,
                                                                  size_t& used) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputCharacterAImpl(IConsoleOutputObject& OutContext,
                                                                   const std::string_view text,
                                                                   const COORD target,
                                                                   size_t& used) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputCharacterWImpl(IConsoleOutputObject& OutContext,
                                                                   const std::wstring_view text,
                                                                   const COORD target,
                                                                   size_t& used) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputAImpl(const IConsoleOutputObject& context,
                                                         gsl::span<CHAR_INFO> buffer,
                                                         const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                         Microsoft::Console::Types::Viewport& readRectangle) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputWImpl(const IConsoleOutputObject& context,
                                                         gsl::span<CHAR_INFO> buffer,
                                                         const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                         Microsoft::Console::Types::Viewport& readRectangle) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleTitleAImpl(gsl::span<char> title,
                                                       size_t& written,
                                                       size_t& needed) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleTitleWImpl(gsl::span<wchar_t> title,
                                                       size_t& written,
                                                       size_t& needed) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleOriginalTitleAImpl(gsl::span<char> title,
                                                               size_t& written,
                                                               size_t& needed) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleOriginalTitleWImpl(gsl::span<wchar_t> title,
                                                               size_t& written,
                                                               size_t& needed) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleTitleAImpl(const std::string_view title) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleTitleWImpl(const std::wstring_view title) noexcept = 0;

#pragma endregion

#pragma region L3
    virtual void GetNumberOfConsoleMouseButtonsImpl(ULONG& buttons) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleFontSizeImpl(const SCREEN_INFORMATION& context,
                                                         const DWORD index,
                                                         COORD& size) noexcept = 0;

    // driver will pare down for non-Ex method
    [[nodiscard]] virtual HRESULT GetCurrentConsoleFontExImpl(const SCREEN_INFORMATION& context,
                                                              const bool isForMaximumWindowSize,
                                                              CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleDisplayModeImpl(SCREEN_INFORMATION& context,
                                                            const ULONG flags,
                                                            COORD& newSize) noexcept = 0;

    virtual void GetConsoleDisplayModeImpl(ULONG& flags) noexcept = 0;

    [[nodiscard]] virtual HRESULT AddConsoleAliasAImpl(const std::string_view source,
                                                       const std::string_view target,
                                                       const std::string_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT AddConsoleAliasWImpl(const std::wstring_view source,
                                                       const std::wstring_view target,
                                                       const std::wstring_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasAImpl(const std::string_view source,
                                                       gsl::span<char> target,
                                                       size_t& written,
                                                       const std::string_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasWImpl(const std::wstring_view source,
                                                       gsl::span<wchar_t> target,
                                                       size_t& written,
                                                       const std::wstring_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasesLengthAImpl(const std::string_view exeName,
                                                               size_t& bufferRequired) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasesLengthWImpl(const std::wstring_view exeName,
                                                               size_t& bufferRequired) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasExesLengthAImpl(size_t& bufferRequired) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasExesLengthWImpl(size_t& bufferRequired) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasesAImpl(const std::string_view exeName,
                                                         gsl::span<char> alias,
                                                         size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasesWImpl(const std::wstring_view exeName,
                                                         gsl::span<wchar_t> alias,
                                                         size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasExesAImpl(gsl::span<char> aliasExes,
                                                           size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasExesWImpl(gsl::span<wchar_t> aliasExes,
                                                           size_t& written) noexcept = 0;

#pragma region CMDext Private API

    [[nodiscard]] virtual HRESULT ExpungeConsoleCommandHistoryAImpl(const std::string_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT ExpungeConsoleCommandHistoryWImpl(const std::wstring_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleNumberOfCommandsAImpl(const std::string_view exeName,
                                                                  const size_t numberOfCommands) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleNumberOfCommandsWImpl(const std::wstring_view exeName,
                                                                  const size_t numberOfCommands) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleCommandHistoryLengthAImpl(const std::string_view exeName,
                                                                      size_t& length) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleCommandHistoryLengthWImpl(const std::wstring_view exeName,
                                                                      size_t& length) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleCommandHistoryAImpl(const std::string_view exeName,
                                                                gsl::span<char> commandHistory,
                                                                size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleCommandHistoryWImpl(const std::wstring_view exeName,
                                                                gsl::span<wchar_t> commandHistory,
                                                                size_t& written) noexcept = 0;

#pragma endregion

    virtual void GetConsoleWindowImpl(HWND& hwnd) noexcept = 0;

    virtual void GetConsoleSelectionInfoImpl(CONSOLE_SELECTION_INFO& consoleSelectionInfo) noexcept = 0;

    virtual void GetConsoleHistoryInfoImpl(CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleHistoryInfoImpl(const CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetCurrentConsoleFontExImpl(IConsoleOutputObject& context,
                                                              const bool isForMaximumWindowSize,
                                                              const CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept = 0;

#pragma endregion
};
