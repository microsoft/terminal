#pragma once

#include "HostClass.g.h"
namespace winrt::ScratchWinRTServer::implementation
{
    struct HostClass : public HostClassT<HostClass>
    {
        HostClass();
        void DoTheThing();

        int DoCount();

    private:
        int _DoCount{ 0 };
    };
}

namespace winrt::ScratchWinRTServer::factory_implementation
{
    struct HostClass : HostClassT<HostClass, implementation::HostClass>
    {
    };
}
