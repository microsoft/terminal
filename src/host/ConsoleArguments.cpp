// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "ConsoleArguments.hpp"
#include "../types/inc/utils.hpp"
#include <shellapi.h>
using namespace Microsoft::Console::Utils;

const std::wstring_view ConsoleArguments::VT_MODE_ARG = L"--vtmode";
const std::wstring_view ConsoleArguments::HEADLESS_ARG = L"--headless";
const std::wstring_view ConsoleArguments::SERVER_HANDLE_ARG = L"--server";
const std::wstring_view ConsoleArguments::SIGNAL_HANDLE_ARG = L"--signal";
const std::wstring_view ConsoleArguments::HANDLE_PREFIX = L"0x";
const std::wstring_view ConsoleArguments::CLIENT_COMMANDLINE_ARG = L"--";
const std::wstring_view ConsoleArguments::FORCE_V1_ARG = L"-ForceV1";
const std::wstring_view ConsoleArguments::FORCE_NO_HANDOFF_ARG = L"-ForceNoHandoff";
const std::wstring_view ConsoleArguments::FILEPATH_LEADER_PREFIX = L"\\??\\";
const std::wstring_view ConsoleArguments::WIDTH_ARG = L"--width";
const std::wstring_view ConsoleArguments::HEIGHT_ARG = L"--height";
const std::wstring_view ConsoleArguments::INHERIT_CURSOR_ARG = L"--inheritcursor";
const std::wstring_view ConsoleArguments::RESIZE_QUIRK = L"--resizeQuirk";
const std::wstring_view ConsoleArguments::WIN32_INPUT_MODE = L"--win32input";
const std::wstring_view ConsoleArguments::FEATURE_ARG = L"--feature";
const std::wstring_view ConsoleArguments::FEATURE_PTY_ARG = L"pty";
const std::wstring_view ConsoleArguments::COM_SERVER_ARG = L"-Embedding";

std::wstring EscapeArgument(std::wstring_view ac)
{
    if (ac.empty())
    {
        return L"\"\"";
    }
    bool hasSpace = false;
    auto n = ac.size();
    for (auto c : ac)
    {
        switch (c)
        {
        case L'"':
        case L'\\':
            n++;
            break;
        case ' ':
        case '\t':
            hasSpace = true;
            break;
        default:
            break;
        }
    }
    if (hasSpace)
    {
        n += 2;
    }
    if (n == ac.size())
    {
        return std::wstring{ ac };
    }
    std::wstring buf;
    if (hasSpace)
    {
        buf.push_back(L'"');
    }
    size_t slashes = 0;
    for (auto c : ac)
    {
        switch (c)
        {
        case L'\\':
            slashes++;
            buf.push_back(L'\\');
            break;
        case L'"':
        {
            for (; slashes > 0; slashes--)
            {
                buf.push_back(L'\\');
            }
            buf.push_back(L'\\');
            buf.push_back(c);
        }
        break;
        default:
            slashes = 0;
            buf.push_back(c);
            break;
        }
    }
    if (hasSpace)
    {
        for (; slashes > 0; slashes--)
        {
            buf.push_back(L'\\');
        }
        buf.push_back(L'"');
    }
    return buf;
}

ConsoleArguments::ConsoleArguments(const std::wstring& commandline,
                                   const HANDLE hStdIn,
                                   const HANDLE hStdOut) :
    _commandline(commandline),
    _vtInHandle(hStdIn),
    _vtOutHandle(hStdOut),
    _receivedEarlySizeChange{ false },
    _originalWidth{ -1 },
    _originalHeight{ -1 }
{
    _clientCommandline = L"";
    _vtMode = L"";
    _headless = false;
    _runAsComServer = false;
    _createServerHandle = true;
    _serverHandle = 0;
    _signalHandle = 0;
    _forceV1 = false;
    _forceNoHandoff = false;
    _width = 0;
    _height = 0;
    _inheritCursor = false;
}

ConsoleArguments::ConsoleArguments() :
    ConsoleArguments(L"", nullptr, nullptr)
{
}

