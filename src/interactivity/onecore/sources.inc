!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Interactivity for OneCore
# -------------------------------------

# This module defines interaction with the user on
# system configurations with minimal input/output support

# -------------------------------------
# Compiler Settings
# -------------------------------------

# Warning 4201: nonstandard extension used: nameless struct/union
MSC_WARNING_LEVEL       = $(MSC_WARNING_LEVEL) /wd4201

# -------------------------------------
# Build System Settings
# -------------------------------------

# Code in the OneCore depot automatically excludes default Win32 libraries.

# -------------------------------------
# Sources, Headers, and Libraries
# -------------------------------------

PRECOMPILED_CXX         = 1
PRECOMPILED_INCLUDE     = ..\precomp.h

SOURCES = \
    ..\AccessibilityNotifier.cpp \
    ..\BgfxEngine.cpp \
    ..\ConIoSrvComm.cpp \
    ..\ConsoleControl.cpp \
    ..\ConsoleInputThread.cpp \
    ..\ConsoleWindow.cpp \
    ..\SystemConfigurationProvider.cpp \
    ..\WindowMetrics.cpp \

INCLUDES = \
    $(INCLUDES); \
    ..; \
    ..\..\..\..\..\ConIoSrv; \
