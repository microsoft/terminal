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
    AppCommandlineArgs();
    ~AppCommandlineArgs() = default;
    int ParseCommand(const Commandline& command);

    static std::vector<Commandline> BuildCommands(const int w_argc, const wchar_t* w_argv[]);

private:
    void _BuildParser();
    void _BuildNewTabParser();
    void _BuildSplitPaneParser();
    bool _NoCommandsProvided();
    void _ResetStateToDefault();

    CLI::App _app{ "yeet, a test of the wt commandline" };
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

    std::vector<winrt::TerminalApp::ActionAndArgs> _startupActions;

    winrt::TerminalApp::NewTerminalArgs _GetNewTerminalArgs();
    void _AddNewTerminalArgs(CLI::App* subcommand);

    friend class TerminalAppLocalTests::CommandlineTest;
};