ConsoleArguments& ConsoleArguments::operator=(const ConsoleArguments& other)
{
    if (this != &other)
    {
        _commandline = other._commandline;
        _clientCommandline = other._clientCommandline;
        _vtInHandle = other._vtInHandle;
        _vtOutHandle = other._vtOutHandle;
        _vtMode = other._vtMode;
        _headless = other._headless;
        _createServerHandle = other._createServerHandle;
        _serverHandle = other._serverHandle;
        _signalHandle = other._signalHandle;
        _forceV1 = other._forceV1;
        _width = other._width;
        _height = other._height;
        _inheritCursor = other._inheritCursor;
        _receivedEarlySizeChange = other._receivedEarlySizeChange;
        _runAsComServer = other._runAsComServer;
        _forceNoHandoff = other._forceNoHandoff;
    }

    return *this;
}

// Routine Description:
// - Consumes the argument at the given index off of the vector.
// Arguments:
// - args - The vector full of args
// - index - The item to consume/remove from the vector.
// Return Value:
// - <none>
void ConsoleArguments::s_ConsumeArg(_Inout_ std::vector<std::wstring>& args, _In_ size_t& index)
{
    args.erase(args.begin() + index);
}

// Routine Description:
//  Given the commandline of tokens `args`, tries to find the argument at
//      index+1, and places its value into pSetting.
//  If there aren't enough args, then returns E_INVALIDARG.
//  If we found a value, then we take the elements at both index and index+1 out
//      of args. We'll also decrement index, so that a caller who is using index
//      as a loop index will autoincrement it to have it point at the correct
//      next index.
//
// EX: for args=[--foo, bar, --baz]
//      index=0 would place "bar" in pSetting,
//          args is now [--baz], index is now -1, caller increments to 0
//      index=2 would return E_INVALIDARG,
//          args is still [--foo, bar, --baz], index is still 2, caller increments to 3.
// Arguments:
//  args: A collection of wstrings representing command-line arguments
//  index: the index of the argument of which to get the value for. The value
//      should be at (index+1). index will be decremented by one on success.
//  pSetting: receives the string at index+1
// Return Value:
//  S_OK if we parsed the string successfully, otherwise E_INVALIDARG indicating
//      failure.
[[nodiscard]] HRESULT ConsoleArguments::s_GetArgumentValue(_Inout_ std::vector<std::wstring>& args,
                                                           _Inout_ size_t& index,
                                                           _Out_opt_ std::wstring* const pSetting)
{
    bool hasNext = (index + 1) < args.size();
    if (hasNext)
    {
        s_ConsumeArg(args, index);
        if (pSetting != nullptr)
        {
            *pSetting = args[index];
        }
        s_ConsumeArg(args, index);
    }
    return (hasNext) ? S_OK : E_INVALIDARG;
}

// Routine Description:
// Similar to s_GetArgumentValue.
// Attempts to get the next arg as a "feature" arg - this can be used for
//      feature detection.
// If the next arg is not recognized, then we don't support that feature.
// Currently, the only supported feature arg is `pty`, to identify pty support.
// Arguments:
//  args: A collection of wstrings representing command-line arguments
//  index: the index of the argument of which to get the value for. The value
//      should be at (index+1). index will be decremented by one on success.
//  pSetting: receives the string at index+1
// Return Value:
//  S_OK if we parsed the string successfully, otherwise E_INVALIDARG indicating
//      failure.
[[nodiscard]] HRESULT ConsoleArguments::s_HandleFeatureValue(_Inout_ std::vector<std::wstring>& args, _Inout_ size_t& index)
{
    HRESULT hr = E_INVALIDARG;
    bool hasNext = (index + 1) < args.size();
    if (hasNext)
    {
        s_ConsumeArg(args, index);
        std::wstring value = args[index];
        if (value == FEATURE_PTY_ARG)
        {
            hr = S_OK;
        }
        s_ConsumeArg(args, index);
    }
    return (hasNext) ? hr : E_INVALIDARG;
}

// Method Description:
// Routine Description:
//  Given the commandline of tokens `args`, tries to find the argument at
//      index+1, and places its value into pSetting. See above for examples.
//  This implementation attempts to parse a short from the argument.
// Arguments:
//  args: A collection of wstrings representing command-line arguments
//  index: the index of the argument of which to get the value for. The value
//      should be at (index+1). index will be decremented by one on success.
//  pSetting: receives the short at index+1
// Return Value:
//  S_OK if we parsed the short successfully, otherwise E_INVALIDARG indicating
//      failure. This could be the case for non-numeric arguments, or for >SHORT_MAX args.
[[nodiscard]] HRESULT ConsoleArguments::s_GetArgumentValue(_Inout_ std::vector<std::wstring>& args,
                                                           _Inout_ size_t& index,
                                                           _Out_opt_ short* const pSetting)
{
    bool succeeded = (index + 1) < args.size();
    if (succeeded)
    {
        s_ConsumeArg(args, index);
        if (pSetting != nullptr)
        {
            try
            {
                size_t pos = 0;
                int value = std::stoi(args[index], &pos);
                // If the entire string was a number, pos will be equal to the
                //      length of the string. Otherwise, a string like 8foo will
                //       be parsed as "8"
                if (value > SHORT_MAX || pos != args[index].length())
                {
                    succeeded = false;
                }
                else
                {
                    *pSetting = static_cast<short>(value);
                    succeeded = true;
                }
            }
            catch (...)
            {
                succeeded = false;
            }
        }
        s_ConsumeArg(args, index);
    }
    return (succeeded) ? S_OK : E_INVALIDARG;
}

