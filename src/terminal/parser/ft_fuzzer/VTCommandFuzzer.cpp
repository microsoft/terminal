// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "stdafx.h"

#define __GENERATE_DIRECTED_FUZZING
#include "fuzzing_logic.h"
#include "fuzzing_directed.h"
#include "string_helper.h"

using namespace fuzz;

// VT100 spec defines the ESC sequence as the char 0x1b
const CHAR ESC[2]{ 0x1b, 0x0 };

// VT100 spec defines the CSI sequence as ESC followed by [
const CHAR CSI[3]{ 0x1b, 0x5b, 0x0 };

// There is an alternative, single-character CSI in the C1 control set:
const CHAR C1CSI[2]{ static_cast<CHAR>(static_cast<unsigned char>(0x9b)), 0x0 };

// VT100 spec defines the OSC sequence as ESC followed by ]
const CHAR OSC[3]{ 0x1b, 0x5d, 0x0 };

static std::string GenerateSGRToken();
static std::string GenerateCUXToken();
static std::string GenerateCUXToken2();
static std::string GenerateCUXToken3();
static std::string GeneratePrivateModeParamToken();
static std::string GenerateDeviceAttributesToken();
static std::string GenerateDeviceStatusReportToken();
static std::string GenerateEraseToken();
static std::string GenerateScrollToken();
static std::string GenerateWhiteSpaceToken();
static std::string GenerateInvalidToken();
static std::string GenerateTextToken();
static std::string GenerateOscTitleToken();
static std::string GenerateHardResetToken();
static std::string GenerateSoftResetToken();
static std::string GenerateOscColorTableToken();

const fuzz::_fuzz_type_entry<BYTE> g_repeatMap[] = {
    { 4, [](BYTE) { return CFuzzChance::GetRandom<BYTE>(2, 0xF); } },
    { 1, [](BYTE) { return CFuzzChance::GetRandom<BYTE>(2, 0xFF); } },
    { 20, [](BYTE) { return (BYTE)0; } }
};

const std::function<std::string()> g_tokenGenerators[] = {
    GenerateSGRToken,
    GenerateCUXToken,
    GenerateCUXToken2,
    GenerateCUXToken3,
    GeneratePrivateModeParamToken,
    GenerateDeviceAttributesToken,
    GenerateDeviceStatusReportToken,
    GenerateScrollToken,
    GenerateEraseToken,
    GenerateOscTitleToken,
    GenerateHardResetToken,
    GenerateSoftResetToken,
    GenerateOscColorTableToken
};

std::string GenerateTokenLowProbability()
{
    const _fuzz_type_entry<std::string> tokenGeneratorMap[] = {
        { 3, [&](std::string) { return CFuzzChance::SelectOne(g_tokenGenerators, ARRAYSIZE(g_tokenGenerators))(); } },
        { 1, [](std::string) { return GenerateInvalidToken(); } },
        { 1, [](std::string) { return GenerateTextToken(); } },
        { 5, [](std::string) { return GenerateWhiteSpaceToken(); } }
    };
    CFuzzType<std::string> ft(FUZZ_MAP(tokenGeneratorMap), std::string(""));

    return (std::string)ft;
}

std::string GenerateToken()
{
    const _fuzz_type_entry<std::string> tokenGeneratorMap[] = {
        { 50, [](std::string) { return GenerateTextToken(); } },
        { 40, [&](std::string) { return CFuzzChance::SelectOne(g_tokenGenerators, ARRAYSIZE(g_tokenGenerators))(); } },
        { 1, [](std::string) { return GenerateInvalidToken(); } },
        { 3, [](std::string) { return GenerateWhiteSpaceToken(); } }
    };
    CFuzzType<std::string> ft(FUZZ_MAP(tokenGeneratorMap), std::string(""));

    return (std::string)ft;
}

std::string GenerateWhiteSpaceToken()
{
    const _fuzz_type_entry<DWORD> ftMap[] = {
        { 5, [](DWORD) { return CFuzzChance::GetRandom<DWORD>(0, 0xF); } },
        { 5, [](DWORD) { return CFuzzChance::GetRandom<DWORD>(0, 0xFF); } }
    };
    CFuzzType<DWORD> ft(FUZZ_MAP(ftMap), 0);

    std::string s;
    for (DWORD i = 0; i < (DWORD)ft; i++)
    {
        s.append(" ");
    }

    return s;
}

