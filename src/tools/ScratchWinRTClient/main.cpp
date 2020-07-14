#include "pch.h"

#include <winrt/ScratchWinRTServer.h>
using namespace winrt;
using namespace winrt::Windows::Foundation;

// DAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
static constexpr GUID MyStringable_clsid{
    0xdaa16d7f,
    0xef66,
    0x4fc9,
    { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
};

// EAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
static constexpr GUID ScratchStringable_clsid{
    0xeaa16d7f,
    0xef66,
    0x4fc9,
    { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
};

// FAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
static constexpr GUID ScratchClass_clsid{
    0xfaa16d7f,
    0xef66,
    0x4fc9,
    { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
};

void scratchApp()
{
    printf("scratchApp\n");
    // {
    //     printf("Trying to directly create a ScratchClass...\n");
    //     auto server = create_instance<winrt::ScratchWinRTServer::ScratchClass>(ScratchClass_clsid, CLSCTX_LOCAL_SERVER);
    //     if (server)
    //     {
    //         printf("%ls\n", server.DoTheThing().c_str());
    //     }
    //     else
    //     {
    //         printf("Could not get the ScratchClass directly\n");
    //     }
    // }

    // 1. Generate a GUID.
    // 2. Spawn a Server.exe, with the guid on the commandline
    // 3. Make an instance of that GUID, as a HostClass
    // 4. Call HostClass::DoTheThing, and get the count with HostClass::DoCount [1]
    // 5. Make another instance of HostClass, and get the count with HostClass::DoCount. It should be the same. [1, 1]
    // 6. On the second HostClass, call DoTheThing. Verify that both instances have the same DoCount. [2. 2]
    // 7. Create a second Server.exe, and create a Third HostClass, using that GUID.
    // 8. Call DoTheThing on the third, and verify the counts of all three instances. [2, 2, 1]
    // 9. QUESTION: Does releasing the first instance leave the first object alive, since the second instance still points at it?
}

int main()
{
    init_apartment();

    try
    {
        scratchApp();
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }

    puts("Press Enter me when you're done.");
    getchar();
}
