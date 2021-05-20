// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../Remoting/Monarch.h"
#include "../Remoting/CommandlineArgs.h"
#include "../Remoting/FindTargetWindowArgs.h"
#include "../Remoting/ProposeCommandlineResult.h"
#include "../inc/WindowingBehavior.h"

using namespace Microsoft::Console;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace WEX::Common;

using namespace winrt;
using namespace winrt::Microsoft::Terminal;

// These are some gross macros that let us call a private ctor for
// Monarch/Peasant. We can't just use make_self, because that doesn't let us
// call a private ctor. We can use com_ptr::attach, but since we're allocating
// the thing on the stack, we need to make sure to call detach before the object
// is destructed.

#define MAKE_MONARCH(name, pid)                               \
    Remoting::implementation::Monarch _local_##name##{ pid }; \
    com_ptr<Remoting::implementation::Monarch> name;          \
    name.attach(&_local_##name##);                            \
    auto cleanup_##name## = wil::scope_exit([&]() { name.detach(); });

#define MAKE_PEASANT(name, pid)                               \
    Remoting::implementation::Peasant _local_##name##{ pid }; \
    com_ptr<Remoting::implementation::Peasant> name;          \
    name.attach(&_local_##name##);                            \
    auto cleanup_##name## = wil::scope_exit([&]() { name.detach(); });

namespace RemotingUnitTests
{
    struct MockDesktopManager : implements<MockDesktopManager, IVirtualDesktopManager>
    {
        IFACEMETHOD(GetWindowDesktopId)
        (HWND /*topLevelWindow*/, GUID* /*desktopId*/)
        {
            VERIFY_IS_TRUE(false, L"We shouldn't need GetWindowDesktopId in the tests.");
            return E_FAIL;
        }
        IFACEMETHOD(MoveWindowToDesktop)
        (HWND /*topLevelWindow*/, REFGUID /*desktopId*/)
        {
            VERIFY_IS_TRUE(false, L"We shouldn't need GetWindowDesktopId in the tests.");
            return E_FAIL;
        }
        IFACEMETHOD(IsWindowOnCurrentVirtualDesktop)
        (HWND topLevelWindow, BOOL* onCurrentDesktop)
        {
            if (pfnIsWindowOnCurrentVirtualDesktop)
            {
                return pfnIsWindowOnCurrentVirtualDesktop(topLevelWindow, onCurrentDesktop);
            }
            VERIFY_IS_TRUE(false, L"You didn't set up the pfnIsWindowOnCurrentVirtualDesktop for this test!");
            return E_FAIL;
        }

        std::function<HRESULT(HWND, BOOL*)> pfnIsWindowOnCurrentVirtualDesktop;
    };

    // This is a silly helper struct.
    // It will always throw an hresult_error on any of its methods.
    //
    // In the tests, it's hard to emulate a peasant process being totally dead
    // once the Monarch has captured a reference to it. Since everything's
    // in-proc in the tests, we can't decrement the refcount in such a way that
    // the monarch's reference will throw a catchable exception. Instead, this
    // class can be used to replace a peasant inside a Monarch, to emulate that
    // peasant process dying. Any time the monarch tries to do something to this
    // peasant, it'll throw an exception.
    struct DeadPeasant : implements<DeadPeasant, winrt::Microsoft::Terminal::Remoting::IPeasant>
    {
        DeadPeasant() = default;
        void AssignID(uint64_t /*id*/) { throw winrt::hresult_error{}; };
        uint64_t GetID() { throw winrt::hresult_error{}; };
        winrt::hstring WindowName() { throw winrt::hresult_error{}; };
        uint64_t GetPID() { throw winrt::hresult_error{}; };
        bool ExecuteCommandline(const Remoting::CommandlineArgs& /*args*/) { throw winrt::hresult_error{}; }
        void ActivateWindow(const Remoting::WindowActivatedArgs& /*args*/) { throw winrt::hresult_error{}; }
        void RequestIdentifyWindows() { throw winrt::hresult_error{}; };
        void DisplayWindowId() { throw winrt::hresult_error{}; };
        Remoting::CommandlineArgs InitialArgs() { throw winrt::hresult_error{}; }
        Remoting::WindowActivatedArgs GetLastActivatedArgs() { throw winrt::hresult_error{}; }
        void RequestRename(const Remoting::RenameRequestArgs& /*args*/) { throw winrt::hresult_error{}; }
        void Summon(const Remoting::SummonWindowBehavior& /*args*/) { throw winrt::hresult_error{}; };
        TYPED_EVENT(WindowActivated, winrt::Windows::Foundation::IInspectable, Remoting::WindowActivatedArgs);
        TYPED_EVENT(ExecuteCommandlineRequested, winrt::Windows::Foundation::IInspectable, Remoting::CommandlineArgs);
        TYPED_EVENT(IdentifyWindowsRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(DisplayWindowIdRequested, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(RenameRequested, winrt::Windows::Foundation::IInspectable, Remoting::RenameRequestArgs);
        TYPED_EVENT(SummonRequested, winrt::Windows::Foundation::IInspectable, Remoting::SummonWindowBehavior);
    };

    class RemotingTests
    {
        BEGIN_TEST_CLASS(RemotingTests)
        END_TEST_CLASS()

        TEST_METHOD(CreateMonarch);
        TEST_METHOD(CreatePeasant);
        TEST_METHOD(CreatePeasantWithNew);
        TEST_METHOD(AddPeasants);
        TEST_METHOD(GetPeasantsByID);
        TEST_METHOD(AddPeasantsToNewMonarch);
        TEST_METHOD(RemovePeasantFromMonarchWhenFreed);

        TEST_METHOD(ProposeCommandlineNoWindow);
        TEST_METHOD(ProposeCommandlineGivenWindow);
        TEST_METHOD(ProposeCommandlineNegativeWindow);
        TEST_METHOD(ProposeCommandlineCurrentWindow);
        TEST_METHOD(ProposeCommandlineNonExistentWindow);
        TEST_METHOD(ProposeCommandlineDeadWindow);

        TEST_METHOD(MostRecentWindowSameDesktops);
        TEST_METHOD(MostRecentWindowDifferentDesktops);
        TEST_METHOD(MostRecentWindowMoveDesktops);
        TEST_METHOD(GetMostRecentAnyDesktop);
        TEST_METHOD(MostRecentIsDead);

        TEST_METHOD(MostRecentIsQuake);

        TEST_METHOD(GetPeasantsByName);
        TEST_METHOD(AddNamedPeasantsToNewMonarch);
        TEST_METHOD(LookupNamedPeasantWhenOthersDied);
        TEST_METHOD(LookupNamedPeasantWhenItDied);
        TEST_METHOD(GetMruPeasantAfterNameLookupForDeadPeasant);

        TEST_METHOD(ProposeCommandlineForNamedDeadWindow);

        TEST_METHOD(TestRenameWindowSuccessfully);
        TEST_METHOD(TestRenameSameNameAsAnother);
        TEST_METHOD(TestRenameSameNameAsADeadPeasant);

        TEST_METHOD(TestSummonMostRecentWindow);
        TEST_METHOD(TestSummonNamedWindow);
        TEST_METHOD(TestSummonNamedDeadWindow);
        TEST_METHOD(TestSummonMostRecentDeadWindow);

        TEST_METHOD(TestSummonOnCurrent);
        TEST_METHOD(TestSummonOnCurrentWithName);
        TEST_METHOD(TestSummonOnCurrentDeadWindow);

        TEST_METHOD(TestSummonMostRecentIsQuake);

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }

        static void _killPeasant(const com_ptr<Remoting::implementation::Monarch>& m,
                                 const uint64_t peasantID);

        static void _findTargetWindowHelper(const winrt::Windows::Foundation::IInspectable& sender,
                                            const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args);

        static void _findTargetWindowByNameHelper(const winrt::Windows::Foundation::IInspectable& sender,
                                                  const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args);
    };

    // Helper to replace the specified peasant in a monarch with a
    // "DeadPeasant", which will emulate what happens when the peasant process
    // dies.
    void RemotingTests::_killPeasant(const com_ptr<Remoting::implementation::Monarch>& m,
                                     const uint64_t peasantID)
    {
        if (peasantID <= 0)
        {
            return;
        }

        com_ptr<DeadPeasant> tombstone;
        tombstone.attach(new DeadPeasant());
        m->_peasants[peasantID] = *tombstone;
    }

    // Helper to get the first argument out of the commandline, and try to
    // convert it to an int.
    void RemotingTests::_findTargetWindowHelper(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args)
    {
        const auto arguments = args.Args().Commandline();
        if (arguments.size() > 0)
        {
            const auto index = std::stoi(arguments.at(0).c_str());
            args.ResultTargetWindow(index >= 0 ? index : -1);
        }
    }

    // Helper to get the first argument out of the commandline, and return it as
    // a name to use.
    void RemotingTests::_findTargetWindowByNameHelper(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                                      const winrt::Microsoft::Terminal::Remoting::FindTargetWindowArgs& args)
    {
        const auto arguments = args.Args().Commandline();
        if (arguments.size() > 0)
        {
            args.ResultTargetWindow(WindowingBehaviorUseName);
            args.ResultTargetWindowName(arguments.at(0));
        }
    }

    void RemotingTests::CreateMonarch()
    {
        auto m1 = winrt::make_self<Remoting::implementation::Monarch>();
        VERIFY_IS_NOT_NULL(m1);
        VERIFY_ARE_EQUAL(GetCurrentProcessId(),
                         m1->GetPID(),
                         L"A Monarch without an explicit PID should use the current PID");

        Log::Comment(L"That's what we need for window process management, but for tests, it'll be more useful to fake the PIDs.");

        auto expectedFakePID = 1234u;
        MAKE_MONARCH(m2, expectedFakePID);

        VERIFY_IS_NOT_NULL(m2);
        VERIFY_ARE_EQUAL(expectedFakePID,
                         m2->GetPID(),
                         L"A Monarch with an explicit PID should use the one we provided");
    }

    void RemotingTests::CreatePeasant()
    {
        auto p1 = winrt::make_self<Remoting::implementation::Peasant>();
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_ARE_EQUAL(GetCurrentProcessId(),
                         p1->GetPID(),
                         L"A Peasant without an explicit PID should use the current PID");

        Log::Comment(L"That's what we need for window process management, but for tests, it'll be more useful to fake the PIDs.");

        auto expectedFakePID = 2345u;
        MAKE_PEASANT(p2, expectedFakePID);

        VERIFY_IS_NOT_NULL(p2);
        VERIFY_ARE_EQUAL(expectedFakePID,
                         p2->GetPID(),
                         L"A Peasant with an explicit PID should use the one we provided");
    }

    void RemotingTests::CreatePeasantWithNew()
    {
        Log::Comment(L"The same thing as the above test, but with `new` instead of insanity on the stack");

        auto p1 = winrt::make_self<Remoting::implementation::Peasant>();
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_ARE_EQUAL(GetCurrentProcessId(),
                         p1->GetPID(),
                         L"A Peasant without an explicit PID should use the current PID");

        auto expectedFakePID = 2345u;

        com_ptr<Remoting::implementation::Peasant> p2;
        VERIFY_IS_NULL(p2);
        p2.attach(new Remoting::implementation::Peasant(expectedFakePID));

        VERIFY_IS_NOT_NULL(p2);
        VERIFY_ARE_EQUAL(expectedFakePID,
                         p2->GetPID(),
                         L"A Peasant with an explicit PID should use the one we provided");
    }

    void RemotingTests::AddPeasants()
    {
        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());
    }

    void RemotingTests::GetPeasantsByID()
    {
        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        auto maybeP1 = m0->_getPeasant(1);
        VERIFY_IS_NOT_NULL(maybeP1);
        VERIFY_ARE_EQUAL(peasant1PID, maybeP1.GetPID());

        auto maybeP2 = m0->_getPeasant(2);
        VERIFY_IS_NOT_NULL(maybeP2);
        VERIFY_ARE_EQUAL(peasant2PID, maybeP2.GetPID());
    }

    void RemotingTests::AddPeasantsToNewMonarch()
    {
        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;
        const auto monarch3PID = 45678u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        com_ptr<Remoting::implementation::Monarch> m3;
        m3.attach(new Remoting::implementation::Monarch(monarch3PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        VERIFY_IS_NOT_NULL(m3);

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        m3->AddPeasant(*p1);
        m3->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());
    }

    void RemotingTests::RemovePeasantFromMonarchWhenFreed()
    {
        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        Log::Comment(L"Kill peasant 1. Make sure that it gets removed from the monarch.");
        RemotingTests::_killPeasant(m0, p1->GetID());

        auto maybeP2 = m0->_getPeasant(2);
        VERIFY_IS_NOT_NULL(maybeP2);
        VERIFY_ARE_EQUAL(peasant2PID, maybeP2.GetPID());

        auto maybeP1 = m0->_getPeasant(1);
        VERIFY_IS_NULL(maybeP1);

        VERIFY_ARE_EQUAL(1u, m0->_peasants.size());
    }

    void RemotingTests::ProposeCommandlineNoWindow()
    {
        Log::Comment(L"Test proposing a commandline that doesn't have a window specified in it");

        const auto monarch0PID = 12345u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        std::vector<winrt::hstring> args{};
        Remoting::CommandlineArgs eventArgs{ { args }, { L"" } };

        auto result = m0->ProposeCommandline(eventArgs);
        VERIFY_ARE_EQUAL(true, result.ShouldCreateWindow());
        VERIFY_ARE_EQUAL(false, (bool)result.Id());

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        Log::Comment(L"Propose the same args again after adding a peasant - we should still return {create new window, no ID}");
        result = m0->ProposeCommandline(eventArgs);
        VERIFY_ARE_EQUAL(true, result.ShouldCreateWindow());
        VERIFY_ARE_EQUAL(false, (bool)result.Id());
    }

    void RemotingTests::ProposeCommandlineGivenWindow()
    {
        Log::Comment(L"Test proposing a commandline for a window that currently exists");

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        p1->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& cmdlineArgs) {
            Log::Comment(L"Commandline dispatched to p1");
            VERIFY_IS_GREATER_THAN(cmdlineArgs.Commandline().size(), 1u);
            VERIFY_ARE_EQUAL(L"arg[1]", cmdlineArgs.Commandline().at(1));
        });

        std::vector<winrt::hstring> args{ L"1", L"arg[1]" };
        Remoting::CommandlineArgs eventArgs{ { args }, { L"" } };

        auto result = m0->ProposeCommandline(eventArgs);
        VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
        VERIFY_ARE_EQUAL(false, (bool)result.Id());
    }
    void RemotingTests::ProposeCommandlineNegativeWindow()
    {
        Log::Comment(L"Test proposing a commandline for an invalid window ID, like -1");

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        {
            std::vector<winrt::hstring> args{ L"-1" };
            Remoting::CommandlineArgs eventArgs{ { args }, { L"" } };

            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(true, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id());
        }
        {
            std::vector<winrt::hstring> args{ L"-2" };
            Remoting::CommandlineArgs eventArgs{ { args }, { L"" } };

            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(true, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id());
        }
    }
    void RemotingTests::ProposeCommandlineCurrentWindow()
    {
        Log::Comment(L"Test proposing a commandline for the current window (ID=0)");

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);
        p1->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& cmdlineArgs) {
            Log::Comment(L"Commandline dispatched to p1");
            VERIFY_IS_GREATER_THAN(cmdlineArgs.Commandline().size(), 1u);
            VERIFY_ARE_EQUAL(L"arg[1]", cmdlineArgs.Commandline().at(1));
        });

        std::vector<winrt::hstring> p1Args{ L"0", L"arg[1]" };
        std::vector<winrt::hstring> p2Args{ L"0", L"this is for p2" };

        {
            Log::Comment(L"Manually activate the first peasant");
            // This would usually happen immediately when the window is created, but
            // there's no actual window in these tests.
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         winrt::guid{},
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);

            Remoting::CommandlineArgs eventArgs{ { p1Args }, { L"" } };

            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id());
        }

        Log::Comment(L"Add a second peasant");
        const auto peasant2PID = 34567u;
        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));
        VERIFY_IS_NOT_NULL(p2);
        m0->AddPeasant(*p2);
        p2->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& cmdlineArgs) {
            Log::Comment(L"Commandline dispatched to p2");
            VERIFY_IS_GREATER_THAN(cmdlineArgs.Commandline().size(), 1u);
            VERIFY_ARE_EQUAL(L"this is for p2", cmdlineArgs.Commandline().at(1));
        });

        {
            Log::Comment(L"Activate the second peasant");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         winrt::guid{},
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);

            Log::Comment(L"Send a commandline to the current window, which should be p2");
            Remoting::CommandlineArgs eventArgs{ { p2Args }, { L"" } };
            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id());
        }
        {
            Log::Comment(L"Reactivate the first peasant");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         winrt::guid{},
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);

            Log::Comment(L"Send a commandline to the current window, which should be p1 again");
            Remoting::CommandlineArgs eventArgs{ { p1Args }, { L"" } };
            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id());
        }
    }
    void RemotingTests::ProposeCommandlineNonExistentWindow()
    {
        Log::Comment(L"Test proposing a commandline for an ID that doesn't have a current peasant");

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        {
            std::vector<winrt::hstring> args{ L"2" };
            Remoting::CommandlineArgs eventArgs{ { args }, { L"" } };

            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(true, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(true, (bool)result.Id());
            VERIFY_ARE_EQUAL(2u, result.Id().Value());
        }
        {
            std::vector<winrt::hstring> args{ L"10" };
            Remoting::CommandlineArgs eventArgs{ { args }, { L"" } };

            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(true, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(true, (bool)result.Id());
            VERIFY_ARE_EQUAL(10u, result.Id().Value());
        }
    }

    void RemotingTests::ProposeCommandlineDeadWindow()
    {
        Log::Comment(L"Test proposing a commandline for a peasant that previously died");

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);
        p1->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& /*cmdlineArgs*/) {
            Log::Comment(L"Commandline dispatched to p1");
            VERIFY_IS_TRUE(false, L"This should not happen, this peasant should be dead.");
        });

        Log::Comment(L"Add a second peasant");
        const auto peasant2PID = 34567u;
        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));
        VERIFY_IS_NOT_NULL(p2);
        m0->AddPeasant(*p2);
        p2->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& cmdlineArgs) {
            Log::Comment(L"Commandline dispatched to p2");
            VERIFY_IS_GREATER_THAN(cmdlineArgs.Commandline().size(), 1u);
            VERIFY_ARE_EQUAL(L"this is for p2", cmdlineArgs.Commandline().at(1));
        });

        std::vector<winrt::hstring> p1Args{ L"1", L"arg[1]" };
        std::vector<winrt::hstring> p2Args{ L"2", L"this is for p2" };

        Log::Comment(L"Kill peasant 1");

        _killPeasant(m0, 1);

        {
            Log::Comment(L"Send a commandline to p2, who is still alive. We won't create a new window.");

            Remoting::CommandlineArgs eventArgs{ { p2Args }, { L"" } };

            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id());
        }
        {
            Log::Comment(L"Send a commandline to p1, who is dead. We will create a new window.");
            Remoting::CommandlineArgs eventArgs{ { p1Args }, { L"" } };

            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(true, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(true, (bool)result.Id());
            VERIFY_ARE_EQUAL(1u, result.Id().Value());
        }
    }

    // TODO:projects/5
    //
    // In order to test WindowingBehaviorUseExisting, we'll have to
    // create our own IVirtualDesktopManager implementation that can be subbed
    // in for testing. We can't _actually_ create HWNDs as a part of the test
    // and move them to different desktops. Instead, we'll have to create a stub
    // impl that can fake a result for IsWindowOnCurrentVirtualDesktop.

    void RemotingTests::MostRecentWindowSameDesktops()
    {
        Log::Comment(L"Make windows on the same desktop. Validate the contents of _mruPeasants are as expected.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{22222222-2222-2222-2222-222222222222}") };

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        Log::Comment(L"Add a second peasant");
        const auto peasant2PID = 34567u;
        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));
        VERIFY_IS_NOT_NULL(p2);
        m0->AddPeasant(*p2);

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(2u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[1].PeasantID());

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(2u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[1].PeasantID());
    }

    void RemotingTests::MostRecentWindowDifferentDesktops()
    {
        Log::Comment(L"Make windows on different desktops. Validate the contents of _mruPeasants are as expected.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{22222222-2222-2222-2222-222222222222}") };

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        Log::Comment(L"Add a second peasant");
        const auto peasant2PID = 34567u;
        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));
        VERIFY_IS_NOT_NULL(p2);
        m0->AddPeasant(*p2);

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid2,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(2u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[1].PeasantID());

        Log::Comment(L"Add a third peasant");
        const auto peasant3PID = 45678u;
        com_ptr<Remoting::implementation::Peasant> p3;
        p3.attach(new Remoting::implementation::Peasant(peasant3PID));
        VERIFY_IS_NOT_NULL(p3);
        m0->AddPeasant(*p3);
        {
            Log::Comment(L"Activate the third peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p3->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p3->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(3u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p3->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[1].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[2].PeasantID());

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(3u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p3->GetID(), m0->_mruPeasants[1].PeasantID());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[2].PeasantID());
    }

    void RemotingTests::MostRecentWindowMoveDesktops()
    {
        Log::Comment(L"Make windows on different desktops. Move one to another "
                     L"desktop. Validate the contents of _mruPeasants are as expected.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{22222222-2222-2222-2222-222222222222}") };

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        Log::Comment(L"Add a second peasant");
        const auto peasant2PID = 34567u;
        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));
        VERIFY_IS_NOT_NULL(p2);
        m0->AddPeasant(*p2);

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid2,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(2u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[1].PeasantID());

        Log::Comment(L"Add a third peasant");
        const auto peasant3PID = 45678u;
        com_ptr<Remoting::implementation::Peasant> p3;
        p3.attach(new Remoting::implementation::Peasant(peasant3PID));
        VERIFY_IS_NOT_NULL(p3);
        m0->AddPeasant(*p3);
        {
            Log::Comment(L"Activate the third peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p3->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p3->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(3u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p3->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[1].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[2].PeasantID());

        {
            Log::Comment(L"Activate the first peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid2,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(3u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p3->GetID(), m0->_mruPeasants[1].PeasantID());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[2].PeasantID());

        {
            Log::Comment(L"Activate the third peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p3->GetID(),
                                                         guid2,
                                                         winrt::clock().now() };
            p3->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(3u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p3->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[1].PeasantID());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[2].PeasantID());

        {
            Log::Comment(L"Activate the second peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(3u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p3->GetID(), m0->_mruPeasants[1].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[2].PeasantID());
    }

    void RemotingTests::GetMostRecentAnyDesktop()
    {
        Log::Comment(L"Make windows on different desktops. Confirm that "
                     L"getting the most recent of all windows works as expected.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{22222222-2222-2222-2222-222222222222}") };

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        Log::Comment(L"Add a second peasant");
        const auto peasant2PID = 34567u;
        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));
        VERIFY_IS_NOT_NULL(p2);
        m0->AddPeasant(*p2);

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid2,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_getMostRecentPeasantID(false, true));

        Log::Comment(L"Add a third peasant");
        const auto peasant3PID = 45678u;
        com_ptr<Remoting::implementation::Peasant> p3;
        p3.attach(new Remoting::implementation::Peasant(peasant3PID));
        VERIFY_IS_NOT_NULL(p3);
        m0->AddPeasant(*p3);
        {
            Log::Comment(L"Activate the third peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p3->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p3->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(p3->GetID(), m0->_getMostRecentPeasantID(false, true));

        {
            Log::Comment(L"Activate the first peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid2,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_getMostRecentPeasantID(false, true));
    }

    void RemotingTests::MostRecentIsDead()
    {
        Log::Comment(L"Make two windows. Activate the first, then the second. "
                     L"Kill the second. The most recent should be the _first_ window.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{22222222-2222-2222-2222-222222222222}") };

        const auto monarch0PID = 12345u;
        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        Log::Comment(L"Add a peasant");
        const auto peasant1PID = 23456u;
        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));
        VERIFY_IS_NOT_NULL(p1);
        m0->AddPeasant(*p1);

        Log::Comment(L"Add a second peasant");
        const auto peasant2PID = 34567u;
        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));
        VERIFY_IS_NOT_NULL(p2);
        m0->AddPeasant(*p2);

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(2u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[1].PeasantID());

        Log::Comment(L"Kill peasant 2");
        RemotingTests::_killPeasant(m0, p2->GetID());
        Log::Comment(L"Peasant 1 should be the new MRU peasant");
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_getMostRecentPeasantID(false, true));

        Log::Comment(L"Peasant 2 should not be in the monarch at all anymore");
        VERIFY_ARE_EQUAL(1u, m0->_peasants.size());
        VERIFY_ARE_EQUAL(1u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(1u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[0].PeasantID());
    }

    void RemotingTests::MostRecentIsQuake()
    {
        Log::Comment(L"When a window is named _quake, it shouldn't participate "
                     L"in window glomming via the MRU window.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"_quake");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the _quake peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }

        VERIFY_ARE_EQUAL(2u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[1].PeasantID());

        Log::Comment(L"When we look up the MRU window, we find peasant 1 (who's name is \"one\"), not 2 (who's name is \"_quake\")");
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_getMostRecentPeasantID(false, true));

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"_quake"));

        {
            Log::Comment(L"rename p2 to \"two\"");
            Remoting::RenameRequestArgs eventArgs{ L"two" };
            p2->RequestRename(eventArgs);
            VERIFY_IS_TRUE(eventArgs.Succeeded());
        }
        VERIFY_ARE_EQUAL(L"two", p2->WindowName());

        Log::Comment(L"Now, the MRU window will correctly be p2");
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_getMostRecentPeasantID(false, true));
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        {
            Log::Comment(L"rename p1 to \"_quake\"");
            Remoting::RenameRequestArgs eventArgs{ L"_quake" };
            p1->RequestRename(eventArgs);
            VERIFY_IS_TRUE(eventArgs.Succeeded());
        }
        VERIFY_ARE_EQUAL(L"_quake", p1->WindowName());

        Log::Comment(L"Now, the MRU window will still be p2");
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_getMostRecentPeasantID(false, true));
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"_quake"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }

        Log::Comment(L"Now, the MRU window will still be p2, because p1 is still named \"_quake\"");
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_getMostRecentPeasantID(false, true));
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"_quake"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));
    }

    void RemotingTests::GetPeasantsByName()
    {
        Log::Comment(L"Test that looking up a peasant by name finds the window we expect");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);

        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());
        VERIFY_ARE_EQUAL(L"one", p1->WindowName());
        VERIFY_ARE_EQUAL(L"two", p2->WindowName());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());
        VERIFY_ARE_EQUAL(L"one", p1->WindowName());
        VERIFY_ARE_EQUAL(L"two", p2->WindowName());

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        Log::Comment(L"Rename p2");

        p2->WindowName(L"foo");

        VERIFY_ARE_EQUAL(0, m0->_lookupPeasantIdForName(L"two"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"foo"));
    }

    void RemotingTests::AddNamedPeasantsToNewMonarch()
    {
        Log::Comment(L"Test that moving peasants to a new monarch persists their original names");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;
        const auto monarch3PID = 45678u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        com_ptr<Remoting::implementation::Monarch> m3;
        m3.attach(new Remoting::implementation::Monarch(monarch3PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        VERIFY_IS_NOT_NULL(m3);

        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());
        VERIFY_ARE_EQUAL(L"one", p1->WindowName());
        VERIFY_ARE_EQUAL(L"two", p2->WindowName());

        Log::Comment(L"When the peasants go to a new monarch, make sure they have the same name");
        m3->AddPeasant(*p1);
        m3->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());
        VERIFY_ARE_EQUAL(L"one", p1->WindowName());
        VERIFY_ARE_EQUAL(L"two", p2->WindowName());

        VERIFY_ARE_EQUAL(p1->GetID(), m3->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m3->_lookupPeasantIdForName(L"two"));
    }

    void RemotingTests::LookupNamedPeasantWhenOthersDied()
    {
        Log::Comment(L"Test that looking for a peasant by name when a different"
                     L" peasant has died cleans up the corpses of any peasants "
                     L"we may have tripped over.");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        Log::Comment(L"Kill peasant 1. Make sure that it gets removed from the monarch.");
        RemotingTests::_killPeasant(m0, p1->GetID());

        // By killing 1, then looking for "two", we happen to iterate over the
        // corpse of 1 when looking for the peasant named "two". This causes us
        // to remove 1 while looking for "two". Technically, we shouldn't be
        // relying on any sort of ordering for an unordered_map iterator, but
        // this one just so happens to work.
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        Log::Comment(L"Peasant 1 should have been pruned");
        VERIFY_ARE_EQUAL(1u, m0->_peasants.size());
    }

    void RemotingTests::LookupNamedPeasantWhenItDied()
    {
        Log::Comment(L"Test that looking up a dead peasant by name returns 0, "
                     L"indicating there's no peasant with that name.");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        Log::Comment(L"Kill peasant 1. Make sure that it gets removed from the monarch.");
        RemotingTests::_killPeasant(m0, p1->GetID());

        VERIFY_ARE_EQUAL(0, m0->_lookupPeasantIdForName(L"one"));

        Log::Comment(L"Peasant 1 should have been pruned");
        VERIFY_ARE_EQUAL(1u, m0->_peasants.size());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));
    }

    void RemotingTests::GetMruPeasantAfterNameLookupForDeadPeasant()
    {
        // This test is trying to hit the catch in Monarch::_lookupPeasantIdForName.
        //
        // We need to:
        // * add some peasants,
        // * make one the mru, then make a named two the mru
        // * then kill two
        // * then try to get the mru peasant -> it should be one

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        Log::Comment(L"Kill peasant 2.");
        RemotingTests::_killPeasant(m0, p2->GetID());

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_getMostRecentPeasantID(false, true));
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_getMostRecentPeasantID(true, true));
    }

    void RemotingTests::ProposeCommandlineForNamedDeadWindow()
    {
        Log::Comment(L"Test proposing a commandline for a named window that's "
                     L"currently dead. This should result in a new window with "
                     L"the given name.");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));
        VERIFY_IS_NOT_NULL(m0);
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowByNameHelper);

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        p1->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& cmdlineArgs) {
            Log::Comment(L"Commandline dispatched to p1");
            VERIFY_IS_GREATER_THAN(cmdlineArgs.Commandline().size(), 1u);
            VERIFY_ARE_EQUAL(L"arg[1]", cmdlineArgs.Commandline().at(1));
        });
        p2->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& cmdlineArgs) {
            Log::Comment(L"Commandline dispatched to p2");
            VERIFY_IS_GREATER_THAN(cmdlineArgs.Commandline().size(), 1u);
            VERIFY_ARE_EQUAL(L"this is for p2", cmdlineArgs.Commandline().at(1));
        });

        p1->WindowName(L"one");
        p2->WindowName(L"two");

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        std::vector<winrt::hstring> p1Args{ L"one", L"arg[1]" };
        std::vector<winrt::hstring> p2Args{ L"two", L"this is for p2" };

        {
            Remoting::CommandlineArgs eventArgs{ { p1Args }, { L"" } };
            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id()); // Casting to (bool) checks if the reference has a value
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }

        {
            Log::Comment(L"Send a commandline to \"two\", which should be p2");
            Remoting::CommandlineArgs eventArgs{ { p2Args }, { L"" } };
            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id()); // Casting to (bool) checks if the reference has a value
            VERIFY_ARE_EQUAL(L"", result.WindowName());
        }

        Log::Comment(L"Kill peasant 2.");
        RemotingTests::_killPeasant(m0, p2->GetID());

        {
            Log::Comment(L"Send a commandline to \"two\", who is now dead.");
            Remoting::CommandlineArgs eventArgs{ { p2Args }, { L"" } };
            auto result = m0->ProposeCommandline(eventArgs);
            VERIFY_ARE_EQUAL(true, result.ShouldCreateWindow());
            VERIFY_ARE_EQUAL(false, (bool)result.Id()); // Casting to (bool) checks if the reference has a value
            VERIFY_ARE_EQUAL(L"two", result.WindowName());
        }
    }

    void RemotingTests::TestRenameWindowSuccessfully()
    {
        Log::Comment(L"Attempt to rename a window. This should succeed.");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        Remoting::RenameRequestArgs eventArgs{ L"foo" };
        p1->RequestRename(eventArgs);

        VERIFY_IS_TRUE(eventArgs.Succeeded());
        VERIFY_ARE_EQUAL(L"foo", p1->WindowName());

        VERIFY_ARE_EQUAL(0, m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"foo"));
    }

    void RemotingTests::TestRenameSameNameAsAnother()
    {
        Log::Comment(L"Try renaming a window to a name used by another peasant."
                     L" This should fail.");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        Remoting::RenameRequestArgs eventArgs{ L"two" };
        p1->RequestRename(eventArgs);

        VERIFY_IS_FALSE(eventArgs.Succeeded());
        VERIFY_ARE_EQUAL(L"one", p1->WindowName());

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));
    }
    void RemotingTests::TestRenameSameNameAsADeadPeasant()
    {
        Log::Comment(L"We'll try renaming a window to the name of a window that"
                     L" has died. This should succeed, without crashing.");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        Remoting::RenameRequestArgs eventArgs{ L"two" };
        p1->RequestRename(eventArgs);

        VERIFY_IS_FALSE(eventArgs.Succeeded());
        VERIFY_ARE_EQUAL(L"one", p1->WindowName());

        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"one"));
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_lookupPeasantIdForName(L"two"));

        RemotingTests::_killPeasant(m0, p2->GetID());

        p1->RequestRename(eventArgs);

        VERIFY_IS_TRUE(eventArgs.Succeeded());
        VERIFY_ARE_EQUAL(L"two", p1->WindowName());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_lookupPeasantIdForName(L"two"));
    }

    void RemotingTests::TestSummonMostRecentWindow()
    {
        Log::Comment(L"Attempt to summon the most recent window");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        bool p1ExpectedToBeSummoned = false;
        bool p2ExpectedToBeSummoned = false;

        p1->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p1 summoned");
            VERIFY_IS_TRUE(p1ExpectedToBeSummoned);
        });
        p2->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p2 summoned");
            VERIFY_IS_TRUE(p2ExpectedToBeSummoned);
        });

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }

        p2ExpectedToBeSummoned = true;
        Remoting::SummonWindowSelectionArgs args;
        // Without setting the WindowName, SummonWindowSelectionArgs defaults to
        // the MRU window
        Log::Comment(L"Summon the MRU window, which is window two");
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }

        Log::Comment(L"Now that one is the MRU, summon it");
        p2ExpectedToBeSummoned = false;
        p1ExpectedToBeSummoned = true;
        args.FoundMatch(false);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());
    }

    void RemotingTests::TestSummonNamedWindow()
    {
        Log::Comment(L"Attempt to summon a window by name. When there isn't a "
                     L"window with that name, set FoundMatch to false, so the "
                     L"caller can handle that case.");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        bool p1ExpectedToBeSummoned = false;
        bool p2ExpectedToBeSummoned = false;

        p1->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p1 summoned");
            VERIFY_IS_TRUE(p1ExpectedToBeSummoned);
        });
        p2->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p2 summoned");
            VERIFY_IS_TRUE(p2ExpectedToBeSummoned);
        });

        Remoting::SummonWindowSelectionArgs args;

        Log::Comment(L"Summon window two by name");
        p2ExpectedToBeSummoned = true;
        args.WindowName(L"two");
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Summon window one by name");
        p2ExpectedToBeSummoned = false;
        p1ExpectedToBeSummoned = true;
        args.FoundMatch(false);
        args.WindowName(L"one");
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Fail to summon window three by name");
        p1ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.WindowName(L"three");
        m0->SummonWindow(args);
        VERIFY_IS_FALSE(args.FoundMatch());
    }

    void RemotingTests::TestSummonNamedDeadWindow()
    {
        Log::Comment(L"Attempt to summon a dead window by name. This will fail, but not crash.");

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        bool p1ExpectedToBeSummoned = false;
        bool p2ExpectedToBeSummoned = false;

        p1->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p1 summoned");
            VERIFY_IS_TRUE(p1ExpectedToBeSummoned);
        });
        p2->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p2 summoned");
            VERIFY_IS_TRUE(p2ExpectedToBeSummoned);
        });

        Remoting::SummonWindowSelectionArgs args;

        Log::Comment(L"Summon window two by name");
        p2ExpectedToBeSummoned = true;
        args.WindowName(L"two");
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Summon window one by name");
        p2ExpectedToBeSummoned = false;
        p1ExpectedToBeSummoned = true;
        args.FoundMatch(false);
        args.WindowName(L"one");
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Kill peasant one.");
        RemotingTests::_killPeasant(m0, p1->GetID());

        Log::Comment(L"Fail to summon window one by name");
        p1ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.WindowName(L"one");
        m0->SummonWindow(args);
        VERIFY_IS_FALSE(args.FoundMatch());
    }

    void RemotingTests::TestSummonMostRecentDeadWindow()
    {
        Log::Comment(L"Attempt to summon the MRU window, when the MRU window "
                     L"has died. This will fall back to the next MRU window.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"two");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        bool p1ExpectedToBeSummoned = false;
        bool p2ExpectedToBeSummoned = false;

        p1->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p1 summoned");
            VERIFY_IS_TRUE(p1ExpectedToBeSummoned);
        });
        p2->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p2 summoned");
            VERIFY_IS_TRUE(p2ExpectedToBeSummoned);
        });

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }

        p2ExpectedToBeSummoned = true;
        Remoting::SummonWindowSelectionArgs args;
        // Without setting the WindowName, SummonWindowSelectionArgs defaults to
        // the MRU window
        Log::Comment(L"Summon the MRU window, which is window two");
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }

        Log::Comment(L"Now that one is the MRU, summon it");
        p2ExpectedToBeSummoned = false;
        p1ExpectedToBeSummoned = true;
        args.FoundMatch(false);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Kill peasant one.");
        RemotingTests::_killPeasant(m0, p1->GetID());

        Log::Comment(L"We now expect to summon two, since the MRU peasant (one) is actually dead.");
        p2ExpectedToBeSummoned = true;
        p1ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());
    }

    void RemotingTests::TestSummonOnCurrent()
    {
        Log::Comment(L"Tests summoning a window, using OnCurrentDesktop to only"
                     L"select windows on the current desktop.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{22222222-2222-2222-2222-222222222222}") };

        constexpr auto monarch0PID = 12345u;
        constexpr auto peasant1PID = 23456u;
        constexpr auto peasant2PID = 34567u;
        constexpr auto peasant3PID = 45678u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        com_ptr<Remoting::implementation::Peasant> p3;
        p3.attach(new Remoting::implementation::Peasant(peasant3PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        VERIFY_IS_NOT_NULL(p3);
        p1->WindowName(L"one");
        p2->WindowName(L"two");
        p3->WindowName(L"three");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());
        VERIFY_ARE_EQUAL(0, p3->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);
        m0->AddPeasant(*p3);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());
        VERIFY_ARE_EQUAL(3, p3->GetID());

        VERIFY_ARE_EQUAL(3u, m0->_peasants.size());

        bool p1ExpectedToBeSummoned = false;
        bool p2ExpectedToBeSummoned = false;
        bool p3ExpectedToBeSummoned = false;

        p1->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p1 summoned");
            VERIFY_IS_TRUE(p1ExpectedToBeSummoned);
        });
        p2->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p2 summoned");
            VERIFY_IS_TRUE(p2ExpectedToBeSummoned);
        });
        p3->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p3 summoned");
            VERIFY_IS_TRUE(p3ExpectedToBeSummoned);
        });

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         p1->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         p2->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid2,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the third peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p3->GetID(),
                                                         p3->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid1,
                                                         winrt::clock().now() };
            p3->ActivateWindow(activatedArgs);
        }

        Log::Comment(L"Create a mock IVirtualDesktopManager to handle checking if a window is on a given desktop");
        winrt::com_ptr<MockDesktopManager> manager;
        manager.attach(new MockDesktopManager());
        m0->_desktopManager = manager.try_as<IVirtualDesktopManager>();

        auto firstCallback = [&](HWND h, BOOL* result) -> HRESULT {
            Log::Comment(L"firstCallback: Checking if window is on desktop 1");

            const uint64_t hwnd = reinterpret_cast<uint64_t>(h);
            if (hwnd == peasant1PID || hwnd == peasant3PID)
            {
                *result = true;
            }
            else if (hwnd == peasant2PID)
            {
                *result = false;
            }
            else
            {
                VERIFY_IS_TRUE(false, L"IsWindowOnCurrentVirtualDesktop called with unexpected value");
            }
            return S_OK;
        };
        manager->pfnIsWindowOnCurrentVirtualDesktop = firstCallback;

        Remoting::SummonWindowSelectionArgs args;

        Log::Comment(L"Summon window three - it is the MRU on desktop 1");
        p3ExpectedToBeSummoned = true;
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         p1->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }

        Log::Comment(L"Summon window one - it is the MRU on desktop 1");
        p1ExpectedToBeSummoned = true;
        p2ExpectedToBeSummoned = false;
        p3ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Now we'll pretend we switched to desktop 2");

        auto secondCallback = [&](HWND h, BOOL* result) -> HRESULT {
            Log::Comment(L"secondCallback: Checking if window is on desktop 2");
            const uint64_t hwnd = reinterpret_cast<uint64_t>(h);
            if (hwnd == peasant1PID || hwnd == peasant3PID)
            {
                *result = false;
            }
            else if (hwnd == peasant2PID)
            {
                *result = true;
            }
            else
            {
                VERIFY_IS_TRUE(false, L"IsWindowOnCurrentVirtualDesktop called with unexpected value");
            }
            return S_OK;
        };
        manager->pfnIsWindowOnCurrentVirtualDesktop = secondCallback;

        Log::Comment(L"Summon window one - it is the MRU on desktop 2");
        p1ExpectedToBeSummoned = false;
        p2ExpectedToBeSummoned = true;
        p3ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        {
            Log::Comment(L"Activate the third peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p3->GetID(),
                                                         p3->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid2,
                                                         winrt::clock().now() };
            p3->ActivateWindow(activatedArgs);
        }

        auto thirdCallback = [&](HWND h, BOOL* result) -> HRESULT {
            Log::Comment(L"thirdCallback: Checking if window is on desktop 2. (windows 2 and 3 are)");
            const uint64_t hwnd = reinterpret_cast<uint64_t>(h);
            if (hwnd == peasant1PID)
            {
                *result = false;
            }
            else if (hwnd == peasant2PID || hwnd == peasant3PID)
            {
                *result = true;
            }
            else
            {
                VERIFY_IS_TRUE(false, L"IsWindowOnCurrentVirtualDesktop called with unexpected value");
            }
            return S_OK;
        };
        manager->pfnIsWindowOnCurrentVirtualDesktop = thirdCallback;

        Log::Comment(L"Summon window three - it is the MRU on desktop 2");
        p1ExpectedToBeSummoned = false;
        p2ExpectedToBeSummoned = false;
        p3ExpectedToBeSummoned = true;
        args.FoundMatch(false);
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Now we'll pretend we switched to desktop 1");

        auto fourthCallback = [&](HWND h, BOOL* result) -> HRESULT {
            Log::Comment(L"fourthCallback: Checking if window is on desktop 1. (window 1 is)");
            const uint64_t hwnd = reinterpret_cast<uint64_t>(h);
            if (hwnd == peasant1PID)
            {
                *result = true;
            }
            else if (hwnd == peasant2PID || hwnd == peasant3PID)
            {
                *result = false;
            }
            else
            {
                VERIFY_IS_TRUE(false, L"IsWindowOnCurrentVirtualDesktop called with unexpected value");
            }
            return S_OK;
        };
        manager->pfnIsWindowOnCurrentVirtualDesktop = fourthCallback;

        Log::Comment(L"Summon window one - it is the only window on desktop 1");
        p1ExpectedToBeSummoned = true;
        p2ExpectedToBeSummoned = false;
        p3ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Now we'll pretend we switched to desktop 3");

        auto fifthCallback = [&](HWND h, BOOL* result) -> HRESULT {
            Log::Comment(L"fifthCallback: Checking if window is on desktop 3. (none are)");
            const uint64_t hwnd = reinterpret_cast<uint64_t>(h);
            if (hwnd == peasant1PID || hwnd == peasant2PID || hwnd == peasant3PID)
            {
                *result = false;
            }
            else
            {
                VERIFY_IS_TRUE(false, L"IsWindowOnCurrentVirtualDesktop called with unexpected value");
            }
            return S_OK;
        };
        manager->pfnIsWindowOnCurrentVirtualDesktop = fifthCallback;

        Log::Comment(L"This summon won't find a window.");
        p1ExpectedToBeSummoned = false;
        p2ExpectedToBeSummoned = false;
        p3ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_FALSE(args.FoundMatch());
    }

    void RemotingTests::TestSummonOnCurrentWithName()
    {
        Log::Comment(L"Test that specifying a WindowName forces us to ignore OnCurrentDesktop");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{22222222-2222-2222-2222-222222222222}") };

        constexpr auto monarch0PID = 12345u;
        constexpr auto peasant1PID = 23456u;
        constexpr auto peasant2PID = 34567u;
        constexpr auto peasant3PID = 45678u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        com_ptr<Remoting::implementation::Peasant> p3;
        p3.attach(new Remoting::implementation::Peasant(peasant3PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        VERIFY_IS_NOT_NULL(p3);
        p1->WindowName(L"one");
        p2->WindowName(L"two");
        p3->WindowName(L"three");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());
        VERIFY_ARE_EQUAL(0, p3->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);
        m0->AddPeasant(*p3);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());
        VERIFY_ARE_EQUAL(3, p3->GetID());

        VERIFY_ARE_EQUAL(3u, m0->_peasants.size());

        bool p1ExpectedToBeSummoned = false;
        bool p2ExpectedToBeSummoned = false;
        bool p3ExpectedToBeSummoned = false;

        p1->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p1 summoned");
            VERIFY_IS_TRUE(p1ExpectedToBeSummoned);
        });
        p2->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p2 summoned");
            VERIFY_IS_TRUE(p2ExpectedToBeSummoned);
        });
        p3->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p3 summoned");
            VERIFY_IS_TRUE(p3ExpectedToBeSummoned);
        });

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         p1->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         p2->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid2,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the third peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p3->GetID(),
                                                         p3->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid1,
                                                         winrt::clock().now() };
            p3->ActivateWindow(activatedArgs);
        }

        Log::Comment(L"Create a mock IVirtualDesktopManager to handle checking if a window is on a given desktop");
        winrt::com_ptr<MockDesktopManager> manager;
        manager.attach(new MockDesktopManager());
        m0->_desktopManager = manager.try_as<IVirtualDesktopManager>();

        auto firstCallback = [&](HWND h, BOOL* result) -> HRESULT {
            Log::Comment(L"firstCallback: Checking if window is on desktop 1");

            const uint64_t hwnd = reinterpret_cast<uint64_t>(h);
            if (hwnd == peasant1PID || hwnd == peasant3PID)
            {
                *result = true;
            }
            else if (hwnd == peasant2PID)
            {
                *result = false;
            }
            else
            {
                VERIFY_IS_TRUE(false, L"IsWindowOnCurrentVirtualDesktop called with unexpected value");
            }
            return S_OK;
        };
        manager->pfnIsWindowOnCurrentVirtualDesktop = firstCallback;

        Remoting::SummonWindowSelectionArgs args;

        Log::Comment(L"Summon window three - it is the MRU on desktop 1");
        p3ExpectedToBeSummoned = true;
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Look for window 1 by name. When given a name, we don't care about OnCurrentDesktop.");
        p1ExpectedToBeSummoned = true;
        p2ExpectedToBeSummoned = false;
        p3ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.WindowName(L"one");
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Look for window 2 by name. When given a name, we don't care about OnCurrentDesktop.");
        p1ExpectedToBeSummoned = false;
        p2ExpectedToBeSummoned = true;
        p3ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.WindowName(L"two");
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Look for window 3 by name. When given a name, we don't care about OnCurrentDesktop.");
        p1ExpectedToBeSummoned = false;
        p2ExpectedToBeSummoned = false;
        p3ExpectedToBeSummoned = true;
        args.FoundMatch(false);
        args.WindowName(L"three");
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());
    }

    void RemotingTests::TestSummonOnCurrentDeadWindow()
    {
        Log::Comment(L"Test that we can summon a window on the current desktop,"
                     L" when the MRU window on that desktop dies.");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };
        const winrt::guid guid2{ Utils::GuidFromString(L"{22222222-2222-2222-2222-222222222222}") };

        constexpr auto monarch0PID = 12345u;
        constexpr auto peasant1PID = 23456u;
        constexpr auto peasant2PID = 34567u;
        constexpr auto peasant3PID = 45678u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        com_ptr<Remoting::implementation::Peasant> p3;
        p3.attach(new Remoting::implementation::Peasant(peasant3PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        VERIFY_IS_NOT_NULL(p3);
        p1->WindowName(L"one");
        p2->WindowName(L"two");
        p3->WindowName(L"three");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());
        VERIFY_ARE_EQUAL(0, p3->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);
        m0->AddPeasant(*p3);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());
        VERIFY_ARE_EQUAL(3, p3->GetID());

        VERIFY_ARE_EQUAL(3u, m0->_peasants.size());

        bool p1ExpectedToBeSummoned = false;
        bool p2ExpectedToBeSummoned = false;
        bool p3ExpectedToBeSummoned = false;

        p1->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p1 summoned");
            VERIFY_IS_TRUE(p1ExpectedToBeSummoned);
        });
        p2->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p2 summoned");
            VERIFY_IS_TRUE(p2ExpectedToBeSummoned);
        });
        p3->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p3 summoned");
            VERIFY_IS_TRUE(p3ExpectedToBeSummoned);
        });

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         p1->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the second peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         p2->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid2,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the third peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p3->GetID(),
                                                         p3->GetPID(), // USE PID as HWND, because these values don't _really_ matter
                                                         guid1,
                                                         winrt::clock().now() };
            p3->ActivateWindow(activatedArgs);
        }

        Log::Comment(L"Create a mock IVirtualDesktopManager to handle checking if a window is on a given desktop");
        winrt::com_ptr<MockDesktopManager> manager;
        manager.attach(new MockDesktopManager());
        m0->_desktopManager = manager.try_as<IVirtualDesktopManager>();

        auto firstCallback = [&](HWND h, BOOL* result) -> HRESULT {
            Log::Comment(L"firstCallback: Checking if window is on desktop 1");

            const uint64_t hwnd = reinterpret_cast<uint64_t>(h);
            if (hwnd == peasant1PID || hwnd == peasant3PID)
            {
                *result = true;
            }
            else if (hwnd == peasant2PID)
            {
                *result = false;
            }
            else
            {
                VERIFY_IS_TRUE(false, L"IsWindowOnCurrentVirtualDesktop called with unexpected value");
            }
            return S_OK;
        };
        manager->pfnIsWindowOnCurrentVirtualDesktop = firstCallback;

        Remoting::SummonWindowSelectionArgs args;

        Log::Comment(L"Summon window three - it is the MRU on desktop 1");
        p3ExpectedToBeSummoned = true;
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());

        Log::Comment(L"Kill window 3. Window 1 is now the MRU on desktop 1.");
        RemotingTests::_killPeasant(m0, p3->GetID());

        Log::Comment(L"Summon window three - it is the MRU on desktop 1");
        p1ExpectedToBeSummoned = true;
        p2ExpectedToBeSummoned = false;
        p3ExpectedToBeSummoned = false;
        args.FoundMatch(false);
        args.OnCurrentDesktop(true);
        m0->SummonWindow(args);
        VERIFY_IS_TRUE(args.FoundMatch());
    }

    void RemotingTests::TestSummonMostRecentIsQuake()
    {
        Log::Comment(L"When a window is named _quake, it shouldn't participate "
                     L"in window glomming via the MRU window, but it should be able to be summoned");

        const winrt::guid guid1{ Utils::GuidFromString(L"{11111111-1111-1111-1111-111111111111}") };

        const auto monarch0PID = 12345u;
        const auto peasant1PID = 23456u;
        const auto peasant2PID = 34567u;

        com_ptr<Remoting::implementation::Monarch> m0;
        m0.attach(new Remoting::implementation::Monarch(monarch0PID));

        com_ptr<Remoting::implementation::Peasant> p1;
        p1.attach(new Remoting::implementation::Peasant(peasant1PID));

        com_ptr<Remoting::implementation::Peasant> p2;
        p2.attach(new Remoting::implementation::Peasant(peasant2PID));

        VERIFY_IS_NOT_NULL(m0);
        VERIFY_IS_NOT_NULL(p1);
        VERIFY_IS_NOT_NULL(p2);
        p1->WindowName(L"one");
        p2->WindowName(L"_quake");

        VERIFY_ARE_EQUAL(0, p1->GetID());
        VERIFY_ARE_EQUAL(0, p2->GetID());

        m0->AddPeasant(*p1);
        m0->AddPeasant(*p2);

        VERIFY_ARE_EQUAL(1, p1->GetID());
        VERIFY_ARE_EQUAL(2, p2->GetID());

        VERIFY_ARE_EQUAL(2u, m0->_peasants.size());

        bool p1ExpectedToBeSummoned = false;
        bool p2ExpectedToBeSummoned = false;

        p1->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p1 summoned");
            VERIFY_IS_TRUE(p1ExpectedToBeSummoned);
        });
        p2->SummonRequested([&](auto&&, auto&&) {
            Log::Comment(L"p2 summoned");
            VERIFY_IS_TRUE(p2ExpectedToBeSummoned);
        });

        bool p1ExpectedCommandline = false;
        bool p2ExpectedCommandline = false;
        p1->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& /*cmdlineArgs*/) {
            Log::Comment(L"Commandline dispatched to p1");
            VERIFY_IS_TRUE(p1ExpectedCommandline);
        });
        p2->ExecuteCommandlineRequested([&](auto&&, const Remoting::CommandlineArgs& /*cmdlineArgs*/) {
            Log::Comment(L"Commandline dispatched to p2");
            VERIFY_IS_TRUE(p2ExpectedCommandline);
        });
        m0->FindTargetWindowRequested(&RemotingTests::_findTargetWindowHelper);

        {
            Log::Comment(L"Activate the first peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        {
            Log::Comment(L"Activate the _quake peasant, first desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p2->GetID(),
                                                         guid1,
                                                         winrt::clock().now() };
            p2->ActivateWindow(activatedArgs);
        }

        VERIFY_ARE_EQUAL(2u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_mruPeasants[0].PeasantID());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[1].PeasantID());

        std::vector<winrt::hstring> commandlineArgs{ L"0", L"arg[1]" };
        Remoting::CommandlineArgs eventArgs{ { commandlineArgs }, { L"" } };

        Log::Comment(L"When we attempt to send a commandline to the MRU window,"
                     L" we should find peasant 1 (who's name is \"one\"), not 2"
                     L" (who's name is \"_quake\")");
        p1ExpectedCommandline = true;
        p2ExpectedCommandline = false;
        auto result = m0->ProposeCommandline(eventArgs);
        VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
        VERIFY_ARE_EQUAL(false, (bool)result.Id());

        {
            Log::Comment(L"When we summon the MRU window, we'll still summon the _quake window");
            p1ExpectedToBeSummoned = false;
            p2ExpectedToBeSummoned = true;
            Remoting::SummonWindowSelectionArgs args;
            args.OnCurrentDesktop(false);
            m0->SummonWindow(args);
            VERIFY_IS_TRUE(args.FoundMatch());
        }

        {
            Log::Comment(L"rename p2 to \"two\"");
            Remoting::RenameRequestArgs renameArgs{ L"two" };
            p2->RequestRename(renameArgs);
            VERIFY_IS_TRUE(renameArgs.Succeeded());
        }
        VERIFY_ARE_EQUAL(L"two", p2->WindowName());

        Log::Comment(L"Now, the MRU window will correctly be p2, and we can glom to it");
        p1ExpectedCommandline = false;
        p2ExpectedCommandline = true;
        result = m0->ProposeCommandline(eventArgs);
        VERIFY_ARE_EQUAL(false, result.ShouldCreateWindow());
        VERIFY_ARE_EQUAL(false, (bool)result.Id());

        {
            Log::Comment(L"When we summon the MRU window, we'll still summon the window 2");
            p1ExpectedToBeSummoned = false;
            p2ExpectedToBeSummoned = true;
            Remoting::SummonWindowSelectionArgs args;
            args.OnCurrentDesktop(false);
            m0->SummonWindow(args);
            VERIFY_IS_TRUE(args.FoundMatch());
        }
    }

}
