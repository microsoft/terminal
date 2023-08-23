#pragma once

#include "CommandlineArgs.g.h"

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
                        const uint32_t showWindowCommand) :
            _args{ args.begin(), args.end() },
            _cwd{ currentDirectory },
            _ShowWindowCommand{ showWindowCommand }
        {
        }

        winrt::hstring CurrentDirectory() { return _cwd; };

        void Commandline(const winrt::array_view<const winrt::hstring>& value);
        winrt::com_array<winrt::hstring> Commandline();

        WINRT_PROPERTY(uint32_t, ShowWindowCommand, SW_NORMAL); // SW_NORMAL is 1, 0 is SW_HIDE

    private:
        winrt::com_array<winrt::hstring> _args;
        winrt::hstring _cwd;
    };

}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(CommandlineArgs);
}
