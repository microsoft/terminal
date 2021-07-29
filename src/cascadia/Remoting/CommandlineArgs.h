#pragma once

#include "CommandlineArgs.g.h"
#include "../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct CommandlineArgs : public CommandlineArgsT<CommandlineArgs>
    {
    public:
        CommandlineArgs() :
            _args{},
            _cwd{ L"" }
        {
        }

        CommandlineArgs(const winrt::array_view<const winrt::hstring>& args,
                        winrt::hstring currentDirectory,
                        winrt::hstring stdInput) :
            _args{ args.begin(), args.end() },
            _cwd{ currentDirectory },
            _stdInput{ stdInput }
        {
        }

        winrt::hstring CurrentDirectory() { return _cwd; };
        winrt::hstring StdInput() { return _stdInput; };

        void Commandline(winrt::array_view<const winrt::hstring> const& value);
        winrt::com_array<winrt::hstring> Commandline();

    private:
        winrt::com_array<winrt::hstring> _args;
        winrt::hstring _cwd;
        winrt::hstring _stdInput;
    };

}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(CommandlineArgs);
}
