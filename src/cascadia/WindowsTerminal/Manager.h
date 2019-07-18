// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

class Manager final
{
public:
    Manager();
    virtual ~Manager();

    void NotifyExit();

    Manager(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager& operator=(Manager&&) = delete;
private:
    wil::unique_mutex _mutex;
    wil::unique_event _exit;
    wil::unique_handle _pipe;
    bool _theServer;
    std::future<void> _waitToBecomeServer;
    std::future<void> _serverWork;
    std::vector<std::future<void>> _perClientWork;

    enum class ManagerMessageTypes
    {
        GetManagerPid
    };

    struct ManagerMessageQuery
    {
        ManagerMessageTypes type;
    };

    struct ManagerMessageReply
    {
        ManagerMessageTypes type;
        union
        {
            struct GetManagerPid
            {
                DWORD id;
            } getPid;
        } reply;
    };

    ManagerMessageReply _ask(ManagerMessageQuery query);
    ManagerMessageReply _getPid();

    void _becomeServer();
    void _serverLoop();
    void _perClientLoop(wil::unique_handle pipe);
};
