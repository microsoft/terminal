!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Virtual Terminal Parser
# -------------------------------------

# This module provides the ability to parse an incoming stream
# of text for Virtual Terminal Sequences.
# These sequences are in-band signaling embedded within the stream
# that signals the console to do something special (beyond just displaying as text).

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

SOURCES = \
    ..\stateMachine.cpp \
    ..\InputStateMachineEngine.cpp \
    ..\OutputStateMachineEngine.cpp \
    ..\telemetry.cpp \
    ..\tracing.cpp \

INCLUDES = \
    $(INCLUDES); \
    ..; \
    $(MINWIN_INTERNAL_PRIV_SDK_INC_PATH_L); \

TARGETLIBS = \
    $(TARGETLIBS) \
    $(MINCORE_INTERNAL_PRIV_SDK_LIB_VPATH_L)\ext-ms-win-ntuser-keyboard-l1.lib \

DLOAD_ERROR_HANDLER     = kernelbase

DELAYLOAD = \
    ext-ms-win-ntuser-keyboard-l1.dll; \
