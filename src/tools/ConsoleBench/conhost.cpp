#include "pch.h"
#include "conhost.h"

#include <conmsgl1.h>
#include <winternl.h>
#include <wil/win32_helpers.h>

#include "arena.h"

static unique_nthandle conhostCreateHandle(HANDLE parent, const wchar_t* typeName, bool inherit, bool synchronous)
{
    UNICODE_STRING name;
    RtlInitUnicodeString(&name, typeName);

    ULONG attrFlags = OBJ_CASE_INSENSITIVE;
    WI_SetFlagIf(attrFlags, OBJ_INHERIT, inherit);

    OBJECT_ATTRIBUTES attr;
    InitializeObjectAttributes(&attr, &name, attrFlags, parent, nullptr);

    ULONG options = 0;
    WI_SetFlagIf(options, FILE_SYNCHRONOUS_IO_NONALERT, synchronous);

    HANDLE handle;
    IO_STATUS_BLOCK ioStatus;
    THROW_IF_NTSTATUS_FAILED(NtCreateFile(
        /* FileHandle        */ &handle,
        /* DesiredAccess     */ FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        /* ObjectAttributes  */ &attr,
        /* IoStatusBlock     */ &ioStatus,
        /* AllocationSize    */ nullptr,
        /* FileAttributes    */ 0,
        /* ShareAccess       */ FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        /* CreateDisposition */ FILE_CREATE,
        /* CreateOptions     */ options,
        /* EaBuffer          */ nullptr,
        /* EaLength          */ 0));
    return unique_nthandle{ handle };
}

static void conhostCopyToStringBuffer(USHORT& length, auto& buffer, const wchar_t* str)
{
    const auto len = wcsnlen(str, sizeof(buffer) / sizeof(wchar_t)) * sizeof(wchar_t);
    length = static_cast<USHORT>(len);
    memcpy(buffer, str, len);
}

