#pragma once

#include "HostClass.g.h"

namespace winrt::ScratchWinRTServer::implementation
{
    struct HostClass : HostClassT<HostClass>
    {
        HostClass(const winrt::guid& g);
        ~HostClass();
        void DoTheThing();

        int DoCount();
        winrt::guid Id();

    private:
        int _DoCount{ 0 };
        winrt::guid _id;
    };
}

namespace winrt::ScratchWinRTServer::factory_implementation
{
    struct HostClass : HostClassT<HostClass, implementation::HostClass>
    {
    };
}