std::string GenerateTextToken()
{
    const LPSTR tokens[] = {
        "The cow jumped over the moon.",
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
        "Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt. Neque porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore et dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam, quis nostrum exercitationem ullam corporis suscipit laboriosam, nisi ut aliquid ex ea commodi consequatur? Quis autem vel eum iure reprehenderit qui in ea voluptate velit esse quam nihil molestiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla pariatur?",
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.",
        "\r\n",
        "\t",
        "%s",
        "%n",
        ";",
        "-",
        "?",
        "1024",
        "0",
        "0xFF"
    };

    return std::string(CFuzzChance::SelectOne(tokens, ARRAYSIZE(tokens)));
}

std::string GenerateInvalidToken()
{
    const LPSTR tokens[]{ ":", "'", "\"", "\\" };
    return std::string(CFuzzChance::SelectOne(tokens, ARRAYSIZE(tokens)));
}

std::string GenerateFuzzedToken(
    __in_ecount(cmap) const _fuzz_type_entry<std::string>* map,
    __in DWORD cmap,
    __in_ecount(ctokens) const LPSTR* tokens,
    __in DWORD ctokens)
{
    std::string csis[] = { CSI, C1CSI };
    std::string s = CFuzzChance::SelectOne(csis);

    BYTE manipulations = (BYTE)CFuzzType<BYTE>(FUZZ_MAP(g_repeatMap), 1);
    for (BYTE i = 0; i < manipulations; i++)
    {
        CFuzzType<std::string> ft(map, cmap, std::string(""));
        s += GenerateTokenLowProbability();
        s += (std::string)ft;
        s += GenerateTokenLowProbability();
        s += (i + 1 == manipulations) ? "" : ";";
        s += GenerateTokenLowProbability();
    }

    s.append(CFuzzChance::SelectOne(tokens, ctokens));
    return s;
}

std::string GenerateFuzzedOscToken(
    __in_ecount(cmap) const _fuzz_type_entry<std::string>* map,
    __in DWORD cmap,
    __in_ecount(ctokens) const LPSTR* tokens,
    __in DWORD ctokens)
{
    std::string s(OSC);
    BYTE manipulations = (BYTE)CFuzzType<BYTE>(FUZZ_MAP(g_repeatMap), 1);
    for (BYTE i = 0; i < manipulations; i++)
    {
        CFuzzType<std::string> ft(map, cmap, std::string(""));
        s += GenerateTokenLowProbability();
        s += (std::string)ft;
        s += GenerateTokenLowProbability();
        s += (i + 1 == manipulations) ? "" : ";";
        s += GenerateTokenLowProbability();
    }

    s.append(CFuzzChance::SelectOne(tokens, ctokens));
    return s;
}

