!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Renderer for GDI
# -------------------------------------

# This module provides a rendering engine implementation that
# utilizes the GDI framework for drawing the console to a window.

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

INCLUDES = \
    ..; \
    ..\..\..\inc; \
    $(MINWIN_INTERNAL_PRIV_SDK_INC_PATH_L); \
