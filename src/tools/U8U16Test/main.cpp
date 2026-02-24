// TEST TOOL U8U16Test
// Performance tests for UTF-8 <--> UTF-16 conversions, related to PR #4093
// NOTE The functions u8u16 and u16u8 contain own algorithms. Tests have shown that they perform
// worse than the platform API functions.
// Thus, these functions are *unrelated* to the til::u8u16 and til::u16u8 implementation.

#include <iostream>
#include <memory>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>

#include "U8U16Test.hpp"

typedef NTSTATUS(WINAPI* t_RtlUTF8ToUnicodeN)(PWSTR, ULONG, PULONG, PCCH, ULONG);
typedef NTSTATUS(WINAPI* t_RtlUnicodeToUTF8N)(PCHAR, ULONG, PULONG, PCWSTR, ULONG);
NTSTATUS(WINAPI* p_RtlUTF8ToUnicodeN)
(
    _Out_ PWSTR UnicodeStringDestination,
    _In_ ULONG UnicodeStringMaxByteCount,
    _Out_opt_ PULONG UnicodeStringActualByteCount,
    _In_ PCCH UTF8StringSource,
    _In_ ULONG UTF8StringByteCount){};
NTSTATUS(WINAPI* p_RtlUnicodeToUTF8N)
(
    _Out_ PCHAR UTF8StringDestination,
    _In_ ULONG UTF8StringMaxByteCount,
    _Out_opt_ PULONG UTF8StringActualByteCount,
    _In_ PCWSTR UnicodeStringSource,
    _In_ ULONG UnicodeStringWCharCount){};

// helper functions
double GetDuration();
ptrdiff_t RandomIndex(ptrdiff_t length);
void PrintHeader(const char* const funcName);

// test functions
void WideCharToMultiByte_WholeString(std::wstring_view testU16)
{
    PrintHeader(__func__);
    GetDuration();
    std::unique_ptr<char[]> u8Buffer{ std::make_unique<char[]>(testU16.length() * 3) };
    const int length = WideCharToMultiByte(65001, 0, testU16.data(), static_cast<int>(testU16.length()), u8Buffer.get(), static_cast<int>(testU16.length()) * 3, nullptr, nullptr);
    const double duration = GetDuration();
    const char randElem8 = u8Buffer[RandomIndex(static_cast<ptrdiff_t>(length))];
    u8Buffer.reset();
    std::cout << " ignore me " << static_cast<int>(static_cast<unsigned char>(randElem8))
              << "\n length " << length << "\n elapsed " << duration << std::endl;
}

void RtlUnicodeToUTF8N_WholeString(std::wstring_view testU16)
{
    PrintHeader(__func__);
    ULONG written{};
    GetDuration();
    std::unique_ptr<char[]> u8Buffer{ std::make_unique<char[]>(testU16.length() * 3) };
    const NTSTATUS status = p_RtlUnicodeToUTF8N(u8Buffer.get(), static_cast<ULONG>(testU16.length()) * 3, &written, testU16.data(), static_cast<ULONG>(testU16.length() * 2));
    const double duration = GetDuration();
    const char randElem8 = u8Buffer[RandomIndex(static_cast<ptrdiff_t>(written))];
    u8Buffer.reset();
    std::cout << " ignore me " << static_cast<int>(static_cast<unsigned char>(randElem8))
              << "\n NTSTATUS " << status << "\n length " << written << "\n elapsed " << duration << std::endl;
}

void u16u8_WholeString(std::wstring_view testU16, std::string& u8Str)
{
    PrintHeader(__func__);
    GetDuration();
    const HRESULT hRes = u16u8(testU16, u8Str);
    const double duration = GetDuration();
    const char randElem8 = u8Str.at(RandomIndex(static_cast<ptrdiff_t>(u8Str.length())));
    std::cout << " ignore me " << static_cast<int>(static_cast<unsigned char>(randElem8))
              << "\n HRESULT " << hRes << "\n length " << u8Str.length() << "\n elapsed " << duration << std::endl;
}

void u16u8_ptr_WholeString(std::wstring_view testU16, std::string& u8Str)
{
    PrintHeader(__func__);
    GetDuration();
    const HRESULT hRes = u16u8_ptr(testU16, u8Str);
    const double duration = GetDuration();
    const char randElem8 = u8Str.at(RandomIndex(static_cast<ptrdiff_t>(u8Str.length())));
    std::cout << " ignore me " << static_cast<int>(static_cast<unsigned char>(randElem8))
              << "\n HRESULT " << hRes << "\n length " << u8Str.length() << "\n elapsed " << duration << std::endl;
}

