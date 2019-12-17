// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "ActionAndArgs.h"

#pragma once
#include "Commandline.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class CommandlineTest;
};

namespace TerminalApp
{
    class AppCommandlineArgs;
};

class TerminalApp::AppCommandlineArgs final
{
public:
    static constexpr std::string_view NixHelpFlag{ "-?" };
    static constexpr std::string_view WindowsHelpFlag{ "/?" };

    AppCommandlineArgs();
    ~AppCommandlineArgs() = default;
    int ParseCommand(const Commandline& command);

    static std::vector<Commandline> BuildCommands(const int argc, const wchar_t* argv[]);
    static std::vector<Commandline> BuildCommands(winrt::array_view<const winrt::hstring>& args);

    std::deque<winrt::TerminalApp::ActionAndArgs>& GetStartupActions();

private:
    static const std::wregex _commandDelimiterRegex;

    CLI::App _app{ "wt - the Windows Terminal" };
    // --- Subcommands ---
    CLI::App* _newTabCommand;
    CLI::App* _newPaneCommand;
    CLI::App* _listProfilesCommand;
    // Are you adding a new sub-command? Make sure to update _NoCommandsProvided!

    std::string _profileName;
    std::string _startingDirectory;
    std::vector<std::string> _commandline;
    bool _splitVertical{ false };
    bool _splitHorizontal{ false };
    // Are you adding more args here? Make sure to reset them in _ResetStateToDefault

    std::deque<winrt::TerminalApp::ActionAndArgs> _startupActions;

    winrt::TerminalApp::NewTerminalArgs _GetNewTerminalArgs();
    void _AddNewTerminalArgs(CLI::App* subcommand);
    void _BuildParser();
    void _BuildNewTabParser();
    void _BuildSplitPaneParser();
    bool _NoCommandsProvided();
    void _ResetStateToDefault();

    static void _addCommandsForArg(std::vector<Commandline>& commands, std::wstring_view arg);

    friend class TerminalAppLocalTests::CommandlineTest;
};