// Routine Description:
// - Parsing helper that will turn a string into a handle value if possible.
// Arguments:
// - handleAsText - The string representation of the handle that was passed in on the command line
// - handleAsVal - The location to store the value if we can appropriately convert it.
// Return Value:
// - S_OK if we could successfully parse the given text and store it in the handle value location.
// - E_INVALIDARG if we couldn't parse the text as a valid hex-encoded handle number OR
//                if the handle value was already filled.
[[nodiscard]] HRESULT ConsoleArguments::s_ParseHandleArg(const std::wstring& handleAsText, _Inout_ DWORD& handleAsVal)
{
    HRESULT hr = S_OK;

    // The handle should have a valid prefix.
    if (handleAsText.substr(0, HANDLE_PREFIX.length()) != HANDLE_PREFIX)
    {
        hr = E_INVALIDARG;
    }
    else if (0 == handleAsVal)
    {
        handleAsVal = wcstoul(handleAsText.c_str(), nullptr /*endptr*/, 16 /*base*/);

        // If the handle didn't parse into a reasonable handle ID, invalid.
        if (handleAsVal == 0)
        {
            hr = E_INVALIDARG;
        }
    }
    else
    {
        // If we're trying to set the handle a second time, invalid.
        hr = E_INVALIDARG;
    }

    return hr;
}

// Routine Description:
//  Given the commandline of tokens `args`, creates a wstring containing all of
//      the remaining args after index joined with spaces.  If skipFirst==true,
//      then we omit the argument at index from this finished string. skipFirst
//      should only be true if the first arg is
//      ConsoleArguments::CLIENT_COMMANDLINE_ARG. Removes all the args starting
//      at index from the collection.
//  The finished commandline is placed in _clientCommandline
// Arguments:
//  args: A collection of wstrings representing command-line arguments
//  index: the index of the argument of which to start the commandline from.
//  skipFirst: if true, omit the arg at index (which should be "--")
// Return Value:
//  S_OK if we parsed the string successfully, otherwise E_INVALIDARG indicating
//       failure.
[[nodiscard]] HRESULT ConsoleArguments::_GetClientCommandline(_Inout_ std::vector<std::wstring>& args, const size_t index, const bool skipFirst)
{
    auto start = args.begin() + index;

    // Erase the first token.
    //  Used to get rid of the explicit commandline token "--"
    if (skipFirst)
    {
        // Make sure that the arg we're deleting is "--"
        FAIL_FAST_IF(!(CLIENT_COMMANDLINE_ARG == start->c_str()));
        args.erase(start);
    }

    _clientCommandline = L"";
    size_t j = 0;
    for (j = index; j < args.size(); j++)
    {
        _clientCommandline += EscapeArgument(args[j]); // escape commandline
        if (j + 1 < args.size())
        {
            _clientCommandline += L" ";
        }
    }
    args.erase(args.begin() + index, args.begin() + j);

    return S_OK;
}

