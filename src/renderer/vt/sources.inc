!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Renderer for VT
# -------------------------------------

# This module provides a rendering engine implementation that
# renders the display to an outgoing VT stream.

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
    ..\invalidate.cpp \
    ..\math.cpp \
    ..\paint.cpp \
    ..\state.cpp \
    ..\tracing.cpp \
    ..\WinTelnetEngine.cpp \
    ..\XtermEngine.cpp \
    ..\Xterm256Engine.cpp \
    ..\VtSequences.cpp \

INCLUDES = \
    $(INCLUDES); \
    ..; \
    ..\..\..\inc; \
    $(MINWIN_INTERNAL_PRIV_SDK_INC_PATH_L); \
