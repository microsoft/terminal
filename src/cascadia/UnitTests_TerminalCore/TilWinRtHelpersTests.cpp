// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <WexTestClass.h>

#include <DefaultSettings.h>

#include "../renderer/inc/DummyRenderer.hpp"
#include "../renderer/base/Renderer.hpp"

#include "../cascadia/TerminalCore/Terminal.hpp"
#include "MockTermSettings.h"
#include "consoletaeftemplates.hpp"
#include "../../inc/TestUtils.h"

#include <til/winrt.h>

using namespace winrt::Microsoft::Terminal::Core;
using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;
using namespace ::Microsoft::Console::Types;

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace TerminalCoreUnitTests
{
    class TilWinRtHelpersTests;

    struct WeakEventSink : winrt::implements<WeakEventSink, winrt::Windows::Foundation::IStringable>
    {
        explicit WeakEventSink(int& calls) noexcept :
            _calls{ calls }
        {
        }

        winrt::hstring ToString() const
        {
            return {};
        }

        void OnVectorChanged(const winrt::Windows::Foundation::Collections::IObservableVector<int>&,
                             const winrt::Windows::Foundation::Collections::IVectorChangedEventArgs&)
        {
            ++_calls;
        }

    private:
        int& _calls;
    };
};
using namespace TerminalCoreUnitTests;

class TerminalCoreUnitTests::TilWinRtHelpersTests final
{
    TEST_CLASS(TilWinRtHelpersTests);
    TEST_METHOD(TestPropertySimple);
    TEST_METHOD(TestPropertyHString);
    TEST_METHOD(TestTruthiness);
    TEST_METHOD(TestSimpleConstProperties);
    TEST_METHOD(TestComposedConstProperties);

    TEST_METHOD(TestEvent);

    TEST_METHOD(TestEventRevoker);
    TEST_METHOD(TestEventRevokerSet);
    TEST_METHOD(TestEventHandler);

    TEST_METHOD(TestTypedEvent);

    TEST_METHOD(TestPropertyChanged);
};

void TilWinRtHelpersTests::TestPropertySimple()
{
    til::property<int> Foo;
    til::property<int> Bar(11);

    VERIFY_ARE_EQUAL(11, Bar());

    Foo(42);
    VERIFY_ARE_EQUAL(42, Foo());

    Foo(Foo() - 5); // 37
    VERIFY_ARE_EQUAL(37, Foo());

    Foo(Foo() + Bar()); // 48
    VERIFY_ARE_EQUAL(48, Foo());
}

void TilWinRtHelpersTests::TestPropertyHString()
{
    til::property<winrt::hstring> Foo{ L"Foo" };

    VERIFY_ARE_EQUAL(L"Foo", Foo());

    Foo(L"bar");
    VERIFY_ARE_EQUAL(L"bar", Foo());
}

void TilWinRtHelpersTests::TestTruthiness()
{
    til::property<bool> Foo{ false };
    til::property<int> Bar(0);
    til::property<winrt::hstring> EmptyString;
    til::property<winrt::hstring> FullString{ L"Full" };

    VERIFY_IS_FALSE(Foo());
    VERIFY_IS_FALSE((bool)Foo);

    VERIFY_IS_FALSE(Bar());
    VERIFY_IS_FALSE((bool)Bar);

    VERIFY_IS_FALSE((bool)EmptyString);
    VERIFY_IS_FALSE(!EmptyString().empty());

    Foo(true);
    VERIFY_IS_TRUE(Foo());
    VERIFY_IS_TRUE((bool)Foo);

    Bar(11);
    VERIFY_IS_TRUE(Bar());
    VERIFY_IS_TRUE((bool)Bar);

    VERIFY_IS_TRUE((bool)FullString);
    VERIFY_IS_TRUE(!FullString().empty());
}

