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
    bool IsHandoffListener() const noexcept;
    const std::string& GetExitMessage();
    bool ShouldExitEarly() const noexcept;

    std::optional<winrt::Microsoft::Terminal::Settings::Model::LaunchMode> GetLaunchMode() const noexcept;

    int ParseArgs(const winrt::Microsoft::Terminal::Settings::Model::ExecuteCommandlineArgs& args);
    void DisableHelpInExitMessage();
    void FullResetState();

    std::string_view GetTargetWindow() const noexcept;

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
        CLI::Option* tabColorOption;
        CLI::Option* suppressApplicationTitleOption;
        CLI::Option* colorSchemeOption;
    };

    struct NewPaneSubcommand : public NewTerminalSubcommand
    {
        CLI::Option* _horizontalOption;
        CLI::Option* _verticalOption;
        CLI::Option* _duplicateOption;
    };

    // --- Subcommands ---
    NewTerminalSubcommand _newTabCommand;
    NewTerminalSubcommand _newTabShort;
    NewPaneSubcommand _newPaneCommand;
    NewPaneSubcommand _newPaneShort;
    CLI::App* _focusTabCommand;
    CLI::App* _focusTabShort;
    CLI::App* _moveFocusCommand;
    CLI::App* _moveFocusShort;

    // Are you adding a new sub-command? Make sure to update _noCommandsProvided!

    std::string _profileName;
    std::string _startingDirectory;
    std::string _startingTitle;
    std::string _startingTabColor;
    std::string _startingColorScheme;
    bool _suppressApplicationTitle{ false };

    winrt::Microsoft::Terminal::Settings::Model::FocusDirection _moveFocusDirection{ winrt::Microsoft::Terminal::Settings::Model::FocusDirection::None };

    // _commandline will contain the command line with which we'll be spawning a new terminal
    std::vector<std::string> _commandline;

    bool _splitVertical{ false };
    bool _splitHorizontal{ false };
    bool _splitDuplicate{ false };
    float _splitPaneSize{ 0.5f };

    int _focusTabIndex{ -1 };
    bool _focusNextTab{ false };
    bool _focusPrevTab{ false };
    // Are you adding more args here? Make sure to reset them in _resetStateToDefault

    const Commandline* _currentCommandline{ nullptr };
    std::optional<winrt::Microsoft::Terminal::Settings::Model::LaunchMode> _launchMode{ std::nullopt };
    bool _isHandoffListener{ false };
    std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> _startupActions;
    std::string _exitMessage;
    bool _shouldExitEarly{ false };

    std::string _windowTarget{};
    // Are you adding more args or attributes here? If they are not reset in _resetStateToDefault, make sure to reset them in FullResetState

    winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs _getNewTerminalArgs(NewTerminalSubcommand& subcommand);
    void _addNewTerminalArgs(NewTerminalSubcommand& subcommand);
    void _buildParser();
    void _buildNewTabParser();
    void _buildSplitPaneParser();
    void _buildFocusTabParser();
    void _buildMoveFocusParser();
    bool _noCommandsProvided();
    void _resetStateToDefault();
    int _handleExit(const CLI::App& command, const CLI::Error& e);

    static void _addCommandsForArg(std::vector<Commandline>& commands, std::wstring_view arg);

#ifdef UNIT_TESTING
    friend class TerminalAppLocalTests::CommandlineTest;
#endif
};
