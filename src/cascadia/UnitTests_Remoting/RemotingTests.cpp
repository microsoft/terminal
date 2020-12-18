// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "../Remoting/Monarch.h"

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
    // It will always throw an hresult_error on any of it's methods.
    // In the tests, it's hard to emulate a peasant process being totally dead once the Monarch has captured a reference to it. Since everything's in-proc in the tests, we can't decrement the refcount in such a way that the monarch's reference will throw a catchable exception.
    // Instead, this class can be used to replace a peasant inside a Monarch, to emulate that peasant process dying. Any time the monarch tries to do something to this peasant, it'll throw an exception.
    struct DeadPeasant : implements<DeadPeasant, winrt::Microsoft::Terminal::Remoting::IPeasant>
    {
        DeadPeasant() = default;
        void AssignID(uint64_t /*id*/) { throw winrt::hresult_error{}; };
        uint64_t GetID() { throw winrt::hresult_error{}; };
        uint64_t GetPID() { throw winrt::hresult_error{}; };
        bool ExecuteCommandline(const winrt::Microsoft::Terminal::Remoting::CommandlineArgs& /*args*/) { throw winrt::hresult_error{}; }
        winrt::Microsoft::Terminal::Remoting::CommandlineArgs InitialArgs() { throw winrt::hresult_error{}; }
        TYPED_EVENT(WindowActivated, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(ExecuteCommandlineRequested, winrt::Windows::Foundation::IInspectable, winrt::Microsoft::Terminal::Remoting::CommandlineArgs);
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

        TEST_CLASS_SETUP(ClassSetup)
        {
            return true;
        }

        static void _killPeasant(const com_ptr<Remoting::implementation::Monarch>& m,
                                 const uint64_t peasantID);
    };

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

        RemotingTests::_killPeasant(m0, p1->GetID());

        auto maybeP2 = m0->_getPeasant(2);
        VERIFY_IS_NOT_NULL(maybeP2);
        VERIFY_ARE_EQUAL(peasant2PID, maybeP2.GetPID());

        auto maybeP1 = m0->_getPeasant(1);
        // VERIFY_ARE_EQUAL(peasant1PID, maybeP1.GetPID());
        VERIFY_IS_NULL(maybeP1);

        VERIFY_ARE_EQUAL(1u, m0->_peasants.size());
    }

}
