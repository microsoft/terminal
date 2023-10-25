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
    ..\CodepointWidthDetector.cpp \
    ..\ColorFix.cpp \
    ..\GlyphWidth.cpp \
    ..\ModifierKeyState.cpp \
    ..\Viewport.cpp \
    ..\convert.cpp \
    ..\colorTable.cpp \
    ..\utils.cpp \
    ..\ThemeUtils.cpp \
    ..\ScreenInfoUiaProviderBase.cpp \
    ..\sgrStack.cpp \
    ..\UiaTextRangeBase.cpp \
    ..\UiaTracing.cpp \
    ..\TermControlUiaProvider.cpp \
    ..\TermControlUiaTextRange.cpp \

INCLUDES= \
    $(INCLUDES); \
    ..; \
