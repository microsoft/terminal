/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- LogIO.h

Abstract:
- TODO DH: Explain logging

Author:
- Dustin Howett (DuHowett) 2020-11-14

Revision History:
--*/

#pragma once

namespace Microsoft::Console::Host::BinaryLogging
{
    enum class LogPacketType : uint16_t
    {
        Read = 1,
        InputBuffer = 2,
    };

#pragma pack(push, 1)
    struct LogHeader
    {
        uint8_t Version;
        uint16_t HostArchitecture;
    };

    struct LogPacketDescriptor
    {
        LogPacketType PacketType;
        uint64_t TimeDeltaInNs;
        uint32_t Length;
        uint32_t _PositionInFile;
    };
#pragma pack(pop)

    class LoggingDeviceComm : public IDeviceComm
    {
    public:
        LoggingDeviceComm(IDeviceComm* loggee, const std::wstring_view file);
        ~LoggingDeviceComm() = default;

        [[nodiscard]] HRESULT SetServerInformation(_In_ CD_IO_SERVER_INFORMATION* const pServerInfo) const override;
        [[nodiscard]] HRESULT ReadIo(_In_opt_ PCONSOLE_API_MSG const pReplyMsg,
                                     _Out_ CONSOLE_API_MSG* const pMessage) const override;
        [[nodiscard]] HRESULT CompleteIo(_In_ CD_IO_COMPLETE* const pCompletion) const override;
        [[nodiscard]] HRESULT ReadInput(_In_ CD_IO_OPERATION* const pIoOperation) const override;
        [[nodiscard]] HRESULT WriteOutput(_In_ CD_IO_OPERATION* const pIoOperation) const override;
        [[nodiscard]] HRESULT AllowUIAccess() const override;
        [[nodiscard]] ULONG_PTR PutHandle(const void* handle) override;
        [[nodiscard]] void* GetHandle(ULONG_PTR handleId) const override;
        void DestroyHandle(ULONG_PTR handleId) override;

    private:
        wil::unique_hfile _file;
        IDeviceComm* _loggee;

        mutable std::chrono::high_resolution_clock::time_point _lastEvent;
        std::vector<ULONG_PTR> _handleTable;

        uint64_t _timeDelta() const;

        mutable std::vector<std::byte> _dataArena;
        void _writeInFull(void* buffer, const size_t length) const;
        void _writeDataWithHeader(LogPacketType packetType, uint64_t timeDelta, size_t length, const void* buffer) const;

        std::thread _logThread;
        void _logThreadBody();
        void _startLogThread();
    };

    class LogReplayDeviceComm : public IDeviceComm
    {
    public:
        LogReplayDeviceComm(const std::wstring_view file, double timeDilation = 1.0);
        ~LogReplayDeviceComm() = default;

        [[nodiscard]] HRESULT SetServerInformation(_In_ CD_IO_SERVER_INFORMATION* const /*pServerInfo*/) const override;
        [[nodiscard]] HRESULT ReadIo(_In_opt_ PCONSOLE_API_MSG const /*pReplyMsg*/,
                                     _Out_ CONSOLE_API_MSG* const pMessage) const override;
        [[nodiscard]] HRESULT CompleteIo(_In_ CD_IO_COMPLETE* const /*pCompletion*/) const override;
        [[nodiscard]] HRESULT ReadInput(_In_ CD_IO_OPERATION* const pIoOperation) const override;
        [[nodiscard]] HRESULT WriteOutput(_In_ CD_IO_OPERATION* const /*pIoOperation*/) const override;
        [[nodiscard]] HRESULT AllowUIAccess() const override;
        [[nodiscard]] ULONG_PTR PutHandle(const void* handle) override;
        [[nodiscard]] void* GetHandle(ULONG_PTR handleId) const override;
        void DestroyHandle(ULONG_PTR handleId) override;

    private:
        wil::unique_hfile _file;
        double _timeDilation;
        std::vector<void*> _handleTable;

        wil::unique_handle _fileMapping;
        wil::unique_mapview_ptr<std::byte> _fileMapView;
        mutable ptrdiff_t _off{ 0 };
        size_t _max{};

        void _readInFull(void* buffer, const size_t length) const;

        const LogPacketDescriptor& _readDescriptor() const;
    };
}
