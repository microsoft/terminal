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
    static constexpr std::wstring_view PlaceholderExeName{ L"wt.exe" };

    AppCommandlineArgs();
    ~AppCommandlineArgs() = default;
    int ParseCommand(const Commandline& command);

    static std::vector<Commandline> BuildCommands(const int argc, const wchar_t* argv[]);
    static std::vector<Commandline> BuildCommands(winrt::array_view<const winrt::hstring>& args);

    void ValidateStartupCommands();
    std::deque<winrt::TerminalApp::ActionAndArgs>& GetStartupActions();
    const std::string& GetExitMessage();

private:
    static const std::wregex _commandDelimiterRegex;

    CLI::App _app{ "wt - the Windows Terminal" };
    // --- Subcommands ---
    CLI::App* _newTabCommand;
    CLI::App* _newPaneCommand;
    CLI::App* _focusTabCommand;
    // Are you adding a new sub-command? Make sure to update _noCommandsProvided!

    std::string _profileName;
    std::string _startingDirectory;
    std::vector<std::string> _commandline;

    bool _splitVertical{ false };
    bool _splitHorizontal{ false };

    int _focusTabIndex{ -1 };
    bool _focusNextTab{ false };
    bool _focusPrevTab{ false };
    // Are you adding more args here? Make sure to reset them in _resetStateToDefault

    std::deque<winrt::TerminalApp::ActionAndArgs> _startupActions;
    std::string _exitMessage;

    winrt::TerminalApp::NewTerminalArgs _getNewTerminalArgs();
    void _addNewTerminalArgs(CLI::App* subcommand);
    void _buildParser();
    void _buildNewTabParser();
    void _buildSplitPaneParser();
    void _buildFocusTabParser();
    bool _noCommandsProvided();
    void _resetStateToDefault();
    int _handleExit(const CLI::App& command, const CLI::Error& e);

    static void _addCommandsForArg(std::vector<Commandline>& commands, std::wstring_view arg);

    friend class TerminalAppLocalTests::CommandlineTest;
};
