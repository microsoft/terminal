// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ApiRoutines.h"

#include "_stream.h"
#include "stream.h"
#include "writeData.hpp"

#include "_output.h"
#include "output.h"
#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "VtIo.hpp"

#include "../types/inc/convert.hpp"
#include "../types/inc/Viewport.hpp"

#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;
using Microsoft::Console::Interactivity::ServiceLocator;

constexpr bool controlCharPredicate(wchar_t wch)
{
    return wch < L' ' || wch == 0x007F;
}

static auto raiseAccessibilityEventsOnExit(SCREEN_INFORMATION& screenInfo)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto bufferBefore = &gci.GetActiveOutputBuffer();
    const auto cursorBefore = bufferBefore->GetTextBuffer().GetCursor().GetPosition();

    auto raise = wil::scope_exit([bufferBefore, cursorBefore] {
        // !!! NOTE !!! `bufferBefore` may now be a stale pointer, because VT
        // sequences can switch between the main and alternative screen buffer.
        auto& an = ServiceLocator::LocateGlobals().accessibilityNotifier;
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        const auto& bufferAfter = &gci.GetActiveOutputBuffer();
        const auto cursorAfter = bufferAfter->GetTextBuffer().GetCursor().GetPosition();

        if (cursorBefore != cursorAfter)
        {
            if (bufferBefore == bufferAfter && an.WantsRegionChangedEvents())
            {
                // Make the range ordered...
                auto beg = cursorBefore;
                auto end = cursorAfter;
                if (beg > end)
                {
                    std::swap(beg, end);
                }

                // ...and make it inclusive.
                end.x--;
                if (end.x < 0)
                {
                    end.y--;
                    end.x = bufferAfter->GetBufferSize().Width() - 1;
                }

                an.RegionChanged(beg, end);
            }

            an.CursorChanged(cursorAfter, false);
        }
    });

    // Don't raise any events for inactive buffers.
    if (bufferBefore != &screenInfo)
    {
        raise.release();
    }

    return raise;
}

// Routine Description:
// - This routine updates the cursor position.  Its input is the non-special
//   cased new location of the cursor.  For example, if the cursor were being
//   moved one space backwards from the left edge of the screen, the X
//   coordinate would be -1.  This routine would set the X coordinate to
//   the right edge of the screen and decrement the Y coordinate by one.
// Arguments:
// - screenInfo - reference to screen buffer information structure.
// - coordCursor - New location of cursor.
// - fKeepCursorVisible - TRUE if changing window origin desirable when hit right edge
// Return Value:
static void AdjustCursorPosition(SCREEN_INFORMATION& screenInfo, _In_ til::point coordCursor, _Inout_opt_ til::CoordType* psScrollY)
{
    const auto bufferSize = screenInfo.GetBufferSize().Dimensions();
    if (coordCursor.x < 0)
    {
        if (coordCursor.y > 0)
        {
            coordCursor.x = bufferSize.width + coordCursor.x;
            coordCursor.y = coordCursor.y - 1;
        }
        else
        {
            coordCursor.x = 0;
        }
    }
    else if (coordCursor.x >= bufferSize.width)
    {
        // at end of line. if wrap mode, wrap cursor.  otherwise leave it where it is.
        if (screenInfo.OutputMode & ENABLE_WRAP_AT_EOL_OUTPUT)
        {
            coordCursor.y += coordCursor.x / bufferSize.width;
            coordCursor.x = coordCursor.x % bufferSize.width;
        }
        else
        {
            coordCursor.x = screenInfo.GetTextBuffer().GetCursor().GetPosition().x;
        }
    }

    if (coordCursor.y >= bufferSize.height)
    {
        auto& buffer = screenInfo.GetTextBuffer();
        buffer.IncrementCircularBuffer(buffer.GetCurrentAttributes());

        // TODO: This is very bad for performance.
        // Track the total scroll offset as a int64 in buffer --> No need to track it here anymore.
        if (buffer.IsActiveBuffer())
        {
            auto& an = ServiceLocator::LocateGlobals().accessibilityNotifier;
            an.ScrollBuffer(-1);

            if (const auto renderer = ServiceLocator::LocateGlobals().pRender)
            {
                static constexpr til::point delta{ 0, -1 };
                renderer->TriggerScroll(&delta);
            }
        }

        if (psScrollY)
        {
            *psScrollY += 1;
        }

        coordCursor.y = bufferSize.height - 1;
    }

    LOG_IF_FAILED(screenInfo.SetCursorPosition(coordCursor));
}

