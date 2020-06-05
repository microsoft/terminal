//
// fuzzing_logic.h
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// The directed fuzzing logic header.  Must be paired together with fuzzing_directed.h.
//
#ifndef __FUZZING_LOGIC_H__
#define __FUZZING_LOGIC_H__

#pragma once

#include "fuzzing_directed.h"
#include "string_helper.h"
#ifdef __GENERATE_DIRECTED_FUZZING
#include <strsafe.h>

namespace fuzz
{
    //
    // Fuzzing manipulations follow the following name conventions:
    //  _[fz/const]_[type]_[description]
    //    [fz/const] : separates randomized actions versus those that produce a constant effect
    //    [type] : the type of data that is being fuzzed, abbreviated by its hungarian notation
    //    [description] : describe the manipulation that is being applied
    //

    //
    // The string fuzzing functions are designed to allow the caller to determine if NULL
    // termination is necessary.  For an example of the appropriate way to call any of the
    // _fz_wsz_* functions, see FuzzStringW and FuzzStringW_NoRealloc.
    //

    // Inserts a format character to a random location within the string.
    // Note that rcch is the count of characters (minus the NULL terminator), not the count of bytes.
    static LPWSTR _fz_wsz_addFormatChar(__inout_ecount(rcch) WCHAR* pwsz, __inout size_t& rcch)
    {
        if (rcch > 1)
        {
            const LPWSTR rgFormatStringChars[] = { L"%n", L"%s", L"%d" };
            size_t cbDestSize = 2 * sizeof(WCHAR);
            memcpy_s(
                &(pwsz[CFuzzChance::GetRandom<size_t>(rcch - 1)]), // -1 because we are writing 2 chars
                cbDestSize,
                CFuzzChance::SelectOne<const LPWSTR>(rgFormatStringChars, ARRAYSIZE(rgFormatStringChars)),
                cbDestSize);
        }

        return pwsz;
    }

    // Inserts a format character to a random location within the string.
    // Note that rcch does not include the NULL terminator
    static LPSTR _fz_sz_addFormatChar(__inout_ecount(rcch) CHAR* psz, __inout size_t& rcch)
    {
        if (rcch > 1)
        {
            const LPSTR rgFormatStringChars[] = { "%n", "%s", "%d" };
            size_t cbDestSize = 2 * sizeof(CHAR);
            memcpy_s(
                &(psz[CFuzzChance::GetRandom<size_t>(rcch - 1)]),
                cbDestSize,
                CFuzzChance::SelectOne<const LPSTR>(rgFormatStringChars, ARRAYSIZE(rgFormatStringChars)),
                cbDestSize);
        }

        return psz;
    }

    // Adds a character related to paths to a random location within the string.
    // Note that rcch is the count of characters (minus the NULL terminator), not the count of bytes.
    static LPWSTR _fz_wsz_addPathChar(__inout_ecount(rcch) WCHAR* pwsz, __inout size_t& rcch)
    {
        if (rcch > 0)
        {
            const WCHAR rgPathChars[] = { L'.', L'\\', L'/', L':', L',', L';' };
            pwsz[CFuzzChance::GetRandom<size_t>(rcch)] =
                CFuzzChance::SelectOne<const WCHAR>(rgPathChars, ARRAYSIZE(rgPathChars));
        }

        return pwsz;
    }

    // Adds a character related to paths to a random location within the string.
    // Note that rcch does not include the NULL terminator
    static LPSTR _fz_sz_addPathChar(__inout_ecount(rcch) CHAR* psz, __inout size_t& rcch)
    {
        if (rcch > 0)
        {
            const CHAR rgPathChars[] = { '.', '\\', '/', ':', ',', ';' };
            psz[CFuzzChance::GetRandom<size_t>(rcch)] =
                CFuzzChance::SelectOne<const CHAR>(rgPathChars, ARRAYSIZE(rgPathChars));
        }

        return psz;
    }

