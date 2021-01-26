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
                        winrt::hstring currentDirectory) :
            _args{ args.begin(), args.end() },
            _cwd{ currentDirectory }
        {
        }

        winrt::hstring CurrentDirectory() { return _cwd; };

        void Args(winrt::array_view<const winrt::hstring> const& value);
        winrt::com_array<winrt::hstring> Args();

    private:
        winrt::com_array<winrt::hstring> _args;
        winrt::hstring _cwd;
    };

}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(CommandlineArgs);
}
