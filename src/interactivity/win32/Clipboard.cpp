// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "clipboard.hpp"
#include "resource.h"

#include "../../host/dbcs.h"
#include "../../host/scrolling.hpp"
#include "../../host/output.h"

#include "../../types/inc/convert.hpp"
#include "../../types/inc/viewport.hpp"

#include "../inc/conint.h"
#include "../inc/EventSynthesis.hpp"
#include "../inc/ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::Console::Types;

#pragma region Public Methods

// Arguments:
// - fAlsoCopyFormatting - Place colored HTML & RTF text onto the clipboard as well as the usual plain text.
// Return Value:
//   <none>
// NOTE:  if the registry is set to always copy color data then we will even if fAlsoCopyFormatting is false
void Clipboard::Copy(bool fAlsoCopyFormatting)
{
    try
    {
        // registry settings may tell us to always copy the color/formatting
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        fAlsoCopyFormatting = fAlsoCopyFormatting || gci.GetCopyColor();

        // store selection in clipboard
        StoreSelectionToClipboard(fAlsoCopyFormatting);
        Selection::Instance().ClearSelection(); // clear selection in console
    }
    CATCH_LOG();
}

/*++

Perform paste request into old app by pulling out clipboard
contents and writing them to the console's input buffer

--*/
void Clipboard::Paste()
{
    HANDLE ClipboardDataHandle;

    // Clear any selection or scrolling that may be active.
    Selection::Instance().ClearSelection();
    Scrolling::s_ClearScroll();

    // Get paste data from clipboard
    if (!OpenClipboard(ServiceLocator::LocateConsoleWindow()->GetWindowHandle()))
    {
        return;
    }

    ClipboardDataHandle = GetClipboardData(CF_UNICODETEXT);
    if (ClipboardDataHandle == nullptr)
    {
        CloseClipboard();
        return;
    }

    auto pwstr = (PWCHAR)GlobalLock(ClipboardDataHandle);
    StringPaste(pwstr, (ULONG)GlobalSize(ClipboardDataHandle) / sizeof(WCHAR));

    // WIP auditing if user is enrolled
    static auto DestinationName = _LoadString(ID_CONSOLE_WIP_DESTINATIONNAME);
    Microsoft::Console::Internal::EdpPolicy::AuditClipboard(DestinationName);

    GlobalUnlock(ClipboardDataHandle);

    CloseClipboard();
}

Clipboard& Clipboard::Instance()
{
    static Clipboard clipboard;
    return clipboard;
}

