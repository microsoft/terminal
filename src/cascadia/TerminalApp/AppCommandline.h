// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "ActionAndArgs.h"

struct Cmdline
{
    size_t Argc() const { return _wargs.size(); }
    char** Argv() const { return _argv; }
    const std::vector<std::wstring>& Wargs() const { return _wargs; };
    char** BuildArgv()
    {
        // If we've already build our array of args, then we don't need to worry
        // about this. Just return the last one we build.
        if (_argv)
        {
            return _argv;
        }

        // Build an argv array. The array should be an array of char* strings,
        // so that CLI11 can parse the args like a normal posix application.
        _argv = new char*[Argc()];
        THROW_IF_NULL_ALLOC(_argv);

        // Convert each
        for (int i = 0; i < Argc(); i++)
        {
            const auto& warg = _wargs[i];
            auto arg = winrt::to_string(warg);
            auto len = arg.size();
            _argv[i] = new char[len + 1];
            THROW_IF_NULL_ALLOC(_argv[i]);

            for (int j = 0; j < len; j++)
            {
                _argv[i][j] = arg.at(j);
            }
            _argv[i][len] = '\0';
        }
        return _argv;
    }

    void AddArg(const std::wstring& nextArg)
    {
        if (_argv)
        {
            _resetArgv();
        }

        _wargs.emplace_back(nextArg);
    }

private:
    std::vector<std::wstring> _wargs;
    char** _argv = nullptr;
    void _resetArgv()
    {
        for (int i = 0; i < Argc(); i++)
        {
            delete[] _argv[i];
        }
        delete[] _argv;
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
