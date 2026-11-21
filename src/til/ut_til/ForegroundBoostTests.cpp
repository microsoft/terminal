// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "ForegroundBoost.h"

using namespace Microsoft::Console;
using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class ForegroundBoostTests
{
    TEST_CLASS(ForegroundBoostTests);

    TEST_METHOD(PublishClientAndRemoveOnDisconnect)
    {
        ForegroundBoostSharedState state;
        ForegroundBoostSnapshot snapshot;
        snapshot.count = 1;
        snapshot.clients[0] = { 1234, { 5678, 9012 } };
        state.Publish(snapshot);
        ForegroundBoostSnapshot actual;
        VERIFY_SUCCEEDED(state.Read(actual));
        VERIFY_ARE_EQUAL(1l, actual.count);
        VERIFY_ARE_EQUAL(1234ul, actual.clients[0].processId);
        VERIFY_ARE_EQUAL(5678ul, actual.clients[0].creationTime.dwLowDateTime);
        VERIFY_ARE_EQUAL(9012ul, actual.clients[0].creationTime.dwHighDateTime);
        state.Publish({});
        VERIFY_SUCCEEDED(state.Read(actual));
        VERIFY_ARE_EQUAL(0l, actual.count);
        VERIFY_ARE_EQUAL(0ul, actual.clients[0].processId);
    }

    TEST_METHOD(RejectTornAndIncompatibleSnapshots)
    {
        ForegroundBoostSharedState state;
        ForegroundBoostSnapshot actual;
        InterlockedIncrement(&state.revision);
        VERIFY_ARE_EQUAL(HRESULT_FROM_WIN32(ERROR_RETRY), state.Read(actual));
        InterlockedIncrement(&state.revision);
        VERIFY_SUCCEEDED(state.Read(actual));
        state.version = ForegroundBoostProtocol + 1;
        VERIFY_ARE_EQUAL(HRESULT_FROM_WIN32(ERROR_REVISION_MISMATCH), state.Read(actual));
    }

    TEST_METHOD(RejectFailedAndOversizedPublications)
    {
        ForegroundBoostSharedState state;
        ForegroundBoostSnapshot snapshot;
        snapshot.status = E_ACCESSDENIED;
        state.Publish(snapshot);
        VERIFY_ARE_EQUAL(E_ACCESSDENIED, state.Read(snapshot));
        snapshot.status = S_OK;
        for (const LONG count : { -1l, 32l })
        {
            snapshot.count = count;
            state.Publish(snapshot);
            VERIFY_ARE_EQUAL(HRESULT_FROM_WIN32(ERROR_MORE_DATA), state.Read(snapshot));
        }
    }

    TEST_METHOD(AcceptExactly31Clients)
    {
        ForegroundBoostSharedState state;
        ForegroundBoostSnapshot snapshot;
        snapshot.count = ForegroundBoostClientCapacity;
        snapshot.clients.back() = { 1234, { 5678, 9012 } };
        state.Publish(snapshot);
        ForegroundBoostSnapshot actual;
        VERIFY_SUCCEEDED(state.Read(actual));
        VERIFY_ARE_EQUAL(31l, actual.count);
        VERIFY_ARE_EQUAL(1234ul, actual.clients.back().processId);
        VERIFY_ARE_EQUAL(5678ul, actual.clients.back().creationTime.dwLowDateTime);
        VERIFY_ARE_EQUAL(9012ul, actual.clients.back().creationTime.dwHighDateTime);
    }
};
