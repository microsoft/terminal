// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <CLI11/CLI11.hpp>

class Commandline
{
public:
    static constexpr std::string_view Delimiter{ ";" };
    static constexpr std::string_view EscapedDelimiter{ "\\;" };

    void AddArg(const std::string& nextArg);

    size_t Argc() const;
    const std::vector<std::string>& Args() const;

private:
    std::vector<std::string> _args;
};

size_t Commandline::Argc() const
{
    return _args.size();
}

const std::vector<std::string>& Commandline::Args() const
{
    return _args;
}

// Method Description:
// - Add the given arg to the list of args for this commandline. If the arg has
//   an escaped delimiter ('\;') in it, we'll de-escape it, so the processed
//   Commandline will have it as just a ';'.
// Arguments:
// - nextArg: The string to add as an arg to the commandline. This string may contain spaces.
// Return Value:
// - <none>
void Commandline::AddArg(const std::string& nextArg)
{
    // Attempt to convert '\;' in the arg to just '\', removing the escaping
    std::string modifiedArg{ nextArg };
    size_t pos = modifiedArg.find(EscapedDelimiter, 0);
    while (pos != std::string::npos)
    {
        modifiedArg.replace(pos, EscapedDelimiter.length(), Delimiter);
        pos = modifiedArg.find(EscapedDelimiter, pos + Delimiter.length());
    }

    _args.emplace_back(modifiedArg);
}
////////////////////////////////////////////////////////////////////////////////
void printArgv(const std::vector<std::string>& v)
{
    std::cout << "argv:[";
    for (auto& arg : v)
    {
        std::cout << "\n\t" << arg << ",";
    }
    std::cout << "\n]\n";
}
////////////////////////////////////////////////////////////////////////////////

// This wmain exists for help in writing scratch programs while debugging.
int __cdecl main(int argc, char* argv[])
{
    Commandline cmdline{};
    std::basic_string_view<char*> argv_view{ argv, (size_t)argc };

    for (auto arg : argv_view)
    {
        cmdline.AddArg({ arg });
    }

    std::cout << "Initial Args():";
    printArgv(cmdline.Args());

    CLI::App _app{ "wt - the Windows Terminal" };

    CLI::App* _newTabCommand;
    std::string _startingDirectory;
    std::string _profileName;
    std::vector<std::string> _commandline;
    std::vector<std::string> realCommandlineArgs;

    _newTabCommand = _app.add_subcommand("new-tab", "Create a new tab");

    auto* profileOpt = _newTabCommand->add_option("-p,--profile",
                                                  _profileName,
                                                  "Open with the given profile. Accepts either the name or guid of a profile");

    auto* cmdlineOpt = _newTabCommand->add_option("cmdline",
                                                  _commandline,
                                                  "Commandline to run in the given profile");

    auto* startingDirectoryOpt = _newTabCommand->add_option("-d,--startingDirectory",
                                                            _startingDirectory,
                                                            "Open in the given directory instead of the profile's set startingDirectory");
    profileOpt;
    startingDirectoryOpt;
    cmdlineOpt;

    _newTabCommand->callback([&]() {
        std::cout << "parsed new tab subcommand\n";

        if (cmdlineOpt)
        {
            std::cout << "parsed _commandline\n";
            auto opts = _newTabCommand->parse_order(); // const std::vector<Option*>&
            auto foundCommandlineStart = false;
            for (auto opt : opts)
            {
                std::cout << "\tchecking an opt\n";
                if (opt == cmdlineOpt)
                {
                    foundCommandlineStart = true;
                    std::cout << "\t\tit was the commandline\n";
                }
                else if (foundCommandlineStart)
                {
                    opt->clear();
                    std::cout << "\t\twe cleared it\n";
                }
                else
                {
                    std::cout << "\t\tit preceeded the commandline\n";
                }
            }

            auto firstCmdlineArg = _commandline.at(0);
            std::cout << "firstCmdlineArg:" << firstCmdlineArg << "\n";
            std::cout << "all args:";
            printArgv(cmdline.Args());
            auto foundFirstArg = false;

            for (const auto& arg : cmdline.Args())
            {
                if (arg == firstCmdlineArg)
                {
                    foundFirstArg = true;
                }
                if (foundFirstArg)
                {
                    realCommandlineArgs.emplace_back(arg);
                }
            }
            std::cout << "real args:";
            printArgv(realCommandlineArgs);
        }

        if (*profileOpt)
        {
            std::cout << "profileOpt set\n";
            std::cout << "_profileName:\"" << _profileName << "\"\n";
        }
        if (*startingDirectoryOpt)
        {
            std::cout << "startingDirectoryOpt set\n";
            std::cout << "_startingDirectory:\"" << _startingDirectory << "\"\n";
        }
        if (*cmdlineOpt)
        {
            std::cout << "cmdlineOpt set\n";
            std::string buffer;
            size_t i = 0;
            for (auto arg : realCommandlineArgs)
            {
                if (arg.find(" ") != std::string::npos)
                {
                    buffer += "\"";
                    buffer += arg;
                    buffer += "\"";
                }
                else
                {
                    buffer += arg;
                }
                if (i + 1 < realCommandlineArgs.size())
                {
                    buffer += " ";
                }
                i++;
            }
            std::cout << "Commandline:\"" << buffer << "\"\n";
        }

        // if (!_commandline.empty())
        // {
        //     std::string buffer;
        //     size_t i = 0;
        //     for (auto arg : _commandline)
        //     {
        //         if (arg.find(" ") != std::string::npos)
        //         {
        //             buffer += "\"";
        //             buffer += arg;
        //             buffer += "\"";
        //         }
        //         else
        //         {
        //             buffer += arg;
        //         }
        //         if (i + 1 < _commandline.size())
        //         {
        //             buffer += " ";
        //         }
        //         i++;
        //     }
        //     std::cout << "Commandline:\"" << buffer << "\"\n";
        // }
        // else
        // {
        //     std::cout << "no commandline\n";
        // }

        // if (!_startingDirectory.empty())
        // {
        //     std::cout << "_startingDirectory:\"" << _startingDirectory << "\"\n";
        // }
        // else
        // {
        //     std::cout << "no _startingDirectory\n";
        // }
    });

    try
    {
        std::vector<std::string> args{ cmdline.Args().begin() + 1, cmdline.Args().end() };
        std::reverse(args.begin(), args.end());

        std::cout << "Parsed():";
        printArgv(args);
        _app.parse(args);
    }
    catch (const CLI::ParseError& e)
    {
        return _app.exit(e);
    }

    return 0;
}