    // Adds an invalid path character to a random location within the string.
    // Note that rcch is the count of characters (minus the NULL terminator), not the count of bytes.
    static LPWSTR _fz_wsz_addInvalidPathChar(__inout_ecount(rcch) WCHAR* pwsz, __inout size_t& rcch)
    {
        if (rcch > 0)
        {
            const WCHAR rgInvalidPathChars[] = { L'?', L'<', L'>', L'"', L'|', L'*' };
            pwsz[CFuzzChance::GetRandom<size_t>(rcch)] =
                CFuzzChance::SelectOne<const WCHAR>(rgInvalidPathChars, ARRAYSIZE(rgInvalidPathChars));
        }

        return pwsz;
    }

    // Adds an invalid path character to a random location within the string.
    // Note that rcch does not include the NULL terminator
    static LPSTR _fz_sz_addInvalidPathChar(__inout_ecount(rcch) CHAR* psz, __inout size_t& rcch)
    {
        if (rcch > 0)
        {
            const CHAR rgInvalidPathChars[] = { '?', '<', '>', '"', '|', '*' };
            psz[CFuzzChance::GetRandom<size_t>(rcch)] =
                CFuzzChance::SelectOne<const CHAR>(rgInvalidPathChars, ARRAYSIZE(rgInvalidPathChars));
        }

        return psz;
    }

    // Implementation depends on CFuzzLogic class and is therefore
    // declared after CFuzzLogic is defined below.
    template<typename _Type>
    static _Type* _fz_flipBYTE(__inout_ecount(rcelms) _Type* p, __inout size_t& rcelms);
    template<typename _Type>
    static _Type* _fz_flipWCHAR(__inout_ecount(rcelms) _Type* p, __inout size_t& rcelms);
    static char* _fz_sz_tokenizeSpaces(__in char* psz);

    // Mirrors the first half of the string across the second half of the string.
    // Note that rcch is the count of characters (minus the NULL terminator), not the count of bytes.
    static LPWSTR _const_wsz_mirror(__inout_ecount(rcch) WCHAR* pwsz, __inout size_t& rcch)
    {
        if (rcch > 0)
        {
            size_t cchStart = 0;
            size_t cchEnd = rcch - 1;
            while (cchStart < cchEnd)
            {
                pwsz[cchEnd--] = pwsz[cchStart++];
            }
        }

        return pwsz;
    }

    // Mirrors the first half of the string across the second half of the string.
    // Note that rcch does not include the NULL terminator
    static LPSTR _const_sz_mirror(__inout_ecount(rcch) CHAR* psz, __inout size_t& rcch)
    {
        if (rcch > 0)
        {
            size_t cchStart = 0;
            size_t cchEnd = rcch - 1;
            while (cchStart < cchEnd)
            {
                psz[cchEnd--] = psz[cchStart++];
            }
        }

        return psz;
    }

    // Replicates the string repeatedly until the end of the buffer is reached.
    // Note that rcch is the count of characters (minus the NULL terminator), not the count of bytes.
    static LPWSTR _const_wsz_replicate(__inout_ecount(rcch) WCHAR* pwsz, __inout size_t& rcch)
    {
        if (rcch > 0)
        {
            size_t cch = wcslen(pwsz);
            size_t cchStart = 0;
            while (cch < rcch)
            {
                pwsz[cch++] = pwsz[cchStart++];
            }
        }

        return pwsz;
    }

    // Replicates the string repeatedly until the end of the buffer is reached.
    // Note that rcch does not include the NULL terminator
    static LPSTR _const_sz_replicate(__inout_ecount(rcch) CHAR* psz, __inout size_t& rcch)
    {
        if (rcch > 0)
        {
            size_t cch = strlen(psz);
            size_t cchStart = 0;
            while (cch < rcch)
            {
                psz[cch++] = psz[cchStart++];
            }
        }

        return psz;
    }

