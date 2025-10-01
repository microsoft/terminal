!include ..\sources.inc

# -------------------------------------
# Program Information
# -------------------------------------

TARGET_DESTINATION      = UnitTests

UNIVERSAL_TEST          = 1
TEST_CODE               = 1

# -------------------------------------
# Preprocessor Settings
# -------------------------------------

C_DEFINES               = $(C_DEFINES) -DINLINE_TEST_METHOD_MARKUP -DUNIT_TESTING

# -------------------------------------
# Sources, Headers, and Libraries
# -------------------------------------

INCLUDES = \
    $(INCLUDES); \
    ..\..\inc\test; \
    $(ONECORESDKTOOLS_INTERNAL_INC_PATH_L)\wextest\cue; \

TARGETLIBS = \
    $(TARGETLIBS) \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Wex.Common.lib \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Wex.Logger.lib \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Te.Common.lib \

