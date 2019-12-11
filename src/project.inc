# -------------------------------------
# Windows Console
# - Common Project Configuration
# -------------------------------------

# -------------------------------------
# Preprocessor Settings
# -------------------------------------

UNICODE                 = 1
C_DEFINES               = $(C_DEFINES) -DUNICODE -D_UNICODE

# -------------------------------------
# CRT Configuration
# -------------------------------------

USE_UNICRT              = 1
USE_MSVCRT              = 1

USE_STL                 = 1
STL_VER                 = STL_VER_CURRENT
USE_NATIVE_EH           = 1

# -------------------------------------
# Compiler Settings
# -------------------------------------

MSC_WARNING_LEVEL       = /W4 /WX

# -------------------------------------
# Common Console Includes and Libraries
# -------------------------------------
CONSOLE_SRC_PATH        = $(PROJECT_ROOT)\core\console\open\src
CONSOLE_OBJ_PATH        = $(WINCORE_OBJ_PATH)\console\open\src

INCLUDES= \
    $(INCLUDES); \
    $(CONSOLE_SRC_PATH)\inc; \
    $(CONSOLE_SRC_PATH)\..\..\inc; \
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