    // Replaces the string with a valid system path to shell32.dll in the system32 dir.
    // Note that rcch is the count of characters (minus the NULL terminator), not the count of bytes.
    static LPWSTR _const_wsz_validPath(__inout_ecount(rcch) WCHAR* pwsz, __inout size_t& rcch)
    {
        WCHAR wszSystemDirectory[MAX_PATH] = { 0 };
        if (GetSystemDirectoryW(wszSystemDirectory, ARRAYSIZE(wszSystemDirectory)))
        {
            StringCchPrintfW(pwsz, rcch, L"%s\\shell32.dll", wszSystemDirectory);
        }

        return pwsz;
    }

    // Replaces the string with a valid system path to shell32.dll in the system32 dir.
    // Note that rcch does not include the NULL terminator
    static LPSTR _const_sz_validPath(__inout_ecount(rcch) CHAR* psz, __inout size_t& rcch)
    {
        CHAR szSystemDirectory[MAX_PATH] = { 0 };
        if (GetSystemDirectoryA(szSystemDirectory, ARRAYSIZE(szSystemDirectory)))
        {
            StringCchPrintfA(psz, rcch, "%s\\shell32.dll", szSystemDirectory);
        }

        return psz;
    }

    // Reverses the string in place.
    static LPWSTR _const_wsz_reverse(__inout WCHAR* pwsz, __inout size_t&)
    {
        return _wcsrev(pwsz);
    }

    // Reverses the string in place.
    static LPSTR _const_sz_reverse(__inout CHAR* psz, __inout size_t&)
    {
        return _strrev(psz);
    }

    // Contains fuzzing logic based upon a variety of default scenarios.  The
    // idea is to capture and make available a comprehensive fuzzing library
    // that does not require external modules or complex setup.  This should
    // make fuzzing easier to implement and test, as well as more explicit
    // with regard to what fuzzing manipulations are possible.
    template<class _Alloc = CFuzzCRTAllocator>
    class CFuzzLogic
    {
    public:
        // Permutes a random element of the array with a valid value that can be
        // contained within the size of a single element.  See _fz_wsz_flipBYTE
        // and _fz_wsz_flipWCHAR for an example of how the element size determines
        // the amount of data manipulated.
        template<typename _Type>
        static _Type* FuzzArrayElement(__in_ecount(cElems) _Type* rg, __in size_t cElems) throw()
        {
            if (rg && cElems)
            {
                rg[CFuzzChance::GetRandom<size_t>(cElems)] =
                    static_cast<_Type>(CFuzzChance::GetRandom<size_t>(
                        static_cast<size_t>(pow(static_cast<double>(2), static_cast<double>(sizeof(_Type) * 8)))));
            }

            return rg;
        }

        // Fuzzes a string by allocating a new fuzzed string.  Note that the string
        // length can shrink or grow in relation to the template data passed in
        // via pwsz.  The maximum size the string can grow is 2 times the current
        // length.
        static LPWSTR FuzzStringW(__in LPCWSTR pwsz) throw()
        {
            const _fuzz_type_entry<size_t> rgfte[] = {
                { 10, [](size_t cch) { return CFuzzChance::GetRandom<size_t>(cch + 1); } },
                { 50, [](size_t cch) { return cch + CFuzzChance::GetRandom<size_t>(cch + 1); } }
            };
            CFuzzType<size_t> fuzz_cb(FUZZ_MAP(rgfte), wcslen(pwsz));

            size_t cch = fuzz_cb + 1; // add 1 for ensuring NULL termination
            LPWSTR pwszRealloc = reinterpret_cast<LPWSTR>(_Alloc::Allocate(cch * sizeof(WCHAR)));
            if (pwszRealloc)
            {
                pwszRealloc[--cch] = L'\0';
                StringCchCopyW(pwszRealloc, cch, pwsz);
                FuzzStringW_NoRealloc(pwszRealloc, cch);
            }

            return pwszRealloc;
        }