// As the name implies, this writes text without processing its control characters.
static bool _writeCharsLegacyUnprocessed(SCREEN_INFORMATION& screenInfo, const std::wstring_view& text, til::CoordType* psScrollY)
{
    const auto wrapAtEOL = WI_IsFlagSet(screenInfo.OutputMode, ENABLE_WRAP_AT_EOL_OUTPUT);
    auto& textBuffer = screenInfo.GetTextBuffer();
    bool wrapped = false;

    RowWriteState state{
        .text = text,
        .columnLimit = textBuffer.GetSize().RightExclusive(),
    };

    while (!state.text.empty())
    {
        auto cursorPosition = textBuffer.GetCursor().GetPosition();

        state.columnBegin = cursorPosition.x;
        textBuffer.Replace(cursorPosition.y, textBuffer.GetCurrentAttributes(), state);
        cursorPosition.x = state.columnEnd;
        wrapped = wrapAtEOL && state.columnEnd >= state.columnLimit;

        if (wrapped)
        {
            textBuffer.SetWrapForced(cursorPosition.y, true);
        }

        AdjustCursorPosition(screenInfo, cursorPosition, psScrollY);
    }

    return wrapped;
}

// This routine writes a string to the screen while handling control characters.
// `interactive` exists for COOKED_READ_DATA which uses it to transform control characters into visible text like "^X".
// Similarly, `psScrollY` is also used by it to track whether the underlying buffer circled. It requires this information to know where the input line moved to.
void WriteCharsLegacy(SCREEN_INFORMATION& screenInfo, const std::wstring_view& text, til::CoordType* psScrollY)
{
    static constexpr wchar_t tabSpaces[8]{ L' ', L' ', L' ', L' ', L' ', L' ', L' ', L' ' };

    auto& textBuffer = screenInfo.GetTextBuffer();
    const auto width = textBuffer.GetSize().Width();
    auto& cursor = textBuffer.GetCursor();
    const auto cursorPosBefore = cursor.GetPosition();
    const auto wrapAtEOL = WI_IsFlagSet(screenInfo.OutputMode, ENABLE_WRAP_AT_EOL_OUTPUT);
    const auto beg = text.begin();
    const auto end = text.end();
    auto it = beg;

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto writer = gci.GetVtWriterForBuffer(&screenInfo);

    const auto a11y = raiseAccessibilityEventsOnExit(screenInfo);
    const auto snap = screenInfo.SnapOnOutput();

    // If we enter this if condition, then someone wrote text in VT mode and now switched to non-VT mode.
    // Since the Console APIs don't support delayed EOL wrapping, we need to first put the cursor back
    // to a position that the Console APIs expect (= not delayed).
    if (const auto delayed = cursor.GetDelayEOLWrap(); delayed && wrapAtEOL)
    {
        auto pos = cursor.GetPosition();
        cursor.ResetDelayEOLWrap();
        if (delayed == pos)
        {
            pos.x = 0;
            pos.y++;
            AdjustCursorPosition(screenInfo, pos, psScrollY);

            if (writer)
            {
                writer.WriteUTF8("\r\n");
            }
        }
    }

    // If ENABLE_PROCESSED_OUTPUT is set we search for C0 control characters and handle them like backspace, tab, etc.
    // If it's not set, we can just straight up give everything to WriteCharsLegacyUnprocessed.
    if (WI_IsFlagClear(screenInfo.OutputMode, ENABLE_PROCESSED_OUTPUT))
    {
        const auto lastCharWrapped = _writeCharsLegacyUnprocessed(screenInfo, text, psScrollY);

        if (writer)
        {
            // We're asked to produce VT output, but also to behave as if these control characters aren't control characters.
            // So, to make it work, we simply replace all the control characters with whitespace.
            writer.WriteUTF16StripControlChars(text);
            if (lastCharWrapped)
            {
                writer.WriteUTF8("\r\n");
            }
            writer.Submit();
        }

        return;
    }

    while (it != end)
    {
        const auto nextControlChar = std::find_if(it, end, controlCharPredicate);
        if (nextControlChar != it)
        {
            const std::wstring_view chunk{ it, nextControlChar };
            const auto lastCharWrapped = _writeCharsLegacyUnprocessed(screenInfo, chunk, psScrollY);
            it = nextControlChar;

            if (writer)
            {
                writer.WriteUTF16(chunk);
                if (lastCharWrapped)
                {
                    writer.WriteUTF8("\r\n");
                }
            }
        }

        if (it == end)
        {
            break;
        }

        do
        {
            auto wch = *it;
            auto lastCharWrapped = false;

            switch (wch)
            {
            case UNICODE_NULL:
            {
                lastCharWrapped = _writeCharsLegacyUnprocessed(screenInfo, { &tabSpaces[0], 1 }, psScrollY);
                wch = L' ';
                break;
            }
            case UNICODE_BELL:
            {
                std::ignore = screenInfo.SendNotifyBeep();
                break;
            }
            case UNICODE_BACKSPACE:
            {
                auto pos = cursor.GetPosition();
                pos.x = textBuffer.GetRowByOffset(pos.y).NavigateToPrevious(pos.x);
                AdjustCursorPosition(screenInfo, pos, psScrollY);
                break;
            }
            case UNICODE_TAB:
            {
                const auto pos = cursor.GetPosition();
                const auto remaining = width - pos.x;
                const auto tabCount = gsl::narrow_cast<size_t>(std::min(remaining, 8 - (pos.x & 7)));
                lastCharWrapped = _writeCharsLegacyUnprocessed(screenInfo, { &tabSpaces[0], tabCount }, psScrollY);
                break;
            }
            case UNICODE_LINEFEED:
            {
                auto pos = cursor.GetPosition();

                // If DISABLE_NEWLINE_AUTO_RETURN is not set, any LF behaves like a CRLF.
                if (WI_IsFlagClear(screenInfo.OutputMode, DISABLE_NEWLINE_AUTO_RETURN))
                {
                    pos.x = 0;

                    // Setting wch=0 and lastCharWrapped=true will cause the code at the end
                    // of the loop to emit a CRLF. However, we only do this if the preceding
                    // character isn't already a CR. We don't want to emit CR CR LF after all.
                    if (it == beg || it[-1] != '\r')
                    {
                        wch = 0;
                        lastCharWrapped = true;
                    }
                }

                textBuffer.GetMutableRowByOffset(pos.y).SetWrapForced(false);
                pos.y = pos.y + 1;
                AdjustCursorPosition(screenInfo, pos, psScrollY);

                break;
            }
            case UNICODE_CARRIAGERETURN:
            {
                auto pos = cursor.GetPosition();
                pos.x = 0;
                AdjustCursorPosition(screenInfo, pos, psScrollY);
                break;
            }
            default:
            {
                // As a special favor to incompetent apps that attempt to display control chars,
                // convert to corresponding OEM Glyph Chars
                const auto cp = ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP;
                const auto ch = gsl::narrow_cast<char>(wch);
                const auto result = MultiByteToWideChar(cp, MB_USEGLYPHCHARS, &ch, 1, &wch, 1);
                if (result != 1)
                {
                    wch = 0;
                }
                if (wch)
                {
                    lastCharWrapped = _writeCharsLegacyUnprocessed(screenInfo, { &wch, 1 }, psScrollY);
                }
                break;
            }
            }

            if (writer)
            {
                if (wch)
                {
                    writer.WriteUCS2(wch);
                }
                if (lastCharWrapped)
                {
                    writer.WriteUTF8("\r\n");
                }
            }

            ++it;
        } while (it != end && controlCharPredicate(*it));
    }

    if (writer)
    {
        writer.Submit();
    }
}

