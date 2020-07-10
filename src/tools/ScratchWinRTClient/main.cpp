#include "pch.h"

#include <winrt/ScratchWinRTServer.h>
using namespace winrt;
using namespace Windows::Foundation;

static constexpr GUID server_clsid // DAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
    {
        0xdaa16d7f,
        0xef66,
        0x4fc9,
        { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
    };

void actualApp()
{
    {
        printf("Trying to directly create a IStringable...\n");
        auto server = create_instance<IStringable>(server_clsid, CLSCTX_LOCAL_SERVER);
        if (server)
        {
            printf("%ls\n", server.ToString().c_str());
            if (auto asScratch = server.try_as<ScratchWinRTServer::IScratchInterface>())
            {
                printf("Found scratch interface\n");
                printf("%ls\n", asScratch.DoTheThing().c_str());
            }
            else
            {
                printf("Could not get the IScratchInterface from the IStringable\n");
            }
        }
        else
        {
            printf("Could not get the IStringable directly\n");
        }
    }
    {
        printf("Trying to directly create a IScratchInterface...\n");
        auto server = create_instance<winrt::ScratchWinRTServer::IScratchInterface>(server_clsid, CLSCTX_LOCAL_SERVER);
        if (server)
        {
            printf("Found scratch interface\n");
            printf("%ls\n", server.DoTheThing().c_str());
        }
        else
        {
            printf("Could not get the IScratchInterface directly\n");
        }
    }
}

int main()
{
    try
    {
        init_apartment();
        actualApp();

        puts("Press Enter me when you're done.");
        getchar();
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls", e.message().c_str());
    }
}
