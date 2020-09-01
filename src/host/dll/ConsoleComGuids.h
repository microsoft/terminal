#pragma once

#define __IID_IConsoleHandoff 2B607BC1-43EB-40C3-95AE-2856ADDB7F23
#define __IID_ITerminalHandoff F89E4430-FF48-4DD7-A22C-182B546A8BA2

#if defined(WT_ReleaseBuild)
    #define __CLSID_IConsoleProxy 3171DE52-6EFA-4AEF-8A9F-D02BD67E7A4F
    #define __CLSID_CConsoleHandoff "2EACA947-7F5F-4CFA-BA87-8F7FBEEFBE69"
    #define __CLSID_CTerminalHandoff "E12CFF52-A866-4C77-9A90-F570A7AA2C6B"
#elif defined(WT_PreviewBuild)
    #define __CLSID_IConsoleProxy 1833E661-CC81-4DD0-87C6-C2F74BD39EFA
    #define __CLSID_CConsoleHandoff "06EC847C-C0A5-46B8-92CB-7C92F6E35CD5"
    #define __CLSID_CTerminalHandoff "86633F1F-6454-40EC-89CE-DA4EBA977EE2"
#else
    #define __CLSID_IConsoleProxy DA2E620C-0B90-45D2-8362-A5B1006D9243
    #define __CLSID_CConsoleHandoff "1F9F2BF5-5BC3-4F17-B0E6-912413F1F451"
    #define __CLSID_CTerminalHandoff "E7B98685-0CC0-45D6-8F4C-B7C863C03DE2"
#endif