void WideCharToMultiByte_Chunks(std::wstring_view testU16, size_t u8CharLen, size_t chunkLen)
{
    PrintHeader(__func__);
    const size_t endLoop{ testU16.length() / chunkLen };
    double duration{};
    GetDuration();
    std::unique_ptr<char[]> u8Buffer{ std::make_unique<char[]>(chunkLen * u8CharLen) };
    duration += GetDuration();
    int length{};

    for (size_t i{}; i < endLoop; ++i)
    {
        const std::wstring_view sv{ &testU16.at(i), chunkLen };
        GetDuration();
        length += WideCharToMultiByte(65001, 0, sv.data(), static_cast<int>(sv.length()), u8Buffer.get(), static_cast<int>(sv.length()) * 3, nullptr, nullptr);
        duration += GetDuration();
    }

    const char randElem8 = u8Buffer[RandomIndex(static_cast<ptrdiff_t>(chunkLen * u8CharLen))];
    u8Buffer.reset();
    std::cout << " ignore me " << static_cast<int>(static_cast<unsigned char>(randElem8))
              << "\n length " << length << "\n elapsed " << duration << std::endl;
}

void RtlUnicodeToUTF8N_Chunks(std::wstring_view testU16, size_t u8CharLen, size_t chunkLen)
{
    PrintHeader(__func__);
    const size_t endLoop{ testU16.length() / chunkLen };
    double duration{};
    ULONG written{};
    ULONG total{};
    NTSTATUS status{};
    GetDuration();
    std::unique_ptr<char[]> u8Buffer{ std::make_unique<char[]>(chunkLen * u8CharLen) };
    duration += GetDuration();

    for (size_t i{}; i < endLoop; ++i)
    {
        const std::wstring_view sv{ &testU16.at(i), chunkLen };
        GetDuration();
        status = p_RtlUnicodeToUTF8N(u8Buffer.get(), static_cast<ULONG>(sv.length()) * 3, &written, sv.data(), static_cast<ULONG>(sv.length() * 2));
        duration += GetDuration();
        total += written;
    }

    const char randElem8 = u8Buffer[RandomIndex(static_cast<ptrdiff_t>(chunkLen * u8CharLen))];
    u8Buffer.reset();
    std::cout << " ignore me " << static_cast<int>(static_cast<unsigned char>(randElem8))
              << "\n NTSTATUS " << status << "\n length " << total << "\n elapsed " << duration << std::endl;
}

void u16u8_Chunks(std::wstring_view testU16, size_t chunkLen)
{
    PrintHeader(__func__);
    const size_t endLoop{ testU16.length() / chunkLen };
    double duration{};
    size_t length{};
    HRESULT hRes{};
    std::string u8Str{};

    for (size_t i{}; i < endLoop; ++i)
    {
        const std::wstring_view sv{ &testU16.at(i), chunkLen };
        GetDuration();
        hRes = u16u8(sv, u8Str);
        duration += GetDuration();
        length += u8Str.length();
    }

    const char randElem8 = u8Str.at(RandomIndex(static_cast<ptrdiff_t>(u8Str.length())));
    std::cout << " ignore me " << static_cast<int>(static_cast<unsigned char>(randElem8))
              << "\n HRESULT " << hRes << "\n length " << length << "\n elapsed " << duration << std::endl;
}

void u16u8_ptr_Chunks(std::wstring_view testU16, size_t chunkLen)
{
    PrintHeader(__func__);
    const size_t endLoop{ testU16.length() / chunkLen };
    double duration{};
    size_t length{};
    HRESULT hRes{};
    std::string u8Str{};

    for (size_t i{}; i < endLoop; ++i)
    {
        const std::wstring_view sv{ &testU16.at(i), chunkLen };
        GetDuration();
        hRes = u16u8_ptr(sv, u8Str);
        duration += GetDuration();
        length += u8Str.length();
    }

    const char randElem8 = u8Str.at(RandomIndex(static_cast<ptrdiff_t>(u8Str.length())));
    std::cout << " ignore me " << static_cast<int>(static_cast<unsigned char>(randElem8))
              << "\n HRESULT " << hRes << "\n length " << length << "\n elapsed " << duration << std::endl;
}

