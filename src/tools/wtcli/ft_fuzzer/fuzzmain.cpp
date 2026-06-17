// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Fuzzing harness for wtcli CLI utility functions.
// Targets: BuildSendEventJson, MatchesEventFilter.
//
// Built under the Fuzzing MSBuild configuration with LibFuzzer
// instrumentation; submittable to OneFuzz via the CI pipeline.

#include "precomp.h"
#include "wtcli_functions.h"

// Split fuzz input into N segments using null bytes as delimiters.
// If fewer than N null bytes exist, remaining segments are empty.
static std::vector<std::string> SplitInput(const uint8_t* data, size_t size, size_t n)
{
    std::vector<std::string> segments(n);
    size_t seg = 0;
    size_t start = 0;
    for (size_t i = 0; i < size && seg < n - 1; ++i)
    {
        if (data[i] == '\0')
        {
            segments[seg].assign(reinterpret_cast<const char*>(data + start), i - start);
            start = i + 1;
            ++seg;
        }
    }
    // Remainder goes into the last segment.
    if (start < size)
    {
        segments[seg].assign(reinterpret_cast<const char*>(data + start), size - start);
    }
    return segments;
}

// Core fuzzing logic — called by both LibFuzzer and the manual main().
static int FuzzOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        return 0;
    }

    // Split the entire input into segments for use across all targets.
    // We need at least 4 segments: [eventType] [paramsJson]
    // [sessionId] [eventTypeFilter]
    auto parts = SplitInput(data, size, 4);

    // ── Target 1: BuildSendEventJson ──
    // Fuzz all three input parameters: eventType, paramsJson, and sessionId.
    {
        Json::Value evt;
        wtcli::BuildSendEventJson(parts[0], parts[1], parts[2], evt);
    }

    // ── Target 2: MatchesEventFilter ──
    // Construct valid JSON from fuzzed fields using Json::Value so the parser
    // succeeds and the deep matching logic (session_id, wildcard) is reached
    // even when fuzzed strings contain quotes/backslashes/control chars.
    {
        // 2a: Fuzzed event structure with fuzzed filter.
        const auto& fuzzedSessionId = parts[2];
        const auto& fuzzedEvent = parts[0];
        const auto& fuzzedFilter = parts[3];

        Json::Value params;
        params["session_id"] = fuzzedSessionId;
        params["event"] = fuzzedEvent;
        Json::Value ev;
        ev["params"] = params;
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        auto syntheticJson = Json::writeString(wb, ev);

        wtcli::MatchesEventFilter(syntheticJson, fuzzedSessionId, fuzzedFilter);

        // 2b: Mismatched session_id — exercises the rejection path.
        wtcli::MatchesEventFilter(syntheticJson, "999", fuzzedFilter);

        // 2c: Raw fuzzed bytes as event JSON — exercises parse-failure path.
        const std::string raw(reinterpret_cast<const char*>(data), size);
        wtcli::MatchesEventFilter(raw, fuzzedSessionId, fuzzedFilter);
    }

    return 0;
}

#ifdef FUZZING_BUILD
extern "C" __declspec(dllexport) int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/)
{
    return 0;
}
#else
int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: WtcliFuzzer <input-file>\n");
        return 1;
    }
    std::ifstream file(argv[1], std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return FuzzOneInput(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}
#endif

extern "C" __declspec(dllexport) int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    return FuzzOneInput(data, size);
}
