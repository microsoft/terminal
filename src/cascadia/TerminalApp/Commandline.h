// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - Commandline.h
//
// Abstract:
// - This is a helper class for representing a single commandline within the
//   Terminal Application. Users can specify multiple commands on a single
//   commandline invocation of the Terminal, and this class is used to represent
//   a single individual command.
// - Args are added to this class as wide strings, because the args provided to
//   the application are typically wide strings.
// - Args are retrieved from this class as a list of narrow-strings, because
//   CLI11 (which we're using to parse the commandlines) requires narrow
//   strings.
//
// Author:
// - Mike Griese (zadjii-msft) 09-Jan-2020

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class CommandlineTest;
};
namespace TerminalApp
{
    class Commandline;
};

class TerminalApp::Commandline
{
public:
    static constexpr std::wstring_view Delimiter{ L";" };
    static constexpr std::wstring_view EscapedDelimiter{ L"\\;" };

    void AddArg(const std::wstring& nextArg);

    size_t Argc() const;
    const std::vector<std::string>& Args() const;

private:
    std::vector<std::string> _args;

    friend class TerminalAppLocalTests::CommandlineTest;
};