// Routine Description:
//  Attempts to parse the commandline that this ConsoleArguments was initialized
//      with. Fills all of our members with values that were specified on the
//      commandline.
// Arguments:
//  <none>
// Return Value:
//  S_OK if we parsed our _commandline successfully, otherwise E_INVALIDARG
//      indicating failure.
[[nodiscard]] HRESULT ConsoleArguments::ParseCommandline()
{
    // If the commandline was empty, quick return.
    if (_commandline.length() == 0)
    {
        return S_OK;
    }

    std::vector<std::wstring> args;
    HRESULT hr = S_OK;

    // Make a mutable copy of the commandline for tokenizing
    std::wstring copy = _commandline;

    // Tokenize the commandline
    int argc = 0;
    wil::unique_hlocal_ptr<PWSTR[]> argv;
    argv.reset(CommandLineToArgvW(copy.c_str(), &argc));
    RETURN_LAST_ERROR_IF(argv == nullptr);

    for (int i = 1; i < argc; ++i)
    {
        args.push_back(argv[i]);
    }

    // Parse args out of the commandline.
    //  As we handle a token, remove it from the args.
    //  At the end of parsing, there should be nothing left.
    for (size_t i = 0; i < args.size();)
    {
        hr = E_INVALIDARG;

        std::wstring arg = args[i];

        if (arg.substr(0, HANDLE_PREFIX.length()) == HANDLE_PREFIX ||
            arg == SERVER_HANDLE_ARG)
        {
            // server handle token accepted two ways:
            // --server 0x4 (new method)
            // 0x4 (legacy method)
            // If we see >1 of these, it's invalid.
            std::wstring serverHandleVal = arg;

            if (arg == SERVER_HANDLE_ARG)
            {
                hr = s_GetArgumentValue(args, i, &serverHandleVal);
            }
            else
            {
                s_ConsumeArg(args, i);
                hr = S_OK;
            }

            if (SUCCEEDED(hr))
            {
                hr = s_ParseHandleArg(serverHandleVal, _serverHandle);
                if (SUCCEEDED(hr))
                {
                    _createServerHandle = false;
                }
            }
        }
        else if (arg == SIGNAL_HANDLE_ARG)
        {
            std::wstring signalHandleVal;
            hr = s_GetArgumentValue(args, i, &signalHandleVal);

            if (SUCCEEDED(hr))
            {
                hr = s_ParseHandleArg(signalHandleVal, _signalHandle);
            }
        }
        else if (arg == FORCE_V1_ARG)
        {
            // -ForceV1 command line switch for NTVDM support
            _forceV1 = true;
            s_ConsumeArg(args, i);
            hr = S_OK;
        }
        else if (arg == FORCE_NO_HANDOFF_ARG)
        {
            // Prevent default application handoff to a different console/terminal
            _forceNoHandoff = true;
            s_ConsumeArg(args, i);
            hr = S_OK;
        }
        else if (arg == COM_SERVER_ARG)
        {
            _runAsComServer = true;
            s_ConsumeArg(args, i);
            hr = S_OK;
        }
        else if (arg.substr(0, FILEPATH_LEADER_PREFIX.length()) == FILEPATH_LEADER_PREFIX)
        {
            // beginning of command line -- includes file path
            // skipped for historical reasons.
            s_ConsumeArg(args, i);
            hr = S_OK;
        }
        else if (arg == VT_MODE_ARG)
        {
            hr = s_GetArgumentValue(args, i, &_vtMode);
        }
        else if (arg == WIDTH_ARG)
        {
            hr = s_GetArgumentValue(args, i, &_width);
        }
        else if (arg == HEIGHT_ARG)
        {
            hr = s_GetArgumentValue(args, i, &_height);
        }
        else if (arg == FEATURE_ARG)
        {
            hr = s_HandleFeatureValue(args, i);
        }
        else if (arg == HEADLESS_ARG)
        {
            _headless = true;
            s_ConsumeArg(args, i);
            hr = S_OK;
        }
        else if (arg == INHERIT_CURSOR_ARG)
        {
            _inheritCursor = true;
            s_ConsumeArg(args, i);
            hr = S_OK;
        }
        else if (arg == RESIZE_QUIRK)
        {
            _resizeQuirk = true;
            s_ConsumeArg(args, i);
            hr = S_OK;
        }
        else if (arg == WIN32_INPUT_MODE)
        {
            _win32InputMode = true;
            s_ConsumeArg(args, i);
            hr = S_OK;
        }
        else if (arg == CLIENT_COMMANDLINE_ARG)
        {
            // Everything after this is the explicit commandline
            hr = _GetClientCommandline(args, i, true);
            break;
        }
        // TODO: handle the rest of the possible params (MSFT:13271366, MSFT:13631640)
        // TODO: handle invalid args
        //  e.g. "conhost --foo bar" should not make the clientCommandline "--foo bar"
        else
        {
            // If we encounter something that doesn't match one of our other
            //      args, then it's the start of the commandline
            hr = _GetClientCommandline(args, i, false);
            break;
        }

        if (FAILED(hr))
        {
            break;
        }
    }

    // We should have consumed every token at this point.
    // if not, it is some sort of parsing error.
    // If we failed to parse an arg, then no need to assert.
    if (SUCCEEDED(hr))
    {
        FAIL_FAST_IF(!args.empty());
    }

    return hr;
}

