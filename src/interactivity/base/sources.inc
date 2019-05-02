!include ..\..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Interactivity Base Layer
# -------------------------------------

# This module defines the interfaces by which the console will interact
# with a user. This includes unifying various input methods into a single
# abstract method of talking to the console.

# -------------------------------------
# Preprocessor Settings
# -------------------------------------

C_DEFINES               = $(C_DEFINES) -DBUILD_ONECORE_INTERACTIVITY

# -------------------------------------
# Compiler Settings
# -------------------------------------

# Warning 4201: nonstandard extension used: nameless struct/union
MSC_WARNING_LEVEL       = $(MSC_WARNING_LEVEL) /wd4201

# -------------------------------------
# Build System Settings
# -------------------------------------

# Code in the OneCore depot automatically excludes default Win32 libraries.

# Defines IME and Codepage support
W32_SB                  = 1

# -------------------------------------
# Sources, Headers, and Libraries
# -------------------------------------

PRECOMPILED_CXX         = 1
PRECOMPILED_INCLUDE     = ..\precomp.h

SOURCES = \
    ..\ApiDetector.cpp \
    ..\InteractivityFactory.cpp \
    ..\ServiceLocator.cpp \
    ..\VtApiRedirection.cpp \

INCLUDES = \
    $(INCLUDES); \
    ..; \
    ..\..\..\..\..\ConIoSrv; \
    