// Routine Description:
// - This routine pastes given Unicode string into the console window.
// Arguments:
// - pData - Unicode string that is pasted to the console window
// - cchData - Size of the Unicode String in characters
// Return Value:
// - None
void Clipboard::StringPaste(_In_reads_(cchData) const wchar_t* const pData,
                            const size_t cchData)
{
    if (pData == nullptr)
    {
        return;
    }

    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        const auto vtInputMode = gci.pInputBuffer->IsInVirtualTerminalInputMode();
        const auto bracketedPasteMode = gci.GetBracketedPasteMode();
        auto inEvents = TextToKeyEvents(pData, cchData, vtInputMode && bracketedPasteMode);
        gci.pInputBuffer->Write(inEvents);
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

#pragma endregion

#pragma region Private Methods

// Routine Description:
// - converts a wchar_t* into a series of KeyEvents as if it was typed
// from the keyboard
// Arguments:
// - pData - the text to convert
// - cchData - the size of pData, in wchars
// - bracketedPaste - should this be bracketed with paste control sequences
// Return Value:
// - deque of KeyEvents that represent the string passed in
// Note:
// - will throw exception on error
InputEventQueue Clipboard::TextToKeyEvents(_In_reads_(cchData) const wchar_t* const pData,
                                           const size_t cchData,
                                           const bool bracketedPaste)
{
    THROW_HR_IF_NULL(E_INVALIDARG, pData);

    InputEventQueue keyEvents;
    const auto pushControlSequence = [&](const std::wstring_view sequence) {
        std::for_each(sequence.begin(), sequence.end(), [&](const auto wch) {
            keyEvents.push_back(SynthesizeKeyEvent(true, 1, 0, 0, wch, 0));
            keyEvents.push_back(SynthesizeKeyEvent(false, 1, 0, 0, wch, 0));
        });
    };

    // When a bracketed paste is requested, we need to wrap the text with
    // control sequences which indicate that the content has been pasted.
    if (bracketedPaste)
    {
        pushControlSequence(L"\x1b[200~");
    }

    for (size_t i = 0; i < cchData; ++i)
    {
        auto currentChar = pData[i];

        const auto charAllowed = FilterCharacterOnPaste(&currentChar);
        // filter out linefeed if it's not the first char and preceded
        // by a carriage return
        const auto skipLinefeed = (i != 0 &&
                                   currentChar == UNICODE_LINEFEED &&
                                   pData[i - 1] == UNICODE_CARRIAGERETURN);
        // filter out escape if bracketed paste mode is enabled
        const auto skipEscape = (bracketedPaste && currentChar == UNICODE_ESC);

        if (!charAllowed || skipLinefeed || skipEscape)
        {
            continue;
        }

        if (currentChar == 0)
        {
            break;
        }

        // MSFT:12123975 / WSL GH#2006
        // If you paste text with ONLY linefeed line endings (unix style) in wsl,
        //      then we faithfully pass those along, which the underlying terminal
        //      interprets as C-j. In nano, C-j is mapped to "Justify text", which
        //      causes the pasted text to get broken at the width of the terminal.
        // This behavior doesn't occur in gnome-terminal, and nothing like it occurs
        //      in vi or emacs.
        // This change doesn't break pasting text into any of those applications
        //      with CR/LF (Windows) line endings either. That apparently always
        //      worked right.
        if (IsInVirtualTerminalInputMode() && currentChar == UNICODE_LINEFEED)
        {
            currentChar = UNICODE_CARRIAGERETURN;
        }

        const auto codepage = ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP;
        CharToKeyEvents(currentChar, codepage, keyEvents);
    }

    if (bracketedPaste)
    {
        pushControlSequence(L"\x1b[201~");
    }

    return keyEvents;
}

// Routine Description:
// - Copies the selected area onto the global system clipboard.
// - NOTE: Throws on allocation and other clipboard failures.
// Arguments:
// - copyFormatting - This will also place colored HTML & RTF text onto the clipboard as well as the usual plain text.
// Return Value:
//   <none>
void Clipboard::StoreSelectionToClipboard(const bool copyFormatting)
{
    const auto& selection = Selection::Instance();

    // See if there is a selection to get
    if (!selection.IsAreaSelected())
    {
        return;
    }

    // read selection area.
    const auto selectionRects = selection.GetSelectionRects();

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& buffer = gci.GetActiveOutputBuffer().GetTextBuffer();
    const auto& renderSettings = gci.GetRenderSettings();

    const auto GetAttributeColors = [&](const auto& attr) {
        return renderSettings.GetAttributeColors(attr);
    };

    bool includeCRLF, trimTrailingWhitespace;
    if (WI_IsFlagSet(OneCoreSafeGetKeyState(VK_SHIFT), KEY_PRESSED))
    {
        // When shift is held, put everything in one line
        includeCRLF = trimTrailingWhitespace = false;
    }
    else
    {
        includeCRLF = trimTrailingWhitespace = true;
    }

    const auto text = buffer.GetText(includeCRLF,
                                     trimTrailingWhitespace,
                                     selectionRects,
                                     GetAttributeColors,
                                     selection.IsKeyboardMarkSelection());

    CopyTextToSystemClipboard(text, copyFormatting);
}

// Routine Description:
// - Copies the text given onto the global system clipboard.
// Arguments:
// - rows - Rows of text data to copy
// - fAlsoCopyFormatting - true if the color and formatting should also be copied, false otherwise
void Clipboard::CopyTextToSystemClipboard(const TextBuffer::TextAndColor& rows, const bool fAlsoCopyFormatting)
{
    std::wstring finalString;

    // Concatenate strings into one giant string to put onto the clipboard.
    for (const auto& str : rows.text)
    {
        finalString += str;
    }

    // allocate the final clipboard data
    const auto cchNeeded = finalString.size() + 1;
    const auto cbNeeded = sizeof(wchar_t) * cchNeeded;
    wil::unique_hglobal globalHandle(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbNeeded));
    THROW_LAST_ERROR_IF_NULL(globalHandle.get());

    auto pwszClipboard = (PWSTR)GlobalLock(globalHandle.get());
    THROW_LAST_ERROR_IF_NULL(pwszClipboard);

    // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
    // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
    const auto hr = StringCchCopyW(pwszClipboard, cchNeeded, finalString.data());
    GlobalUnlock(globalHandle.get());
    THROW_IF_FAILED(hr);

    // Set global data to clipboard
    THROW_LAST_ERROR_IF(!OpenClipboard(ServiceLocator::LocateConsoleWindow()->GetWindowHandle()));

    { // Clipboard Scope
        auto clipboardCloser = wil::scope_exit([]() {
            THROW_LAST_ERROR_IF(!CloseClipboard());
        });

        THROW_LAST_ERROR_IF(!EmptyClipboard());
        THROW_LAST_ERROR_IF_NULL(SetClipboardData(CF_UNICODETEXT, globalHandle.get()));

        if (fAlsoCopyFormatting)
        {
            const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
            const auto& fontData = gci.GetActiveOutputBuffer().GetCurrentFont();
            const auto iFontHeightPoints = fontData.GetUnscaledSize().height * 72 / ServiceLocator::LocateGlobals().dpi;
            const auto bgColor = gci.GetRenderSettings().GetAttributeColors({}).second;

            auto HTMLToPlaceOnClip = TextBuffer::GenHTML(rows, iFontHeightPoints, fontData.GetFaceName(), bgColor);
            CopyToSystemClipboard(HTMLToPlaceOnClip, L"HTML Format");

            auto RTFToPlaceOnClip = TextBuffer::GenRTF(rows, iFontHeightPoints, fontData.GetFaceName(), bgColor);
            CopyToSystemClipboard(RTFToPlaceOnClip, L"Rich Text Format");
        }
    }

    // only free if we failed.
    // the memory has to remain allocated if we successfully placed it on the clipboard.
    // Releasing the smart pointer will leave it allocated as we exit scope.
    globalHandle.release();
}