void TilWinRtHelpersTests::TestSimpleConstProperties()
{
    struct InnerType
    {
        int first{ 1 };
        int second{ 2 };
    };

    struct Helper
    {
        til::property<int> Foo{ 0 };
        til::property<struct InnerType> Composed;
        til::property<winrt::hstring> MyString;
    };

    struct Helper changeMe;
    const struct Helper noTouching;

    VERIFY_ARE_EQUAL(0, changeMe.Foo());
    VERIFY_ARE_EQUAL(1, changeMe.Composed().first);
    VERIFY_ARE_EQUAL(2, changeMe.Composed().second);
    VERIFY_ARE_EQUAL(L"", changeMe.MyString());

    VERIFY_ARE_EQUAL(0, noTouching.Foo());
    VERIFY_ARE_EQUAL(1, noTouching.Composed().first);
    VERIFY_ARE_EQUAL(2, noTouching.Composed().second);
    VERIFY_ARE_EQUAL(L"", noTouching.MyString());

    changeMe.Foo(42);
    VERIFY_ARE_EQUAL(42, changeMe.Foo());
    // noTouching.Foo = 123; // will not compile

    // None of this compiles.
    // Composed() doesn't return an l-value, it returns an _int_
    //
    // changeMe.Composed().first = 5;
    // VERIFY_ARE_EQUAL(5, changeMe.Composed().first);
    // noTouching.Composed().first = 0x0f; // will not compile

    changeMe.MyString(L"Foo");
    VERIFY_ARE_EQUAL(L"Foo", changeMe.MyString());
    // noTouching.MyString = L"Bar"; // will not compile
}
void TilWinRtHelpersTests::TestComposedConstProperties()
{
    // This is an intentionally obtuse test, to show a weird edge case you
    // should avoid.
    //
    // In this sample, `Helper` has a `property` of a raw struct
    // `InnerType`, which itself is composed of two `property`s. This is not
    // something that will actually occur in practice. In practice, the things
    // inside the `property` will be WinRT types (or primitive types), and
    // things that contain properties will THEMSELVES be WinRT types.
    //
    // But if you do it like this, you can't call
    //
    //    changeMe.Composed().first(5);
    //
    // Or any variation of that, without ~ unexpected ~ behavior. This demonstrates that.
    struct InnerType
    {
        til::property<int> first{ 3 };
        til::property<int> second{ 2 };
    };

    struct Helper
    {
        til::property<int> Foo{ 0 };
        til::property<struct InnerType> Composed;
        til::property<winrt::hstring> MyString;
    };

    struct Helper changeMe;
    const struct Helper noTouching;

    VERIFY_ARE_EQUAL(0, changeMe.Foo());
    VERIFY_ARE_EQUAL(3, changeMe.Composed().first());
    VERIFY_ARE_EQUAL(2, changeMe.Composed().second());
    VERIFY_ARE_EQUAL(L"", changeMe.MyString());

    VERIFY_ARE_EQUAL(0, noTouching.Foo());
    VERIFY_ARE_EQUAL(3, noTouching.Composed().first());
    VERIFY_ARE_EQUAL(2, noTouching.Composed().second());
    VERIFY_ARE_EQUAL(L"", noTouching.MyString());

    changeMe.Foo(42);
    VERIFY_ARE_EQUAL(42, changeMe.Foo());
    // noTouching.Foo = 123; // will not compile

    // This test was authored to work through a potential foot gun.
    // If you have property::operator() return `T`, then
    //     changeMe.Composed().first = 5;
    //
    // Roughly translates to:
    //     auto copy = changeMe.Composed();
    //     copy.first(5);
    //
    // Which rather seems like a foot gun.
    changeMe.Composed().first(5);
    VERIFY_ARE_EQUAL(3, changeMe.Composed().first());

    // IN PRACTICE, this shouldn't ever occur. Composed would be a WinRT type,
    // and you'd get a ref to it, rather than a copy.

    changeMe.MyString(L"Foo");
    VERIFY_ARE_EQUAL(L"Foo", changeMe.MyString());
}

void TilWinRtHelpersTests::TestEvent()
{
    bool handledOne = false;
    bool handledTwo = false;
    auto handler = [&](const int& v) -> void {
        VERIFY_ARE_EQUAL(42, v);
        handledOne = true;
    };

    til::event<winrt::delegate<void(int)>> MyEvent;
    MyEvent(handler);
    MyEvent([&](int) { handledTwo = true; });
    MyEvent.raise(42);
    VERIFY_ARE_EQUAL(true, handledOne);
    VERIFY_ARE_EQUAL(true, handledTwo);
}

