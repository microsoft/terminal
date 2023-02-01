
#pragma once

#include "WindowManager2.g.h"
#include "Peasant.h"
#include "Monarch.h"

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct WindowManager2 : public WindowManager2T<WindowManager2>
    {
    public:
        WindowManager2();
        winrt::Microsoft::Terminal::Remoting::ProposeCommandlineResult ProposeCommandline2(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& args);
        TYPED_EVENT(FindTargetWindowRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs);

    private:
        DWORD _registrationHostClass{ 0 };
        winrt::Microsoft::Terminal::Remoting::IMonarch _monarch{ nullptr };

        void _createMonarch();
        void _registerAsMonarch();

        void _proposeToMonarch(const Remoting::CommandlineArgs& args,
                               std::optional<uint64_t>& givenID,
                               winrt::hstring& givenName);
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(WindowManager2);
}
