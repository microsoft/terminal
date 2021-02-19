// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../Remoting/Monarch.h"
#include "../Remoting/CommandlineArgs.h"
#include "../Remoting/FindTargetWindowArgs.h"
#include "../Remoting/ProposeCommandlineResult.h"

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
        uint64_t GetPID() { throw winrt::hresult_error{}; };
        bool ExecuteCommandline(const Remoting::CommandlineArgs& /*args*/) { throw winrt::hresult_error{}; }
        void ActivateWindow(const Remoting::WindowActivatedArgs& /*args*/) { throw winrt::hresult_error{}; }
        Remoting::CommandlineArgs InitialArgs() { throw winrt::hresult_error{}; }
        Remoting::WindowActivatedArgs GetLastActivatedArgs() { throw winrt::hresult_error{}; }
        TYPED_EVENT(WindowActivated, winrt::Windows::Foundation::IInspectable, Remoting::WindowActivatedArgs);
        TYPED_EVENT(ExecuteCommandlineRequested, winrt::Windows::Foundation::IInspectable, Remoting::CommandlineArgs);
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

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }

        static void _killPeasant(const com_ptr<Remoting::implementation::Monarch>& m,
                                 const uint64_t peasantID);

        static void _findTargetWindowHelper(const winrt::Windows::Foundation::IInspectable& sender,
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
        VERIFY_ARE_EQUAL(p2->GetID(), m0->_getMostRecentPeasantID(false));

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
        VERIFY_ARE_EQUAL(p3->GetID(), m0->_getMostRecentPeasantID(false));

        {
            Log::Comment(L"Activate the first peasant, second desktop");
            Remoting::WindowActivatedArgs activatedArgs{ p1->GetID(),
                                                         guid2,
                                                         winrt::clock().now() };
            p1->ActivateWindow(activatedArgs);
        }
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_getMostRecentPeasantID(false));
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
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_getMostRecentPeasantID(false));

        Log::Comment(L"Peasant 2 should not be in the monarch at all anymore");
        VERIFY_ARE_EQUAL(1u, m0->_peasants.size());
        VERIFY_ARE_EQUAL(1u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(1u, m0->_mruPeasants.size());
        VERIFY_ARE_EQUAL(p1->GetID(), m0->_mruPeasants[0].PeasantID());
    }

}
