// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "LogIO.h"

using namespace Microsoft::Console::Host::BinaryLogging;

LoggingDeviceComm::LoggingDeviceComm(IDeviceComm* loggee, const std::wstring_view file) :
    _loggee(loggee),
    _lastEvent(std::chrono::high_resolution_clock::now())
{
    _dataArena.resize(1024);
    _file.reset(CreateFileW(file.data(), GENERIC_WRITE | DELETE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));

    USHORT processMachine{};
    USHORT nativeMachine{};
    THROW_IF_WIN32_BOOL_FALSE(IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine));

    LogHeader header{};
    header.Version = 1;
    header.HostArchitecture = nativeMachine;
    _writeInFull(&header, sizeof(LogHeader));

    //_startLogThread();
}

[[nodiscard]] HRESULT LoggingDeviceComm::SetServerInformation(CD_IO_SERVER_INFORMATION* const pServerInfo) const
{
    return _loggee->SetServerInformation(pServerInfo);
}

[[nodiscard]] HRESULT LoggingDeviceComm::ReadIo(PCONSOLE_API_MSG const pReplyMsg, CONSOLE_API_MSG* const pMessage) const
{
    RETURN_IF_FAILED(_loggee->ReadIo(pReplyMsg, pMessage));
    static constexpr uint32_t apiMsgLen{ sizeof(CONSOLE_API_MSG) - offsetof(CONSOLE_API_MSG, Descriptor) };
    _writeDataWithHeader(LogPacketType::Read, _timeDelta(), apiMsgLen, &pMessage->Descriptor);
    return S_OK;
}

[[nodiscard]] HRESULT LoggingDeviceComm::CompleteIo(CD_IO_COMPLETE* const pCompletion) const
{
    return _loggee->CompleteIo(pCompletion);
}

[[nodiscard]] HRESULT LoggingDeviceComm::ReadInput(CD_IO_OPERATION* const pIoOperation) const
{
    RETURN_IF_FAILED(_loggee->ReadInput(pIoOperation));
    _writeDataWithHeader(LogPacketType::InputBuffer, _timeDelta(), pIoOperation->Buffer.Size, pIoOperation->Buffer.Data);
    return S_OK;
}

[[nodiscard]] HRESULT LoggingDeviceComm::WriteOutput(CD_IO_OPERATION* const pIoOperation) const
{
    return _loggee->WriteOutput(pIoOperation);
}

[[nodiscard]] HRESULT LoggingDeviceComm::AllowUIAccess() const
{
    return _loggee->AllowUIAccess();
}

[[nodiscard]] ULONG_PTR LoggingDeviceComm::PutHandle(const void* handle)
{
    auto upstreamMapping{ _loggee->PutHandle(handle) };
    auto found{ std::find(_handleTable.begin(), _handleTable.end(), upstreamMapping) };
    if (found == _handleTable.end())
    {
        // append, get pos
        found = _handleTable.emplace(found, upstreamMapping);
    }
    return found - _handleTable.begin();
}

[[nodiscard]] void* LoggingDeviceComm::GetHandle(ULONG_PTR handleId) const
{
    return _loggee->GetHandle(_handleTable.at(handleId));
}

void LoggingDeviceComm::DestroyHandle(ULONG_PTR handleId)
{
    auto& pos{ _handleTable.at(handleId) };
    _loggee->DestroyHandle(pos);
    pos = 0;
}

uint64_t LoggingDeviceComm::_timeDelta() const
{
    auto thisEventTime{ std::chrono::high_resolution_clock::now() };
    int64_t delta{ (thisEventTime - _lastEvent).count() };
    _lastEvent = thisEventTime;
    return delta;
}

void LoggingDeviceComm::_writeInFull(void* buffer, const size_t length) const
{
    auto remaining{ length };
    DWORD written{};
    while (WriteFile(_file.get(), reinterpret_cast<std::byte*>(buffer) + (length - remaining), gsl::narrow_cast<DWORD>(remaining), &written, nullptr) != 0)
    {
        if ((remaining -= written) == 0)
        {
            break;
        }
    }

    if (remaining)
    {
        THROW_HR(E_BOUNDS);
    }
}

void LoggingDeviceComm::_writeDataWithHeader(LogPacketType packetType, uint64_t timeDelta, size_t length, const void* buffer) const
{
    auto fullPacketLen{ length + offsetof(LogPacketDescriptor, _PositionInFile) };
    if (_dataArena.size() < fullPacketLen)
    {
        _dataArena.resize(fullPacketLen);
    }
    *(reinterpret_cast<LogPacketDescriptor*>(_dataArena.data())) = LogPacketDescriptor{ packetType, timeDelta, gsl::narrow_cast<uint32_t>(length) };
    const auto bufferAsByte{ reinterpret_cast<const std::byte*>(buffer) };
    std::copy(bufferAsByte, bufferAsByte + length, _dataArena.data() + (offsetof(LogPacketDescriptor, _PositionInFile)));

    _writeInFull(_dataArena.data(), fullPacketLen);
}

void LoggingDeviceComm::_logThreadBody()
{
}

void LoggingDeviceComm::_startLogThread()
{
    _logThread = std::thread{
        [this]() {
            _logThreadBody();
        }
    };
}

/******************************************************REPLAY********************************/

