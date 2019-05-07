!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Renderer Base
# -------------------------------------

# This module provides the base layer for all rendering activities.
# It will fetch data from the main console host server and prepare it
# in a rendering-engine-agnostic fashion so the console code
# can interface with displays on varying platforms.

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
    ..\Cluster.cpp \
    ..\FontInfo.cpp \
    ..\FontInfoBase.cpp \
    ..\FontInfoDesired.cpp \
    ..\RenderEngineBase.cpp \
    ..\renderer.cpp \
    ..\thread.cpp \

INCLUDES = \
    ..; \
    ..\..\..\inc; \
    $(MINWIN_INTERNAL_PRIV_SDK_INC_PATH_L); \
