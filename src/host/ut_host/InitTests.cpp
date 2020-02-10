// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include "CommonState.hpp"

#include "globals.h"
#include "srvinit.h"

#include "..\interactivity\inc\ServiceLocator.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

class InitTests
{
    TEST_CLASS(InitTests);

    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
    static UINT const s_uiOEMJapaneseCP = 932;
    static UINT const s_uiOEMSimplifiedChineseCP = 936;
    static UINT const s_uiOEMKoreanCP = 949;
    static UINT const s_uiOEMTraditionalChineseCP = 950;

    static LANGID const s_langIdJapanese = MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
    static LANGID const s_langIdSimplifiedChinese = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
    static LANGID const s_langIdKorean = MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN);
    static LANGID const s_langIdTraditionalChinese = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
    static LANGID const s_langIdEnglish = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

    // This test exists to ensure the continued behavior of the code in the Windows loader.
    // See the LOAD BEARING CODE comment inside GetConsoleLangId or the investigation results in MSFT: 9808579 for more detail.
    TEST_METHOD(TestGetConsoleLangId)
    {
        using Microsoft::Console::Interactivity::ServiceLocator;
        BEGIN_TEST_METHOD_PROPERTIES()
            // https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
            // The interesting ones for us are:
            // Japanese Shift JIS = 932
            // Chinese Simplified GB2312 = 936
            // Korean Unified Hangul = 949
            // Chinese Traditional Big5 = 950
            TEST_METHOD_PROPERTY(L"Data:uiStartupCP", L"{437, 850, 932, 936, 949, 950}")
            TEST_METHOD_PROPERTY(L"Data:uiOutputCP", L"{437, 850, 932, 936, 949, 950}")
        END_TEST_METHOD_PROPERTIES()

        // if ServiceLocator::LocateGlobals().uiWindowsCP = a CJK one
        // we should get SUCCESS and a matching result to our input
        // for any other ServiceLocator::LocateGlobals().uiWindowsCP we should get STATUS_NOT_SUPPORTED and do nothing with the langid.

        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiStartupCP", ServiceLocator::LocateGlobals().uiWindowsCP));

        UINT outputCP;
        VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"uiOutputCP", outputCP));

        LANGID langId = 0;
        NTSTATUS const status = GetConsoleLangId(outputCP, &langId);

        if (s_uiOEMJapaneseCP == ServiceLocator::LocateGlobals().uiWindowsCP ||
            s_uiOEMSimplifiedChineseCP == ServiceLocator::LocateGlobals().uiWindowsCP ||
            s_uiOEMKoreanCP == ServiceLocator::LocateGlobals().uiWindowsCP ||
            s_uiOEMTraditionalChineseCP == ServiceLocator::LocateGlobals().uiWindowsCP)
        {
            VERIFY_ARE_EQUAL(STATUS_SUCCESS, status);

            LANGID langIdExpected;
            switch (outputCP)
            {
            case s_uiOEMJapaneseCP:
                langIdExpected = s_langIdJapanese;
                break;
            case s_uiOEMSimplifiedChineseCP:
                langIdExpected = s_langIdSimplifiedChinese;
                break;
            case s_uiOEMKoreanCP:
                langIdExpected = s_langIdKorean;
                break;
            case s_uiOEMTraditionalChineseCP:
                langIdExpected = s_langIdTraditionalChinese;
                break;
            default:
                langIdExpected = s_langIdEnglish;
                break;
            }

            VERIFY_ARE_EQUAL(langIdExpected, langId);
        }
        else
        {
            VERIFY_ARE_EQUAL(STATUS_NOT_SUPPORTED, status);
        }
    }
};
