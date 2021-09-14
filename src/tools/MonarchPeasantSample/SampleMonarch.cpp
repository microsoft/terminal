#include "pch.h"
#include "SampleMonarch.h"

#include "Monarch.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::MonarchPeasantSample::implementation
{
    Monarch::Monarch()
    {
        printf("Instantiated a Monarch\n");
    }

    Monarch::~Monarch()
    {
        printf("~Monarch()\n");
    }

    uint64_t Monarch::GetPID()
    {
        return GetCurrentProcessId();
    }

    uint64_t Monarch::AddPeasant(winrt::MonarchPeasantSample::IPeasant peasant)
    {
        // This whole algorithm is terrible. There's gotta be a better way
        // of finding the first opening in a non-consecutive map of int->object
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
        auto newPeasantsId = peasant.GetID();
        _peasants[newPeasantsId] = peasant;
        _setMostRecentPeasant(newPeasantsId);
        printf("(the next new peasant will get the ID %lld)\n", _nextPeasantID);

        peasant.WindowActivated({ this, &Monarch::_peasantWindowActivated });

        return newPeasantsId;
    }

    void Monarch::_peasantWindowActivated(const winrt::Windows::Foundation::IInspectable& sender,
                                          const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        if (auto peasant{ sender.try_as<winrt::MonarchPeasantSample::Peasant>() })
        {
            auto theirID = peasant.GetID();
            _setMostRecentPeasant(theirID);
        }
    }

    winrt::MonarchPeasantSample::IPeasant Monarch::_getPeasant(uint64_t peasantID)
    {
        auto peasantSearch = _peasants.find(peasantID);
        return peasantSearch == _peasants.end() ? nullptr : peasantSearch->second;
    }

    void Monarch::_setMostRecentPeasant(const uint64_t peasantID)
    {
        _mostRecentPeasant = peasantID;
        printf("\x1b[90mThe most recent peasant is now \x1b[m#%llu\n", _mostRecentPeasant);
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

    bool Monarch::ProposeCommandline(array_view<const winrt::hstring> args, winrt::hstring cwd)
    {
        auto argsProcessed = 0;
        std::wstring fullCmdline;
        for (const auto& arg : args)
        {
            fullCmdline += argsProcessed++ == 0 ? L"sample.exe" : arg;
            fullCmdline += L" ";
        }
        wprintf(L"\x1b[36mProposed Commandline\x1b[m: \"");
        wprintf(fullCmdline.c_str());
        wprintf(L"\"\n");

        bool createNewWindow = true;

        if (args.size() >= 3)
        {
            // We'll need three args at least - [MonarchPeasantSample.exe, -s,
            // id] to be able to have a session ID passed on the commandline.

            if (args[1] == L"-s" || args[1] == L"--session")
            {
                auto sessionId = std::stoi({ args[2].data(), args[2].size() });
                printf("Found a commandline intended for session %d\n", sessionId);
                if (sessionId < 0)
                {
                    printf("That certainly isn't a valid ID, they should make a new window.\n");
                    createNewWindow = true;
                }
                else if (sessionId == 0)
                {
                    printf("Session 0 is actually #%llu\n", _mostRecentPeasant);
                    if (auto mruPeasant = _getPeasant(_mostRecentPeasant))
                    {
                        mruPeasant.ExecuteCommandline(args, cwd);
                        createNewWindow = false;
                    }
                }
                else
                {
                    if (auto otherPeasant = _getPeasant(sessionId))
                    {
                        otherPeasant.ExecuteCommandline(args, cwd);
                        createNewWindow = false;
                    }
                    else
                    {
                        printf("I couldn't find a peasant for that ID, they should make a new window.\n");
                    }
                }
            }
        }
        else if (_windowingBehavior == WindowingBehavior::UseExisting)
        {
            if (auto mruPeasant = _getPeasant(_mostRecentPeasant))
            {
                mruPeasant.ExecuteCommandline(args, cwd);
                createNewWindow = false;
            }
        }
        else
        {
            printf("They definitely weren't an existing process. They should make a new window.\n");
        }

        return createNewWindow;
    }
    void Monarch::ToggleWindowingBehavior()
    {
        switch (_windowingBehavior)
        {
        case WindowingBehavior::UseNew:
            _windowingBehavior = WindowingBehavior::UseExisting;
            break;
        case WindowingBehavior::UseExisting:
            _windowingBehavior = WindowingBehavior::UseNew;
            break;
        }

        printf("windowingBehavior: ");
        switch (_windowingBehavior)
        {
        case WindowingBehavior::UseNew:
            printf("useNew");
            break;
        case WindowingBehavior::UseExisting:
            printf("useExisting");
            break;
        }
        printf("\n");
    }

}