        // Fuzzes a string by allocating a new fuzzed string.  Note that the string
        // length can shrink or grow in relation to the template data passed in
        // via pwsz.  The maximum size the string can grow is 2 times the current
        // length.
        static LPSTR FuzzStringA(__in LPCSTR psz) throw()
        {
            LPSTR pszRealloc = nullptr;

            const _fuzz_type_entry<size_t> rgfte[] = {
                { 10, [](size_t cch) { return CFuzzChance::GetRandom<size_t>(cch + 1); } },
                { 50, [](size_t cch) { return cch + CFuzzChance::GetRandom<size_t>(cch + 1); } }
            };
            CFuzzType<size_t> fuzz_cch(FUZZ_MAP(rgfte), strlen(psz));

            size_t cchTemp = fuzz_cch + 1; // add 1 for ensuring NULL termination
            LPSTR pszReallocTemp = reinterpret_cast<LPSTR>(_Alloc::Allocate(cchTemp * sizeof(CHAR)));
            if (pszReallocTemp)
            {
                pszReallocTemp[--cchTemp] = '\0';
                StringCchCopyA(pszReallocTemp, cchTemp, psz);

                const _fuzz_type_entry<LPSTR> fuzzMap[] = {
                    { 5, _fz_sz_tokenizeSpaces, FreeFuzzedBuffer },
                    { 95, [=](LPSTR p) {
                         size_t cchInner = cchTemp;
                         return FuzzStringA_NoRealloc(p, cchInner);
                     } }
                };
                CFuzzString<__FUZZING_ALLOCATOR, CHAR> eval(FUZZ_MAP(fuzzMap), pszReallocTemp);

                // Performance optimization: 95% of the time the buffer returned from eval will not be reallocated
                // and thus will still be pointing at pszReallocTemp.  In this case, it is not necessary to duplicate
                // the string a second time, just transfer ownership to the calling function.
                if ((LPSTR)eval == pszReallocTemp)
                {
                    pszRealloc = pszReallocTemp;
                }
                else
                {
                    pszRealloc = DuplicateStringA(eval);
                    FreeFuzzedBuffer(pszReallocTemp);
                }
            }

            return pszRealloc;
        }

        // Fuzzes a string in place, no new memory is allocated to perform this
        // fuzzing.  This means that the return value is the same as the pwsz
        // parameter.
        static LPWSTR FuzzStringW_NoRealloc(__inout LPWSTR pwsz) throw()
        {
            size_t cch = wcslen(pwsz);
            return FuzzStringW_NoRealloc(pwsz, cch);
        }

        // Fuzzes a string in place, no new memory is allocated to perform this
        // fuzzing.  This means that the return value is the same as the psz
        // parameter.
        static LPSTR FuzzStringA_NoRealloc(__inout LPSTR psz) throw()
        {
            size_t cch = strlen(psz);
            return FuzzStringA_NoRealloc(psz, cch);
        }

        static LPSTR DuplicateStringA(__in LPCSTR psz) throw()
        {
            size_t cch = strlen(psz) + 1;
            LPSTR pszDuplicate = reinterpret_cast<LPSTR>(_Alloc::Allocate(cch * sizeof(CHAR)));
            if (pszDuplicate)
            {
                StringCchCopyA(pszDuplicate, cch, psz);
            }

            return pszDuplicate;
        }

        // This function will free any allocations that are made as part of this
        // class.  Specifically, fuzzed strings created with FuzzStringW should
        // always be freed with FreeFuzzedBuffer.  The prototype of this function
        // should always support being used within a fuzz array entry or a fuzz
        // type entry as the pfnDealloc function.
        static void FreeFuzzedBuffer(void* pv) throw()
        {
            _Alloc::Free(pv);
        }

    private:
        CFuzzLogic(){};
        virtual ~CFuzzLogic(){};

