#include "pch.h"

#include "CommandlineArgs.h"
#include "CommandlineArgs.g.cpp"
using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    void CommandlineArgs::Args(winrt::array_view<const winrt::hstring> const& value)
    {
        _args = { value.begin(), value.end() };
    }

    winrt::com_array<winrt::hstring> CommandlineArgs::Args()
    {
        return winrt::com_array<winrt::hstring>{ _args.begin(), _args.end() };
    }
}
