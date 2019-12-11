!include ..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Host
# -------------------------------------

# The console host is the application that services all requests from a Win32
# console mode application. Since a console application has no visual representation
# on its own (it simply uses STDIN and STDOUT to process a text stream)
# the console host window provides a visual representation (output) and a means
# of capturing user-interaction (input) on behalf of the hosted application.


# -------------------------------------
# Preprocessor Settings
# -------------------------------------

C_DEFINES               = $(C_DEFINES) -DFE_IME

# -------------------------------------
# Compiler Settings
# -------------------------------------

# Warning 4201: nonstandard extension used: nameless struct/union
MSC_WARNING_LEVEL       = $(MSC_WARNING_LEVEL) /wd4201

# -------------------------------------
# Program Information
# -------------------------------------

TARGET_DESTINATION      = retail

# -------------------------------------
# Build System Settings
# -------------------------------------

# Code in the OneCore depot automatically excludes default Win32 libraries.

# Defines IME and Codepage support
W32_SB                  = 1


# -------------------------------------
# Sources, Headers, and Libraries
# -------------------------------------

SOURCES = \
    $(SOURCES) \
    ..\selection.cpp \
    ..\selectionInput.cpp \
    ..\selectionState.cpp \
    ..\scrolling.cpp \
    ..\cmdline.cpp   \
    ..\CursorBlinker.cpp   \
    ..\popup.cpp   \
    ..\alias.cpp   \
    ..\history.cpp   \
    ..\VtIo.cpp   \
    ..\VtInputThread.cpp   \
    ..\PtySignalInputThread.cpp \
    ..\consoleInformation.cpp \
    ..\directio.cpp  \
    ..\getset.cpp    \
    ..\globals.cpp   \
    ..\handle.cpp    \
    ..\init.cpp      \
    ..\input.cpp     \
    ..\inputBuffer.cpp \
    ..\inputKeyInfo.cpp \
    ..\inputReadHandleData.cpp \
    ..\misc.cpp      \
    ..\output.cpp    \
    ..\srvinit.cpp   \
    ..\outputStream.cpp \
    ..\stream.cpp    \
    ..\dbcs.cpp      \
    ..\convarea.cpp  \
    ..\screenInfo.cpp \
    ..\ScreenBufferRenderTarget.cpp \
    ..\_output.cpp   \
    ..\_stream.cpp   \
    ..\utils.cpp     \
    ..\telemetry.cpp \
    ..\tracing.cpp   \
    ..\registry.cpp  \
    ..\settings.cpp  \
    ..\ntprivapi.cpp \
    ..\readData.cpp \
    ..\readDataCooked.cpp \
    ..\readDataDirect.cpp \
    ..\readDataRaw.cpp \
    ..\writeData.cpp \
    ..\renderData.cpp \
    ..\renderFontDefaults.cpp \
    ..\utf8ToWideCharParser.cpp \
    ..\conareainfo.cpp \
    ..\conimeinfo.cpp \
    ..\conattrs.cpp \
    ..\ConsoleArguments.cpp \
    ..\CommandNumberPopup.cpp \
    ..\CommandListPopup.cpp \
    ..\CopyFromCharPopup.cpp \
    ..\CopyToCharPopup.cpp \


# -------------------------------------
# Sources, Headers, and Libraries
# -------------------------------------

PRECOMPILED_CXX         = 1
PRECOMPILED_INCLUDE     = ..\precomp.h
PRECOMPILED_PCH         = precomp.pch
PRECOMPILED_OBJ         = precomp.obj

INCLUDES = \
    $(INCLUDES); \
    ..; \
    ..\inc; \
    ..\..\buffer\out; \
    ..\..\propsheet; \
    ..\..\propslib; \
    ..\..\terminal\parser; \
    ..\..\terminal\adapter; \
    ..\..\types; \
    ..\..\renderer\inc; \
    ..\..\renderer\gdi; \
    ..\..\renderer\vt; \
    ..\..\renderer\base; \
    $(ONECOREBASE_PRIVATE_WIL_INC_PATH_L); \
    $(SHELL_INC_PATH); \
    $(INTERNAL_SDK_INC_PATH); \
    $(ONECORE_INTERNAL_SDK_INC_PATH); \
    $(ONECORE_EXTERNAL_SDK_INC_PATH); \

# Anything defined here will be defined BEFORE the CRT items the build system adds.
CRTLIBS = \
    $(CRTLIBS) \

# The LIB linking order is $(CRTLIBS) <Build System added refs to msvcrt.lib and friends> $(TARGETLIBS)

