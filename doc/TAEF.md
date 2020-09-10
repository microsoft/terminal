### TAEF Overview ###

TAEF, the Test Authoring and Execution Framework, is used extensively within the Windows organization to test the operating system code in a unified manner for system, driver, and application code. As the console is a Windows OS Component, we strive to continue using the same system such that tests can be ran in a unified manner both externally to Microsoft as well as inside the official OS Build/Test system.

The [official documentation](https://msdn.microsoft.com/en-us/library/windows/hardware/hh439725\(v=vs.85\).aspx) for TAEF describes the basic architecture, usage, and functionality of the test system. It is similar to Visual Studio test, but a bit more comprehensive and flexible.

### Writing Tests

You may want to read the section [Authoring Tests in C++](https://docs.microsoft.com/en-us/windows-hardware/drivers/taef/authoring-tests-in-c--) before getting your hands dirty. Note that the quoted header name in `#include "WexTestClass.h"` might be a bit confusing. You are not required to copy TAEF headers into the project folder.

Use the [TAEF Verify Macros for C++](https://docs.microsoft.com/en-us/windows-hardware/drivers/taef/verify) in your test code to perform verifications.

### Running Tests

If you have Visual Studio and related C++ toolchain installed, you should have `te.exe` available locally.

In a "normal" CMD enviroment, `te.exe` may not be directly available. Try the following command to set up the development enviroment first:

```shell
.\tools\razzle.cmd
```

You should be able to use `%TAEF%` as an alias of the actual `te.exe`.

For the purposes of the OpenConsole project, you can run the tests using the `te.exe` that matches the architecture for which the test was build (x86/x64) in the pattern

	te.exe Console.Unit.Tests.dll

Replacing the binary name with any other test binary name that might need running. Wildcard patterns or multiple binaries can be specified and all found tests will be executed.

Limiting the tests to be run is also useful with:

	te.exe Console.Unit.Tests.dll /name:*BufferTests*

Any pattern of class/method names can be specified after the */name:* flag with wildcard patterns.

For any further details on the functionality of the TAEF test runner, *TE.exe*, please see the documentation above or run the embedded help with

	te.exe /!
