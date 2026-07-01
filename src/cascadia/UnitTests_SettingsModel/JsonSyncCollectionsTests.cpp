// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/JsonSyncCollections.h"

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace SettingsModelUnitTests
{
    // Unit tests for the JsonSyncVector / JsonSyncMap wrappers in isolation
    // (with a stub onChange callback), independent of any settings object.
    class JsonSyncCollectionsTests
    {
        TEST_CLASS(JsonSyncCollectionsTests);

        TEST_METHOD(VectorReadsDelegateToBacking);
        TEST_METHOD(VectorMutationsEachFireOnChangeOnce);
        TEST_METHOD(VectorOnChangeReceivesMutatedBacking);
        TEST_METHOD(VectorIterationWorks);
        TEST_METHOD(VectorNullOnChangeIsSafe);

        TEST_METHOD(MapReadsDelegateToBacking);
        TEST_METHOD(MapMutationsEachFireOnChangeOnce);
        TEST_METHOD(MapOnChangeReceivesMutatedBacking);
    };

    void JsonSyncCollectionsTests::VectorReadsDelegateToBacking()
    {
        const auto backing = winrt::single_threaded_vector<winrt::hstring>({ L"a", L"b", L"c" });
        auto fired = 0;
        const IVector<winrt::hstring> wrapper = winrt::make<JsonSyncVector<winrt::hstring>>(
            backing, [&](IVector<winrt::hstring> const&) { ++fired; });

        VERIFY_ARE_EQUAL(3u, wrapper.Size());
        VERIFY_ARE_EQUAL(L"b", wrapper.GetAt(1));

        uint32_t index{};
        VERIFY_IS_TRUE(wrapper.IndexOf(L"c", index));
        VERIFY_ARE_EQUAL(2u, index);

        VERIFY_ARE_EQUAL(3u, wrapper.GetView().Size());

        // Reads must not trigger the sync callback.
        VERIFY_ARE_EQUAL(0, fired);
    }

    void JsonSyncCollectionsTests::VectorMutationsEachFireOnChangeOnce()
    {
        const auto backing = winrt::single_threaded_vector<winrt::hstring>({ L"a" });
        auto fired = 0;
        const IVector<winrt::hstring> wrapper = winrt::make<JsonSyncVector<winrt::hstring>>(
            backing, [&](IVector<winrt::hstring> const&) { ++fired; });

        wrapper.Append(L"b");
        VERIFY_ARE_EQUAL(1, fired);

        wrapper.InsertAt(0, L"z");
        VERIFY_ARE_EQUAL(2, fired);

        wrapper.SetAt(0, L"y");
        VERIFY_ARE_EQUAL(3, fired);

        wrapper.RemoveAt(0);
        VERIFY_ARE_EQUAL(4, fired);

        wrapper.RemoveAtEnd();
        VERIFY_ARE_EQUAL(5, fired);

        const std::array<winrt::hstring, 2> replacement{ L"p", L"q" };
        wrapper.ReplaceAll(replacement);
        VERIFY_ARE_EQUAL(6, fired);
        VERIFY_ARE_EQUAL(2u, wrapper.Size());

        wrapper.Clear();
        VERIFY_ARE_EQUAL(7, fired);
        VERIFY_ARE_EQUAL(0u, wrapper.Size());
    }

    void JsonSyncCollectionsTests::VectorOnChangeReceivesMutatedBacking()
    {
        const auto backing = winrt::single_threaded_vector<winrt::hstring>({ L"a" });
        uint32_t lastSize{};
        const IVector<winrt::hstring> wrapper = winrt::make<JsonSyncVector<winrt::hstring>>(
            backing, [&](IVector<winrt::hstring> const& current) { lastSize = current.Size(); });

        wrapper.Append(L"b");
        // The callback sees the already-mutated backing, and the mutation lands
        // on the shared backing the caller passed in.
        VERIFY_ARE_EQUAL(2u, lastSize);
        VERIFY_ARE_EQUAL(2u, backing.Size());
        VERIFY_ARE_EQUAL(L"b", backing.GetAt(1));
    }

    void JsonSyncCollectionsTests::VectorIterationWorks()
    {
        const auto backing = winrt::single_threaded_vector<winrt::hstring>({ L"x", L"y" });
        const IVector<winrt::hstring> wrapper = winrt::make<JsonSyncVector<winrt::hstring>>(
            backing, nullptr);

        std::vector<winrt::hstring> seen;
        for (const auto& item : wrapper)
        {
            seen.push_back(item);
        }
        VERIFY_ARE_EQUAL(2u, static_cast<uint32_t>(seen.size()));
        VERIFY_ARE_EQUAL(L"x", seen[0]);
        VERIFY_ARE_EQUAL(L"y", seen[1]);
    }

    void JsonSyncCollectionsTests::VectorNullOnChangeIsSafe()
    {
        const auto backing = winrt::single_threaded_vector<winrt::hstring>();
        const IVector<winrt::hstring> wrapper = winrt::make<JsonSyncVector<winrt::hstring>>(
            backing, nullptr);

        // A null onChange must be tolerated (the wrapper guards the callback).
        wrapper.Append(L"a");
        VERIFY_ARE_EQUAL(1u, wrapper.Size());
    }

    void JsonSyncCollectionsTests::MapReadsDelegateToBacking()
    {
        const auto backing = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
        backing.Insert(L"k1", L"v1");

        auto fired = 0;
        const IMap<winrt::hstring, winrt::hstring> wrapper = winrt::make<JsonSyncMap<winrt::hstring, winrt::hstring>>(
            backing, [&](IMap<winrt::hstring, winrt::hstring> const&) { ++fired; });

        VERIFY_ARE_EQUAL(1u, wrapper.Size());
        VERIFY_IS_TRUE(wrapper.HasKey(L"k1"));
        VERIFY_IS_FALSE(wrapper.HasKey(L"nope"));
        VERIFY_ARE_EQUAL(L"v1", wrapper.Lookup(L"k1"));
        VERIFY_ARE_EQUAL(1u, wrapper.GetView().Size());

        VERIFY_ARE_EQUAL(0, fired);
    }

    void JsonSyncCollectionsTests::MapMutationsEachFireOnChangeOnce()
    {
        const auto backing = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
        auto fired = 0;
        const IMap<winrt::hstring, winrt::hstring> wrapper = winrt::make<JsonSyncMap<winrt::hstring, winrt::hstring>>(
            backing, [&](IMap<winrt::hstring, winrt::hstring> const&) { ++fired; });

        VERIFY_IS_FALSE(wrapper.Insert(L"k1", L"v1")); // new key → not a replacement
        VERIFY_ARE_EQUAL(1, fired);

        VERIFY_IS_TRUE(wrapper.Insert(L"k1", L"v2")); // existing key → replacement
        VERIFY_ARE_EQUAL(2, fired);

        wrapper.Remove(L"k1");
        VERIFY_ARE_EQUAL(3, fired);

        wrapper.Clear();
        VERIFY_ARE_EQUAL(4, fired);
    }

    void JsonSyncCollectionsTests::MapOnChangeReceivesMutatedBacking()
    {
        const auto backing = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
        uint32_t lastSize{};
        const IMap<winrt::hstring, winrt::hstring> wrapper = winrt::make<JsonSyncMap<winrt::hstring, winrt::hstring>>(
            backing, [&](IMap<winrt::hstring, winrt::hstring> const& current) { lastSize = current.Size(); });

        wrapper.Insert(L"k1", L"v1");
        VERIFY_ARE_EQUAL(1u, lastSize);
        VERIFY_ARE_EQUAL(1u, backing.Size());
        VERIFY_ARE_EQUAL(L"v1", backing.Lookup(L"k1"));
    }
}
