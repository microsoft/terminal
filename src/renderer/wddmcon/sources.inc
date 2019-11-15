!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Renderer for WDDMCON
# -------------------------------------

# This module provides a rendering engine implementation that
# utilizes the WDDMCON library for drawing the console to a fullscreen
# DirectX surface.

# -------------------------------------
# CRT Configuration
# -------------------------------------

BUILD_FOR_CORESYSTEM    = 1

# -------------------------------------
# Sources, Headers, and Libraries
# -------------------------------------

PRECOMPILED_CXX         = 1
PRECOMPILED_INCLUDE     = ..\precomp.h

INCLUDES = \
    $(INCLUDES); \
    ..; \
    ..\..\inc; \
    ..\..\..\inc; \
    ..\..\..\host; \
    $(MINWIN_INTERNAL_PRIV_SDK_INC_PATH_L); \
    $(MINWIN_RESTRICTED_PRIV_SDK_INC_PATH_L); \

SOURCES = \
    $(SOURCES) \
    ..\main.cxx \
    ..\WddmConRenderer.cpp \
