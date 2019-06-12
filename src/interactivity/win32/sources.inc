!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Interactivity for Win32
# -------------------------------------

# This module provides user interaction with the standard
# windowing and input system used by classic Win32 platforms.

# -------------------------------------
# Preprocessor Settings
# -------------------------------------

C_DEFINES               = $(C_DEFINES) -DFE_SB

# -------------------------------------
# Compiler Settings
# -------------------------------------

# Warning 4201: nonstandard extension used: nameless struct/union
MSC_WARNING_LEVEL       = $(MSC_WARNING_LEVEL) /wd4201

# -------------------------------------
# Build System Settings
# -------------------------------------

# Code in the OneCore depot automatically excludes default Win32 libraries.

# Defines IME and Codepage support
W32_SB                  = 1

# -------------------------------------
# Sources, Headers, and Libraries
# -------------------------------------

PRECOMPILED_CXX         = 1
PRECOMPILED_INCLUDE     = ..\precomp.h

SOURCES = \
    ..\AccessibilityNotifier.cpp \
    ..\clipboard.cpp \
    ..\ConsoleControl.cpp \
    ..\ConsoleInputThread.cpp \
    ..\consoleKeyInfo.cpp \
    ..\find.cpp \
    ..\icon.cpp \
    ..\InputServices.cpp \
    ..\menu.cpp \
    ..\screenInfoUiaProvider.cpp \
    ..\SystemConfigurationProvider.cpp \
    ..\UiaTextRange.cpp \
    ..\window.cpp \
    ..\windowdpiapi.cpp \
    ..\windowime.cpp \
    ..\windowio.cpp \
    ..\WindowMetrics.cpp \
    ..\windowproc.cpp \
    ..\windowtheme.cpp \
    ..\windowUiaProvider.cpp \

INCLUDES = \
    $(INCLUDES); \
    ..; \

TARGETLIBS = \
    $(ONECORE_EXTERNAL_SDK_LIB_VPATH_L)\onecore.lib  \
