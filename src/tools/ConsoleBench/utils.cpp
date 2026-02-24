#include "pch.h"
#include "utils.h"

#include "arena.h"

BufferedWriter::BufferedWriter(HANDLE out, std::span<char> buffer) :
    m_out{ out },
    m_buffer{ buffer }
{
}

BufferedWriter::~BufferedWriter()
{
    flush();
}

void BufferedWriter::flush()
{
    _write(m_buffer.data(), m_buffer_usage);
    m_buffer_usage = 0;
}

void BufferedWriter::write(std::string_view str)
{
    if (m_buffer_usage + str.size() > m_buffer.size())
    {
        flush();
    }

    if (str.size() >= m_buffer.size())
    {
        _write(str.data(), str.size());
    }
    else
    {
        mem::copy(m_buffer.data() + m_buffer_usage, str.data(), str.size());
        m_buffer_usage += str.size();
    }
}

void BufferedWriter::_write(const void* data, size_t bytes) const
{
    DWORD written;
    THROW_IF_WIN32_BOOL_FALSE(WriteFile(m_out, data, static_cast<DWORD>(bytes), &written, nullptr));
    THROW_HR_IF(E_FAIL, written != bytes);
}

std::string_view get_file_version(mem::Arena& arena, const wchar_t* path)
{
    const auto bytes = GetFileVersionInfoSizeExW(0, path, nullptr);
    if (!bytes)
    {
        return {};
    }

    const auto scratch = mem::get_scratch_arena(arena);
    const auto buffer = scratch.arena.push_uninitialized<char>(bytes);
    if (!GetFileVersionInfoExW(0, path, 0, bytes, buffer))
    {
        return {};
    }

    VS_FIXEDFILEINFO* info;
    UINT varLen = 0;
    if (!VerQueryValueW(buffer, L"\\", reinterpret_cast<void**>(&info), &varLen))
    {
        return {};
    }

    return mem::format(
        arena,
        "%d.%d.%d.%d",
        HIWORD(info->dwFileVersionMS),
        LOWORD(info->dwFileVersionMS),
        HIWORD(info->dwFileVersionLS),
        LOWORD(info->dwFileVersionLS));
}

void set_clipboard(HWND hwnd, std::wstring_view contents)
{
    const auto global = GlobalAlloc(GMEM_MOVEABLE, (contents.size() + 1) * sizeof(wchar_t));
    {
        const auto ptr = static_cast<wchar_t*>(GlobalLock(global));

        mem::copy(ptr, contents.data(), contents.size());
        ptr[contents.size()] = 0;

        GlobalUnlock(global);
    }

    for (DWORD sleep = 10;; sleep *= 2)
    {
        if (OpenClipboard(hwnd))
        {
            break;
        }
        // 10 iterations
        if (sleep > 10000)
        {
            THROW_LAST_ERROR();
        }
        Sleep(sleep);
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, global);
    CloseClipboard();
}
