// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Commandline.h"

using namespace TerminalApp;

size_t Commandline::Argc() const
{
    return _args.size();
}

const std::vector<std::string>& Commandline::Args() const
{
    return _args;
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
    // Attempt to convert '\;' in the arg to just '\', removing the escaping
    auto modifiedArg{ nextArg };
    auto pos = modifiedArg.find(EscapedDelimiter, 0);
    while (pos != std::string::npos)
    {
        modifiedArg.replace(pos, EscapedDelimiter.length(), Delimiter);
        pos = modifiedArg.find(EscapedDelimiter, pos + Delimiter.length());
    }

    _args.emplace_back(winrt::to_string(modifiedArg));
}
