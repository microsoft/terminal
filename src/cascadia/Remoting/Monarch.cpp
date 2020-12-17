#include "pch.h"
#include "Monarch.h"
#include "CommandlineArgs.h"

#include "Monarch.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::Microsoft::Terminal::Remoting::implementation
{
    Monarch::Monarch() :
        _ourPID{ GetCurrentProcessId() }
    {
    }
    Monarch::Monarch(const uint64_t testPID) :
        _ourPID{ testPID }
    {
    }

    Monarch::~Monarch()
    {
    }

    uint64_t Monarch::GetPID()
    {
        return _ourPID;
    }

    uint64_t Monarch::AddPeasant(Remoting::IPeasant peasant)
    {
        // TODO:projects/5 This is terrible. There's gotta be a better way
        // of finding the first opening in a non-consecutive map of int->object
        auto providedID = peasant.GetID();

        if (providedID == 0)
        {
            // Peasant doesn't currently have an ID. Assign it a new one.
            peasant.AssignID(_nextPeasantID++);
        }
        else
        {
            // Peasant already had an ID (from an older monarch). Leave that one
            // be. Make sure that the next peasant's ID is higher than it.
            _nextPeasantID = providedID >= _nextPeasantID ? providedID + 1 : _nextPeasantID;
        }

        auto newPeasantsId = peasant.GetID();
        _peasants[newPeasantsId] = peasant;

        // Add an event listener to the peasant's WindowActivated event.
        peasant.WindowActivated({ this, &Monarch::_peasantWindowActivated });

        return newPeasantsId;
    }

    void Monarch::_peasantWindowActivated(const winrt::Windows::Foundation::IInspectable& sender,
                                          const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        // TODO:projects/5 Pass the desktop and timestamp of when the window was
        // activated in `args`.

        if (auto peasant{ sender.try_as<Remoting::Peasant>() })
        {
            auto theirID = peasant.GetID();
            _setMostRecentPeasant(theirID);
        }
    }

    // Method Description:
    // - Lookup a peasant by its ID.
    // Arguments:
    // - peasantID: The ID Of the peasant to find
    // Return Value:
    // - the peasant if it exists in our map, otherwise null
    Remoting::IPeasant Monarch::_getPeasant(uint64_t peasantID)
    {
        auto peasantSearch = _peasants.find(peasantID);
        return peasantSearch == _peasants.end() ? nullptr : peasantSearch->second;
    }

    void Monarch::_setMostRecentPeasant(const uint64_t peasantID)
    {
        // TODO:projects/5 Use a heap/priority queue per-desktop to track which
        // peasant was the most recent per-desktop. When we want to get the most
        // recent of all desktops (WindowingBehavior::UseExisting), then use the
        // most recent of all desktops.
        _mostRecentPeasant = peasantID;
    }

    void Monarch::SetSelfID(const uint64_t selfID)
    {
        this->_thisPeasantID = selfID;
        // Right now, the monarch assumes the role of the most recent
        // window. If the monarch dies, and a new monarch takes over, then the
        // entire stack of MRU windows will go with it. That's not what you
        // want!
        //
        // In the real app, we'll have each window also track the timestamp it
        // was activated at, and the monarch will cache these. So a new monarch
        // could re-query these last activated timestamps, and reconstruct the
        // MRU stack.
        //
        // This is a sample though, and we're not too worried about complete
        // correctness here.
        _setMostRecentPeasant(_thisPeasantID);
    }

    bool Monarch::ProposeCommandline(array_view<const winrt::hstring> /*args*/,
                                     winrt::hstring /*cwd*/)
    {
        // TODO:projects/5
        // The branch dev/migrie/f/remote-commandlines has a more complete
        // version of this function, with a naive implementation. For now, we
        // always want to create a new window, so we'll just return true. This
        // will tell the caller that we didn't handle the commandline, and they
        // should open a new window to deal with it themself.
        return true;
    }

}
