!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Virtual Terminal Adapter
# -------------------------------------

# This module converts Virtual Terminal style actions into
# class Win32 API calls back into the console host application.
# In conjunction with the parser module, this allows APIs to be called
# simply using the STDOUT stream.

# -------------------------------------
# Preprocessor Settings
# -------------------------------------

C_DEFINES               = $(C_DEFINES)

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
    ..\adaptDispatch.cpp \
    ..\FontBuffer.cpp \
    ..\InteractDispatch.cpp \
    ..\MacroBuffer.cpp \
    ..\adaptDispatchGraphics.cpp \
    ..\terminalOutput.cpp \
    ..\telemetry.cpp \
    ..\tracing.cpp \

INCLUDES = \
    $(INCLUDES); \
    ..; \
    ..\..\parser; \

TARGETLIBS= \
    $(TARGETLIBS) \
    $(ONECORE_EXTERNAL_SDK_LIB_VPATH_L)\onecore.lib \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-keyboard-l1.lib \

DLOAD_ERROR_HANDLER     = kernelbase

DELAYLOAD = \
    ext-ms-win-ntuser-keyboard-l1.dll; \
