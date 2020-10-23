#pragma once

#include "Monarch.g.h"
#include "Peasant.h"
#include "../cascadia/inc/cppwinrt_utils.h"

// 50dba6cd-2222-4b12-8363-5e06f5d0082c
constexpr GUID Monarch_clsid{
    0x50dba6cd,
    0x2222,
    0x4b12,
    { 0x83, 0x63, 0x5e, 0x06, 0xf5, 0xd0, 0x08, 0x2c }
};

namespace winrt::MonarchPeasantSample::implementation
{
    struct Monarch : public MonarchT<Monarch>, public PeasantBase
    {
        Monarch();
        ~Monarch();

        uint64_t AddPeasant(winrt::MonarchPeasantSample::IPeasant peasant);
        bool IsInSingleInstanceMode() { return false; }
        winrt::MonarchPeasantSample::IPeasant GetPeasant(uint64_t peasantID)
        {
            peasantID;
            return nullptr;
        }
        winrt::MonarchPeasantSample::IPeasant GetMostRecentPeasant() { return nullptr; }

    private:
        uint64_t _nextPeasantID{ 1 };
        Windows::Foundation::Collections::IObservableMap<uint64_t, winrt::MonarchPeasantSample::IPeasant> _peasants{ nullptr };
    };
}

namespace winrt::MonarchPeasantSample::factory_implementation
{
    struct Monarch : MonarchT<Monarch, implementation::Monarch>
    {
    };
}