void MultiByteToWideChar_WholeString(std::string_view u8Str)
{
    PrintHeader(__func__);
    GetDuration();
    std::unique_ptr<wchar_t[]> u16Buffer{ std::make_unique<wchar_t[]>(u8Str.length()) };
    const int length = MultiByteToWideChar(65001, 0, u8Str.data(), static_cast<int>(u8Str.length()), u16Buffer.get(), static_cast<int>(u8Str.length()));
    const double duration = GetDuration();
    const wchar_t randElem16 = u16Buffer[RandomIndex(static_cast<ptrdiff_t>(length))];
    u16Buffer.reset();
    std::cout << " ignore me " << static_cast<int>(randElem16)
              << "\n length " << length << "\n elapsed " << duration << std::endl;
}

void RtlUTF8ToUnicodeN_WholeString(std::string_view u8Str)
{
    PrintHeader(__func__);
    ULONG written{};
    GetDuration();
    std::unique_ptr<wchar_t[]> u16Buffer{ std::make_unique<wchar_t[]>(u8Str.length()) };
    const NTSTATUS status = p_RtlUTF8ToUnicodeN(u16Buffer.get(), static_cast<ULONG>(u8Str.length() * sizeof(wchar_t)), &written, u8Str.data(), static_cast<ULONG>(u8Str.length()));
    const double duration = GetDuration();
    const wchar_t randElem16 = u16Buffer[RandomIndex(static_cast<ptrdiff_t>(written / sizeof(wchar_t)))];
    u16Buffer.reset();
    std::cout << " ignore me " << static_cast<int>(randElem16)
              << "\n NTSTATUS " << status << "\n length " << (written / sizeof(wchar_t)) << "\n elapsed " << duration << std::endl;
}

void u8u16_WholeString(std::string_view u8Str)
{
    PrintHeader(__func__);
    GetDuration();
    std::wstring u16Str{};
    const HRESULT hRes = u8u16(u8Str, u16Str);
    const double duration = GetDuration();
    const wchar_t randElem16 = u16Str.at(RandomIndex(static_cast<ptrdiff_t>(u16Str.length())));
    std::cout << " ignore me " << static_cast<int>(randElem16)
              << "\n HRESULT " << hRes << "\n length " << u16Str.length() << "\n elapsed " << duration << std::endl;
}

void u8u16_ptr_WholeString(std::string_view u8Str)
{
    PrintHeader(__func__);
    GetDuration();
    std::wstring u16Str{};
    const HRESULT hRes = u8u16_ptr(u8Str, u16Str);
    const double duration = GetDuration();
    const wchar_t randElem16 = u16Str.at(RandomIndex(static_cast<ptrdiff_t>(u16Str.length())));
    std::cout << " ignore me " << static_cast<int>(randElem16)
              << "\n HRESULT " << hRes << "\n length " << u16Str.length() << "\n elapsed " << duration << std::endl;
}

void MultiByteToWideChar_Chunks(std::string_view u8Str, size_t u8CharLen, size_t u16ChunkLen)
{
    PrintHeader(__func__);
    const size_t endLoop{ u8Str.length() / u16ChunkLen };
    double duration{};
    int length{};
    GetDuration();
    std::unique_ptr<wchar_t[]> u16Buffer{ std::make_unique<wchar_t[]>(u8Str.length()) };
    duration += GetDuration();

    for (size_t i{}; i < endLoop; i += u8CharLen)
    {
        const std::string_view sv{ &u8Str.at(i), u16ChunkLen * u8CharLen };
        GetDuration();
        length += MultiByteToWideChar(65001, 0, sv.data(), static_cast<int>(sv.length()), u16Buffer.get(), static_cast<int>(sv.length()));
        duration += GetDuration();
    }

    const wchar_t randElem16 = u16Buffer[RandomIndex(static_cast<ptrdiff_t>(u16ChunkLen))];
    u16Buffer.reset();
    std::cout << " ignore me " << static_cast<int>(randElem16)
              << "\n length " << length << "\n elapsed " << duration << std::endl;
}

