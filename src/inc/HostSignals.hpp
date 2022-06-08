namespace Microsoft::Console
{
    // These values match the enumeration values of `ControlType` for the `ConsoleControl` class
    // but are defined here similarly to not pollute other projects.
    // They don't *have* to be the same values, but matching them seemed to make sense.
    enum class HostSignals : uint8_t
    {
        NotifyApp = 1u,
        SetForeground = 5u,
        EndTask = 7u
    };

    struct HostSignalNotifyAppData
    {
        uint32_t sizeInBytes;
        uint32_t processId; // THIS IS A PID
    };

    struct HostSignalSetForegroundData
    {
        uint32_t sizeInBytes;
        uint32_t processId; // THIS IS A HANDLE, NOT A PID
        bool isForeground;
    };

    struct HostSignalEndTaskData
    {
        uint32_t sizeInBytes;
        uint32_t processId; // THIS IS A PID
        uint32_t eventType;
        uint32_t ctrlFlags;
    };
};
