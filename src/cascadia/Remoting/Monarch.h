#pragma once

#include "Monarch.g.h"
#include "Peasant.h"
#include "../cascadia/inc/cppwinrt_utils.h"

// {06171993-7eb1-4f3e-85f5-8bdd7386cce3}
constexpr GUID Monarch_clsid{
    0x06171993,
    0x7eb1,
    0x4f3e,
    { 0x85, 0xf5, 0x8b, 0xdd, 0x73, 0x86, 0xcc, 0xe3 }
};

enum class WindowingBehavior : uint64_t
{
    UseNew = 0,
    UseExisting = 1,
};

namespace RemotingUnitTests
{
    class RemotingTests;
};

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    struct Monarch : public MonarchT<Monarch>
    {
        Monarch();
        ~Monarch();

        uint64_t GetPID();

        uint64_t AddPeasant(winrt::Microsoft::Terminal::Remoting::IPeasant peasant);

        bool ProposeCommandline(array_view<const winrt::hstring> args, winrt::hstring cwd);

    private:
        Monarch(const uint64_t testPID);
        uint64_t _ourPID;

        uint64_t _nextPeasantID{ 1 };
        uint64_t _thisPeasantID{ 0 };
        uint64_t _mostRecentPeasant{ 0 };
        WindowingBehavior _windowingBehavior{ WindowingBehavior::UseNew };
        std::unordered_map<uint64_t, winrt::Microsoft::Terminal::Remoting::IPeasant> _peasants;

        winrt::Microsoft::Terminal::Remoting::IPeasant _getPeasant(uint64_t peasantID);
        void _setMostRecentPeasant(const uint64_t peasantID);

        void _peasantWindowActivated(const winrt::Windows::Foundation::IInspectable& sender,
                                     const winrt::Windows::Foundation::IInspectable& args);

        friend class RemotingUnitTests::RemotingTests;
    };
}

namespace winrt::Microsoft::Terminal::Remoting::factory_implementation
{
    BASIC_FACTORY(Monarch);
}
