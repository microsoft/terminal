// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
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
        query.size = sizeof(query);
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

bool Manager::s_TrySendToManager(const HANDLE server)
{
    try
    {
        // Get PID from remote process.
        ManagerMessageQuery query;
        query.size = sizeof(query);
        query.type = ManagerMessageTypes::GetManagerPid;

        auto reply = _ask(query);
        const auto processId = reply.reply.getPid.id;

        // Open for handle duping.
        wil::unique_handle otherProcess(OpenProcess(PROCESS_DUP_HANDLE, FALSE, processId));
        THROW_LAST_ERROR_IF_NULL(otherProcess.get());

        // Send handle into that process.
        HANDLE targetHandle;
        THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(GetCurrentProcess(), server,
                                                  otherProcess.get(), &targetHandle,
                                                  0,
                                                  FALSE,
                                                  DUPLICATE_SAME_ACCESS));

        // Tell remote process about new handle ID
        query.size = sizeof(query);
        query.type = ManagerMessageTypes::SendConnection;
        query.query.sendConn.handle = targetHandle;

        reply = _ask(query);

        return reply.reply.sendConn.ok;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return false;
    }
}

bool Manager::s_TrySendToManager(const std::wstring_view cmdline,
                                 const std::wstring_view workingDir)
{
    try
    {
        // Figure out how much data we actually need to send with the variable length strings
        size_t size = sizeof(ManagerMessageQuery) + 1;
        size += 2*(cmdline.size() + 1);
        size += 2*(workingDir.size() + 1);

        // Make a big buffer to hold it all contiguously
        const auto buffer = std::make_unique<byte[]>(size);

        // Make pointers to fill up each piece of data
        const auto query = reinterpret_cast<ManagerMessageQuery*>(buffer.get());
        const auto cmdPayload = reinterpret_cast<PWCHAR>(buffer.get() + sizeof(ManagerMessageQuery) + 1);
        const auto workingPayload = reinterpret_cast<PWCHAR>(buffer.get() + sizeof(ManagerMessageQuery) + 1 + (cmdline.size() + 1) * 2);

        // Tell the remote process the command line and working directory
        query->size = gsl::narrow<DWORD>(size);
        query->type = ManagerMessageTypes::SendCmdAndWorking;
        query->query.sendCmdAndWorking.cmd = gsl::narrow<DWORD>(cmdline.size());
        query->query.sendCmdAndWorking.working = gsl::narrow<DWORD>(workingDir.size());

        THROW_IF_FAILED(StringCchCopyW(cmdPayload, cmdline.size() + 1, cmdline.data()));
        THROW_IF_FAILED(StringCchCopyW(workingPayload, workingDir.size() + 1, workingDir.data()));

        const auto reply = _ask(*query);

        return reply.reply.sendConn.ok;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return false;
    }
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
            const auto loosePipeHandle = pipe.release();
            auto future = std::async(std::launch::async, [this, loosePipeHandle] {
                _perClientLoop(wil::unique_handle(loosePipeHandle));
                                     });
            _perClientWork.push_back(std::move(future));
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
    /*bool keepGoing = true;
    while (keepGoing)*/
    {
        ManagerMessageQuery query;
        DWORD bytesRead = 0;
        SetLastError(S_OK);
        const auto result = ReadFile(pipe.get(),
                                     &query,
                                     sizeof(query),
                                     &bytesRead,
                                     nullptr);

        // False is OK if it's ERROR_MORE_DATA.
        const auto gle = GetLastError();
        THROW_LAST_ERROR_IF(!result && gle != ERROR_MORE_DATA);

        std::unique_ptr<byte[]> buffer;
        if (gle == ERROR_MORE_DATA)
        {
            const auto remainingBytes = query.size - gsl::narrow<DWORD>(sizeof(query));
            buffer = std::make_unique<byte[]>(remainingBytes);

            bytesRead = 0;
            THROW_IF_WIN32_BOOL_FALSE(ReadFile(pipe.get(),
                                               buffer.get(),
                                               remainingBytes,
                                               &bytesRead,
                                               nullptr));
        }

        ManagerMessageReply reply;
        switch (query.type)
        {
        case ManagerMessageTypes::GetManagerPid:
            reply = _getPid(query);
            break;
        case ManagerMessageTypes::SendConnection:
            reply = _sendConnection(query);
            break;
        case ManagerMessageTypes::SendCmdAndWorking:
            reply = _sendCmdAndWorking(query, buffer);
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

Manager::ManagerMessageReply Manager::_ask(Manager::ManagerMessageQuery& query)
{
    ManagerMessageReply reply;

    DWORD bytesRead = 0;
    THROW_IF_WIN32_BOOL_FALSE(CallNamedPipeW(PIPE_NAME.data(),
                                             &query,
                                             query.size,
                                             &reply,
                                             sizeof(reply),
                                             &bytesRead,
                                             NMPWAIT_WAIT_FOREVER));

    return reply;
}

Manager::ManagerMessageReply Manager::_getPid(ManagerMessageQuery /*query*/)
{
    ManagerMessageReply reply;

    reply.type = ManagerMessageTypes::GetManagerPid;
    reply.reply.getPid.id = GetCurrentProcessId();

    return reply;
}

static std::vector<std::function<void(HANDLE)>> s_onHandleConnection;

void Manager::s_RegisterOnConnection(std::function<void(HANDLE)> func)
{
    s_onHandleConnection.push_back(func);
}

Manager::ManagerMessageReply Manager::_sendConnection(ManagerMessageQuery query)
{
    const auto serverHandle = query.query.sendConn.handle;

    // create new conhost connection...
    for (const auto& func : s_onHandleConnection)
    {
        func(serverHandle);
    }

    ManagerMessageReply reply;

    reply.type = ManagerMessageTypes::SendConnection;
    reply.reply.sendConn.ok = true;

    return reply;
}

static std::vector<std::function<void(std::wstring_view, std::wstring_view)>> s_onLaunchConnection;

void Manager::s_RegisterOnConnection(std::function<void(std::wstring_view, std::wstring_view)> func)
{
    s_onLaunchConnection.push_back(func);
}

Manager::ManagerMessageReply Manager::_sendCmdAndWorking(ManagerMessageQuery query, std::unique_ptr<BYTE[]>& buffer)
{
    const auto cmd = query.query.sendCmdAndWorking.cmd;
    const auto work = query.query.sendCmdAndWorking.working;

    std::wstring_view cmdline(reinterpret_cast<wchar_t*>(buffer.get() + 1), cmd);
    std::wstring_view workingDir(reinterpret_cast<wchar_t*>(buffer.get() + 1 + (cmd + 1) * 2), work);

    // create new conhost connection...
    for (const auto& func : s_onLaunchConnection)
    {
        func(cmdline, workingDir);
    }

    ManagerMessageReply reply;

    reply.type = ManagerMessageTypes::SendCmdAndWorking;
    reply.reply.sendCmdAndWorking.ok = true;

    return reply;
}