TARGETLIBS = \
    $(TARGETLIBS) \
    $(ONECORE_INTERNAL_SDK_LIB_PATH)\onecoreuuid.lib \
    $(ONECOREUAP_INTERNAL_SDK_LIB_PATH)\onecoreuapuuid.lib \
    $(ONECORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\onecore_internal.lib \
    $(ONECOREUAP_EXTERNAL_SDK_LIB_PATH)\propsys.lib \
    $(ONECOREUAP_EXTERNAL_SDK_LIB_PATH)\d2d1.lib \
    $(ONECOREUAP_EXTERNAL_SDK_LIB_PATH)\dwrite.lib \
    $(ONECOREUAP_EXTERNAL_SDK_LIB_PATH)\dxgi.lib \
    $(ONECOREUAP_EXTERNAL_SDK_LIB_PATH)\d3d11.lib \
    $(MODERNCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\api-ms-win-mm-playsound-l1.lib \
    $(ONECORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-dwmapi-ext-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-edputil-policy-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-gdi-dc-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-gdi-dc-create-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-gdi-draw-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-gdi-font-l1.lib \
    $(ONECOREWINDOWS_INTERNAL_LIB_PATH_L)\ext-ms-win-gdi-internal-desktop-l1-1-0.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-caret-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-dialogbox-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-draw-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-keyboard-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-gui-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-menu-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-message-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-misc-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-mouse-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-rectangle-ext-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-server-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-sysparams-ext-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-window-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-rtcore-gdi-object-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-rtcore-gdi-rgn-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-rtcore-ntuser-cursor-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-rtcore-ntuser-dc-access-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-rtcore-ntuser-rawinput-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-rtcore-ntuser-sysparams-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-rtcore-ntuser-window-ext-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-shell-shell32-l1.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-uxtheme-themes-l1.lib \
    $(ONECORESHELL_INTERNAL_LIB_VPATH_L)\api-ms-win-shell-namespace-l1.lib \
    $(MODERNCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-uiacore-l1.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\host\lib\$(O)\conhostv2.lib \
    $(WINCORE_OBJ_PATH)\console\conint\$(O)\conint.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\buffer\out\lib\$(O)\conbufferout.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\tsf\$(O)\contsf.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\propslib\$(O)\conprops.lib \
    $(CONSOLE_OBJ_PATH)\terminal\input\lib\$(O)\ConTermInput.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\terminal\adapter\lib\$(O)\ConTermAdapter.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\terminal\parser\lib\$(O)\ConTermParser.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\renderer\base\lib\$(O)\ConRenderBase.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\renderer\dx\lib\$(O)\ConRenderDx.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\renderer\gdi\lib\$(O)\ConRenderGdi.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\renderer\vt\lib\$(O)\ConRenderVt.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\renderer\wddmcon\lib\$(O)\ConRenderWddmCon.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\server\lib\$(O)\ConServer.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\interactivity\base\lib\$(O)\ConInteractivityBaseLib.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\interactivity\win32\lib\$(O)\ConInteractivityWin32Lib.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\interactivity\onecore\lib\$(O)\ConInteractivityOneCoreLib.lib \
    $(WINCORE_OBJ_PATH)\console\open\src\types\lib\$(O)\ConTypes.lib \

DELAYLOAD = \
    PROPSYS.dll; \
    D2D1.dll; \
    DWrite.dll; \
    DXGI.dll; \
    D3D11.dll; \
    OLEAUT32.dll; \
    api-ms-win-mm-playsound-l1.dll; \
    api-ms-win-shcore-scaling-l1.dll; \
    api-ms-win-shell-namespace-l1.dll; \
    ext-ms-win-dwmapi-ext-l1.dll; \
    ext-ms-win-edputil-policy-l1.dll; \
    ext-ms-win-gdi-dc-l1.dll; \
    ext-ms-win-gdi-dc-create-l1.dll; \
    ext-ms-win-gdi-draw-l1.dll; \
    ext-ms-win-gdi-font-l1.dll; \
    ext-ms-win-gdi-internal-desktop-l1.dll; \
    ext-ms-win-ntuser-caret-l1.dll; \
    ext-ms-win-ntuser-dialogbox-l1.dll; \
    ext-ms-win-ntuser-draw-l1.dll; \
    ext-ms-win-ntuser-keyboard-l1.dll; \
    ext-ms-win-ntuser-gui-l1.dll; \
    ext-ms-win-ntuser-menu-l1.dll; \
    ext-ms-win-ntuser-message-l1.dll; \
    ext-ms-win-ntuser-misc-l1.dll; \
    ext-ms-win-ntuser-mouse-l1.dll; \
    ext-ms-win-ntuser-rectangle-ext-l1.dll; \
    ext-ms-win-ntuser-server-l1.dll; \
    ext-ms-win-ntuser-sysparams-ext-l1.dll; \
    ext-ms-win-ntuser-window-l1.dll; \
    ext-ms-win-rtcore-gdi-object-l1.dll; \
    ext-ms-win-rtcore-gdi-rgn-l1.dll; \
    ext-ms-win-rtcore-ntuser-cursor-l1.dll; \
    ext-ms-win-rtcore-ntuser-dc-access-l1.dll; \
    ext-ms-win-rtcore-ntuser-rawinput-l1.dll; \
    ext-ms-win-rtcore-ntuser-sysparams-l1.dll; \
    ext-ms-win-rtcore-ntuser-window-ext-l1.dll; \
    ext-ms-win-shell-shell32-l1.dll; \
    ext-ms-win-uiacore-l1.dll; \
    ext-ms-win-uxtheme-themes-l1.dll; \

DLOAD_ERROR_HANDLER = kernelbase
