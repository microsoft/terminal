// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "ActionAndArgs.h"

struct Cmdline
{
    std::vector<std::wstring> wargs;
    char** _argv = nullptr;
    size_t argc() const { return wargs.size(); }
    char** Argv() const { return _argv; }
    const std::vector<std::wstring>& Wargs() const { return wargs; };
    char** BuildArgv()
    {
        // If we've already build our array of args, then we don't need to worry
        // about this. Just return the last one we build.
        if (_argv)
        {
            return _argv;
        }

        // TODO: This is horrifying
        _argv = new char*[argc()];
        for (int i = 0; i < argc(); i++)
        {
            auto len = wargs[i].size();
            _argv[i] = new char[len + 1];
            for (int j = 0; j <= len; j++)
            {
                _argv[i][j] = char(wargs[i][j]);
            }
        }
        return _argv;
    }
};

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class CommandlineTest;
};

class AppCommandline
{
public:
    AppCommandline();
    ~AppCommandline() = default;
    int ParseCommand(const Cmdline& command);

    static std::vector<Cmdline> BuildCommands(const int w_argc, const wchar_t* w_argv[]);

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