void TilWinRtHelpersTests::TestEventRevoker()
{
    auto first = winrt::single_threaded_observable_vector<int>();
    auto second = winrt::single_threaded_observable_vector<int>();
    int firstCalls = 0;
    int secondCalls = 0;

    til::event_revoker revoker;
    VERIFY_IS_FALSE(static_cast<bool>(revoker));

    revoker = first.VectorChanged(winrt::auto_revoke, [&](auto&&, auto&&) {
        ++firstCalls;
    });
    VERIFY_IS_TRUE(static_cast<bool>(revoker));

    first.Append(1);
    VERIFY_ARE_EQUAL(1, firstCalls);

    // Replacing a generic revoker must revoke the previous event.
    revoker = second.VectorChanged(winrt::auto_revoke, [&](auto&&, auto&&) {
        ++secondCalls;
    });
    first.Append(2);
    second.Append(1);
    VERIFY_ARE_EQUAL(1, firstCalls);
    VERIFY_ARE_EQUAL(1, secondCalls);

    revoker.revoke();
    VERIFY_IS_FALSE(static_cast<bool>(revoker));
    second.Append(2);
    VERIFY_ARE_EQUAL(1, secondCalls);

    // Destruction must revoke the event too.
    {
        til::event_revoker scopedRevoker;
        scopedRevoker = first.VectorChanged(winrt::auto_revoke, [&](auto&&, auto&&) {
            ++firstCalls;
        });
        first.Append(3);
        VERIFY_ARE_EQUAL(2, firstCalls);
    }
    first.Append(4);
    VERIFY_ARE_EQUAL(2, firstCalls);

    // Revoking an event can call arbitrary source code. Reentrant replacement
    // of the same revoker must leave the replacement alive.
    struct ReentrantContext
    {
        til::event_revoker* owner;
        int oldRevocations = 0;
        int replacementRevocations = 0;
    } context{ &revoker };
    struct CountingRevoker
    {
        int* revocations;
        explicit CountingRevoker(int* revocations) noexcept :
            revocations{ revocations }
        {
        }
        CountingRevoker(CountingRevoker&& other) noexcept :
            revocations{ std::exchange(other.revocations, nullptr) }
        {
        }
        ~CountingRevoker() noexcept { revoke(); }
        void revoke() noexcept
        {
            if (const auto count = std::exchange(revocations, nullptr))
            {
                ++*count;
            }
        }
        explicit operator bool() const noexcept { return revocations != nullptr; }
    };
    struct ReentrantRevoker
    {
        ReentrantContext* context;
        explicit ReentrantRevoker(ReentrantContext* context) noexcept :
            context{ context }
        {
        }
        ReentrantRevoker(ReentrantRevoker&& other) noexcept :
            context{ std::exchange(other.context, nullptr) }
        {
        }
        ~ReentrantRevoker() noexcept { revoke(); }
        void revoke() noexcept
        {
            if (const auto state = std::exchange(context, nullptr))
            {
                ++state->oldRevocations;
                *state->owner = CountingRevoker{ &state->replacementRevocations };
            }
        }
        explicit operator bool() const noexcept { return context != nullptr; }
    };

    revoker = ReentrantRevoker{ &context };
    revoker.revoke();
    VERIFY_ARE_EQUAL(1, context.oldRevocations);
    VERIFY_IS_TRUE(static_cast<bool>(revoker));
    revoker.revoke();
    VERIFY_ARE_EQUAL(1, context.replacementRevocations);
}