void RtlUTF8ToUnicodeN_Chunks(std::string_view u8Str, size_t u8CharLen, size_t u16ChunkLen)
{
    PrintHeader(__func__);
    const size_t endLoop{ u8Str.length() / u16ChunkLen };
    double duration{};
    ULONG written{};
    ULONG total{};
    NTSTATUS status{};
    GetDuration();
    std::unique_ptr<wchar_t[]> u16Buffer{ std::make_unique<wchar_t[]>(u8Str.length()) };
    duration += GetDuration();

    for (size_t i{}; i < endLoop; i += u8CharLen)
    {
        const std::string_view sv{ &u8Str.at(i), u16ChunkLen * u8CharLen };
        GetDuration();
        status = p_RtlUTF8ToUnicodeN(u16Buffer.get(), static_cast<ULONG>(sv.length() * sizeof(wchar_t)), &written, sv.data(), static_cast<ULONG>(sv.length()));
        duration += GetDuration();
        total += written;
    }

    const wchar_t randElem16 = u16Buffer[RandomIndex(static_cast<ptrdiff_t>(u16ChunkLen))];
    u16Buffer.reset();
    std::cout << " ignore me " << static_cast<int>(randElem16)
              << "\n NTSTATUS " << status << "\n length " << (total / sizeof(wchar_t)) << "\n elapsed " << duration << std::endl;
}

void u8u16_Chunks(std::string_view u8Str, size_t u8CharLen, size_t u16ChunkLen)
{
    PrintHeader(__func__);
    const size_t endLoop{ u8Str.length() / u16ChunkLen };
    double duration{};
    size_t length{};
    HRESULT hRes{};
    std::wstring u16Str{};

    for (size_t i{}; i < endLoop; i += u8CharLen)
    {
        const std::string_view sv{ &u8Str.at(i), u16ChunkLen * u8CharLen };
        GetDuration();
        hRes = u8u16(sv, u16Str);
        duration += GetDuration();
        length += u16Str.length();
    }

    const wchar_t randElem16 = u16Str.at(RandomIndex(static_cast<ptrdiff_t>(u16Str.length())));
    std::cout << " ignore me " << static_cast<int>(randElem16)
              << "\n HRESULT " << hRes << "\n length " << length << "\n elapsed " << duration << std::endl;
}

void u8u16_ptr_Chunks(std::string_view u8Str, size_t u8CharLen, size_t u16ChunkLen)
{
    PrintHeader(__func__);
    const size_t endLoop{ u8Str.length() / u16ChunkLen };
    double duration{};
    size_t length{};
    HRESULT hRes{};
    std::wstring u16Str{};

    for (size_t i{}; i < endLoop; i += u8CharLen)
    {
        const std::string_view sv{ &u8Str.at(i), u16ChunkLen * u8CharLen };
        GetDuration();
        hRes = u8u16_ptr(sv, u16Str);
        duration += GetDuration();
        length += u16Str.length();
    }

    const wchar_t randElem16 = u16Str.at(RandomIndex(static_cast<ptrdiff_t>(u16Str.length())));
    std::cout << " ignore me " << static_cast<int>(randElem16)
              << "\n HRESULT " << hRes << "\n length " << length << "\n elapsed " << duration << std::endl;
}

void CompNaturalLang_WholeString(const std::string& fileName)
{
    std::string head{ __func__ };
    head += " - " + fileName;
    PrintHeader(head.c_str());
    std::ostringstream u8Ss{};
    std::ostringstream buf{};
    buf << std::ifstream{ fileName }.rdbuf();
    std::fill_n(std::ostream_iterator<const char*>{ u8Ss }, 300000u, buf.str().c_str());
    std::string u8Str = u8Ss.str();

    GetDuration();
    std::unique_ptr<wchar_t[]> u16Buffer{ std::make_unique<wchar_t[]>(u8Str.length()) };
    int length = MultiByteToWideChar(65001, 0, u8Str.data(), static_cast<int>(u8Str.length()), u16Buffer.get(), static_cast<int>(u8Str.length()));
    double duration = GetDuration();
    u16Buffer.reset();
    std::cout << " MultiByteToWideChar length " << length << " elapsed " << duration << std::endl;

    GetDuration();
    std::wstring u16Str{};
    HRESULT hRes = u8u16_ptr(u8Str, u16Str);
    duration = GetDuration();
    std::cout << " u8u16_ptr           length " << u16Str.length() << " elapsed " << duration << std::endl;

    GetDuration();
    std::unique_ptr<char[]> u8Buffer{ std::make_unique<char[]>(u16Str.length() * 3) };
    length = WideCharToMultiByte(65001, 0, u16Str.data(), static_cast<int>(u16Str.length()), u8Buffer.get(), static_cast<int>(u16Str.length()) * 3, nullptr, nullptr);
    duration = GetDuration();
    u8Buffer.reset();
    std::cout << " WideCharToMultiByte length " << length << " elapsed " << duration << std::endl;

    GetDuration();
    std::string u8StrOut{};
    hRes = u16u8_ptr(u16Str, u8StrOut);
    duration = GetDuration();
    std::cout << " u16u8_ptr           length " << u8StrOut.length() << " elapsed " << duration << std::endl;
}

