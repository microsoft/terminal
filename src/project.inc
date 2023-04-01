# -------------------------------------
# Windows Console
# - Common Project Configuration
# -------------------------------------

!include $(PROJECT_ROOT)\vcpkg\consume.inc

# -------------------------------------
# Preprocessor Settings
# -------------------------------------

UNICODE                 = 1
C_DEFINES               = $(C_DEFINES) -DUNICODE -D_UNICODE -DFMT_HEADER_ONLY -D__INSIDE_WINDOWS -DBUILD_ONECORE_INTERACTIVITY

# -------------------------------------
# CRT Configuration
# -------------------------------------

USE_UNICRT              = 1
USE_MSVCRT              = 1
NO_WCHAR_T              = 1 # use native wchar_t
USE_CXX17_STD_BYTE      = 1 # Windows disables std::byte by default...


USE_STL                 = 1
STL_VER                 = STL_VER_CURRENT
USE_NATIVE_EH           = 1

# -------------------------------------
# Compiler Settings
# -------------------------------------

USE_STD_CPP20           = 1
MSC_WARNING_LEVEL       = /W4 /WX
USER_C_FLAGS            = $(USER_C_FLAGS) /fp:contract /utf-8

# -------------------------------------
# Common Console Includes and Libraries
# -------------------------------------
CONSOLE_SRC_PATH        = $(PROJECT_ROOT)\core\console\open\src
CONSOLE_OBJ_PATH        = $(WINCORE_OBJ_PATH)\console\open\src

USER_C_FLAGS            = $(USER_C_FLAGS) /FI$(CONSOLE_OBJ_PATH)\staging\$(O)\TilFeatureStaging.h

INCLUDES= \
    $(INCLUDES); \
    $(CONSOLE_SRC_PATH)\inc; \
    $(CONSOLE_SRC_PATH)\..\..\inc; \
    $(CONSOLE_SRC_PATH)\..\oss\dynamic_bitset; \
    $(CONSOLE_SRC_PATH)\..\oss\libpopcnt; \
    $(CONSOLE_SRC_PATH)\..\oss\chromium; \
    $(CONSOLE_SRC_PATH)\..\oss\fmt\include; \
    $(CONSOLE_SRC_PATH)\..\oss\interval_tree; \
    $(CONSOLE_SRC_PATH)\..\oss\pcg\include; \
    $(MINWIN_INTERNAL_PRIV_SDK_INC_PATH_L); \
    $(MINWIN_RESTRICTED_PRIV_SDK_INC_PATH_L); \
    $(MINCORE_INTERNAL_PRIV_SDK_INC_PATH_L); \
    $(ONECORE_INTERNAL_SDK_INC_PATH); \
    $(ONECORE_EXTERNAL_SDK_INC_PATH); \

# -------------------------------------
# Linker Settings
# -------------------------------------

# Add the ConsoleTypes.natvis to our binaries, so the PDB's will have the info
# embedded by default
LINKER_FLAGS            = $(LINKER_FLAGS) /natvis:$(CONSOLE_SRC_PATH)\..\tools\ConsoleTypes.natvis
