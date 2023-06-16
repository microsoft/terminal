// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifdef NODEFAULTLIB

#include <intrin.h>

#pragma function(memcpy)
void* memcpy(void* dst, const void* src, size_t size)
{
    __movsb(static_cast<BYTE*>(dst), static_cast<const BYTE*>(src), size);
    return dst;
}

#pragma function(memset)
void* memset(void* dst, int val, size_t size)
{
    __stosb(static_cast<BYTE*>(dst), static_cast<BYTE>(val), size);
    return dst;
}

#endif

namespace ucrt
{
    // Copyright (c) Microsoft Corporation. All rights reserved.
    //
    // This is a copy of "startup/argv_parsing.cpp" from the Windows SDK (10.0.22000.0).
    // The source code was slightly modified to fit this code style.

    template<typename Character>
    static bool should_copy_another_character(Character) noexcept
    {
        return false;
    }

    template<typename Character>
    static void __cdecl parse_command_line(Character* cmdstart, Character** argv, Character* args, size_t* argument_count, size_t* character_count) noexcept
    {
        *character_count = 0;
        *argument_count = 1; // We'll have at least the program name

        Character c;
        int copy_character; /* 1 = copy char to *args */
        unsigned numslash; /* num of backslashes seen */

        /* first scan the program name, copy it, and count the bytes */
        Character* p = cmdstart;
        if (argv)
            *argv++ = args;

        // A quoted program name is handled here. The handling is much
        // simpler than for other arguments. Basically, whatever lies
        // between the leading double-quote and next one, or a terminal null
        // character is simply accepted. Fancier handling is not required
        // because the program name must be a legal NTFS/HPFS file name.
        // Note that the double-quote characters are not copied, nor do they
        // contribute to character_count.
        bool in_quotes = false;
        do
        {
            if (*p == '"')
            {
                in_quotes = !in_quotes;
                c = *p++;
                continue;
            }

            ++*character_count;
            if (args)
                *args++ = *p;

            c = *p++;

            if (should_copy_another_character(c))
            {
                ++*character_count;
                if (args)
                    *args++ = *p; // Copy 2nd byte too
                ++p; // skip over trail byte
            }
        } while (c != '\0' && (in_quotes || (c != ' ' && c != '\t')));

        if (c == '\0')
        {
            p--;
        }
        else
        {
            if (args)
                *(args - 1) = '\0';
        }

        in_quotes = false;

        // Loop on each argument
        for (;;)
        {
            if (*p)
            {
                while (*p == ' ' || *p == '\t')
                    ++p;
            }

            if (*p == '\0')
                break; // End of arguments

            // Scan an argument:
            if (argv)
                *argv++ = args;

            ++*argument_count;

            // Loop through scanning one argument:
            for (;;)
            {
                copy_character = 1;

                // Rules:
                // 2N     backslashes   + " ==> N backslashes and begin/end quote
                // 2N + 1 backslashes   + " ==> N backslashes + literal "
                // N      backslashes       ==> N backslashes
                numslash = 0;

                while (*p == '\\')
                {
                    // Count number of backslashes for use below
                    ++p;
                    ++numslash;
                }

                if (*p == '"')
                {
                    // if 2N backslashes before, start/end quote, otherwise
                    // copy literally:
                    if (numslash % 2 == 0)
                    {
                        if (in_quotes && p[1] == '"')
                        {
                            p++; // Double quote inside quoted string
                        }
                        else
                        {
                            // Skip first quote char and copy second:
                            copy_character = 0; // Don't copy quote
                            in_quotes = !in_quotes;
                        }
                    }

                    numslash /= 2;
                }

                // Copy slashes:
                while (numslash--)
                {
                    if (args)
                        *args++ = '\\';
                    ++*character_count;
                }

                // If at end of arg, break loop:
                if (*p == '\0' || (!in_quotes && (*p == ' ' || *p == '\t')))
                    break;

                // Copy character into argument:
                if (copy_character)
                {
                    if (args)
                        *args++ = *p;

                    if (should_copy_another_character(*p))
                    {
                        ++p;
                        ++*character_count;

                        if (args)
                            *args++ = *p;
                    }

                    ++*character_count;
                }

                ++p;
            }

            // Null-terminate the argument:
            if (args)
                *args++ = '\0'; // Terminate the string

            ++*character_count;
        }

        // We put one last argument in -- a null pointer:
        if (argv)
            *argv++ = nullptr;

        ++*argument_count;
    }

    char* acrt_allocate_buffer_for_argv(size_t const argument_count, size_t const character_count, size_t const character_size)
    {
        const size_t argument_array_size = argument_count * sizeof(void*);
        const size_t character_array_size = character_count * character_size;
        const size_t total_size = argument_array_size + character_array_size;
        return reinterpret_cast<char*>(VirtualAlloc(nullptr, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    }

    static void common_configure_argv(const char*** argv, int* argc)
    {
        const auto command_line = GetCommandLineA();

        size_t argument_count = 0;
        size_t character_count = 0;
        ucrt::parse_command_line<char>(command_line, nullptr, nullptr, &argument_count, &character_count);

        const auto buffer = ucrt::acrt_allocate_buffer_for_argv(argument_count, character_count, sizeof(char));
        const auto first_argument = reinterpret_cast<char**>(buffer);
        const auto first_string = reinterpret_cast<char*>(buffer + argument_count * sizeof(char*));
        ucrt::parse_command_line(command_line, first_argument, first_string, &argument_count, &character_count);

        *argv = const_cast<const char**>(first_argument);
        *argc = static_cast<int>(argument_count - 1);
    }
} // namespace ucrt
