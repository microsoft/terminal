#pragma once

#define __IID_IConsoleHandoff 2B607BC1-43EB-40C3-95AE-2856ADDB7F23
#define __IID_ITerminalHandoff FA1E3AB4-9AEC-4A3C-96CA-E6078C30BD74

#if defined(WT_ReleaseBuild)
    #define __CLSID_IConsoleProxy 3171DE52-6EFA-4AEF-8A9F-D02BD67E7A4F
    #define __CLSID_CConsoleHandoff "2EACA947-7F5F-4CFA-BA87-8F7FBEEFBE69"
    #define __CLSID_CTerminalHandoff "E12CFF52-A866-4C77-9A90-F570A7AA2C6B"
#elif defined(WT_PreviewBuild)
    #define __CLSID_IConsoleProxy 1833E661-CC81-4DD0-87C6-C2F74BD39EFA
    #define __CLSID_CConsoleHandoff "06EC847C-C0A5-46B8-92CB-7C92F6E35CD5"
    #define __CLSID_CTerminalHandoff "86633F1F-6454-40EC-89CE-DA4EBA977EE2"
#else
    #define __CLSID_IConsoleProxy DEC4804D-56D1-4F73-9FBE-6828E7C85C56
    #define __CLSID_CConsoleHandoff "1F9F2BF5-5BC3-4F17-B0E6-912413F1F451"
    #define __CLSID_CTerminalHandoff "051F34EE-C1FD-4B19-AF75-9BA54648434C"
#endif
