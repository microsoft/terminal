!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Virtual Terminal Input
# -------------------------------------

# This module converts Windows-style input into Virtual Terminal style character
# sequences for consumption from vt-aware applications.

# -------------------------------------
# Preprocessor Settings
# -------------------------------------

C_DEFINES               = $(C_DEFINES) -DBUILD_ONECORE_INTERACTIVITY

# -------------------------------------
# Build System Settings
# -------------------------------------

# Code in the OneCore depot automatically excludes default Win32 libraries.

# -------------------------------------
# Sources, Headers, and Libraries
# -------------------------------------

PRECOMPILED_CXX         = 1
PRECOMPILED_INCLUDE     = ..\precomp.h

SOURCES= \
    ..\terminalInput.cpp \

INCLUDES = \
    $(INCLUDES); \
    ..; \

TARGETLIBS= \
    $(TARGETLIBS) \
    $(ONECORE_SDK_LIB_VPATH)\onecore.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-keyboard-l1.lib \

DLOAD_ERROR_HANDLER     = kernelbase

DELAYLOAD = \
    ext-ms-win-ntuser-keyboard-l1.dll; \
