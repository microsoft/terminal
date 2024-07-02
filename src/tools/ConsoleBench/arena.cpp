#include "pch.h"
#include "arena.h"

using namespace mem;

Arena::Arena(size_t bytes)
{
    m_alloc = static_cast<uint8_t*>(THROW_IF_NULL_ALLOC(VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_READWRITE)));
}

Arena::~Arena()
{
    VirtualFree(m_alloc, 0, MEM_RELEASE);
}

void Arena::clear()
{
    m_pos = 0;
}

size_t Arena::get_pos() const
{
    return m_pos;
}

void Arena::pop_to(size_t pos)
{
    if (m_pos <= pos)
    {
        return;
    }

#ifndef NDEBUG
    memset(m_alloc + pos, 0xDD, m_pos - pos);
#endif

    m_pos = pos;
}

void* Arena::_push_raw(size_t bytes, size_t alignment)
{
    const auto mask = alignment - 1;
    const auto pos = (m_pos + mask) & ~mask;
    const auto pos_new = pos + bytes;
    const auto ptr = m_alloc + pos;

    if (pos_new > m_commit)
    {
        // Commit in 1MB chunks and pre-commit 1MiB in advance.
        const auto commit_new = (pos_new + 0x1FFFFF) & ~0xFFFFF;
        THROW_IF_NULL_ALLOC(VirtualAlloc(m_alloc + m_commit, commit_new - m_commit, MEM_COMMIT, PAGE_READWRITE));
        m_commit = commit_new;
    }

    m_pos = pos_new;
    return ptr;
}

void* Arena::_push_zeroed(size_t bytes, size_t alignment)
{
    const auto ptr = _push_raw(bytes, alignment);
    memset(ptr, 0, bytes);
    return ptr;
}

void* Arena::_push_uninitialized(size_t bytes, size_t alignment)
{
    const auto ptr = _push_raw(bytes, alignment);
#ifndef NDEBUG
    memset(ptr, 0xCD, bytes);
#endif
    return ptr;
}

ScopedArena::ScopedArena(Arena& arena) :
    arena{ arena },
    m_pos_backup{ arena.get_pos() }
{
}

ScopedArena::~ScopedArena()
{
    arena.pop_to(m_pos_backup);
}

static [[msvc::noinline]] std::array<Arena, 2> thread_arenas_init()
{
    return {
        Arena{ 1024 * 1024 * 1024 },
        Arena{ 1024 * 1024 * 1024 },
    };
}

// This is based on an idea publicly described by Ryan Fleury as "scratch arena".
// Assuming you have "persistent" data and "scratch" data, where the former is data that is returned to
// the caller (= upwards) and the latter is data that is used locally, including calls (= downwards).
// The fundamental realisation now is that regular, linear function calls (not coroutines) are sufficiently
// covered with just N+1 arenas, where N is the number of in-flight "persistent" arenas across a call stack.
// Often N is 1, because in most code, there's only 1 arena being passed as a parameter at a time.
// This is also what this code specializes for.
//
// For instance, imagine you call A, which calls B, which calls C, and all 3 of those want to
// return data and also allocate data for themselves, and that you have 2 arenas: 1 and 2.
// Down in C the two arenas now look like this:
//   1: [A (return)][B (local) ][C (return)]
//   2: [A (local) ][B (return)][C (local) ]
//
// Now when each call returns and the arena's position is popped to the state before the call, this
// interleaving ensures that you neither pop local data from, nor return data intended for a parent call.
// After C returns:
//   1: [A (return)][B (local) ][C (return)]
//   2: [A (local) ][B (return)]
// After B returns:
//   1: [A (return)]
//   2: [A (local) ][B (return)]
//   If this step confused you: B() got passed arena 2 from A() and uses arena 1 for local data.
//   When B() returns it restores this local arena to how it was before it used it, which means
//   that both, B's local data and C's return data are deallocated simultaneously. Neat!
// After A returns:
//   1: [A (return)]
//   2:
static ScopedArena get_scratch_arena_impl(const Arena* conflict)
{
    thread_local auto thread_arenas = thread_arenas_init();
    auto& ta = thread_arenas;
    return ScopedArena{ ta[conflict == &ta[0]] };
}

