#include "pch.h"
#include "HostClass.h"

#include "HostClass.g.cpp"
namespace winrt::ScratchWinRTServer::implementation
{
    HostClass::HostClass(const winrt::guid& g) :
        _id{ g }
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
    winrt::guid HostClass::Id()
    {
        return _id;
    }
}
