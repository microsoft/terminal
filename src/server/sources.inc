!include ..\..\project.inc

# -------------------------------------
# Windows Console
# - Console Server Communications Layer
# -------------------------------------

# This module encapsulates the communication layer between the console driver
# and the console server application.
# All IOCTL driver communications are handled in this layer as well as management
# activities like process and handle tracking.

# -------------------------------------
# Compiler Settings
# -------------------------------------

# Warning 4201: nonstandard extension used: nameless struct/union
# Warning 4702: unreachable code
MSC_WARNING_LEVEL       = $(MSC_WARNING_LEVEL) /wd4201 /wd4702

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
    ..\ApiDispatchers.cpp \
    ..\ApiDispatchersInternal.cpp \
    ..\ApiMessage.cpp \
    ..\ApiMessageState.cpp \
    ..\ApiSorter.cpp \
    ..\DeviceComm.cpp \
    ..\DeviceHandle.cpp \
    ..\Entrypoints.cpp \
    ..\IoDispatchers.cpp \
    ..\IoSorter.cpp \
    ..\ObjectHandle.cpp \
    ..\ObjectHeader.cpp \
    ..\ProcessHandle.cpp \
    ..\ProcessList.cpp \
    ..\ProcessPolicy.cpp \
    ..\WaitBlock.cpp \
    ..\WaitQueue.cpp \
    ..\WinNTControl.cpp \

INCLUDES= \
    $(INCLUDES); \
    ..; \