ConhostHandle spawn_conhost(mem::Arena& arena, const wchar_t* path)
{
    const auto pathLen = wcslen(path);
    const auto isDLL = pathLen > 4 && wcscmp(&path[pathLen - 4], L".dll") == 0;

    const auto scratch = mem::get_scratch_arena(arena);
    auto server = conhostCreateHandle(nullptr, L"\\Device\\ConDrv\\Server", true, false);
    auto reference = conhostCreateHandle(server.get(), L"\\Reference", false, true);

    {
        const auto selfPath = scratch.arena.push_uninitialized<wchar_t>(64 * 1024);
        GetModuleFileNameW(nullptr, selfPath, 64 * 1024);

        std::wstring_view cmd;

        if (isDLL)
        {
            cmd = format(scratch.arena, LR"("%s" host %zx "%s")", selfPath, server.get(), path);
        }
        else
        {
            cmd = format(scratch.arena, LR"("%s" --server 0x%zx)", path, server.get());
        }

        uint8_t attrListBuffer[64];

        STARTUPINFOEXW siEx{};
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(&attrListBuffer[0]);

        HANDLE inheritedHandles[1];
        inheritedHandles[0] = server.get();

        auto listSize = sizeof(attrListBuffer);
        THROW_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &listSize));
        const auto cleanupProcThreadAttribute = wil::scope_exit([&]() noexcept {
            DeleteProcThreadAttributeList(siEx.lpAttributeList);
        });

        THROW_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(
            siEx.lpAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
            &inheritedHandles[0],
            sizeof(inheritedHandles),
            nullptr,
            nullptr));

        wil::unique_process_information pi;
        THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(
            /* lpApplicationName    */ nullptr,
            /* lpCommandLine        */ const_cast<wchar_t*>(cmd.data()),
            /* lpProcessAttributes  */ nullptr,
            /* lpThreadAttributes   */ nullptr,
            /* bInheritHandles      */ TRUE,
            /* dwCreationFlags      */ EXTENDED_STARTUPINFO_PRESENT,
            /* lpEnvironment        */ nullptr,
            /* lpCurrentDirectory   */ nullptr,
            /* lpStartupInfo        */ &siEx.StartupInfo,
            /* lpProcessInformation */ pi.addressof()));
    }

    unique_nthandle connection;
    {
        UNICODE_STRING name;
        RtlInitUnicodeString(&name, L"\\Connect");

        OBJECT_ATTRIBUTES attr;
        InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE, reference.get(), nullptr);

        CONSOLE_SERVER_MSG msg{};
        {
            // This must be RTL_USER_PROCESS_PARAMETERS::ProcessGroupId unless it's 0,
            // but winternl doesn't know about that field. ;)
            msg.ProcessGroupId = GetCurrentProcessId();
            msg.ConsoleApp = TRUE;
            msg.WindowVisible = TRUE;

            conhostCopyToStringBuffer(msg.TitleLength, msg.Title, L"ConsoleBench.exe");
            conhostCopyToStringBuffer(msg.ApplicationNameLength, msg.ApplicationName, L"ConsoleBench.exe");
            conhostCopyToStringBuffer(msg.CurrentDirectoryLength, msg.CurrentDirectory, L"C:\\Windows\\System32\\");
        }

        // From wdm.h, but without the trailing `CHAR EaName[1];` field, as this makes
        // appending the string at the end of the messages unnecessarily difficult.
        struct FILE_FULL_EA_INFORMATION
        {
            ULONG NextEntryOffset;
            UCHAR Flags;
            UCHAR EaNameLength;
            USHORT EaValueLength;
        };
        static constexpr auto bufferAppend = [](uint8_t* out, const auto& in) {
            return static_cast<uint8_t*>(memcpy(out, &in, sizeof(in))) + sizeof(in);
        };

        alignas(__STDCPP_DEFAULT_NEW_ALIGNMENT__) uint8_t eaBuffer[2048];
        auto eaBufferEnd = &eaBuffer[0];
        // Curiously, EaNameLength is the length without \0,
        // but the data payload only starts after the name *with* \0.
        eaBufferEnd = bufferAppend(eaBufferEnd, FILE_FULL_EA_INFORMATION{ .EaNameLength = 6, .EaValueLength = sizeof(msg) });
        eaBufferEnd = bufferAppend(eaBufferEnd, "server");
        eaBufferEnd = bufferAppend(eaBufferEnd, msg);

        IO_STATUS_BLOCK ioStatus;
        THROW_IF_NTSTATUS_FAILED(NtCreateFile(
            /* FileHandle        */ connection.addressof(),
            /* DesiredAccess     */ FILE_GENERIC_READ | FILE_GENERIC_WRITE,
            /* ObjectAttributes  */ &attr,
            /* IoStatusBlock     */ &ioStatus,
            /* AllocationSize    */ nullptr,
            /* FileAttributes    */ 0,
            /* ShareAccess       */ FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            /* CreateDisposition */ FILE_CREATE,
            /* CreateOptions     */ FILE_SYNCHRONOUS_IO_NONALERT,
            /* EaBuffer          */ &eaBuffer[0],
            /* EaLength          */ static_cast<ULONG>(eaBufferEnd - &eaBuffer[0])));
    }

    return ConhostHandle{
        .reference = std::move(reference),
        .connection = std::move(connection),
    };
}

// A continuation of spawn_conhost().
void check_spawn_conhost_dll(int argc, const wchar_t* argv[])
{
    if (argc == 4 && wcscmp(argv[1], L"host") == 0)
    {
        const auto serverHandle = reinterpret_cast<HANDLE>(wcstoull(argv[2], nullptr, 16));
        const auto path = argv[3];

        using Entrypoint = NTSTATUS(NTAPI*)(HANDLE);
        const auto h = THROW_LAST_ERROR_IF_NULL(LoadLibraryExW(path, nullptr, 0));
        const auto f = THROW_LAST_ERROR_IF_NULL(reinterpret_cast<Entrypoint>(GetProcAddress(h, "ConsoleCreateIoThread")));
        THROW_IF_NTSTATUS_FAILED(f(serverHandle));
        ExitThread(S_OK);
    }
}

HANDLE get_active_connection()
{
    // (Not actually) FUN FACT! The handles don't mean anything and the cake is a lie!
    // Whenever you call any console API function, the handle you pass is completely and entirely ignored.
    // Instead, condrv will probe the PEB, extract the ConsoleHandle field and use that to send
    // the message to the server (conhost). ConsoleHandle happens to be at Reserved2[0].
    // Unfortunately, the reason for this horrifying approach has been lost to time.
    return NtCurrentTeb()->ProcessEnvironmentBlock->ProcessParameters->Reserved2[0];
}

void set_active_connection(HANDLE connection)
{
    NtCurrentTeb()->ProcessEnvironmentBlock->ProcessParameters->Reserved2[0] = connection;
}
