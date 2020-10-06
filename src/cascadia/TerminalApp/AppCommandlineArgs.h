// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#pragma once

#include "Commandline.h"

#ifdef UNIT_TESTING
// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class CommandlineTest;
};
#endif

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
    int ParseArgs(winrt::array_view<const winrt::hstring>& args);

    static std::vector<Commandline> BuildCommands(const std::vector<const wchar_t*>& args);
    static std::vector<Commandline> BuildCommands(winrt::array_view<const winrt::hstring>& args);

    void ValidateStartupCommands();
    std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs>& GetStartupActions();
    const std::string& GetExitMessage();
    bool ShouldExitEarly() const noexcept;

    std::optional<winrt::Microsoft::Terminal::Settings::Model::LaunchMode> GetLaunchMode() const noexcept;

private:
    static const std::wregex _commandDelimiterRegex;

    CLI::App _app{ "wt - the Windows Terminal" };

    // This is a helper struct to encapsulate all the options for a subcommand
    // that produces a NewTerminalArgs.
    struct NewTerminalSubcommand
    {
        CLI::App* subcommand;
        CLI::Option* commandlineOption;
        CLI::Option* profileNameOption;
        CLI::Option* startingDirectoryOption;
        CLI::Option* titleOption;
    };

    struct NewPaneSubcommand : public NewTerminalSubcommand
    {
        CLI::Option* _horizontalOption;
        CLI::Option* _verticalOption;
    };

    // --- Subcommands ---
    NewTerminalSubcommand _newTabCommand;
    NewTerminalSubcommand _newTabShort;
    NewPaneSubcommand _newPaneCommand;
    NewPaneSubcommand _newPaneShort;
    CLI::App* _focusTabCommand;
    CLI::App* _focusTabShort;
    // Are you adding a new sub-command? Make sure to update _noCommandsProvided!

    std::string _profileName;
    std::string _startingDirectory;
    std::string _startingTitle;

    // _commandline will contain the command line with which we'll be spawning a new terminal
    std::vector<std::string> _commandline;

    const Commandline* _currentCommandline{ nullptr };

    bool _splitVertical{ false };
    bool _splitHorizontal{ false };

    int _focusTabIndex{ -1 };
    bool _focusNextTab{ false };
    bool _focusPrevTab{ false };

    std::optional<winrt::Microsoft::Terminal::Settings::Model::LaunchMode> _launchMode{ std::nullopt };
    // Are you adding more args here? Make sure to reset them in _resetStateToDefault

    std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> _startupActions;
    std::string _exitMessage;
    bool _shouldExitEarly{ false };

    winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs _getNewTerminalArgs(NewTerminalSubcommand& subcommand);
    void _addNewTerminalArgs(NewTerminalSubcommand& subcommand);
    void _buildParser();
    void _buildNewTabParser();
    void _buildSplitPaneParser();
    void _buildFocusTabParser();
    bool _noCommandsProvided();
    void _resetStateToDefault();
    int _handleExit(const CLI::App& command, const CLI::Error& e);

    static void _addCommandsForArg(std::vector<Commandline>& commands, std::wstring_view arg);

#ifdef UNIT_TESTING
    friend class TerminalAppLocalTests::CommandlineTest;
#endif
};
