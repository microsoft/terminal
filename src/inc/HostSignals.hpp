// These values match the enumeration values of `ControlType` for the `ConsoleControl` class
// but are defined here similarly to not pollute other projects.zs
// They don't *have* to be the same values, but matching them seemed to make sense.
#define HOST_SIGNAL_NOTIFY_APP 1u
struct HOST_SIGNAL_NOTIFY_APP_DATA
{
    DWORD cbSize;
    DWORD dwProcessId;
};

#define HOST_SIGNAL_SET_FOREGROUND 5u
struct HOST_SIGNAL_SET_FOREGROUND_DATA
{
    DWORD cbSize;
    ULONG dwProcessId;
    BOOL fForeground;
};

#define HOST_SIGNAL_END_TASK 7u
struct HOST_SIGNAL_END_TASK_DATA
{

    DWORD cbSize;
    ULONG dwProcessId;
    DWORD dwEventType;
    ULONG ulCtrlFlags;
};
