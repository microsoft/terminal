// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Fuzzing harness for Terminal Protocol server parsing logic.
// Targets: ClassifySendEvent, ParseSplitDirection, ClassifyPaneOutputSource.
//
// Built under the Fuzzing MSBuild configuration with LibFuzzer
// instrumentation; submittable to OneFuzz via the CI pipeline.

#include "precomp.h"
#include "ProtocolParsing.h"

namespace ProtocolParsing = Microsoft::Terminal::Protocol::Parsing;

// Core fuzzing logic — called by both LibFuzzer and the manual main().
static int FuzzOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        return 0;
    }

    const std::string input(reinterpret_cast<const char*>(data), size);

    // ── Target 1: ClassifySendEvent ──
    // Feed fuzzed data as a SendEvent JSON payload. Exercises JSON parsing,
    // method field inspection, and params.event validation across all 3
    // dispatch routes (AutofixState, AgentStatus, Broadcast).
    {
        Json::Value evt;
        ProtocolParsing::ClassifySendEvent(input, evt);
    }

    // ── Target 2: ParseSplitDirection ──
    // Feed fuzzed data as a direction string. Exercises all 8 recognized
    // values plus arbitrary unrecognized strings.
    {
        ProtocolParsing::ParseSplitDirection(input);
    }

    // ── Target 3: ClassifyPaneOutputSource ──
    // Feed fuzzed data as a source parameter.
    {
        ProtocolParsing::ClassifyPaneOutputSource(input);
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
        fprintf(stderr, "Usage: ProtocolFuzzer <input-file>\n");
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