void TilWinRtHelpersTests::TestEventRevokerSet()
{
    auto first = winrt::single_threaded_observable_vector<int>();
    auto second = winrt::single_threaded_observable_vector<int>();
    int calls = 0;

    til::event_revoker_set revokers;
    revokers.add(first.VectorChanged(winrt::auto_revoke, [&](auto&&, auto&&) {
        ++calls;
    }));
    revokers.add(second.VectorChanged(winrt::auto_revoke, [&](auto&&, auto&&) {
        ++calls;
    }));
    VERIFY_IS_TRUE(static_cast<bool>(revokers));

    first.Append(1);
    second.Append(1);
    VERIFY_ARE_EQUAL(2, calls);

    revokers.revoke();
    VERIFY_IS_FALSE(static_cast<bool>(revokers));
    first.Append(2);
    second.Append(2);
    VERIFY_ARE_EQUAL(2, calls);

    // Reentrant additions belong to the next lifetime of the set and must not
    // be invalidated by the revocation currently in progress.
    struct ReentrantSetContext
    {
        til::event_revoker_set* owner;
        decltype(first)* source;
        int* calls;
    } context{ &revokers, &first, &calls };
    struct ReentrantSetRevoker
    {
        ReentrantSetContext* context;
        explicit ReentrantSetRevoker(ReentrantSetContext* context) noexcept :
            context{ context }
        {
        }
        ReentrantSetRevoker(ReentrantSetRevoker&& other) noexcept :
            context{ std::exchange(other.context, nullptr) }
        {
        }
        ~ReentrantSetRevoker() noexcept { revoke(); }
        void revoke() noexcept
        {
            if (const auto state = std::exchange(context, nullptr))
            {
                state->owner->add(state->source->VectorChanged(winrt::auto_revoke, [calls = state->calls](auto&&, auto&&) {
                    ++*calls;
                }));
            }
        }
        explicit operator bool() const noexcept { return context != nullptr; }
    };

    revokers.add(ReentrantSetRevoker{ &context });
    revokers.revoke();
    VERIFY_IS_TRUE(static_cast<bool>(revokers));
    first.Append(3);
    VERIFY_ARE_EQUAL(3, calls);
    revokers.revoke();
    VERIFY_IS_FALSE(static_cast<bool>(revokers));
}

void TilWinRtHelpersTests::TestEventHandler()
{
    auto source = winrt::single_threaded_observable_vector<int>();
    auto calls = 0;
    til::event_revoker_set revokers;
    auto sink = winrt::make_self<WeakEventSink>(calls);
    auto handlers = revokers.bind(source, sink.get());

    handlers.add_handler(&winrt::Windows::Foundation::Collections::IObservableVector<int>::VectorChanged,
                         &WeakEventSink::OnVectorChanged);
    source.Append(1);
    VERIFY_ARE_EQUAL(1, calls);

    // add_handler creates a weak delegate. Keeping the source and revokers
    // alive must not keep the handler owner alive or invoke it after destruction.
    sink = nullptr;
    handlers.add_handler(&winrt::Windows::Foundation::Collections::IObservableVector<int>::VectorChanged,
                         &WeakEventSink::OnVectorChanged);
    source.Append(2);
    VERIFY_ARE_EQUAL(1, calls);
    revokers.revoke();
}

void TilWinRtHelpersTests::TestTypedEvent()
{
    bool handledOne = false;
    bool handledTwo = false;

    auto handler = [&](const winrt::hstring sender, const int& v) -> void {
        VERIFY_ARE_EQUAL(L"sure", sender);
        VERIFY_ARE_EQUAL(42, v);
        handledOne = true;
    };

    til::typed_event<winrt::hstring, int> MyEvent;
    MyEvent(handler);
    MyEvent([&](winrt::hstring, int) { handledTwo = true; });
    MyEvent.raise(L"sure", 42);
    VERIFY_ARE_EQUAL(true, handledOne);
    VERIFY_ARE_EQUAL(true, handledTwo);
}

void TilWinRtHelpersTests::TestPropertyChanged()
{
    auto handler = [&](const auto& /*sender*/, const auto& args) -> void {
        VERIFY_ARE_EQUAL(L"BackgroundBrush", args.PropertyName());
    };

    til::property_changed_event PropertyChanged;
    PropertyChanged(handler);

    // We can't actually run this test in our usual unit tests. As you may
    // suspect, because the PropertyChanged event is a _XAML_ event, it expects
    // to be run on the UI thread. Which, we most definitely don't have here.
    //
    // At least, this does compile. We use this everywhere, so the scream test is LOUD.

    // winrt::Windows::Foundation::IInspectable mySender{};
    // PropertyChanged.raise(mySender, winrt::Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"BackgroundBrush" });
}