// Routine Description:
// - Returns true if we already have opened handles to use for the VT server
//   streams.
// - If false, try next to see if we have pipe names to open instead.
// Arguments:
// - <none> - uses internal state
// Return Value:
// - True or false (see description)
bool ConsoleArguments::HasVtHandles() const
{
    return IsValidHandle(_vtInHandle) && IsValidHandle(_vtOutHandle);
}

// Routine Description:
// - Returns true if we were passed a seemingly valid signal handle on startup.
// Arguments:
// - <none> - uses internal state
// Return Value:
// - True or false (see description)
bool ConsoleArguments::HasSignalHandle() const
{
    return IsValidHandle(GetSignalHandle());
}

// Routine Description:
// - Returns true if we already have at least one handle for conpty streams.
// Arguments:
// - <none> - uses internal state
// Return Value:
// - True or false (see description)
bool ConsoleArguments::InConptyMode() const noexcept
{
    // If we only have a signal handle, then that's fine, they probably called
    //      CreatePseudoConsole with neither handle.
    // If we only have one of the other handles, that's fine they're still
    //      invoking us by passing in pipes, so they know what they're doing.
    return IsValidHandle(_vtInHandle) || IsValidHandle(_vtOutHandle) || HasSignalHandle();
}

bool ConsoleArguments::IsHeadless() const
{
    return _headless;
}

bool ConsoleArguments::ShouldCreateServerHandle() const
{
    return _createServerHandle;
}

bool ConsoleArguments::ShouldRunAsComServer() const
{
    return _runAsComServer;
}

HANDLE ConsoleArguments::GetServerHandle() const
{
    return ULongToHandle(_serverHandle);
}

HANDLE ConsoleArguments::GetSignalHandle() const
{
    return ULongToHandle(_signalHandle);
}

HANDLE ConsoleArguments::GetVtInHandle() const
{
    return _vtInHandle;
}

HANDLE ConsoleArguments::GetVtOutHandle() const
{
    return _vtOutHandle;
}

std::wstring ConsoleArguments::GetOriginalCommandLine() const
{
    return _commandline;
}

std::wstring ConsoleArguments::GetClientCommandline() const
{
    return _clientCommandline;
}

std::wstring ConsoleArguments::GetVtMode() const
{
    return _vtMode;
}

bool ConsoleArguments::GetForceV1() const
{
    return _forceV1;
}

bool ConsoleArguments::GetForceNoHandoff() const
{
    return _forceNoHandoff;
}

short ConsoleArguments::GetWidth() const
{
    return _width;
}

short ConsoleArguments::GetHeight() const
{
    return _height;
}

bool ConsoleArguments::GetInheritCursor() const
{
    return _inheritCursor;
}
bool ConsoleArguments::IsResizeQuirkEnabled() const
{
    return _resizeQuirk;
}
bool ConsoleArguments::IsWin32InputModeEnabled() const
{
    return _win32InputMode;
}

// Method Description:
// - Tell us to use a different size than the one parsed as the size of the
//      console. This is called by the PtySignalInputThread when it receives a
//      resize before the first client has connected. Because there's no client,
//      there's also no buffer yet, so it has nothing to resize.
//      However, we shouldn't just discard that first resize message. Instead,
//      store it in here, so we can use the value when the first client does connect.
// Arguments:
// - dimensions: the new size in characters of the conpty buffer & viewport.
// Return Value:
// - <none>
void ConsoleArguments::SetExpectedSize(COORD dimensions) noexcept
{
    _width = dimensions.X;
    _height = dimensions.Y;
    // Stash away the original values we parsed when this is called.
    // This is to help debugging - if the signal thread DOES change these values,
    //      we can still recover what was given to us on the commandline.
    if (!_receivedEarlySizeChange)
    {
        _originalWidth = _width;
        _originalHeight = _height;
        // Mark that we've changed size from what our commandline values were
        _receivedEarlySizeChange = true;
    }
}

#ifdef UNIT_TESTING
// Method Description:
// - This is a test helper method. It can be used to trick us into thinking
//   we're headless (in conpty mode), even without parsing any arguments.
// Arguments:
// - <none>
// Return Value:
// - <none>
void ConsoleArguments::EnableConptyModeForTests()
{
    _headless = true;
}
#endif