void CompNaturalLang_Chunks(const std::string& fileName)
{
    std::string head{ __func__ };
    head += " - " + fileName;
    PrintHeader(head.c_str());
    std::ostringstream u8Ss{};
    std::ostringstream buf{};
    buf << std::ifstream{ fileName }.rdbuf();
    std::fill_n(std::ostream_iterator<const char*>{ u8Ss }, 300000u, buf.str().c_str());
    std::string u8Str = u8Ss.str();

    std::wstring u16Str{ 10u };
    if (FAILED(u8u16_ptr(u8Str, u16Str)))
    {
        return;
    }

    constexpr const size_t chunkSize{ 10u };
    HRESULT hRes{};
    int lenTotalMB2WC{};
    int lenTotalWC2MB{};
    size_t lenTotalU8U16{};
    size_t lenTotalU16U8{};
    double durTotalMB2WC{};
    double durTotalWC2MB{};
    double durTotalU8U16{};
    double durTotalU16U8{};

    GetDuration();
    std::unique_ptr<wchar_t[]> u16Buffer{ std::make_unique<wchar_t[]>(chunkSize) };
    durTotalMB2WC += GetDuration();

    GetDuration();
    std::wstring u16StrOut{};
    durTotalU8U16 += GetDuration();

    GetDuration();
    std::unique_ptr<char[]> u8Buffer{ std::make_unique<char[]>(chunkSize * 3) };
    durTotalWC2MB += GetDuration();

    GetDuration();
    std::string u8StrOut{};
    durTotalU16U8 += GetDuration();

    for (size_t idx = 0u; idx < u16Str.length(); idx += chunkSize)
    {
        std::wstring u16Chunk{ u16Str.substr(idx, chunkSize) };
        std::string u8Chunk{ u16u8(u16Chunk) };

        GetDuration();
        lenTotalMB2WC += MultiByteToWideChar(65001, 0, u8Chunk.data(), static_cast<int>(u8Chunk.length()), u16Buffer.get(), static_cast<int>(u8Str.length()));
        durTotalMB2WC += GetDuration();

        GetDuration();
        hRes = u8u16_ptr(u8Chunk, u16StrOut);
        durTotalU8U16 += GetDuration();
        lenTotalU8U16 += u16StrOut.length();

        GetDuration();
        lenTotalWC2MB += WideCharToMultiByte(65001, 0, u16Chunk.data(), static_cast<int>(u16Chunk.length()), u8Buffer.get(), static_cast<int>(u16Chunk.length()) * 3, nullptr, nullptr);
        durTotalWC2MB += GetDuration();

        GetDuration();
        hRes = u16u8_ptr(u16Chunk, u8StrOut);
        durTotalU16U8 += GetDuration();
        lenTotalU16U8 += u8StrOut.length();
    }

    std::cout << " MultiByteToWideChar length " << lenTotalMB2WC << " elapsed " << durTotalMB2WC << std::endl;
    std::cout << " u8u16_ptr           length " << lenTotalU8U16 << " elapsed " << durTotalU8U16 << std::endl;
    std::cout << " WideCharToMultiByte length " << lenTotalWC2MB << " elapsed " << durTotalWC2MB << std::endl;
    std::cout << " u16u8_ptr           length " << lenTotalU16U8 << " elapsed " << durTotalU16U8 << std::endl;
}

