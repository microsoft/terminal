// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "srvinit.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class TitleTests
{
    TEST_CLASS(TitleTests);

    TEST_CLASS_SETUP(ClassSetup)
    {
        // This class assumes that %SystemRoot% == c:\windows
        WCHAR szSystemRoot[MAX_PATH];
        if (0 != GetWindowsDirectoryW(szSystemRoot, ARRAYSIZE(szSystemRoot)))
        {
            String strSystemRoot(szSystemRoot);
            String strExpectedSystemRoot(L"c:\\windows");
            return (strSystemRoot.ToLower() == strExpectedSystemRoot.ToLower());
        }

        return false;
    }

    TEST_METHOD(TestTranslateConsoleTitle)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:consoleTitle", L"{foo\\bar, c:\\windows\\system32\\cmd.exe, x:\\file\\path}")
            TEST_METHOD_PROPERTY(L"Data:unexpand", L"{true, false}")
            TEST_METHOD_PROPERTY(L"Data:substitute", L"{true, false}")
        END_TEST_METHOD_PROPERTIES();

        String strConsoleTitle;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"consoleTitle", strConsoleTitle));

        bool fUnexpand;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"unexpand", fUnexpand));

        bool fSubstitute;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"substitute", fSubstitute));

        PWSTR pszTranslated = TranslateConsoleTitle(strConsoleTitle, fUnexpand, fSubstitute);
        VERIFY_IS_NOT_NULL(pszTranslated);
        Log::Comment(String().Format(L"Translated title: %s", pszTranslated));

        String strTranslatedTitle(pszTranslated);
        if (strConsoleTitle.Find(L"foo") == 0)
        {
            // dealing with non-filesystem parameter
            if (fSubstitute)
            {
                // shouldn't have a backslash -- just an underscore
                VERIFY_ARE_EQUAL(strTranslatedTitle, String(L"foo_bar"));
            }
            else
            {
                // string shouldn't be modified
                VERIFY_ARE_EQUAL(strConsoleTitle, strTranslatedTitle);
            }
        }
        else
        {
            // dealing with filesystem parameter
            if (strConsoleTitle.Find(L"c") == 0)
            {
                // dealing with c:\windows\system32\cmd.exe
                if (fUnexpand)
                {
                    if (fSubstitute)
                    {
                        VERIFY_ARE_EQUAL(strTranslatedTitle, String(L"%SystemRoot%_system32_cmd.exe"));
                    }
                    else
                    {
                        VERIFY_ARE_EQUAL(strTranslatedTitle, String(L"%SystemRoot%\\system32\\cmd.exe"));
                    }
                }
                else
                {
                    if (fSubstitute)
                    {
                        VERIFY_ARE_EQUAL(strTranslatedTitle, String(L"c:_windows_system32_cmd.exe"));
                    }
                    else
                    {
                        VERIFY_ARE_EQUAL(strConsoleTitle, strTranslatedTitle);
                    }
                }
            }
            else
            {
                // dealing with x:\file\path
                if (fSubstitute)
                {
                    VERIFY_ARE_EQUAL(strTranslatedTitle, String(L"x:_file_path"));
                }
                else
                {
                    VERIFY_ARE_EQUAL(strConsoleTitle, strTranslatedTitle);
                }
            }
        }

        delete[] pszTranslated;
        pszTranslated = nullptr;
    }
};
