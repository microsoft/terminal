#include "pch.h"
#include "Monarch.h"

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
        // TODO: This whole algorithm is terrible. There's gotta be a better way
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
        _mostRecentPeasant = newPeasantsId;
        printf("(the next new peasant will get the ID %lld)\n", _nextPeasantID);
        return newPeasantsId;
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
        // TODO: Right now, the monarch assumes the role of the most recent
        // window. If the monarch dies, and a new monarch takes over, then the
        // entire stack of MRU windows will go with it. That's not what you
        // want!
        _mostRecentPeasant = _thisPeasantID;
    }

    bool Monarch::ProposeCommandline(array_view<const winrt::hstring> args, winrt::hstring cwd)
    {
        auto argsProcessed = 0;
        std::wstring fullCmdline;
        for (const auto& arg : args)
        {
            fullCmdline += argsProcessed++ == 0 ? L"EXENAME.exe" : arg;
            fullCmdline += L" ";
        }
        wprintf(L"\x1b[36mProposed Commandline\x1b[m: \"");
        wprintf(fullCmdline.c_str());
        wprintf(L"\"\n");

        bool createNewWindow = true;

        if (_windowingBehavior == GlomToLastWindow::Always)
        {
            // This is "single instance mode". We should always eat the commandline.

            if (auto thisPeasant = GetPeasant(_thisPeasantID))
            {
                thisPeasant.ExecuteCommandline(args, cwd);
                createNewWindow = false;
                return createNewWindow;
            }
        }

        if (args.size() >= 3)
        {
            // We'll need three args at least - [exename.exe, -s, id] to be able
            // to have a session ID passed on the commandline.
            // printf("The new process provided tribute, we'll eat it. No need to create a new window.\n");

            if (args[1] == L"-s" || args[1] == L"--session")
            {
                auto sessionId = std::stoi({ args[2].data(), args[2].size() });
                printf("Found a commandline intended for session %d\n", sessionId);
                if (sessionId < 0)
                {
                    createNewWindow = false;
                }
                else if (sessionId == 0)
                {
                    if (auto mruPeasant = GetPeasant(_mostRecentPeasant))
                    {
                        mruPeasant.ExecuteCommandline(args, cwd);
                        createNewWindow = false;
                    }
                }
                else
                {
                    if (auto otherPeasant = GetPeasant(sessionId))
                    {
                        otherPeasant.ExecuteCommandline(args, cwd);
                        createNewWindow = false;
                    }
                }
            }
        }
        else if (_windowingBehavior == GlomToLastWindow::LastActive)
        {
            if (auto mruPeasant = GetPeasant(_mostRecentPeasant))
            {
                mruPeasant.ExecuteCommandline(args, cwd);
                createNewWindow = false;
            }
        }
        else
        {
            // printf("They definitely weren't an existing process. They should make a new window.\n");
        }

        return createNewWindow;
    }
    void Monarch::ToggleWindowingBehavior()
    {
        switch (_windowingBehavior)
        {
        case GlomToLastWindow::Never:
            _windowingBehavior = GlomToLastWindow::LastActive;
            break;
        case GlomToLastWindow::LastActive:
            _windowingBehavior = GlomToLastWindow::Always;
            break;
        case GlomToLastWindow::Always:
            _windowingBehavior = GlomToLastWindow::Never;
            break;
        }

        printf("glomToLastWindow: ");
        switch (_windowingBehavior)
        {
        case GlomToLastWindow::Never:
            printf("never");
            break;
        case GlomToLastWindow::LastActive:
            printf("lastActive");
            break;
        case GlomToLastWindow::Always:
            printf("always");
            break;
        }
        printf("\n");
    }

}
