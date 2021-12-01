#pragma once

#include "Peasant.g.h"

namespace winrt::MonarchPeasantSample::implementation
{
    struct Peasant : public PeasantT<Peasant>
    {
        Peasant();

        void AssignID(uint64_t id);
        uint64_t GetID();
        uint64_t GetPID();

        bool ExecuteCommandline(winrt::array_view<const winrt::hstring> args, winrt::hstring currentDirectory);

        void raiseActivatedEvent();

        TYPED_EVENT(WindowActivated, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);

    private:
        uint64_t _id{ 0 };
    };
}

namespace winrt::MonarchPeasantSample::factory_implementation
{
    BASIC_FACTORY(Peasant);
}
