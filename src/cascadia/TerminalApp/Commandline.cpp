// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Commandline.h"

using namespace TerminalApp;

Commandline::~Commandline()
{
    _resetArgv();
}

size_t Commandline::Argc() const
{
    return _wargs.size();
}

char** Commandline::Argv() const
{
    return _argv;
}

const std::vector<std::wstring>& Commandline::Wargs() const
{
    return _wargs;
}

// Method Description:
// - Creates a c-style argv array of char* strings, and returns it. If we've
//   previously run this, we don't need to re-generate the buffer, we can just
//   return it early.
// - We need this method because CLI11 only operates on c-style (argc, argv),
//   and not std::strings.
// Arguments:
// - <none>
// Return Value:
// - a c-style argv array.
char** Commandline::BuildArgv()
{
    // If we've already build our array of args, then we don't need to worry
    // about this. Just return the last one we built.
    if (_argv)
    {
        return _argv;
    }

    // Build an argv array. The array should be an array of char* strings,
    // so that CLI11 can parse the args like a normal posix application.
    _argv = new char*[Argc()];
    THROW_IF_NULL_ALLOC(_argv);

    // Convert each from wchar_t to char, using winrt::to_string
    for (int i = 0; i < Argc(); i++)
    {
        const auto& warg = _wargs[i];
        const auto arg = winrt::to_string(warg);
        const auto len = arg.size();
        // Create a new c-style array to hold the string
        _argv[i] = new char[len + 1];
        THROW_IF_NULL_ALLOC(_argv[i]);

        // copy the string to the c-style buffer
        for (int j = 0; j < len; j++)
        {
            _argv[i][j] = arg.at(j);
        }
        _argv[i][len] = '\0';
    }
    return _argv;
}

// Method Description:
// - Add the given arg to the list of args for this commandline. If the arg has
//   an escaped delimiter ('\;') in it, we'll de-escape it, so the processed
//   Commandline will have it as just a ';'.
// Arguments:
// - nextArg: The string to add as an arg to the commandline. This string may contain spaces.
// Return Value:
// - <none>
void Commandline::AddArg(const std::wstring& nextArg)
{
    if (_argv)
    {
        _resetArgv();
    }

    // Attempt to convert '\;' in the arg to just '\', removing the escaping
    std::wstring modifiedArg{ nextArg };
    size_t pos = modifiedArg.find(EscapedDelimiter, 0);
    while (pos != std::string::npos)
    {
        modifiedArg.replace(pos, EscapedDelimiter.length(), Delimiter);
        pos = modifiedArg.find(EscapedDelimiter, pos + Delimiter.length());
    }

    _wargs.emplace_back(modifiedArg);
}

// Method Description:
// - Delete our previously constructed _argv array
// Arguments:
// - <none>
// Return Value:
// - <none>
void Commandline::_resetArgv()
{
    if (_argv)
    {
        for (int i = 0; i < Argc(); i++)
        {
            delete[] _argv[i];
        }
        delete[] _argv;
        _argv = 0;
    }
}
