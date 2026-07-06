// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Fuzzing harness for wtcli CLI utility functions.
//
// Built under the Fuzzing MSBuild configuration with LibFuzzer
// instrumentation; submittable to OneFuzz via the CI pipeline.

#include "precomp.h"
#include "wtcli_functions.h"

// Core fuzzing logic — called by both LibFuzzer and the manual main().
static int FuzzOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        return 0;
    }

    // Fuzz ExpandEnvironmentStringsInUtf8
    const std::string input(reinterpret_cast<const char*>(data), size);
    (void)wtcli::ExpandEnvironmentStringsInUtf8(input);

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
