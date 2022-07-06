!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Audio Functions
# -------------------------------------

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
    ..\MidiAudio.cpp \

INCLUDES = \
    $(INCLUDES); \
    ..; \
    ..\..\..\inc; \
    $(MINWIN_INTERNAL_PRIV_SDK_INC_PATH_L); \