// For SGR attributes, multiple can be specified in a row separated by ; and processed accordingly.
// For instance, 37;1;44m will do foreground white (low intensity, so effectively a gray) then set high intensity blue background.
std::string GenerateSGRToken()
{
    const BYTE psValid[] = {
        0,
        1,
        2,
        3,
        4,
        5,
        6,
        7,
        8,
        9,
        10,
        11,
        12,
        13,
        14,
        15,
        24,
        27,
        30,
        31,
        32,
        33,
        34,
        35,
        36,
        37,
        39,
        40,
        41,
        42,
        43,
        44,
        45,
        46,
        47,
        49,
    };

    const LPSTR tokens[] = { "m" };
    const _fuzz_type_entry<std::string> map[] = {
        { 40, [&](std::string) { std::string s; AppendFormat(s, "%02d", CFuzzChance::SelectOne(psValid, ARRAYSIZE(psValid))); return s; } },
        { 10, [](std::string) { std::string s; AppendFormat(s, "%d", CFuzzChance::GetRandom<BYTE>()); return s; } },
        { 25, [](std::string) { return std::string("35;5"); } },
        { 25, [](std::string) { return std::string("48;5"); } },
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Cursor positioning, this function handles moving the cursor relative to the current location
// For example, moving the cursor to the next line, previous line, up, down, etc.
std::string GenerateCUXToken()
{
    const LPSTR tokens[] = { "A", "B", "C", "D", "E", "F", "G" };
    const _fuzz_type_entry<std::string> map[] = {
        { 25, [](std::string) { std::string s; AppendFormat(s, "%d", CFuzzChance::GetRandom<USHORT>()); return s; } },
        { 25, [](std::string) { std::string s; AppendFormat(s, "%d", CFuzzChance::GetRandom<BYTE>()); return s; } }
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Cursor positioning, this function handles saving and restoring the cursor position.
// Differs from other cursor functions since these are ESC sequences and not CSI sequences.
std::string GenerateCUXToken2()
{
    const LPSTR tokens[] = { "7", "8" };
    std::string cux(ESC);
    cux += GenerateTokenLowProbability();
    cux += CFuzzChance::SelectOne(tokens, ARRAYSIZE(tokens));
    cux += GenerateTokenLowProbability();
    return cux;
}

// Cursor positioning with two arguments
std::string GenerateCUXToken3()
{
    const LPSTR tokens[]{ "H" };
    const _fuzz_type_entry<std::string> map[] = {
        { 60, [](std::string) { std::string s; AppendFormat(s, "%d;%d", CFuzzChance::GetRandom<BYTE>(), CFuzzChance::GetRandom<BYTE>()); return s; } }, // 60% give us two numbers in the valid range
        { 10, [](std::string) { return std::string(";"); } }, // 10% give us just a ;
        { 10, [](std::string) { std::string s; AppendFormat(s, "%d;", CFuzzChance::GetRandom<BYTE>()); return s; } }, // 10% give us a column and no line
        { 10, [](std::string) { std::string s; AppendFormat(s, ";%d", CFuzzChance::GetRandom<BYTE>()); return s; } }, // 10% give us a line and no column
        // 10% give us nothing
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Hard Reset (has no args)
std::string GenerateHardResetToken()
{
    const LPSTR tokens[] = { "c" };
    std::string cux(ESC);
    cux += GenerateTokenLowProbability();
    cux += CFuzzChance::SelectOne(tokens, ARRAYSIZE(tokens));
    cux += GenerateTokenLowProbability();
    return cux;
}

// Soft Reset (has no args)
std::string GenerateSoftResetToken()
{
    const LPSTR tokens[] = { "p" };
    std::string cux(CSI);
    cux += GenerateTokenLowProbability();
    cux += CFuzzChance::SelectOne(tokens, ARRAYSIZE(tokens));
    cux += GenerateTokenLowProbability();
    return cux;
}

// Private Mode parameters. These cover a wide range of behaviors - hiding the cursor,
//      enabling mouse mode, changing to the alt buffer, blinking the cursor, etc.
std::string GeneratePrivateModeParamToken()
{
    const LPSTR tokens[] = { "h", "l" };
    const _fuzz_type_entry<std::string> map[] = {
        { 12, [](std::string) { std::string s; AppendFormat(s, "?%02d", CFuzzChance::GetRandom<BYTE>()); return s; } },
        { 8, [](std::string) { return std::string("?1"); } },
        { 8, [](std::string) { return std::string("?3"); } },
        { 8, [](std::string) { return std::string("?12"); } },
        { 8, [](std::string) { return std::string("?25"); } },
        { 8, [](std::string) { return std::string("?1000"); } },
        { 8, [](std::string) { return std::string("?1002"); } },
        { 8, [](std::string) { return std::string("?1003"); } },
        { 8, [](std::string) { return std::string("?1005"); } },
        { 8, [](std::string) { return std::string("?1006"); } },
        { 8, [](std::string) { return std::string("?1007"); } },
        { 8, [](std::string) { return std::string("?1049"); } }
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Erase sequences, valid numerical values are 0-2.  If no numeric value is specified, 0 is assumed.
std::string GenerateEraseToken()
{
    const LPSTR tokens[] = { "J", "K" };
    const _fuzz_type_entry<std::string> map[] = {
        { 9, [](std::string) { return std::string(""); } },
        { 25, [](std::string) { return std::string("0"); } },
        { 25, [](std::string) { return std::string("1"); } },
        { 25, [](std::string) { return std::string("2"); } },
        { 25, [](std::string) { return std::string("3"); } },
        { 1, [](std::string) { std::string s; AppendFormat(s, "%02d", CFuzzChance::GetRandom<BYTE>()); return s; } }
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Device Attributes
std::string GenerateDeviceAttributesToken()
{
    const LPSTR tokens[] = { "c" };
    const _fuzz_type_entry<std::string> map[] = {
        { 70, [](std::string) { return std::string(""); } }, // 70% leave it blank (valid)
        { 29, [](std::string) { return std::string("0"); } }, // 29% put in a 0 (valid)
        { 1, [](std::string) { std::string s; AppendFormat(s, "%02d", CFuzzChance::GetRandom<BYTE>()); return s; } } // 1% make a mess (anything else)
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Device Attributes
std::string GenerateDeviceStatusReportToken()
{
    const LPSTR tokens[] = { "n" };
    const _fuzz_type_entry<std::string> map[] = {
        { 50, [](std::string) { return std::string("6"); } }, // 50% of the time, give us the one we were looking for (6, cursor report)
        { 49, [](std::string) { std::string s; AppendFormat(s, "%02d", CFuzzChance::GetRandom<BYTE>()); return s; } } // 49% of the time, put in a random value
        // 1% leave it blank
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Scroll sequences, valid numeric values include 0-16384.
std::string GenerateScrollToken()
{
    const LPSTR tokens[] = { "S", "T" };
    const _fuzz_type_entry<std::string> map[] = {
        { 5, [](std::string) { std::string s; AppendFormat(s, "%08d", CFuzzChance::GetRandom<ULONG>()); return s; } },
        { 5, [](std::string) { std::string s; AppendFormat(s, "%08d", CFuzzChance::GetRandom<USHORT>()); return s; } },
        { 50, [](std::string) { std::string s; AppendFormat(s, "%d", CFuzzChance::GetRandom<USHORT>(0, 0x4000)); return s; } },
        { 20, [](std::string) { std::string s; AppendFormat(s, "%02d", CFuzzChance::GetRandom<BYTE>()); return s; } }
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Resize sequences, valid numeric values include 0-16384.
std::string GenerateResizeToken()
{
    const LPSTR tokens[] = { "t" };
    // 5% - generate a random window manipulation with 1 params
    // 5% - generate a random window manipulation with 2 params
    // 5% - generate a random window manipulation with no params
    // 45% - generate a resize with two params
    // 10% - generate a resize with only the first param
    // 10% - generate a resize with only the second param
    const _fuzz_type_entry<std::string> map[] = {
        { 5, [](std::string) { std::string s; AppendFormat(s, "%d;%d;%d", CFuzzChance::GetRandom<USHORT>(0, 0x4000), CFuzzChance::GetRandom<USHORT>(0, 0x4000), CFuzzChance::GetRandom<USHORT>(0, 0x4000)); return s; } },
        { 5, [](std::string) { std::string s; AppendFormat(s, "%d;%d", CFuzzChance::GetRandom<USHORT>(0, 0x4000), CFuzzChance::GetRandom<USHORT>(0, 0x4000)); return s; } },
        { 5, [](std::string) { std::string s; AppendFormat(s, "%d", CFuzzChance::GetRandom<USHORT>(0, 0x4000)); return s; } },
        { 45, [](std::string) { std::string s; AppendFormat(s, "8;%d;%d", CFuzzChance::GetRandom<USHORT>(0, 0x4000), CFuzzChance::GetRandom<USHORT>(0, 0x4000)); return s; } },
        { 10, [](std::string) { std::string s; AppendFormat(s, "8;%d;", CFuzzChance::GetRandom<USHORT>(0, 0x4000)); return s; } },
        { 10, [](std::string) { std::string s; AppendFormat(s, "8;;%d", CFuzzChance::GetRandom<USHORT>(0, 0x4000)); return s; } },
    };

    return GenerateFuzzedToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Osc Window Title String. An Osc followed by a param on [0, SHORT_MAX], followed by a ";", followed by a string,
// and BEL terminated.
std::string GenerateOscTitleToken()
{
    const LPSTR tokens[] = { "\x7" };
    const _fuzz_type_entry<std::string> map[] = {
        { 100,
          [](std::string) {
              std::string s;
              SHORT limit = CFuzzChance::GetRandom<SHORT>(0, 10);
              // append up to 10 numbers for the param
              for (SHORT i = 0; i < limit; i++)
              {
                  AppendFormat(s, "%d", CFuzzChance::GetRandom<BYTE>(0, 9));
              }
              s.append(";");
              // append some characters for the string
              limit = CFuzzChance::GetRandom<SHORT>();
              for (SHORT i = 0; i < limit; i++)
              {
                  AppendFormat(s, "%c", CFuzzChance::GetRandom<BYTE>());
              }
              return s;
          } }
    };

    return GenerateFuzzedOscToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

// Osc Window Title String. An Osc followed by a param on [0, SHORT_MAX], followed by a ";", followed by a string,
// and BEL terminated.
std::string GenerateOscColorTableToken()
{
    const LPSTR tokens[] = { "\x7", "\x1b\\" };
    const _fuzz_type_entry<std::string> map[] = {
        { 100,
          [](std::string) {
              std::string s;
              SHORT limit = CFuzzChance::GetRandom<SHORT>(0, 10);
              // append up to 10 numbers for the param
              for (SHORT i = 0; i < limit; i++)
              {
                  AppendFormat(s, "%d", CFuzzChance::GetRandom<BYTE>(0, 9));
              }
              s.append(";");

              // Append some random numbers for the index
              limit = CFuzzChance::GetRandom<SHORT>(0, 10);
              // append up to 10 numbers for the param
              for (SHORT i = 0; i < limit; i++)
              {
                  AppendFormat(s, "%d", CFuzzChance::GetRandom<BYTE>(0, 9));
              }
              // Maybe add more text
              if (CFuzzChance::GetRandom<BOOL>())
              {
                  // usually add a RGB
                  limit = CFuzzChance::GetRandom<SHORT>(0, 10);
                  switch (limit)
                  {
                  case 0:
                  case 1:
                  case 2:
                  case 3:
                  case 4:
                  case 5:
                  case 6:
                      s.append("rgb:");
                      break;
                  case 7:
                      s.append("rgbi:");
                      break;
                  case 8:
                      s.append("cmyk:");
                      break;
                  default:
                      // append some characters for the string
                      limit = CFuzzChance::GetRandom<SHORT>();
                      for (SHORT i = 0; i < limit; i++)
                      {
                          AppendFormat(s, "%c", CFuzzChance::GetRandom<BYTE>());
                      }
                  }

                  SHORT numColors = CFuzzChance::GetRandom<SHORT>(0, 5);

                  // append up to 10 numbers for the param
                  for (SHORT i = 0; i < numColors; i++)
                  {
                      // Append some random numbers for the value
                      limit = CFuzzChance::GetRandom<SHORT>(0, 10);
                      // append up to 10 numbers for the param
                      for (SHORT j = 0; j < limit; j++)
                      {
                          AppendFormat(s, "%d", CFuzzChance::GetRandom<BYTE>(0, 9));
                      }
                      // Sometimes don't add a '/'
                      if (CFuzzChance::GetRandom<SHORT>(0, 10) != 0)
                      {
                          s.append("/");
                      }
                  }
              }
              return s;
          } }
    };

    return GenerateFuzzedOscToken(FUZZ_MAP(map), tokens, ARRAYSIZE(tokens));
}

int __cdecl wmain(int argc, WCHAR* argv[])
{
    if (argc != 3)
    {
        wprintf(L"Usage: <file count> <output directory>");
        return -1;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        LPWSTR pwszOutputDir = argv[2];
        DWORD dwFileCount = _wtoi(argv[1]);
        for (DWORD i = 0; i < dwFileCount; i++)
        {
            GUID guid = { 0 };
            hr = CoCreateGuid(&guid);
            if (SUCCEEDED(hr))
            {
                std::wstring sGuid(MAX_PATH, L'\0');
                const auto len = StringFromGUID2(guid, sGuid.data(), MAX_PATH);
                if (len != 0)
                    sGuid.resize(len - 1);
                TrimLeft(sGuid, L'{');
                TrimRight(sGuid, L'}');

                std::wstring outputFile(pwszOutputDir);
                outputFile += L"\\" + sGuid + L".bin";

                std::string text;
                for (int j = 0; j < CFuzzChance::GetRandom<BYTE>(); j++)
                {
                    text.append(GenerateToken());
                }

                wil::com_ptr<IStream> spStream;
                hr = SHCreateStreamOnFileW(outputFile.c_str(), STGM_CREATE | STGM_READWRITE, &spStream);
                if (SUCCEEDED(hr))
                {
                    ULONG cbWritten = 0;
                    hr = spStream->Write(text.c_str(), static_cast<ULONG>(text.length()), &cbWritten);
                    if (SUCCEEDED(hr))
                    {
                        wprintf(L"Wrote file (%d bytes): %s\n", cbWritten, outputFile.c_str());
                    }
                }
            }
        }

        CoUninitialize();
    }

    return 0;
}
