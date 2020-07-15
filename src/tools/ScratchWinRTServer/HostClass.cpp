#include "pch.h"
#include "HostClass.h"

#include "HostClass.g.cpp"

extern std::mutex m;
extern std::condition_variable cv;
extern bool dtored;

namespace winrt::ScratchWinRTServer::implementation
{
    HostClass::HostClass(const winrt::guid& g) :
        _id{ g }
    {
    }
    HostClass::~HostClass()
    {
        printf("~HostClass()\n");
        std::unique_lock<std::mutex> lk(m);
        dtored = true;
        cv.notify_one();
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

    HRESULT __stdcall HostClass::Call()
    {
        _DoCount += 4;
        return S_OK;
    }
}