// This is the main entrypoint for conhost to write VT to the buffer.
// This wrapper around StateMachine exists so that we can add the necessary ConPTY transformations.
void WriteCharsVT(SCREEN_INFORMATION& screenInfo, const std::wstring_view& str)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& stateMachine = screenInfo.GetStateMachine();
    // If the given screenInfo is the alternate screen buffer, disabling the alternate screen buffer in this
    // VT payload will cause the pointer to be invalidated. We thus need to get all the information we need now.
    const auto disableNewlineTranslation = WI_IsFlagSet(screenInfo.OutputMode, DISABLE_NEWLINE_AUTO_RETURN);
    // When switch between the main and alt-buffer SCREEN_INFORMATION::GetActiveBuffer()
    // may change, so get the VtIo reference now, just in case.
    auto writer = gci.GetVtWriterForBuffer(&screenInfo);

    const auto a11y = raiseAccessibilityEventsOnExit(screenInfo);
    const auto snap = screenInfo.SnapOnOutput();

    stateMachine.ProcessString(str);

    if (writer)
    {
        const auto& injections = stateMachine.GetInjections();
        size_t offset = 0;

        // DISABLE_NEWLINE_AUTO_RETURN not being set is equivalent to a LF -> CRLF translation.
        const auto write = [&](size_t beg, size_t end) {
            const auto chunk = til::safe_slice_abs(str, beg, end);
            if (disableNewlineTranslation)
            {
                writer.WriteUTF16(chunk);
            }
            else
            {
                writer.WriteUTF16TranslateCRLF(chunk);
            }
        };

        // When we encounter something like a RIS (hard reset), we must re-enable
        // modes that we rely on (like the Win32 Input Mode). To do this, the VT
        // parser tells us the positions of any such relevant VT sequences.
        for (const auto& injection : injections)
        {
            write(offset, injection.offset);
            offset = injection.offset;

            static constexpr std::array<std::string_view, 3> mapping{ {
                { "\x1b[?1004h\x1b[?9001h" }, // RIS: Focus Event Mode + Win32 Input Mode
                { "\x1b[?1004h" }, // DECSET_FOCUS: Focus Event Mode
                { "\x1b[?9001h" }, // Win32 Input Mode
            } };
            static_assert(static_cast<size_t>(InjectionType::Count) == mapping.size(), "you need to update the mapping array");

            writer.WriteUTF8(mapping[static_cast<size_t>(injection.type)]);
        }

        write(offset, std::wstring_view::npos);
        writer.Submit();
    }
}

