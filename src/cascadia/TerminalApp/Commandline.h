// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

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
    ~Commandline();

    static constexpr std::wstring_view Delimiter{ L";" };
    static constexpr std::wstring_view EscapedDelimiter{ L"\\;" };

    size_t Argc() const;
    const std::vector<std::string>& Args() const;
    void AddArg(const std::wstring& nextArg);

private:
    std::vector<std::string> _args;

    friend class TerminalAppLocalTests::CommandlineTest;
};
