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
        _peasants = winrt::single_threaded_observable_vector<winrt::MonarchPeasantSample::Peasant>();
    }
    Monarch::~Monarch()
    {
        printf("~Monarch()\n");
        std::unique_lock<std::mutex> lk(m);
        dtored = true;
        cv.notify_one();
    }
    uint64_t Monarch::AddPeasant(winrt::MonarchPeasantSample::Peasant peasant)
    {
        _peasants.Append(peasant);
        peasant.AssignID(++_nextPeasantID);
        printf("Added a new peasant, assigned them the ID=%d\n", _nextPeasantID);
        return _nextPeasantID;
    }
}
