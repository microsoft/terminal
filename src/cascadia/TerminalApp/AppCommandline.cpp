// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppCommandline.h"
#include "ActionArgs.h"

using namespace winrt::TerminalApp;

AppCommandline::AppCommandline()
{
    _BuildParser();
    _ResetStateToDefault();
}

void AppCommandline::_ResetStateToDefault()
{
    _profileName = "";
    _startingDirectory = "";
    _commandline.clear();
}

int AppCommandline::ParseCommand(const Cmdline& command)
{
    int localArgc = static_cast<int>(command.argc());
    auto localArgv = command.Argv();

    // std::cout << "######################### starting command #########################\n";
    // for (int i = 0; i < localArgc; i++)
    // {
    //     char* arg = localArgv[i];
    //     std::cout << "arg[" << i << "]=\"";
    //     printf("%s\"\n", arg);
    // }
    _ResetStateToDefault();

    try
    {
        if (localArgc == 2 && (std::string("/?") == localArgv[1] || std::string("-?") == localArgv[1]))
        {
            throw CLI::CallForHelp();
        }
        _app.clear();
        _app.parse(localArgc, localArgv);

        if (_NoCommandsProvided())
        {
            // std::cout << "Didn't find _any_ commands, using newTab to parse\n";
            _newTabCommand->clear();
            _newTabCommand->parse(localArgc, localArgv);
        }
    }
    catch (const CLI::CallForHelp& e)
    {
        return _app.exit(e);
    }
    catch (const CLI::ParseError& e)
    {
        if (_NoCommandsProvided())
        {
            // std::cout << "EXCEPTIONALLY Didn't find _any_ commands, using newTab to parse\n";
            try
            {
                _newTabCommand->clear();
                _newTabCommand->parse(localArgc, localArgv);
            }
            catch (const CLI::ParseError& e)
            {
                return _newTabCommand->exit(e);
            }
        }
        else
        {
            return _app.exit(e);
        }
    }
    return 0;
}

void AppCommandline::_AddNewTerminalArgs(CLI::App* subcommand)
{
    subcommand->add_option("-p,--profile", _profileName, "Open with the give profile");
    subcommand->add_option("-d,--startingDirectory", _startingDirectory, "Open in the given directory instead of the profile's set startingDirectory");
    subcommand->add_option("cmdline", _commandline, "Commandline to run in the given profile");
}

void AppCommandline::_BuildParser()
{
    ////////////////////////////////////////////////////////////////////////////
    _newTabCommand = _app.add_subcommand("new-tab", "Create a new tab");
    _AddNewTerminalArgs(_newTabCommand);
    _newTabCommand->callback([&, this]() {
        // std::cout << "######################### new-tab #########################\n";

        auto newTabAction = winrt::make_self<implementation::ActionAndArgs>();
        newTabAction->Action(ShortcutAction::NewTab);
        auto args = winrt::make_self<implementation::NewTabArgs>();
        args->TerminalArgs(_GetNewTerminalArgs());
        newTabAction->Args(*args);
        _startupActions.push_back(*newTabAction);
    });
    ////////////////////////////////////////////////////////////////////////////
    _newPaneCommand = _app.add_subcommand("split-pane", "Create a new pane");
    _AddNewTerminalArgs(_newPaneCommand);
    auto* horizontalOpt = _newPaneCommand->add_flag("-H,--horizontal", _splitHorizontal, "TODO");
    auto* verticalOpt = _newPaneCommand->add_flag("-V,--vertical", _splitVertical, "TODO");
    verticalOpt->excludes(horizontalOpt);

    _newPaneCommand->callback([&, this]() {
        // std::cout << "######################### new-pane #########################\n";
        auto newPaneAction = winrt::make_self<implementation::ActionAndArgs>();
        newPaneAction->Action(ShortcutAction::SplitPane);
        auto args = winrt::make_self<implementation::SplitPaneArgs>();
        args->TerminalArgs(_GetNewTerminalArgs());

        if (_splitHorizontal)
        {
            args->SplitStyle(SplitState::Horizontal);
        }
        else
        {
            args->SplitStyle(SplitState::Vertical);
        }

        newPaneAction->Args(*args);
        _startupActions.push_back(*newPaneAction);
    });
    ////////////////////////////////////////////////////////////////////////////
    _listProfilesCommand = _app.add_subcommand("list-profiles", "List all the available profiles");
    ////////////////////////////////////////////////////////////////////////////
}

NewTerminalArgs AppCommandline::_GetNewTerminalArgs()
{
    auto args = winrt::make_self<implementation::NewTerminalArgs>();

    if (!_profileName.empty())
    {
        std::cout << "profileName: " << _profileName << std::endl;
    }
    // else
    // {
    //     std::cout << "Use the default profile" << std::endl;
    // }
    if (!_startingDirectory.empty())
    {
        std::cout << "startingDirectory: " << _startingDirectory << std::endl;
    }
    // else
    // {
    //     std::cout << "Use the default startingDirectory" << std::endl;
    // }
    if (!_commandline.empty())
    {
        auto i = 0;
        for (auto arg : _commandline)
        {
            std::cout << "arg[" << i << "]=\"" << arg << "\"\n";
            i++;
        }
    }
    // else
    // {
    //     std::cout << "Use the default cmdline" << std::endl;
    // }

    return *args;
}

bool AppCommandline::_NoCommandsProvided()
{
    return !(*_listProfilesCommand ||
             *_newTabCommand ||
             *_newPaneCommand);
}

std::vector<Cmdline> AppCommandline::BuildCommands(const int w_argc, const wchar_t* w_argv[])
{
    std::wstring cmdSeperator = L";";
    std::vector<Cmdline> commands;
    commands.emplace_back(Cmdline{});
    for (auto i = 0; i < w_argc; i++)
    {
        const auto nextFullArg = std::wstring{ w_argv[i] };
        auto nextDelimiter = nextFullArg.find(cmdSeperator);
        if (nextDelimiter == std::wstring::npos)
        {
            commands.rbegin()->wargs.emplace_back(nextFullArg);
        }
        else
        {
            auto remaining = nextFullArg;
            auto nextArg = remaining.substr(0, nextDelimiter);
            remaining = remaining.substr(nextDelimiter + 1);
            if (nextArg != L"")
            {
                commands.rbegin()->wargs.emplace_back(nextArg);
            }
            do
            {
                // TODO: For delimiters that are escaped, skip them and go to the next
                nextDelimiter = remaining.find(cmdSeperator);
                commands.emplace_back(Cmdline{});
                commands.rbegin()->wargs.emplace_back(std::wstring{ L"wt.exe" });
                nextArg = remaining.substr(0, nextDelimiter);
                if (nextArg != L"")
                {
                    commands.rbegin()->wargs.emplace_back(nextArg);
                }
                remaining = remaining.substr(nextDelimiter + 1);
            } while (nextDelimiter != std::wstring::npos);
        }
    }

    return commands;
}
