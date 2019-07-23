// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

class Manager final
{
public:
    Manager();
    virtual ~Manager();

    void NotifyExit();

    static void s_RegisterOnConnection(std::function<void(HANDLE)> func);
    static void s_RegisterOnConnection(std::function<void(std::wstring_view, std::wstring_view)> func);
    static bool s_TrySendToManager(const HANDLE server);
    static bool s_TrySendToManager(const std::wstring_view cmdline,
                                   const std::wstring_view workingDir);

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
        SendConnection,
        SendCmdAndWorking
    };

    struct ManagerMessageQuery
    {
        DWORD size;
        ManagerMessageTypes type;
        union
        {
            struct SendConnection
            {
                HANDLE handle;
            } sendConn;
            struct SendCmdAndWorking
            {
                DWORD cmd;
                DWORD working;
            } sendCmdAndWorking;
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
            struct SendCmdAndWorking
            {
                bool ok;
            } sendCmdAndWorking;
        } reply;
    };

    static ManagerMessageReply _ask(ManagerMessageQuery& query);
    static ManagerMessageReply _getPid(ManagerMessageQuery query);
    static ManagerMessageReply _sendConnection(ManagerMessageQuery query);
    static ManagerMessageReply _sendCmdAndWorking(ManagerMessageQuery query, std::unique_ptr<byte[]>& buffer);

    void _becomeServer();
    void _serverLoop();
    void _perClientLoop(wil::unique_handle pipe);
};
