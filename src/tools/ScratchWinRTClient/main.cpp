#include "pch.h"

#include <winrt/ScratchWinRTServer.h>
using namespace winrt;
using namespace Windows::Foundation;

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

void actualApp()
{
    {
        printf("Trying to directly create a IStringable...\n");
        auto server = create_instance<IStringable>(MyStringable_clsid, CLSCTX_LOCAL_SERVER);
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
        auto server = create_instance<winrt::ScratchWinRTServer::IScratchInterface>(MyStringable_clsid, CLSCTX_LOCAL_SERVER);
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

void closeApp()
{
    printf("closeApp\n");
    {
        printf("Trying to directly create a IStringable...\n");
        auto server = create_instance<IStringable>(ScratchStringable_clsid, CLSCTX_LOCAL_SERVER);
        if (server)
        {
            printf("%ls\n", server.ToString().c_str());
            if (auto asScratch = server.try_as<winrt::ScratchWinRTServer::IScratchInterface>())
            {
                printf("Found scratch interface\n");
                printf("%ls\n", asScratch.DoTheThing().c_str());
            }
            else
            {
                printf("Could not get the IScratchInterface from the IStringable\n");
            }

            if (auto asClosable = server.try_as<IClosable>())
            {
                asClosable.Close();
                printf("Closed!!!!!!\n");
            }
            else
            {
                printf("Could not get the IClosable from the IStringable\n");
            }
        }
        else
        {
            printf("Could not get the IStringable directly\n");
        }
    }
    {
        printf("Trying to directly create a IClosable...\n");
        auto server = create_instance<IClosable>(ScratchStringable_clsid, CLSCTX_LOCAL_SERVER);
        if (server)
        {
            server.Close();
            printf("Closed?\n");
            if (auto asScratch = server.try_as<ScratchWinRTServer::IScratchInterface>())
            {
                printf("Found scratch interface\n");
                printf("%ls\n", asScratch.DoTheThing().c_str());
            }
            else
            {
                printf("Could not get the IScratchInterface from the IClosable\n");
            }
        }
        else
        {
            printf("Could not get the IClosable directly\n");
        }
    }
}

void scratchApp()
{
    printf("scratchApp\n");
    {
        printf("Trying to directly create a ScratchClass...\n");
        auto server = create_instance<winrt::ScratchWinRTServer::ScratchClass>(ScratchClass_clsid, CLSCTX_LOCAL_SERVER);
        if (server)
        {
            printf("%ls\n", server.DoTheThing().c_str());

            auto btn = server.MyButton();
            auto content = btn.Content();
            auto label = winrt::unbox_value_or<winrt::hstring>(content, L"");
            if (!label.empty())
            {
                printf("Got the button's label\"%ls\"\n", label.c_str());
            }
            else
            {
                printf("Could not get the button label\n");
            }
        }
        else
        {
            printf("Could not get the ScratchClass directly\n");
        }
    }
}

int main()
{
    init_apartment();
    try
    {
        actualApp();
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }

    printf("------------------\n");
    try
    {
        closeApp();
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }
    printf("------------------\n");
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
