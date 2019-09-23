# -------------------------------------
# Windows Console
# - Common Test Project Configuration
# -------------------------------------

!include project.inc

# -------------------------------------
# Preprocessor Settings
# -------------------------------------

C_DEFINES               = $(C_DEFINES) -DINLINE_TEST_METHOD_MARKUP -DUNIT_TESTING

# -------------------------------------
# Program Information
# -------------------------------------

TARGET_DESTINATION      = UnitTests
UNIVERSAL_TEST          = 1
TEST_CODE               = 1

# -------------------------------------
# Common Console Includes and Libraries
# -------------------------------------

INCLUDES = \
    $(INCLUDES); \
    $(CONSOLE_SRC_PATH)\inc\test; \
    $(CONSOLE_SRC_PATH)\..\..\inc; \
    $(ONECORESDKTOOLS_INTERNAL_INC_PATH_L)\wextest\cue; \

TARGETLIBS = \
    $(TARGETLIBS) \
    $(ONECORE_EXTERNAL_SDK_LIB_VPATH_L)\onecore.lib \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Wex.Common.lib \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Wex.Logger.lib \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Te.Common.lib \
