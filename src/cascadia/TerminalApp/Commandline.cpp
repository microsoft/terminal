
#include "pch.h"
#include "Commandline.h"

using namespace TerminalApp;

size_t Commandline::Argc() const
{
    return _wargs.size();
}
char** Commandline::Argv() const
{
    return _argv;
}
const std::vector<std::wstring>& Commandline::Wargs() const
{
    return _wargs;
};
char** Commandline::BuildArgv()
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

void Commandline::AddArg(const std::wstring& nextArg)
{
    if (_argv)
    {
        _resetArgv();
    }

    _wargs.emplace_back(nextArg);
}

void Commandline::_resetArgv()
{
    for (int i = 0; i < Argc(); i++)
    {
        delete[] _argv[i];
    }
    delete[] _argv;
}
