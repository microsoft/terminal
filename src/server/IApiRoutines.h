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
class InputBuffer;
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
    /*virtual HRESULT CreateInitialObjects(_Out_ InputBuffer** const ppInputObject,
                                          _Out_ SCREEN_INFORMATION** const ppOutputObject);
*/

#pragma endregion

    virtual ~IApiRoutines() = default;

#pragma region L1
    virtual void GetConsoleInputCodePageImpl(ULONG& codepage) noexcept = 0;

    virtual void GetConsoleOutputCodePageImpl(ULONG& codepage) noexcept = 0;

    virtual void GetConsoleInputModeImpl(InputBuffer& inputBuffer,
                                         ULONG& mode) noexcept = 0;

    virtual void GetConsoleOutputModeImpl(SCREEN_INFORMATION& screenInfo,
                                          ULONG& mode) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleInputModeImpl(InputBuffer& inputBuffer,
                                                          const ULONG mode) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleOutputModeImpl(SCREEN_INFORMATION& screenInfo,
                                                           const ULONG mode) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetNumberOfConsoleInputEventsImpl(const InputBuffer& inputBuffer,
                                                                    ULONG& events) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleInputImpl(InputBuffer& inputBuffer,
                                                      InputEventQueue& outEvents,
                                                      const size_t eventReadCount,
                                                      INPUT_READ_HANDLE_DATA& readHandleState,
                                                      const bool IsUnicode,
                                                      const bool IsPeek,
                                                      std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleImpl(InputBuffer& inputBuffer,
                                                  std::span<char> buffer,
                                                  size_t& written,
                                                  std::unique_ptr<IWaitRoutine>& waiter,
                                                  const std::wstring_view initialData,
                                                  const std::wstring_view exeName,
                                                  INPUT_READ_HANDLE_DATA& readHandleState,
                                                  const bool IsUnicode,
                                                  const HANDLE clientHandle,
                                                  const DWORD controlWakeupMask,
                                                  DWORD& controlKeyState) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleAImpl(SCREEN_INFORMATION& screenInfo,
                                                    const std::string_view buffer,
                                                    size_t& read,
                                                    bool requiresVtQuirk,
                                                    std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleWImpl(SCREEN_INFORMATION& screenInfo,
                                                    const std::wstring_view buffer,
                                                    size_t& read,
                                                    bool requiresVtQuirk,
                                                    std::unique_ptr<IWaitRoutine>& waiter) noexcept = 0;

#pragma region Thread Creation Info
    [[nodiscard]] virtual HRESULT GetConsoleLangIdImpl(LANGID& langId) noexcept = 0;
#pragma endregion

#pragma endregion

#pragma region L2

    [[nodiscard]] virtual HRESULT FillConsoleOutputAttributeImpl(SCREEN_INFORMATION& OutContext,
                                                                 const WORD attribute,
                                                                 const size_t lengthToWrite,
                                                                 const til::point startingCoordinate,
                                                                 size_t& cellsModified) noexcept = 0;

    [[nodiscard]] virtual HRESULT FillConsoleOutputCharacterAImpl(SCREEN_INFORMATION& OutContext,
                                                                  const char character,
                                                                  const size_t lengthToWrite,
                                                                  const til::point startingCoordinate,
                                                                  size_t& cellsModified) noexcept = 0;

    [[nodiscard]] virtual HRESULT FillConsoleOutputCharacterWImpl(SCREEN_INFORMATION& OutContext,
                                                                  const wchar_t character,
                                                                  const size_t lengthToWrite,
                                                                  const til::point startingCoordinate,
                                                                  size_t& cellsModified,
                                                                  const bool enablePowershellShim = false) noexcept = 0;

    virtual void SetConsoleActiveScreenBufferImpl(SCREEN_INFORMATION& newContext) noexcept = 0;

    virtual void FlushConsoleInputBuffer(InputBuffer& inputBuffer) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleInputCodePageImpl(const ULONG codepage) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleOutputCodePageImpl(const ULONG codepage) noexcept = 0;

    virtual void GetConsoleCursorInfoImpl(const SCREEN_INFORMATION& screenInfo,
                                          ULONG& size,
                                          bool& isVisible) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleCursorInfoImpl(SCREEN_INFORMATION& screenInfo,
                                                           const ULONG size,
                                                           const bool isVisible) noexcept = 0;

    // driver will pare down for non-Ex method
    virtual void GetConsoleScreenBufferInfoExImpl(const SCREEN_INFORMATION& screenInfo,
                                                  CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleScreenBufferInfoExImpl(SCREEN_INFORMATION& OutContext,
                                                                   const CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleScreenBufferSizeImpl(SCREEN_INFORMATION& screenInfo,
                                                                 const til::size size) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleCursorPositionImpl(SCREEN_INFORMATION& screenInfo,
                                                               const til::point position) noexcept = 0;

    virtual void GetLargestConsoleWindowSizeImpl(const SCREEN_INFORMATION& screenInfo,
                                                 til::size& size) noexcept = 0;

    [[nodiscard]] virtual HRESULT ScrollConsoleScreenBufferAImpl(SCREEN_INFORMATION& screenInfo,
                                                                 const til::inclusive_rect& source,
                                                                 const til::point target,
                                                                 std::optional<til::inclusive_rect> clip,
                                                                 const char fillCharacter,
                                                                 const WORD fillAttribute) noexcept = 0;

    [[nodiscard]] virtual HRESULT ScrollConsoleScreenBufferWImpl(SCREEN_INFORMATION& screenInfo,
                                                                 const til::inclusive_rect& source,
                                                                 const til::point target,
                                                                 std::optional<til::inclusive_rect> clip,
                                                                 const wchar_t fillCharacter,
                                                                 const WORD fillAttribute,
                                                                 const bool enableCmdShim = false) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleTextAttributeImpl(SCREEN_INFORMATION& screenInfo,
                                                              const WORD attribute) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleWindowInfoImpl(SCREEN_INFORMATION& screenInfo,
                                                           const bool isAbsolute,
                                                           const til::inclusive_rect& windowRect) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputAttributeImpl(const SCREEN_INFORMATION& screenInfo,
                                                                 const til::point origin,
                                                                 std::span<WORD> buffer,
                                                                 size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputCharacterAImpl(const SCREEN_INFORMATION& screenInfo,
                                                                  const til::point origin,
                                                                  std::span<char> buffer,
                                                                  size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputCharacterWImpl(const SCREEN_INFORMATION& screenInfo,
                                                                  const til::point origin,
                                                                  std::span<wchar_t> buffer,
                                                                  size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleInputAImpl(InputBuffer& inputBuffer,
                                                         const std::span<const INPUT_RECORD> buffer,
                                                         size_t& written,
                                                         const bool append) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleInputWImpl(InputBuffer& inputBuffer,
                                                         const std::span<const INPUT_RECORD> buffer,
                                                         size_t& written,
                                                         const bool append) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputAImpl(SCREEN_INFORMATION& screenInfo,
                                                          std::span<CHAR_INFO> buffer,
                                                          const Microsoft::Console::Types::Viewport& requestRectangle,
                                                          Microsoft::Console::Types::Viewport& writtenRectangle) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputWImpl(SCREEN_INFORMATION& screenInfo,
                                                          std::span<CHAR_INFO> buffer,
                                                          const Microsoft::Console::Types::Viewport& requestRectangle,
                                                          Microsoft::Console::Types::Viewport& writtenRectangle) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputAttributeImpl(SCREEN_INFORMATION& OutContext,
                                                                  const std::span<const WORD> attrs,
                                                                  const til::point target,
                                                                  size_t& used) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputCharacterAImpl(SCREEN_INFORMATION& OutContext,
                                                                   const std::string_view text,
                                                                   const til::point target,
                                                                   size_t& used) noexcept = 0;

    [[nodiscard]] virtual HRESULT WriteConsoleOutputCharacterWImpl(SCREEN_INFORMATION& OutContext,
                                                                   const std::wstring_view text,
                                                                   const til::point target,
                                                                   size_t& used) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputAImpl(const SCREEN_INFORMATION& screenInfo,
                                                         std::span<CHAR_INFO> buffer,
                                                         const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                         Microsoft::Console::Types::Viewport& readRectangle) noexcept = 0;

    [[nodiscard]] virtual HRESULT ReadConsoleOutputWImpl(const SCREEN_INFORMATION& screenInfo,
                                                         std::span<CHAR_INFO> buffer,
                                                         const Microsoft::Console::Types::Viewport& sourceRectangle,
                                                         Microsoft::Console::Types::Viewport& readRectangle) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleTitleAImpl(std::span<char> title,
                                                       size_t& written,
                                                       size_t& needed) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleTitleWImpl(std::span<wchar_t> title,
                                                       size_t& written,
                                                       size_t& needed) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleOriginalTitleAImpl(std::span<char> title,
                                                               size_t& written,
                                                               size_t& needed) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleOriginalTitleWImpl(std::span<wchar_t> title,
                                                               size_t& written,
                                                               size_t& needed) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleTitleAImpl(const std::string_view title) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleTitleWImpl(const std::wstring_view title) noexcept = 0;

#pragma endregion

#pragma region L3
    virtual void GetNumberOfConsoleMouseButtonsImpl(ULONG& buttons) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleFontSizeImpl(const SCREEN_INFORMATION& screenInfo,
                                                         const DWORD index,
                                                         til::size& size) noexcept = 0;

    // driver will pare down for non-Ex method
    [[nodiscard]] virtual HRESULT GetCurrentConsoleFontExImpl(const SCREEN_INFORMATION& screenInfo,
                                                              const bool isForMaximumWindowSize,
                                                              CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleDisplayModeImpl(SCREEN_INFORMATION& screenInfo,
                                                            const ULONG flags,
                                                            til::size& newSize) noexcept = 0;

    virtual void GetConsoleDisplayModeImpl(ULONG& flags) noexcept = 0;

    [[nodiscard]] virtual HRESULT AddConsoleAliasAImpl(const std::string_view source,
                                                       const std::string_view target,
                                                       const std::string_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT AddConsoleAliasWImpl(const std::wstring_view source,
                                                       const std::wstring_view target,
                                                       const std::wstring_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasAImpl(const std::string_view source,
                                                       std::span<char> target,
                                                       size_t& written,
                                                       const std::string_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasWImpl(const std::wstring_view source,
                                                       std::span<wchar_t> target,
                                                       size_t& written,
                                                       const std::wstring_view exeName) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasesLengthAImpl(const std::string_view exeName,
                                                               size_t& bufferRequired) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasesLengthWImpl(const std::wstring_view exeName,
                                                               size_t& bufferRequired) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasExesLengthAImpl(size_t& bufferRequired) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasExesLengthWImpl(size_t& bufferRequired) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasesAImpl(const std::string_view exeName,
                                                         std::span<char> alias,
                                                         size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasesWImpl(const std::wstring_view exeName,
                                                         std::span<wchar_t> alias,
                                                         size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasExesAImpl(std::span<char> aliasExes,
                                                           size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleAliasExesWImpl(std::span<wchar_t> aliasExes,
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
                                                                std::span<char> commandHistory,
                                                                size_t& written) noexcept = 0;

    [[nodiscard]] virtual HRESULT GetConsoleCommandHistoryWImpl(const std::wstring_view exeName,
                                                                std::span<wchar_t> commandHistory,
                                                                size_t& written) noexcept = 0;

#pragma endregion

    virtual void GetConsoleWindowImpl(HWND& hwnd) noexcept = 0;

    virtual void GetConsoleSelectionInfoImpl(CONSOLE_SELECTION_INFO& consoleSelectionInfo) noexcept = 0;

    virtual void GetConsoleHistoryInfoImpl(CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetConsoleHistoryInfoImpl(const CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept = 0;

    [[nodiscard]] virtual HRESULT SetCurrentConsoleFontExImpl(SCREEN_INFORMATION& screenInfo,
                                                              const bool isForMaximumWindowSize,
                                                              const CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept = 0;

#pragma endregion
};
