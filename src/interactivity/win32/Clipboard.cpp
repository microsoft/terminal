// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "clipboard.hpp"
#include "resource.h"

#include "..\..\host\dbcs.h"
#include "..\..\host\scrolling.hpp"
#include "..\..\host\output.h"

#include "..\..\types\inc\convert.hpp"
#include "..\..\types\inc\viewport.hpp"

#include "..\inc\conint.h"
#include "..\inc\ServiceLocator.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::Console::Types;

#pragma region Public Methods

// Arguments:
// - fAlsoCopyHtml - Place colored HTML text onto the clipboard as well as the usual plain text.
// Return Value:
//   <none>
// NOTE:  if the registry is set to always copy color data then we will even if fAlsoCopyHTML is false
void Clipboard::Copy(bool fAlsoCopyHtml)
{
    try
    {
        // registry settings may tell us to always copy the color/formating
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        fAlsoCopyHtml = fAlsoCopyHtml || gci.GetCopyColor();

        // store selection in clipboard
        StoreSelectionToClipboard(fAlsoCopyHtml);
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

    PWCHAR pwstr = (PWCHAR)GlobalLock(ClipboardDataHandle);
    StringPaste(pwstr, (ULONG)GlobalSize(ClipboardDataHandle) / sizeof(WCHAR));

    // WIP auditing if user is enrolled
    static std::wstring DestinationName = _LoadString(ID_CONSOLE_WIP_DESTINATIONNAME);
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

    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        std::deque<std::unique_ptr<IInputEvent>> inEvents = TextToKeyEvents(pData, cchData);
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
// Return Value:
// - deque of KeyEvents that represent the string passed in
// Note:
// - will throw exception on error
std::deque<std::unique_ptr<IInputEvent>> Clipboard::TextToKeyEvents(_In_reads_(cchData) const wchar_t* const pData,
                                                                    const size_t cchData)
{
    THROW_IF_NULL_ALLOC(pData);

    std::deque<std::unique_ptr<IInputEvent>> keyEvents;

    for (size_t i = 0; i < cchData; ++i)
    {
        wchar_t currentChar = pData[i];

        const bool charAllowed = FilterCharacterOnPaste(&currentChar);
        // filter out linefeed if it's not the first char and preceded
        // by a carriage return
        const bool skipLinefeed = (i != 0 &&
                                   currentChar == UNICODE_LINEFEED &&
                                   pData[i - 1] == UNICODE_CARRIAGERETURN);

        if (!charAllowed || skipLinefeed)
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

        const UINT codepage = ServiceLocator::LocateGlobals().getConsoleInformation().OutputCP;
        std::deque<std::unique_ptr<KeyEvent>> convertedEvents = CharToKeyEvents(currentChar, codepage);
        while (!convertedEvents.empty())
        {
            keyEvents.push_back(std::move(convertedEvents.front()));
            convertedEvents.pop_front();
        }
    }
    return keyEvents;
}

// Routine Description:
// - Copies the selected area onto the global system clipboard.
// - NOTE: Throws on allocation and other clipboard failures.
// Arguments:
// - fAlsoCopyHtml - This will also place colored HTML text onto the clipboard as well as the usual plain text.
// Return Value:
//   <none>
void Clipboard::StoreSelectionToClipboard(bool const fAlsoCopyHtml)
{
    const auto& selection = Selection::Instance();

    // See if there is a selection to get
    if (!selection.IsAreaSelected())
    {
        return;
    }

    // read selection area.
    const auto selectionRects = selection.GetSelectionRects();
    const bool lineSelection = Selection::Instance().IsLineSelection();

    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    const auto& screenInfo = gci.GetActiveOutputBuffer();

    const auto text = RetrieveTextFromBuffer(screenInfo,
                                             lineSelection,
                                             selectionRects);

    CopyTextToSystemClipboard(text, fAlsoCopyHtml);
}

// Routine Description:
// - Retrieves the text data from the selected region of the text buffer
// Arguments:
// - screenInfo - what is rendered on the screen
// - lineSelection - true if entire line is being selected. False otherwise (box selection)
// - selectionRects - the selection regions from which the data will be extracted from the buffer
TextBuffer::TextAndColor Clipboard::RetrieveTextFromBuffer(const SCREEN_INFORMATION& screenInfo,
                                                           const bool lineSelection,
                                                           const std::vector<SMALL_RECT>& selectionRects)
{
    const auto& buffer = screenInfo.GetTextBuffer();
    const bool trimTrailingWhitespace = !WI_IsFlagSet(GetKeyState(VK_SHIFT), KEY_PRESSED);
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    std::function<COLORREF(TextAttribute&)> GetForegroundColor = std::bind(&CONSOLE_INFORMATION::LookupForegroundColor, &gci, std::placeholders::_1);
    std::function<COLORREF(TextAttribute&)> GetBackgroundColor = std::bind(&CONSOLE_INFORMATION::LookupBackgroundColor, &gci, std::placeholders::_1);

    return buffer.GetTextForClipboard(lineSelection,
                                      trimTrailingWhitespace,
                                      selectionRects,
                                      GetForegroundColor,
                                      GetBackgroundColor);
}

// Routine Description:
// - Generates a CF_HTML compliant structure based on the passed in text and color data
// Arguments:
// - rows - the text and color data we will format & encapsulate
// Return Value:
// - string containing the generated HTML
std::string Clipboard::GenHTML(const TextBuffer::TextAndColor& rows)
{
    std::string szClipboard; // we will build the data going back in this string buffer

    try
    {
        std::string const szHtmlClipFormat =
            "Version:0.9\r\n"
            "StartHTML:%010d\r\n"
            "EndHTML:%010d\r\n"
            "StartFragment:%010d\r\n"
            "EndFragment:%010d\r\n"
            "StartSelection:%010d\r\n"
            "EndSelection:%010d\r\n";

        // measure clip header
        size_t const cbHeader = 157; // when formats are expanded, there will be 157 bytes in the header.

        std::string const szHtmlHeader =
            "<!DOCTYPE><HTML><HEAD><TITLE>Windows Console Host</TITLE></HEAD><BODY>";
        size_t const cbHtmlHeader = szHtmlHeader.size();

        std::string const szHtmlFragStart = "<!--StartFragment -->";
        std::string const szHtmlFragEnd = "<!--EndFragment -->";
        std::string const szHtmlFooter = "</BODY></HTML>";
        size_t const cbHtmlFooter = szHtmlFooter.size();

        std::string const szDivOuterBackgroundPattern = R"X(<DIV STYLE="background-color:#%02x%02x%02x;white-space:pre;">)X";

        size_t const cbDivOuter = 55;
        std::string szDivOuter;
        szDivOuter.reserve(cbDivOuter);

        std::string const szSpanFontSizePattern = R"X(<SPAN STYLE="font-size: %dpt">)X";

        const auto& fontData = ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetCurrentFont();
        int const iFontHeightPoints = fontData.GetUnscaledSize().Y * 72 / ServiceLocator::LocateGlobals().dpi;
        size_t const cbSpanFontSize = 28 + (iFontHeightPoints / 10) + 1;

        std::string szSpanFontSize;
        szSpanFontSize.resize(cbSpanFontSize + 1); // reserve space for null after string for sprintf
        sprintf_s(szSpanFontSize.data(), cbSpanFontSize + 1, szSpanFontSizePattern.data(), iFontHeightPoints);
        szSpanFontSize.resize(cbSpanFontSize); //chop off null at end

        std::string const szSpanStartPattern = R"X(<SPAN STYLE="color:#%02x%02x%02x;background-color:#%02x%02x%02x">)X";

        size_t const cbSpanStart = 53; // when format is expanded, there will be 53 bytes per color pattern.
        std::string szSpanStart;
        szSpanStart.resize(cbSpanStart + 1); // +1 for null terminator

        std::string const szSpanStartFontPattern = R"X(<SPAN STYLE="font-family: '%s', monospace">)X";
        size_t const cbSpanStartFontPattern = 41;

        std::string const szSpanStartFontConstant = R"X(<SPAN STYLE="font-family: monospace">)X";
        size_t const cbSpanStartFontConstant = 37;

        std::string szSpanStartFont;
        size_t cbSpanStartFont;
        bool fDeleteSpanStartFont = false;

        std::wstring const wszFontFaceName = fontData.GetFaceName();
        size_t const cchFontFaceName = wszFontFaceName.size();
        if (cchFontFaceName > 0)
        {
            // measure and create buffer to convert face name to UTF8
            int const cbNeeded = WideCharToMultiByte(CP_UTF8, 0, wszFontFaceName.data(), static_cast<int>(cchFontFaceName), nullptr, 0, nullptr, nullptr);
            std::string szBuffer;
            szBuffer.resize(cbNeeded);

            // do conversion
            WideCharToMultiByte(CP_UTF8, 0, wszFontFaceName.data(), static_cast<int>(cchFontFaceName), szBuffer.data(), cbNeeded, nullptr, nullptr);

            // format converted font name into pattern
            std::string const szFinalFontPattern = R"X(<SPAN STYLE="font-family: ')X" + szBuffer + R"X(', monospace\">)X";
            size_t const cbBytesNeeded = szFinalFontPattern.length();

            fDeleteSpanStartFont = true;
            szSpanStartFont = szFinalFontPattern;
            cbSpanStartFont = cbBytesNeeded;
        }
        else
        {
            szSpanStartFont = szSpanStartFontConstant;
            cbSpanStartFont = cbSpanStartFontConstant;
        }

        std::string const szSpanEnd = "</SPAN>";
        std::string const szDivEnd = "</DIV>";

        // Start building the HTML formated string to return
        // First we have to add the required header and then
        // some standard HTML boiler plate required for CF_HTML
        // as part of the HTML Clipboard format
        szClipboard.append(cbHeader, 'H'); // reserve space for a header we fill in later
        szClipboard.append(szHtmlHeader);
        szClipboard.append(szHtmlFragStart);

        COLORREF iBgColor = rows.BkAttr.at(0).at(0);

        szDivOuter.resize(cbDivOuter + 1);
        sprintf_s(szDivOuter.data(), cbDivOuter + 1, szDivOuterBackgroundPattern.data(), GetRValue(iBgColor), GetGValue(iBgColor), GetBValue(iBgColor));
        szDivOuter.resize(cbDivOuter);
        szClipboard.append(szDivOuter);

        // copy font face start
        szClipboard.append(szSpanStartFont);

        // copy font size start
        szClipboard.append(szSpanFontSize);

        bool bColorFound = false;

        // copy all text into the final clipboard data handle. There should be no nulls between rows of
        // characters, but there should be a \0 at the end.
        for (UINT iRow = 0; iRow < rows.text.size(); iRow++)
        {
            size_t cbStartOffset = 0;
            size_t cchCharsToPrint = 0;

            COLORREF const Blackness = RGB(0x00, 0x00, 0x00);
            COLORREF fgColor = Blackness;
            COLORREF bkColor = Blackness;

            for (UINT iCol = 0; iCol < rows.text.at(iRow).length(); iCol++)
            {
                bool fColorDelta = false;

                if (!bColorFound)
                {
                    fgColor = rows.FgAttr.at(iRow).at(iCol);
                    bkColor = rows.BkAttr.at(iRow).at(iCol);
                    bColorFound = true;
                    fColorDelta = true;
                }
                else if ((rows.FgAttr.at(iRow).at(iCol) != fgColor) || (rows.BkAttr.at(iRow).at(iCol) != bkColor))
                {
                    fgColor = rows.FgAttr.at(iRow).at(iCol);
                    bkColor = rows.BkAttr.at(iRow).at(iCol);
                    fColorDelta = true;
                }

                if (fColorDelta)
                {
                    if (cchCharsToPrint > 0)
                    {
                        // write accumulated characters to stream ....
                        std::string TempBuff;
                        int const cbTempCharsNeeded = WideCharToMultiByte(CP_UTF8, 0, rows.text[iRow].data() + cbStartOffset, static_cast<int>(cchCharsToPrint), nullptr, 0, nullptr, nullptr);
                        TempBuff.resize(cbTempCharsNeeded);
                        WideCharToMultiByte(CP_UTF8, 0, rows.text[iRow].data() + cbStartOffset, static_cast<int>(cchCharsToPrint), TempBuff.data(), cbTempCharsNeeded, nullptr, nullptr);
                        szClipboard.append(TempBuff);
                        cbStartOffset += cchCharsToPrint;
                        cchCharsToPrint = 0;

                        // close previous span
                        szClipboard += szSpanEnd;
                    }

                    // start new span

                    // format with color then copy formatted string
                    szSpanStart.resize(cbSpanStart + 1); // add room for null
                    sprintf_s(szSpanStart.data(), cbSpanStart + 1, szSpanStartPattern.data(), GetRValue(fgColor), GetGValue(fgColor), GetBValue(fgColor), GetRValue(bkColor), GetGValue(bkColor), GetBValue(bkColor));
                    szSpanStart.resize(cbSpanStart); // chop null from sprintf
                    szClipboard.append(szSpanStart);
                }

                // accumulate 1 character
                cchCharsToPrint++;
            }

            PCWCHAR pwchAccumulateStart = rows.text.at(iRow).data() + cbStartOffset;

            // write accumulated characters to stream
            std::string CharsConverted;
            int cbCharsConverted = WideCharToMultiByte(CP_UTF8, 0, pwchAccumulateStart, static_cast<int>(cchCharsToPrint), nullptr, 0, nullptr, nullptr);
            CharsConverted.resize(cbCharsConverted);
            WideCharToMultiByte(CP_UTF8, 0, pwchAccumulateStart, static_cast<int>(cchCharsToPrint), CharsConverted.data(), cbCharsConverted, nullptr, nullptr);
            szClipboard.append(CharsConverted);
        }

        if (bColorFound)
        {
            // copy end span
            szClipboard.append(szSpanEnd);
        }

        // after we have copied all text we must wrap up
        // with a standard set of HTML boilerplate required
        // by CF_HTML

        // copy end font size span
        szClipboard.append(szSpanEnd);

        // copy end font face span
        szClipboard.append(szSpanEnd);

        // copy end background color span
        szClipboard.append(szDivEnd);

        // copy HTML end fragment
        szClipboard.append(szHtmlFragEnd);

        // copy HTML footer
        szClipboard.append(szHtmlFooter);

        // null terminate the clipboard data
        szClipboard += '\0';

        // we are done generating formating & building HTML for the selection
        // prepare the header text with the byte counts now that we know them
        size_t const cbHtmlStart = cbHeader; // bytecount to start of HTML context
        size_t const cbHtmlEnd = szClipboard.size() - 1; // don't count the null at the end
        size_t const cbFragStart = cbHeader + cbHtmlHeader; // bytecount to start of selection fragment
        size_t const cbFragEnd = cbHtmlEnd - cbHtmlFooter;

        // push the values into the required HTML 0.9 header format
        std::string szHtmlClipHeaderFinal;
        szHtmlClipHeaderFinal.resize(cbHeader + 1); // add room for a null
        sprintf_s(szHtmlClipHeaderFinal.data(), cbHeader + 1, szHtmlClipFormat.data(), cbHtmlStart, cbHtmlEnd, cbFragStart, cbFragEnd, cbFragStart, cbFragEnd);
        szHtmlClipHeaderFinal.resize(cbHeader); // chop off the null

        // overwrite the reserved space with the actual header & offsets we calculated
        szClipboard.replace(0, cbHeader, szHtmlClipHeaderFinal.data());
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
        szClipboard.clear(); // dont return a partial html fragment...
    }

    return szClipboard;
}

// Routine Description:
// - Copies the text given onto the global system clipboard.
// Arguments:
// - rows - Rows of text data to copy
void Clipboard::CopyTextToSystemClipboard(const TextBuffer::TextAndColor& rows, bool const fAlsoCopyHtml)
{
    std::wstring finalString;

    // Concatenate strings into one giant string to put onto the clipboard.
    for (const auto& str : rows.text)
    {
        finalString += str;
    }

    // allocate the final clipboard data
    const size_t cchNeeded = finalString.size() + 1;
    const size_t cbNeeded = sizeof(wchar_t) * cchNeeded;
    wil::unique_hglobal globalHandle(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbNeeded));
    THROW_LAST_ERROR_IF_NULL(globalHandle.get());

    PWSTR pwszClipboard = (PWSTR)GlobalLock(globalHandle.get());
    THROW_LAST_ERROR_IF_NULL(pwszClipboard);

    // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
    // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
    const HRESULT hr = StringCchCopyW(pwszClipboard, cchNeeded, finalString.data());
    GlobalUnlock(globalHandle.get());
    THROW_IF_FAILED(hr);

    // Set global data to clipboard
    THROW_LAST_ERROR_IF(!OpenClipboard(ServiceLocator::LocateConsoleWindow()->GetWindowHandle()));
    THROW_LAST_ERROR_IF(!EmptyClipboard());
    THROW_LAST_ERROR_IF_NULL(SetClipboardData(CF_UNICODETEXT, globalHandle.get()));

    if (fAlsoCopyHtml)
    {
        std::string HTMLToPlaceOnClip = GenHTML(rows);
        const size_t cbNeededHTML = HTMLToPlaceOnClip.size();
        if (cbNeededHTML)
        {
            wil::unique_hglobal globalHandleHTML(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbNeededHTML));
            THROW_LAST_ERROR_IF_NULL(globalHandleHTML.get());

            PSTR pszClipboardHTML = (PSTR)GlobalLock(globalHandleHTML.get());
            THROW_LAST_ERROR_IF_NULL(pszClipboardHTML);

            // The pattern gets a bit strange here because there's no good wil built-in for global lock of this type.
            // Try to copy then immediately unlock. Don't throw until after (so the hglobal won't be freed until we unlock).
            const HRESULT hr2 = StringCchCopyA(pszClipboardHTML, cbNeededHTML, HTMLToPlaceOnClip.data());
            GlobalUnlock(globalHandleHTML.get());
            THROW_IF_FAILED(hr2);

            UINT const CF_HTML = RegisterClipboardFormatW(L"HTML Format");
            THROW_LAST_ERROR_IF(0 == CF_HTML);

            THROW_LAST_ERROR_IF_NULL(SetClipboardData(CF_HTML, globalHandleHTML.get()));

            // only free if we failed.
            // the memory has to remain allocated if we successfully placed it on the clipboard.
            // Releasing the smart pointer will leave it allocated as we exit scope.
            globalHandleHTML.release();
        }
    }

    THROW_LAST_ERROR_IF(!CloseClipboard());

    // only free if we failed.
    // the memory has to remain allocated if we successfully placed it on the clipboard.
    // Releasing the smart pointer will leave it allocated as we exit scope.
    globalHandle.release();
}

// Returns true if the character should be emitted to the paste stream
// -- in some cases, we will change what character should be emitted, as in the case of "smart quotes"
// Returns false if the character should not be emitted (e.g. <TAB>)
bool Clipboard::FilterCharacterOnPaste(_Inout_ WCHAR* const pwch)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    bool fAllowChar = true;
    if (gci.GetFilterOnPaste() &&
        (WI_IsFlagSet(gci.pInputBuffer->InputMode, ENABLE_PROCESSED_INPUT)))
    {
        switch (*pwch)
        {
            // swallow tabs to prevent inadvertant tab expansion
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

        // Replace Unicode dashes with a standard hypen
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
