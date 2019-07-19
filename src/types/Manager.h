// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

class Manager final
{
public:
    Manager();
    virtual ~Manager();

    void NotifyExit();

    static void s_RegisterOnConnection(std::function<void()> func);
    static bool s_TrySendToManager(const HANDLE server);

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
        GetManagerPid,
        SendConnection
    };

    struct ManagerMessageQuery
    {
        ManagerMessageTypes type;
        union
        {
            struct SendConnection
            {
                HANDLE handle;
            } sendConn;
        } query;
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
            struct SendConnection
            {
                bool ok;
            } sendConn;
        } reply;
    };

    static ManagerMessageReply _ask(ManagerMessageQuery query);
    static ManagerMessageReply _getPid(ManagerMessageQuery query);
    static ManagerMessageReply _sendConnection(ManagerMessageQuery query);

    void _becomeServer();
    void _serverLoop();
    void _perClientLoop(wil::unique_handle pipe);
};