// Routine Description:
// - Copies the given string onto the global system clipboard in the specified format
// Arguments:
// - stringToCopy - The string to copy
// - lpszFormat - the name of the format
void Clipboard::CopyToSystemClipboard(std::string stringToCopy, LPCWSTR lpszFormat)
{
    const auto cbData = stringToCopy.size() + 1; // +1 for '\0'
    if (cbData)
    {
        wil::unique_hglobal globalHandleData(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbData));
        THROW_LAST_ERROR_IF_NULL(globalHandleData.get());

        auto pszClipboardHTML = (PSTR)GlobalLock(globalHandleData.get());
        THROW_LAST_ERROR_IF_NULL(pszClipboardHTML);

        // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
        // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
        const auto hr2 = StringCchCopyA(pszClipboardHTML, cbData, stringToCopy.data());
        GlobalUnlock(globalHandleData.get());
        THROW_IF_FAILED(hr2);

        const auto CF_FORMAT = RegisterClipboardFormatW(lpszFormat);
        THROW_LAST_ERROR_IF(0 == CF_FORMAT);

        THROW_LAST_ERROR_IF_NULL(SetClipboardData(CF_FORMAT, globalHandleData.get()));

        // only free if we failed.
        // the memory has to remain allocated if we successfully placed it on the clipboard.
        // Releasing the smart pointer will leave it allocated as we exit scope.
        globalHandleData.release();
    }
}

// Returns true if the character should be emitted to the paste stream
// -- in some cases, we will change what character should be emitted, as in the case of "smart quotes"
// Returns false if the character should not be emitted (e.g. <TAB>)
bool Clipboard::FilterCharacterOnPaste(_Inout_ WCHAR* const pwch)
{
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto fAllowChar = true;
    if (gci.GetFilterOnPaste() &&
        (WI_IsFlagSet(gci.pInputBuffer->InputMode, ENABLE_PROCESSED_INPUT)))
    {
        switch (*pwch)
        {
            // swallow tabs to prevent inadvertent tab expansion
        case UNICODE_TAB:
        {
            fAllowChar = false;
            break;
        }

        // Replace Unicode space with standard space
        case UNICODE_NBSP:
        case UNICODE_NARROW_NBSP:
        {
            *pwch = UNICODE_SPACE;
            break;
        }

        // Replace "smart quotes" with "dumb ones"
        case UNICODE_LEFT_SMARTQUOTE:
        case UNICODE_RIGHT_SMARTQUOTE:
        {
            *pwch = UNICODE_QUOTE;
            break;
        }

        // Replace Unicode dashes with a standard hyphen
        case UNICODE_EM_DASH:
        case UNICODE_EN_DASH:
        {
            *pwch = UNICODE_HYPHEN;
            break;
        }
        }
    }

    return fAllowChar;
}

#pragma endregion
