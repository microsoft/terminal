// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Monarch.g.h"
#include "Peasant.h"
#include "../cascadia/inc/cppwinrt_utils.h"

// We sure different GUIDs here depending on whether we're running a Release,
// Preview, or Dev build. This ensures that different installs don't
// accidentally talk to one another.
//
// * Release: {06171993-7eb1-4f3e-85f5-8bdd7386cce3}
// * Preview: {04221993-7eb1-4f3e-85f5-8bdd7386cce3}
// * Dev:     {08302020-7eb1-4f3e-85f5-8bdd7386cce3}
constexpr GUID Monarch_clsid
{
#if defined(WT_BRANDING_RELEASE)
    0x06171993,
#elif defined(WT_BRANDING_PREVIEW)
    0x04221993,
#else
    0x08302020,
#endif
        0x7eb1,
        0x4f3e,
    {
        0x85, 0xf5, 0x8b, 0xdd, 0x73, 0x86, 0xcc, 0xe3
    }
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

        bool ProposeCommandline(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& args);

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
