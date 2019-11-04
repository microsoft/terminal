!include ..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Types Library
# -------------------------------------

# This module encapsulates types and helpers that are common
# across the entire console project

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
    ..\CodepointWidthDetector.cpp \
    ..\IInputEvent.cpp \
    ..\FocusEvent.cpp \
    ..\GlyphWidth.cpp \
    ..\KeyEvent.cpp \
    ..\MenuEvent.cpp \
    ..\ModifierKeyState.cpp \
    ..\MouseEvent.cpp \
    ..\Viewport.cpp \
    ..\WindowBufferSizeEvent.cpp \
    ..\convert.cpp \
    ..\Utf16Parser.cpp \
    ..\utils.cpp \
    ..\ThemeUtils.cpp \

INCLUDES= \
    $(INCLUDES); \
    ..; \
