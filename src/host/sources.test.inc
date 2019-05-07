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

# prepend the ConRenderVt.Unittest.lib, so that it's linked before the non-ut version.

TARGETLIBS = \
    $(WINCORE_OBJ_PATH)\console\open\src\renderer\vt\ut_lib\$(O)\ConRenderVt.Unittest.lib \
    $(TARGETLIBS) \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Wex.Common.lib \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Wex.Logger.lib \
    $(ONECORESDKTOOLS_INTERNAL_LIB_PATH_L)\WexTest\Cue\Te.Common.lib \

