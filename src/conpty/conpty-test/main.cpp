#include <wil/com.h>

#include <conpty.h>

int main()
{
    wil::com_ptr<IPtyServer> server;
    THROW_IF_FAILED(PtyCreateServer(IID_PPV_ARGS(server.addressof())));
    wil::unique_process_information pi;
    THROW_IF_FAILED(server->CreateProcessW(
        nullptr,
        _wcsdup(L"C:\\Windows\\System32\\edit.exe"),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        pi.addressof()
    ));
    THROW_IF_FAILED(server->Run());
    return 0;
}