// Erases all contents of the given screenInfo, including the current screen and scrollback.
void WriteClearScreen(SCREEN_INFORMATION& screenInfo)
{
    WriteCharsVT(
        screenInfo,
        L"\x1b[H" // CUP to home
        L"\x1b[2J" // Erase in Display: clear the screen
        L"\x1b[3J" // Erase in Display: clear the scrollback buffer
    );
}

// Routine Description:
// - Takes the given text and inserts it into the given screen buffer.
// Note:
// - Console lock must be held when calling this routine
// - String has been translated to unicode at this point.
// Arguments:
// - pwchBuffer - wide character text to be inserted into buffer
// - pcbBuffer - byte count of pwchBuffer on the way in, number of bytes consumed on the way out.
// - screenInfo - Screen Information class to write the text into at the current cursor position
[[nodiscard]] HRESULT DoWriteConsole(SCREEN_INFORMATION& screenInfo, std::wstring_view str)
try
{
    if (WI_IsAnyFlagClear(screenInfo.OutputMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT))
    {
        WriteCharsLegacy(screenInfo, str, nullptr);
    }
    else
    {
        WriteCharsVT(screenInfo, str);
    }
    return S_OK;
}
CATCH_RETURN()

// Routine Description:
// - Writes non-Unicode formatted data into the given console output object.
// - This method will convert from the given input into wide characters before chain calling the wide character version of the function.
//   It uses the current Output Codepage for conversions (set via SetConsoleOutputCP).
// - NOTE: This may be blocked for various console states and will return a wait context pointer if necessary.
// Arguments:
// - context - the console output object to write the new text into
// - buffer - char/byte text buffer provided by client application to insert
// - read - character count of the number of characters (also bytes because A version) we were able to insert before returning
// - waiter - If we are blocked from writing now and need to wait, this is filled with contextual data for the server to restore the call later
// Return Value:
// - S_OK if successful.
// - S_OK if we need to wait (check if ppWaiter is not nullptr).
// - Or a suitable HRESULT code for math/string/memory failures.
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleAImpl(IConsoleOutputObject& context,
                                                     const std::string_view buffer,
                                                     size_t& read,
                                                     CONSOLE_API_MSG* pWaitReplyMessage) noexcept
{
    try
    {
        // Ensure output variables are initialized.
        read = 0;

        if (buffer.empty())
        {
            return S_OK;
        }

        LockConsole();
        auto unlock{ wil::scope_exit([&] { UnlockConsole(); }) };

        auto& screenInfo{ context.GetActiveBuffer() };
        const auto& consoleInfo{ ServiceLocator::LocateGlobals().getConsoleInformation() };
        const auto codepage{ consoleInfo.OutputCP };
        auto leadByteCaptured{ false };
        auto leadByteConsumed{ false };
        std::wstring wstr{};
        static til::u8state u8State{};

        // Convert our input parameters to Unicode
        if (codepage == CP_UTF8)
        {
            RETURN_IF_FAILED(til::u8u16(buffer, wstr, u8State));
            read = buffer.size();
        }
        else
        {
            // In case the codepage changes from UTF-8 to another,
            // we discard partials that might still be cached.
            u8State.reset();

            int mbPtrLength{};
            RETURN_IF_FAILED(SizeTToInt(buffer.size(), &mbPtrLength));

            // (buffer.size() + 2) I think because we might be shoving another unicode char
            // from screenInfo->WriteConsoleDbcsLeadByte in front
            // because we previously checked that buffer.size() fits into an int, +2 won't cause an overflow of size_t
            wstr.resize(buffer.size() + 2);

            auto wcPtr{ wstr.data() };
            auto mbPtr{ buffer.data() };
            size_t dbcsLength{};
            if (screenInfo.WriteConsoleDbcsLeadByte[0] != 0 && gsl::narrow_cast<byte>(*mbPtr) >= byte{ ' ' })
            {
                // there was a portion of a dbcs character stored from a previous
                // call so we take the 2nd half from mbPtr[0], put them together
                // and write the wide char to wcPtr[0]
                screenInfo.WriteConsoleDbcsLeadByte[1] = gsl::narrow_cast<byte>(*mbPtr);

                try
                {
                    const auto wFromComplemented{
                        ConvertToW(codepage, { reinterpret_cast<const char*>(screenInfo.WriteConsoleDbcsLeadByte), ARRAYSIZE(screenInfo.WriteConsoleDbcsLeadByte) })
                    };

                    FAIL_FAST_IF(wFromComplemented.size() != 1);
                    dbcsLength = sizeof(wchar_t);
                    wcPtr[0] = wFromComplemented.at(0);
                    mbPtr++;
                }
                catch (...)
                {
                    dbcsLength = 0;
                }

                // this looks weird to be always incrementing even if the conversion failed, but this is the
                // original behavior so it's left unchanged.
                wcPtr++;
                mbPtrLength--;

                // Note that we used a stored lead byte from a previous call in order to complete this write
                // Use this to offset the "number of bytes consumed" calculation at the end by -1 to account
                // for using a byte we had internally, not off the stream.
                leadByteConsumed = true;
            }

            screenInfo.WriteConsoleDbcsLeadByte[0] = 0;

            // if the last byte in mbPtr is a lead byte for the current code page,
            // save it for the next time this function is called and we can piece it
            // back together then
            if (mbPtrLength != 0 && CheckBisectStringA(const_cast<char*>(mbPtr), mbPtrLength, &consoleInfo.OutputCPInfo))
            {
                screenInfo.WriteConsoleDbcsLeadByte[0] = gsl::narrow_cast<byte>(mbPtr[mbPtrLength - 1]);
                mbPtrLength--;

                // Note that we captured a lead byte during this call, but won't actually draw it until later.
                // Use this to offset the "number of bytes consumed" calculation at the end by +1 to account
                // for taking a byte off the stream.
                leadByteCaptured = true;
            }

            if (mbPtrLength != 0)
            {
                // convert the remaining bytes in mbPtr to wide chars
                mbPtrLength = sizeof(wchar_t) * MultiByteToWideChar(codepage, 0, mbPtr, mbPtrLength, wcPtr, mbPtrLength);
            }

            wstr.resize((dbcsLength + mbPtrLength) / sizeof(wchar_t));
        }

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        if (WI_IsAnyFlagSet(gci.Flags, (CONSOLE_SUSPENDED | CONSOLE_SELECTING | CONSOLE_SCROLLBAR_TRACKING)))
        {
            const auto waiter = new WriteData(screenInfo, std::move(wstr), gci.OutputCP);

            // If there is a waiter, then we need to stow some additional information in the wait structure so
            // we can synthesize the correct byte count later when the wait routine is triggered.
            if (CP_UTF8 != codepage)
            {
                // For non-UTF8 codepages, save the lead byte captured/consumed data so we can +1 or -1 the final decoded count
                // in the WaitData::Notify method later.
                waiter->SetLeadByteAdjustmentStatus(leadByteCaptured, leadByteConsumed);
            }
            else
            {
                // For UTF8 codepages, just remember the consumption count from the UTF-8 parser.
                waiter->SetUtf8ConsumedCharacters(read);
            }

            std::ignore = ConsoleWaitQueue::s_CreateWait(pWaitReplyMessage, waiter);
            return CONSOLE_STATUS_WAIT;
        }

        // Make the W version of the call
        const auto hr = DoWriteConsole(screenInfo, wstr);

        // Calculate how many bytes of the original A buffer were consumed in the W version of the call to satisfy mbBufferRead.
        // For UTF-8 conversions, we've already returned this information above.
        if (CP_UTF8 != codepage)
        {
            size_t mbBufferRead{};

            // Start by counting the number of A bytes we used in printing our W string to the screen.
            try
            {
                mbBufferRead = GetALengthFromW(codepage, wstr);
            }
            CATCH_LOG();

            // If we captured a byte off the string this time around up above, it means we didn't feed
            // it into the WriteConsoleW above, and therefore its consumption isn't accounted for
            // in the count we just made. Add +1 to compensate.
            if (leadByteCaptured)
            {
                mbBufferRead++;
            }

            // If we consumed an internally-stored lead byte this time around up above, it means that we
            // fed a byte into WriteConsoleW that wasn't a part of this particular call's request.
            // We need to -1 to compensate and tell the caller the right number of bytes consumed this request.
            if (leadByteConsumed)
            {
                mbBufferRead--;
            }

            read = mbBufferRead;
        }

        return hr;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Writes Unicode formatted data into the given console output object.
// - NOTE: This may be blocked for various console states and will return a wait context pointer if necessary.
// Arguments:
// - OutContext - the console output object to write the new text into
// - pwsTextBuffer - wide character text buffer provided by client application to insert
// - cchTextBufferLength - text buffer counted in characters
// - pcchTextBufferRead - character count of the number of characters we were able to insert before returning
// - ppWaiter - If we are blocked from writing now and need to wait, this is filled with contextual data for the server to restore the call later
// Return Value:
// - S_OK if successful.
// - S_OK if we need to wait (check if ppWaiter is not nullptr).
// - Or a suitable HRESULT code for math/string/memory failures.
[[nodiscard]] HRESULT ApiRoutines::WriteConsoleWImpl(IConsoleOutputObject& context,
                                                     const std::wstring_view buffer,
                                                     size_t& read,
                                                     CONSOLE_API_MSG* pWaitReplyMessage) noexcept
{
    try
    {
        LockConsole();
        auto unlock = wil::scope_exit([&] { UnlockConsole(); });

        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        if (WI_IsAnyFlagSet(gci.Flags, (CONSOLE_SUSPENDED | CONSOLE_SELECTING | CONSOLE_SCROLLBAR_TRACKING)))
        {
            std::ignore = ConsoleWaitQueue::s_CreateWait(pWaitReplyMessage, new WriteData(context, std::wstring{ buffer }, gci.OutputCP));
            return CONSOLE_STATUS_WAIT;
        }

        read = 0;
        auto Status = DoWriteConsole(context, buffer);
        read = buffer.size();
        return Status;
    }
    CATCH_RETURN();
}
