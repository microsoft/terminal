#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;

static constexpr GUID server_clsid // DAA16D7F-EF66-4FC9-B6F2-3E5B6D924576
    {
        0xdaa16d7f,
        0xef66,
        0x4fc9,
        { 0xb6, 0xf2, 0x3e, 0x5b, 0x6d, 0x92, 0x45, 0x76 }
    };

int main()
{
    try
    {
        init_apartment();

        auto server = create_instance<IStringable>(server_clsid, CLSCTX_LOCAL_SERVER);

        printf("%ls\n", server.ToString().c_str());
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls", e.message().c_str());
    }
    puts("Press Enter me when you're done.");
    getchar();
}