ScopedArena mem::get_scratch_arena()
{
    return get_scratch_arena_impl(nullptr);
}

ScopedArena mem::get_scratch_arena(const Arena& conflict)
{
    return get_scratch_arena_impl(&conflict);
}

#pragma warning(push)
#pragma warning(disable : 4996)

std::string_view mem::format(Arena& arena, const char* fmt, va_list args)
{
    auto len = _vsnprintf(nullptr, 0, fmt, args);
    if (len < 0)
    {
        return {};
    }

    len++;
    const auto buffer = arena.push_uninitialized<char>(len);

    len = _vsnprintf(buffer, len, fmt, args);
    if (len < 0)
    {
        return {};
    }

    return { buffer, static_cast<size_t>(len) };
}

std::string_view mem::format(Arena& arena, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const auto str = format(arena, fmt, args);
    va_end(args);
    return str;
}

std::wstring_view mem::format(Arena& arena, const wchar_t* fmt, va_list args)
{
    auto len = _vsnwprintf(nullptr, 0, fmt, args);
    if (len < 0)
    {
        return {};
    }

    // Make space for a terminating \0 character.
    len++;

    const auto buffer = arena.push_uninitialized<wchar_t>(len);

    len = _vsnwprintf(buffer, len, fmt, args);
    if (len < 0)
    {
        return {};
    }

    return { buffer, static_cast<size_t>(len) };
}

std::wstring_view mem::format(Arena& arena, const wchar_t* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const auto str = format(arena, fmt, args);
    va_end(args);
    return str;
}

#pragma warning(pop)

void mem::print_literal(const char* str)
{
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), str, static_cast<DWORD>(strlen(str)), nullptr, nullptr);
}

// printf() in the uCRT prints every single char individually and thus breaks surrogate
// pairs apart which Windows Terminal treats as invalid input and replaces it with U+FFFD.
void mem::print_format(Arena& arena, const char* fmt, ...)
{
    const auto scratch = get_scratch_arena(arena);

    va_list args;
    va_start(args, fmt);
    const auto str = format(scratch.arena, fmt, args);
    va_end(args);

    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), str.data(), static_cast<DWORD>(str.size()), nullptr, nullptr);
}

std::string_view mem::read_line(Arena& arena, size_t max_bytes)
{
    auto read = static_cast<DWORD>(max_bytes);
    const auto buffer = arena.push_uninitialized<char>(max_bytes);
    if (!ReadConsoleA(GetStdHandle(STD_INPUT_HANDLE), buffer, read, &read, nullptr))
    {
        read = 0;
    }
    return { buffer, read };
}

std::wstring_view mem::u8u16(Arena& arena, const char* ptr, size_t count)
{
    if (count == 0 || count > INT_MAX)
    {
        return {};
    }

    const auto int_count = static_cast<int>(count);
    auto length = MultiByteToWideChar(CP_UTF8, 0, ptr, int_count, nullptr, 0);
    if (length <= 0)
    {
        return {};
    }

    const auto buffer = arena.push_uninitialized<wchar_t>(length);
    length = MultiByteToWideChar(CP_UTF8, 0, ptr, int_count, buffer, length);
    if (length <= 0)
    {
        return {};
    }

    return { buffer, static_cast<size_t>(length) };
}

std::string_view mem::u16u8(Arena& arena, const wchar_t* ptr, size_t count)
{
    if (count == 0 || count > INT_MAX)
    {
        return {};
    }

    const auto int_count = static_cast<int>(count);
    auto length = WideCharToMultiByte(CP_UTF8, 0, ptr, int_count, nullptr, 0, nullptr, nullptr);
    if (length <= 0)
    {
        return {};
    }

    const auto buffer = arena.push_uninitialized<char>(length);
    length = WideCharToMultiByte(CP_UTF8, 0, ptr, int_count, buffer, length, nullptr, nullptr);
    if (length <= 0)
    {
        return {};
    }

    return { buffer, static_cast<size_t>(length) };
}
