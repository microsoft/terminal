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
        // _peasants = winrt::single_threaded_observable_map<uint64_t, winrt::MonarchPeasantSample::IPeasant>();
    }

    Monarch::~Monarch()
    {
        printf("~Monarch()\n");
        std::unique_lock<std::mutex> lk(m);
        dtored = true;
        cv.notify_one();
    }

    uint64_t Monarch::GetPID()
    {
        return GetCurrentProcessId();
    }

    uint64_t Monarch::AddPeasant(winrt::MonarchPeasantSample::IPeasant peasant)
    {
        // TODO: This whole algo is terrible. There's gotta be a better way of
        // finding the first opening in a non-consecutive map of int->object
        auto providedID = peasant.GetID();

        if (providedID == 0)
        {
            peasant.AssignID(_nextPeasantID++);
            printf("Assigned the peasant the ID %lld\n", peasant.GetID());
        }
        else
        {
            printf("Peasant already had an ID, %lld\n", peasant.GetID());
            _nextPeasantID = providedID >= _nextPeasantID ? providedID + 1 : _nextPeasantID;
        }
        _peasants[peasant.GetID()] = peasant;
        printf("(the next new peasant will get the ID %lld)\n", _nextPeasantID);
        return peasant.GetID();
    }

    bool Monarch::IsInSingleInstanceMode()
    {
        return false;
    }

    winrt::MonarchPeasantSample::IPeasant Monarch::GetPeasant(uint64_t peasantID)
    {
        return _peasants.at(peasantID);
    }

    winrt::MonarchPeasantSample::IPeasant Monarch::GetMostRecentPeasant()
    {
        return nullptr;
    }

    void Monarch::SetSelfID(const uint64_t selfID)
    {
        _thisPeasantID = selfID;
    }
}