int main()
{
    // UTF-16 string length
    //constexpr const size_t u16Length{ 100000000u }; // 100,000 code points
    constexpr const size_t u16Length{ 10000000u }; // 10,000 code points

    // chunk length in code points
    constexpr const size_t chunkLen = 10u;

    // UTF-16 character to be used
    //const std::wstring testU16(u16Length, static_cast<wchar_t>(0x007E)); // TILDE (1 Byte in UTF-8)
    //const std::wstring testU16(u16Length, static_cast<wchar_t>(0x00F6)); // LATIN SMALL LETTER O WITH DIAERESIS (2 Bytes in UTF-8)
    const std::wstring testU16(u16Length, static_cast<wchar_t>(0x20AC)); // // EURO SIGN (3 Bytes in UTF-8)

    HMODULE ntdll = LoadLibraryA("ntdll.dll");
    if (ntdll != nullptr)
    {
        p_RtlUTF8ToUnicodeN = reinterpret_cast<t_RtlUTF8ToUnicodeN>(GetProcAddress(ntdll, "RtlUTF8ToUnicodeN"));
        p_RtlUnicodeToUTF8N = reinterpret_cast<t_RtlUnicodeToUTF8N>(GetProcAddress(ntdll, "RtlUnicodeToUTF8N"));
        if (!p_RtlUTF8ToUnicodeN || !p_RtlUnicodeToUTF8N)
        {
            FreeLibrary(ntdll);
            return 1;
        }
    }
    else
    {
        return 1;
    }

    std::string u8Str{};

    std::cout << "### UTF-16 To UTF-8 ###" << std::endl;

    WideCharToMultiByte_WholeString(testU16);
    RtlUnicodeToUTF8N_WholeString(testU16);
    u16u8_WholeString(testU16, u8Str);
    u16u8_ptr_WholeString(testU16, u8Str);

    const size_t u8CharLen{ u8Str.length() / testU16.length() };
    const size_t u8ChunkLen{ u8CharLen * chunkLen };
    if (u8Str.length() % u8ChunkLen != 0)
    {
        std::cerr << "Chunk length has to be a divisor of string length!" << std::endl;
        FreeLibrary(ntdll);
        return 1;
    }

    WideCharToMultiByte_Chunks(testU16, u8CharLen, chunkLen);
    RtlUnicodeToUTF8N_Chunks(testU16, u8CharLen, chunkLen);
    u16u8_Chunks(testU16, chunkLen);
    u16u8_ptr_Chunks(testU16, chunkLen);

    std::cout << "\n\n### UTF-8 To UTF-16 ###" << std::endl;

    MultiByteToWideChar_WholeString(u8Str);
    RtlUTF8ToUnicodeN_WholeString(u8Str);
    u8u16_WholeString(u8Str);
    u8u16_ptr_WholeString(u8Str);

    MultiByteToWideChar_Chunks(u8Str, u8CharLen, chunkLen);
    RtlUTF8ToUnicodeN_Chunks(u8Str, u8CharLen, chunkLen);
    u8u16_Chunks(u8Str, u8CharLen, chunkLen);
    u8u16_ptr_Chunks(u8Str, u8CharLen, chunkLen);

    std::cout << "\n\n### Natural Languages ###" << std::endl;

    CompNaturalLang_WholeString("en.txt");
    CompNaturalLang_WholeString("fr.txt");
    CompNaturalLang_WholeString("ru.txt");
    CompNaturalLang_WholeString("zh.txt");

    CompNaturalLang_Chunks("en.txt");
    CompNaturalLang_Chunks("fr.txt");
    CompNaturalLang_Chunks("ru.txt");
    CompNaturalLang_Chunks("zh.txt");

    FreeLibrary(ntdll);
    return 0;
}

// returns the time elapsed between two calls (the return value of the first call is undefined)
double GetDuration()
{
    static std::chrono::time_point<std::chrono::high_resolution_clock> previous{};
    const auto current = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = current - previous;
    previous = current;
    return elapsed.count();
}

// returns a value 0..(length - 1), or -1 if the function failed
ptrdiff_t RandomIndex(ptrdiff_t length)
{
    static bool generatorInitialized{ false };
    static std::default_random_engine generator;
    if (!generatorInitialized)
    {
        generator.seed(static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count()));
        generatorInitialized = true;
    }
    if (length > 0)
    {
        std::uniform_int_distribution<ptrdiff_t> distribution{ static_cast<ptrdiff_t>(0), --length };
        return distribution(generator);
    }
    return static_cast<ptrdiff_t>(-1);
}

// print the header for a test in function funcName
void PrintHeader(const char* const funcName)
{
    std::cout << "\n~~~\ntest \"" << funcName << "\"" << std::endl;
}
