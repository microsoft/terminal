#include "pch.h"

#include "CommandlineArgs.h"
#include "CommandlineArgs.g.cpp"
using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    // LOAD BEARING CODE
    // If you try to move this into the header, you will experience P A I N
    // It must be defined after CommandlineArgs.g.cpp, otherwise the compiler
    // will give you just the most impossible template errors to try and
    // decipher.
    void CommandlineArgs::Commandline(const winrt::array_view<const winrt::hstring>& value)
    {
        _args = { value.begin(), value.end() };
    }

    winrt::com_array<winrt::hstring> CommandlineArgs::Commandline()
    {
        return winrt::com_array<winrt::hstring>{ _args.begin(), _args.end() };
    }
}
