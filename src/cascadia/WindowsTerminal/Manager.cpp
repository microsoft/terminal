// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Manager.h"

static constexpr std::wstring_view MUTEX_NAME = L"Local\\WindowsTerminalManager";
static constexpr std::wstring_view PIPE_NAME = L"\\\\.\\pipe\\WindowsTerminalManagerPipe";
static constexpr DWORD PIPE_BUFFER_SIZE = 4096;

Manager::Manager() :
    _mutex(),
    _pipe(),
    _exit(),
    _theServer(false)
{
    // Create exit event.
    _exit.create();

    // Try to open the mutex. 
    if (!_mutex.try_open(MUTEX_NAME.data()))
    {
        // If we can't open it, create one because no one else did.
        _mutex.create(MUTEX_NAME.data());
        _theServer = true;
    }
    else
    {
        // If it could just be opened, we're not the server.
        _theServer = false;
    }

    // If we're the server, establish communication listener and thread
    if (_theServer)
    {
        _becomeServer();
    }
    // If we're not the server, find out who it is so we can get notified when they leave and become the server.
    else
    {
        ManagerMessageQuery query;
        query.type = ManagerMessageTypes::GetManagerPid;

        const auto reply = _ask(query);

        _waitToBecomeServer = std::async(std::launch::async, [reply, this] {

            wil::unique_handle managerProcess(OpenProcess(SYNCHRONIZE, FALSE, reply.reply.getPid.id));
            THROW_LAST_ERROR_IF_NULL(managerProcess.get());

            WaitForSingleObject(managerProcess.get(), INFINITE);
            _becomeServer();
        });
    }
}

Manager::~Manager()
{

}

void Manager::NotifyExit()
{
    _exit.SetEvent();
}

void Manager::_becomeServer()
{
    // lock?

    // test if pipe exists?

    _theServer = true;

    // enter loop to make server connections
    _serverWork = std::async(std::launch::async, [this] {
        _serverLoop();
    });
}

void Manager::_serverLoop()
{
    wil::unique_event newClient;
    newClient.create(wil::EventOptions::ManualReset);

    bool keepGoing = true;
    while (keepGoing)
    {
        OVERLAPPED overlap = { 0 };
        overlap.hEvent = newClient.get();

        wil::unique_handle pipe(CreateNamedPipeW(PIPE_NAME.data(),
                                                 PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                                 PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_ACCEPT_REMOTE_CLIENTS,
                                                 PIPE_UNLIMITED_INSTANCES,
                                                 PIPE_BUFFER_SIZE,
                                                 PIPE_BUFFER_SIZE,
                                                 0,
                                                 nullptr)); // replace with actual security descriptor?

        THROW_LAST_ERROR_IF(pipe.get() == INVALID_HANDLE_VALUE);

        if (!ConnectNamedPipe(pipe.get(), &overlap))
        {
            // IO pending and Pipe Connected are OK error codes. Go into the wait.
            const auto gle = GetLastError();
            if (gle != ERROR_IO_PENDING && gle != ERROR_PIPE_CONNECTED)
            {
                THROW_LAST_ERROR();
            }
        }

        std::array<HANDLE, 2> waitOn;
        waitOn.at(0) = _exit.get();
        waitOn.at(1) = newClient.get();
        const auto ret = WaitForMultipleObjects(gsl::narrow<DWORD>(waitOn.size()), waitOn.data(), FALSE, INFINITE);

        if (ret == WAIT_OBJECT_0)
        {
            keepGoing = false;
            continue;
        }
        else if (ret == WAIT_OBJECT_0 + 1)
        {
            auto future = std::async(std::launch::async, [this, &pipe] {
                _perClientLoop(std::move(pipe));
            });
        }
        else if (ret == WAIT_FAILED)
        {
            THROW_LAST_ERROR();
        }
        else
        {
            THROW_WIN32(ret);
        }
    }
}

void Manager::_perClientLoop(wil::unique_handle pipe)
{
    bool keepGoing = true;
    while (keepGoing)
    {
        ManagerMessageQuery query;
        DWORD bytesRead = 0;
        THROW_IF_WIN32_BOOL_FALSE(ReadFile(pipe.get(),
                                           &query,
                                           sizeof(query),
                                           &bytesRead,
                                           nullptr));

        ManagerMessageReply reply;
        switch (query.type)
        {
        case ManagerMessageTypes::GetManagerPid:
            reply = _getPid();
            break;
        default:
            THROW_HR(E_NOTIMPL);
        }

        DWORD bytesWritten = 0;
        THROW_IF_WIN32_BOOL_FALSE(WriteFile(pipe.get(),
                                            &reply,
                                            sizeof(reply),
                                            &bytesWritten,
                                            nullptr));
    }
}

Manager::ManagerMessageReply Manager::_ask(Manager::ManagerMessageQuery query)
{
    ManagerMessageReply reply;

    DWORD bytesRead = 0;
    THROW_IF_WIN32_BOOL_FALSE(CallNamedPipeW(PIPE_NAME.data(),
                                             &query,
                                             sizeof(query),
                                             &reply,
                                             sizeof(reply),
                                             &bytesRead,
                                             NMPWAIT_USE_DEFAULT_WAIT));

    return reply;
}

Manager::ManagerMessageReply Manager::_getPid()
{
    ManagerMessageReply reply;

    reply.type = ManagerMessageTypes::GetManagerPid;
    reply.reply.getPid.id = GetCurrentProcessId();

    return reply;
}
