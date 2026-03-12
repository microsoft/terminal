#include <wil/com.h>

#include <conpty.h>

int main()
{
    wil::com_ptr<IPtyServer> server;
    THROW_IF_FAILED(PtyCreateServer(IID_PPV_ARGS(server.addressof())));
    THROW_IF_FAILED(server->Run());
    return 0;
}