void LogReplayDeviceComm::_readInFull(void* buffer, const size_t length) const
{
    /*
        auto remaining{ length };
        DWORD read{};
        while (!!ReadFile(_file.get(), reinterpret_cast<std::byte*>(buffer) + (length - remaining), gsl::narrow_cast<DWORD>(remaining), &read, nullptr))
        {
            if ((remaining -= read) == 0)
            {
                break;
            }
        }

        if (remaining)
        {
            THROW_HR(E_BOUNDS);
        }
        */
    if (length > _max - _off)
    {
        THROW_HR(E_BOUNDS);
    }
    memcpy(buffer, _fileMapView.get() + _off, length);
    _off += length;
}

const LogPacketDescriptor& LogReplayDeviceComm::_readDescriptor() const
{
    //LogPacketDescriptor descriptor{};
    //auto filepos{ SetFilePointer(_file.get(), 0, nullptr, FILE_CURRENT) };
    //_readInFull(&descriptor, offsetof(LogPacketDescriptor, _PositionInFile));
    //descriptor._PositionInFile = filepos;
    auto oldOff{ _off };
    _off += offsetof(LogPacketDescriptor, _PositionInFile);
    return *reinterpret_cast<LogPacketDescriptor*>(_fileMapView.get() + oldOff);
    //return descriptor;
}

LogReplayDeviceComm::LogReplayDeviceComm(const std::wstring_view file, double timeDilation) :
    _timeDilation(timeDilation)
{
    _file.reset(CreateFileW(file.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    _fileMapping.reset(CreateFileMappingW(_file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr));
    THROW_HR_IF_NULL(E_FAIL, _fileMapping);
    _fileMapView.reset(static_cast<std::byte*>(MapViewOfFile(_fileMapping.get(), FILE_MAP_READ, 0, 0, 0)));
    THROW_HR_IF_NULL(E_FAIL, _fileMapView);

    MEMORY_BASIC_INFORMATION mbi{};
    VirtualQuery(_fileMapView.get(), &mbi, sizeof(mbi));
    _max = mbi.RegionSize;

    USHORT processMachine{};
    USHORT nativeMachine{};
    THROW_IF_WIN32_BOOL_FALSE(IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine));

    LogHeader header{};
    _readInFull(&header, sizeof(header));
    if (nativeMachine != header.HostArchitecture)
    {
        MessageBoxW(nullptr, wil::str_printf<std::wstring>(L"This dump was created on a conhost of a different architecture (expected %2.02x, got %2.02x).", header.HostArchitecture, nativeMachine).c_str(), L"Error", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }
}

[[nodiscard]] HRESULT LogReplayDeviceComm::SetServerInformation(CD_IO_SERVER_INFORMATION* const) const
{
    return S_OK;
}

[[nodiscard]] HRESULT LogReplayDeviceComm::ReadIo(PCONSOLE_API_MSG const, CONSOLE_API_MSG* const pMessage) const
{
    auto requestStartTime{ std::chrono::high_resolution_clock::now() };

    static constexpr uint32_t maxLen{ sizeof(CONSOLE_API_MSG) - offsetof(CONSOLE_API_MSG, Descriptor) };
    auto& descriptor{ _readDescriptor() };
    THROW_HR_IF(E_UNEXPECTED, descriptor.PacketType != LogPacketType::Read || descriptor.Length < maxLen);

    _readInFull(&pMessage->Descriptor, descriptor.Length);

    auto finishTime{ requestStartTime + std::chrono::nanoseconds(static_cast<long long>(descriptor.TimeDeltaInNs * _timeDilation)) };
    if (finishTime > std::chrono::high_resolution_clock::now())
        std::this_thread::sleep_until(finishTime);
    return S_OK;
}

[[nodiscard]] HRESULT LogReplayDeviceComm::CompleteIo(CD_IO_COMPLETE* const) const
{
    return S_OK;
}

[[nodiscard]] HRESULT LogReplayDeviceComm::ReadInput(CD_IO_OPERATION* const pIoOperation) const
{
    auto requestStartTime{ std::chrono::high_resolution_clock::now() };
    auto& descriptor{ _readDescriptor() };
    THROW_HR_IF(E_UNEXPECTED, descriptor.PacketType != LogPacketType::InputBuffer);

    _readInFull(static_cast<uint8_t*>(pIoOperation->Buffer.Data), descriptor.Length);
    auto finishTime{ requestStartTime + std::chrono::nanoseconds(static_cast<long long>(descriptor.TimeDeltaInNs * _timeDilation)) };
    if (finishTime > std::chrono::high_resolution_clock::now())
        std::this_thread::sleep_until(finishTime);
    return S_OK;
}

[[nodiscard]] HRESULT LogReplayDeviceComm::WriteOutput(CD_IO_OPERATION* const) const
{
    return S_OK;
}

[[nodiscard]] HRESULT LogReplayDeviceComm::AllowUIAccess() const
{
    return S_OK;
}

[[nodiscard]] ULONG_PTR LogReplayDeviceComm::PutHandle(const void* handle)
{
    auto found{ std::find(_handleTable.begin(), _handleTable.end(), handle) };
    if (found == _handleTable.end())
    {
        // append, get pos
        found = _handleTable.emplace(found, const_cast<void*>(handle));
    }
    return found - _handleTable.begin();
}

[[nodiscard]] void* LogReplayDeviceComm::GetHandle(ULONG_PTR handleId) const
{
    return _handleTable.at(handleId);
}

void LogReplayDeviceComm::DestroyHandle(ULONG_PTR handleId)
{
    auto& pos{ _handleTable.at(handleId) };
    pos = nullptr;
}