        static LPWSTR FuzzStringW_NoRealloc(__inout LPWSTR pwsz, __inout size_t& rcch)
        {
            if (rcch > 0)
            {
                const _fuzz_array_entry<WCHAR, size_t> rgfae[] = {
                    // small randomized manipulations
                    { 21, _fz_wsz_addFormatChar },
                    { 21, _fz_wsz_addPathChar },
                    { 21, _fz_wsz_addInvalidPathChar },
                    { 11, [](WCHAR* pwsz, size_t& rcch) { return _fz_flipByte(pwsz, rcch); } },
                    { 10, [](WCHAR* pwsz, size_t& rcch) { return _fz_flipEntry(pwsz, rcch); } },

                    // non-random manipulations
                    { 4, _const_wsz_replicate },
                    { 4, _const_wsz_mirror },
                    { 4, _const_wsz_validPath },
                    { 4, _const_wsz_reverse }
                };
                CFuzzArray<__FUZZING_ALLOCATOR, WCHAR, size_t> fa(FUZZ_MAP(rgfae), pwsz, rcch);
                fa.GetValueFromMap();
            }

            return pwsz;
        }

        static LPSTR FuzzStringA_NoRealloc(__inout LPSTR psz, __inout size_t& rcch)
        {
            if (rcch > 0)
            {
                const _fuzz_array_entry<CHAR, size_t> rgfae[] = {
                    // small randomized manipulations
                    { 21, _fz_sz_addFormatChar },
                    { 21, _fz_sz_addPathChar },
                    { 21, _fz_sz_addInvalidPathChar },
                    { 21, [](CHAR* psz, size_t& rcch) { return _fz_flipByte<CHAR>(psz, rcch); } },

                    // non-random manipulations
                    { 4, _const_sz_replicate },
                    { 4, _const_sz_mirror },
                    { 4, _const_sz_validPath },
                    { 4, _const_sz_reverse }
                };
                CFuzzArray<__FUZZING_ALLOCATOR, CHAR, size_t> fa(FUZZ_MAP(rgfae), psz, rcch);
                fa.GetValueFromMap();
            }

            return psz;
        }
    };

    // Flips a random byte value within the buffer.
    template<typename _Type>
    static _Type* _fz_flipByte(__inout_ecount(rcelms) _Type* p, __inout size_t& rcelms)
    {
        if (rcelms > 0)
        {
            return reinterpret_cast<_Type*>(CFuzzLogic<>::FuzzArrayElement(
                reinterpret_cast<BYTE*>(p), (rcelms) * sizeof(_Type)));
        }

        return p;
    }

    // Flips a random entry value within the buffer
    template<typename _Type>
    static _Type* _fz_flipEntry(__inout_ecount(rcelms) _Type* p, __inout size_t& rcelms)
    {
        if (rcelms > 0)
        {
            return CFuzzLogic<>::FuzzArrayElement(p, rcelms);
        }

        return p;
    }

    static char* _fz_sz_tokenizeSpaces(__in char* psz)
    {
        const _fuzz_type_entry<DWORD> repeatMap[] = {
            { 10, [](DWORD) { return 0; } },
            { 10, [](DWORD) { return 2; } },
            { 1, [](DWORD) { return CFuzzChance::GetRandom<DWORD>(0xF); } }
        };

        std::string sFuzzed;
        char* next_token = nullptr;
        char* token = strtok_s(psz, " ", &next_token);
        while (token)
        {
            CFuzzType<DWORD> repeat(FUZZ_MAP(repeatMap), 1);
            for (DWORD i = 0; i < (DWORD)repeat; i++)
            {
                sFuzzed += token;
                sFuzzed += " ";
            }

            token = strtok_s(nullptr, " ", &next_token);
        }

        // If psz has a final trailing space, avoid trimming it away.  Otherwise, remove
        // the extra added final space appended via the loop above.
        size_t cch = strlen(psz);
        if (psz[cch] == ' ')
            TrimRight(sFuzzed, ' ');
        return CFuzzLogic<>::DuplicateStringA(sFuzzed.c_str());
    }
}
#endif

#endif
