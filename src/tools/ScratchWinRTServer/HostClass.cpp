#include "pch.h"
#include "HostClass.h"

#include "HostClass.g.cpp"
namespace winrt::ScratchWinRTServer::implementation
{
    HostClass::HostClass()
    {
    }
    int HostClass::DoCount()
    {
        return _DoCount;
    }

    void HostClass::DoTheThing()
    {
        _DoCount++;
    }
}
