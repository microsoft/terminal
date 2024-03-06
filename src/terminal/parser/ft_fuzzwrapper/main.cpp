// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "echoDispatch.hpp"
#include "../stateMachine.hpp"
#include "../OutputStateMachineEngine.hpp"

using namespace Microsoft::Console::VirtualTerminal;

void PrintUsage()
{
    wprintf(L"Usage: conterm.parser.fuzzwrapper.exe <input file name> <codepage>\r\n");
    wprintf(L"Use codepage 1200 for Unicode. 437 for US English. 0 for reading straight as ASCII.\r\n");
}

const UINT UNICODE_CP = 1200;
const UINT ASCII_CP = 0;
UINT uiCodePage = UNICODE_CP;
FILE* hFile = nullptr;

bool GetChar(wchar_t* pwch)
{
    if (uiCodePage == UNICODE_CP)
    {
        *pwch = fgetwc(hFile);
        return *pwch != WEOF;
    }
    else if (uiCodePage == ASCII_CP)
    {
        auto ch = fgetc(hFile);
        *pwch = 0;
        *pwch = (wchar_t)ch;
        return ch != EOF;
    }
    else
    {
        auto ch = fgetc(hFile);
        MultiByteToWideChar(uiCodePage, 0, (char*)&ch, 1, pwch, 1);
        return ch != EOF;
    }
}

int __cdecl wmain(int argc, wchar_t* argv[])
{
    auto ret = 0;

    if (argc != 3)
    {
        PrintUsage();
        ret = E_INVALIDARG;
    }
    else
    {
        uiCodePage = (UINT)_wtoi(argv[2]);
        wprintf(L"Using codepage '%d'", uiCodePage);

        wprintf(L"Opening file '%s'...\r\n", argv[1]);
        hFile = _wfopen(argv[1], L"r");
        wchar_t wch;
        auto fGotChar = GetChar(&wch);
        auto dispatch = std::make_unique<EchoDispatch>();
        auto engine = std::make_unique<OutputStateMachineEngine>(std::move(dispatch));

        StateMachine machine(std::move(engine));

        wprintf(L"Sending characters to state machine...\r\n");
        while (fGotChar)
        {
            machine.ProcessCharacter(wch);
            fGotChar = GetChar(&wch);
        }

        if (hFile)
        {
            fclose(hFile);
        }
        wprintf(L"Done.\r\n");
    }

    return ret;
}
