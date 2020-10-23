#include "pch.h"
#include "Monarch.h"

#include "Monarch.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

extern std::mutex m;
extern std::condition_variable cv;
extern bool dtored;

namespace winrt::MonarchPeasantSample::implementation
{
    Monarch::Monarch()
    {
        printf("Instantiated a Monarch\n");
    }
    Monarch::~Monarch()
    {
        printf("~Monarch()\n");
        std::unique_lock<std::mutex> lk(m);
        dtored = true;
        cv.notify_one();
    }
}
