#include "pch.h"
#include <argb.h>
#include <DefaultSettings.h>
#include "HostClass.h"
#include "HostClass.g.cpp"

// #include <wrl.h>
extern std::mutex m;
extern std::condition_variable cv;
extern bool dtored;

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::System;
// using namespace winrt::Microsoft::Terminal::Settings;
// using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::ScratchWinRTServer::implementation
{
    HostClass::HostClass(const winrt::guid& g) :
        _id{ g }
    {
        printf("HostClass()\n");
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

}
