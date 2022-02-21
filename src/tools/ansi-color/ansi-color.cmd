@ECHO OFF & :: ANSI-COLOR :: Authored by Ryan Beesley :: https://github.com/rbeesley
GOTO :DEFINE_MACROS

%=- Entry point after macro definitions -=%
:MAIN
SETLOCAL ENABLEDELAYEDEXPANSION
CALL :PARSE_ARGS %1 %2 %3 %4 %5 %6 %7 %8 %9
:: Error when parsing
IF ERRORLEVEL 1 %@exit% %ERRORLEVEL%
:: Parsing success
IF ERRORLEVEL 0 GOTO :CONFIGURATION
:: Help requested, but this shouldn't actually be an error from CMD's perspective
IF ERRORLEVEL -1 %@exit% 0


%=- Configuration -=%
:CONFIGURATION
:: Default test text if not defined in the data segment
SET "CELL= gYw "
:: Uncomment to use UTF-8 for output, such as when the test text is Unicode
REM SET "SHOW.UTF8=#TRUE#"
:: Uncomment to disable the spinner and speed up processing
REM SET "SPINNER.DISABLED=#TRUE#"
:: Uncomment to display the ANSI Escape Sequence control characters
REM SET "SHOW.ANSI=#TRUE#"
:: Uncomment to show cell R1C1 reference addressing instead of cell text
REM SET "SHOW.R1C1_REFERENCE=#TRUE#"
%=- End Configuration -=%


%=- Data Segment -=%
GOTO :END_DATA_SEGMENT

:: Complete matrix of SGR parameters available
:: This definition also demonstrates the various configuration changes
:: which are used to control the way the table is generated
__DATA__
:: Select Graphic Rendition (SGR) parameters
:: #NUL# is treated as a special case to provide cells in that column
:: or row, but there is no row or column value applied to the cell.
:: This has the effect that the row or column has no SGR parameter applied
:: and so this will show the default.
:: #SPC# is a special case which can be used to make gaps in the table.
:: Whereas #NUL# still outputs the CELL text, #SPC# won't show anything in
:: that row. #SPC# can be used in columns to also provide a gap which matches
:: cell width.
:: #LBL# is also a special case similar to #SPC#. #LBL# makes it possible to
:: write a text label in the middle of a generated table for a particular row.
:: Formatting can be applied to the labels and an SGR RESET is automatically
:: applied at the end of the string.
__TABLE__
:: If the definition is defined using Unicode characters, uncomment and include
:: the following line in the __TABLE__ section of the definition file.
REM SET "UTF8.REQUIRED=#TRUE#"
:: The test text
SET "STUBHEAD=SGR"
:: Alignment properties for the cells and headers
REM SET "ALIGN.CELL=C"
REM SET "ALIGN.BOXHEAD=R"
REM SET "ALIGN.STUB=L"
REM SET "ALIGN.STUBHEAD=C"
:: Separator characters for cells and headers
REM SET "SEPARATOR.STUB= "
REM SET "SEPARATOR.STUB=│"                & :: UTF-8
REM SET "SEPARATOR.BOXHEAD= "
REM SET "SEPARATOR.BOXHEAD=─"             & :: UTF-8
SET "SEPARATOR.COL= "
REM SET "SEPARATOR.COL=╎"
REM SET "SEPARATOR.STUBHEAD_BOXHEAD= "
REM SET "SEPARATOR.STUBHEAD_BOXHEAD=:"
REM SET "SEPARATOR.STUBHEAD_BOXHEAD=│"    & :: UTF-8
REM SET "SEPARATOR.STUBHEAD_BOXHEAD=▓▓"   & :: UTF-8
REM SET "SEPARATOR.STUBHEAD_STUB= "
REM SET "SEPARATOR.STUBHEAD_STUB=-"
REM SET "SEPARATOR.STUBHEAD_STUB=─"       & :: UTF-8
REM SET "SEPARATOR.STUBHEAD_STUB=▓"       & :: UTF-8
REM SET "SEPARATOR.INTERSECT= "
REM SET "SEPARATOR.INTERSECT=┼"           & :: UTF-8
REM SET "SEPARATOR.INTERSECT=┘"           & :: UTF-8
REM SET "SEPARATOR.INTERSECT=▓▒▒░"        & :: UTF-8
REM SET "SEPARATOR.INTERSECT=+"
REM SET "SEPARATOR.BOXHEAD_BODY= "
REM SET "SEPARATOR.BOXHEAD_BODY=─"        & :: UTF-8
REM SET "SEPARATOR.BOXHEAD_BODY=░"        & :: UTF-8
REM SET "SEPARATOR.STUB_BODY= "
REM SET "SEPARATOR.STUB_BODY=│"           & :: UTF-8
REM SET "SEPARATOR.STUB_BODY=░░"          & :: UTF-8
REM SET "SEPARATOR.BOXHEADERS= "
REM SET "SEPARATOR.BOXHEADERS=╎"          & :: UTF-8
REM SET "SEPARATOR.CELL= "
REM SET "SEPARATOR.CELL=╎"                & :: UTF-8
:: Conditional definitions like this must be one line per statement
:: and can be used to define how to fall back to ANSI if Unicode isn't supported
IF DEFINED SHOW.UTF8 (SET "SEPARATOR.STUBHEAD_BOXHEAD=│") ELSE (SET "SEPARATOR.STUBHEAD_BOXHEAD=:")
IF DEFINED SHOW.UTF8 (SET "SEPARATOR.STUBHEAD_STUB=─") ELSE (SET "SEPARATOR.STUBHEAD_STUB=-")
IF DEFINED SHOW.UTF8 (SET "SEPARATOR.INTERSECT=┘") ELSE (SET "SEPARATOR.INTERSECT=+")
:: You can also define control for formatting
SET "SECTION=!CSI!1;4m"
__TABLE:END__

:: Background
__COLS__
#NUL#
REM 1m
REM 2m
REM 3m
REM 4m
REM 5m
REM 6m
REM 7m
REM 8m
REM 9m
REM 21m
40m
100m
41m
101m
42m
102m
43m
103m
44m
104m
45m
105m
46m
106m
47m
107m
__COLS:END__

:: [Intensity;][Attribute;]Foreground
__ROWS__
#NUL#
#SPC#
:: Attributes
#LBL# !SECTION!Attributes
1m
2m
3m
4m
5m
6m
7m
8m
9m
REM 10m
REM 11m
REM 12m
REM 13m
REM 14m
REM 15m
REM 16m
REM 17m
REM 18m
REM 19m
REM 20m
21m
REM 22m
REM 23m
REM 24m
REM 25m
REM 26m
REM 27m
REM 28m
REM 29m
#SPC#
:: Normal
#LBL# !SECTION!Normal
30m
90m
31m
91m
32m
92m
33m
93m
34m
94m
35m
95m
36m
96m
37m
97m
#SPC#
:: Bold or increased intensity, 1
#LBL# !SECTION!Bold or increased intensity, 1
1;30m
1;90m
1;31m
1;91m
1;32m
1;92m
1;33m
1;93m
1;34m
1;94m
1;35m
1;95m
1;36m
1;96m
1;37m
1;97m
#SPC#
:: Faint (decreased intensity), 2
#LBL# !SECTION!Faint (decreased intensity), 2
2;30m
2;90m
2;31m
2;91m
2;32m
2;92m
2;33m
2;93m
2;34m
2;94m
2;35m
2;95m
2;36m
2;96m
2;37m
2;97m
#SPC#
:: Italic, 3
#LBL# !SECTION!Italic, 3
3;30m
3;90m
3;31m
3;91m
3;32m
3;92m
3;33m
3;93m
3;34m
3;94m
3;35m
3;95m
3;36m
3;96m
3;37m
3;97m
#SPC#
:: Underline, 4
#LBL# !SECTION!Underline, 4
4;30m
4;90m
4;31m
4;91m
4;32m
4;92m
4;33m
4;93m
4;34m
4;94m
4;35m
4;95m
4;36m
4;96m
4;37m
4;97m
#SPC#
:: Slow Blink, 5
#LBL# !SECTION!Slow Blink, 5
5;30m
5;90m
5;31m
5;91m
5;32m
5;92m
5;33m
5;93m
5;34m
5;94m
5;35m
5;95m
5;36m
5;96m
5;37m
5;97m
#SPC#
:: Rapid Blink, 6
#LBL# !SECTION!Rapid Blink, 6
6;30m
6;90m
6;31m
6;91m
6;32m
6;92m
6;33m
6;93m
6;34m
6;94m
6;35m
6;95m
6;36m
6;96m
6;37m
6;97m
#SPC#
:: Reverse video, 7
#LBL# !SECTION!Reverse video, 7
7;30m
7;90m
7;31m
7;91m
7;32m
7;92m
7;33m
7;93m
7;34m
7;94m
7;35m
7;95m
7;36m
7;96m
7;37m
7;97m
#SPC#
:: Conceal, 8
#LBL# !SECTION!Conceal, 8
8;30m
8;90m
8;31m
8;91m
8;32m
8;92m
8;33m
8;93m
8;34m
8;94m
8;35m
8;95m
8;36m
8;96m
8;37m
8;97m
#SPC#
:: Crossed-out, 9
#LBL# !SECTION!Crossed-out, 9
9;30m
9;90m
9;31m
9;91m
9;32m
9;92m
9;33m
9;93m
9;34m
9;94m
9;35m
9;95m
9;36m
9;96m
9;37m
9;97m
#SPC#
:: Double Underline, 21
#LBL# !SECTION!Double Underline, 21
21;30m
21;90m
21;31m
21;91m
21;32m
21;92m
21;33m
21;93m
21;34m
21;94m
21;35m
21;95m
21;36m
21;96m
21;37m
21;97m
__ROWS:END__
__DATA:END__

:END_DATA_SEGMENT
%=- End Data Segment -=%


%=- Main Script -=%
SETLOCAL ENABLEDELAYEDEXPANSION

:INITIALIZATION
:: Configure ANSI escape sequences. This has a dependency on PowerShell for creating the escape code
REM FOR /F "tokens=* USEBACKQ" %%G IN (`PowerShell -Command "[char]0x1B"`) DO (SET "ESC=%%G")
:: Otherwise you need to use an unprintable character
SET "ESC="

:: If we're going to display the ANSI Escape Sequence control characters
:: we require UTF-8, will disable the spinner, and override the ANSI ESC
:: character with a Unicode codepoint which represents ESC.
IF DEFINED SHOW.ANSI (
  SET "UTF8.REQUIRED=#TRUE#"
  SET "SPINNER.DISABLED=#TRUE#"
)

:: The console (CMD) is normally not UTF-8, so preserve the codepage so we can reset it.
:: Because the output of CHCP is localized, we grab the last value of the string,
:: which we have our fingers crossed, will be the codepage.
FOR /F "tokens=*" %%_ IN ('chcp 2^>^&1 ^& FOR /F %%_ IN ^("ERRORLEVEL"^) DO @CALL ECHO __ERRORLEVEL__:%%%%_%%') DO (
  SET CHCP_OUT=%%_
  :: Check if the CHCP call failed
  IF ["!CHCP_OUT:~0,15!"] EQU ["__ERRORLEVEL__:"] (
    SET "CHCP_ERR=!CHCP_OUT:__ERRORLEVEL__:=!"
    IF [!CHCP_ERR!] NEQ [0] (
      :: This means that the call to CHCP failed
      ECHO Warning: Error reading the active codepage, check the output from the command CHCP.
      IF DEFINED SHOW.UTF8 (
        :: UTF-8 support is required if SHOW.UTF8 is defined, so error out
        ECHO.
        ECHO Error: UTF-8 support is required for the definition file in use or flags provided.
        :: Since this was fatal, also pass the error code received trying to call CHCP
        %@exit% !CHCP_ERR!
      ) ELSE (
        :: If UTF-8 support isn't required, maybe we can swallow this error and still show ANSI definition file
        SET "SHOW.UTF8="
        :: Clear the CHCP_RET value as nothing returned should be considered valid
        SET "CHCP_RET="
      )
    )
  ) ELSE (
    SET "CHCP_RET=!CHCP_OUT!"
  )
)

:: If we didn't get an error, read the last value of the output string
FOR %%_ IN (!CHCP_RET!) DO (SET CHCP=%%_)
IF DEFINED CHCP (
  IF [!CHCP!] EQU [65001] (
    :: If the CHCP value was 65001, we can show UTF8 and we don't need to check more
    SET "SHOW.UTF8=#TRUE#" 
  ) ELSE (
    :: Otherwise we now want to set the codepage to 65001 and exit with the error if that fails
    IF DEFINED SHOW.UTF8 (
      chcp 65001 > NUL 2>&1
      IF ERRORLEVEL 1 (
        ECHO Error: Unable to set the active codepage
        %@exit% %ErrorLevel%
      )
    )
  )
)
SET "CHCP_OUT="
SET "CHCP_ERR="
SET "CHCP_RET="

:: This is a separate code block from the test above so that the active console Code Page 
:: is changed first. If we want to show the ANSI characters rather than actually drawing 
:: them, we need to replace ESC with a Unicode codepoint which represents ESC. We do this
:: now that UTF8 has been configured for the console and before we define the CSI commands.
IF DEFINED SHOW.ANSI (
  SET "ESC=␛"
)

:: Control Sequence Introducer (CSI)
SET "CSI=!ESC!["

:: Cursor Up
SET "CUU=!CSI!A"
:: Cursor Down
SET "CUD=!CSI!B"
:: Cursor Forward
SET "CUF=!CSI!C"
:: Cursor Back
SET "CUB=!CSI!D"
:: Cursor Previous Line // will work like a CR without LF when appended to an echo
SET "CPL=!CSI!F"
:: Select Graphic Rendition (SGR) Reset // reset colors and attributes
SET "RESET=!CSI!m"

:: We will want to calculate the max widths as we process the data segment
SET /A STUB.MAX_WIDTH=0
SET /A COL.MAX_WIDTH=0

:: If showing the R1C1 reference address of each cell instead of the cell value, assume
:: there will be at most R999C99 cells and set the width at 7
IF DEFINED SHOW.R1C1_REFERENCE (
  SET /A COL.MAX_WIDTH=7
)

:: Default properties if not defined in the data segment
:: See Figure 1.1 in the PDF for Wang Terminology
:: https://uwspace.uwaterloo.ca/handle/10012/10962

:: Fields
SET "STUBHEAD="

:: Alignment
SET "ALIGN.CELL=C"
SET "ALIGN.CELL.R1C1=R"
SET "ALIGN.STUBHEAD=C"
SET "ALIGN.BOXHEAD=C"
SET "ALIGN.STUB=R"
SET "ALIGN.STUBHEAD_STUB=C"
SET "ALIGN.STUB_BODY=C"
SET "ALIGN.BOXHEAD_BODY=C"
SET "ALIGN.INTERSECT=C"
SET "ALIGN.STUB_BOXHEAD_INTERSECT="

:: Separators
SET "SEPARATOR.STUB="
SET "SEPARATOR.STUBHEAD_BOXHEAD="
SET "SEPARATOR.STUB_BODY="
SET "SEPARATOR.COL="
SET "SEPARATOR.BOXHEADERS="
SET "SEPARATOR.CELL="
SET "SEPARATOR.STUBHEAD_STUB="
SET "SEPARATOR.BOXHEAD_BODY="
SET "SEPARATOR.BOXHEAD="
SET "SEPARATOR.INTERSECT="
SET "SEPARATOR.STUB_BOXHEAD_INTERSECT="
SET "SEPARATOR.CELL_INTERSECT="

:: Flags
SET "SEPARATOR.VERTICAL="
SET "SEPARATOR.HORIZONTAL="
SET "SEPARATOR.COLUMN="

:: Read and parse the data, validate the configuration, calculate table headings,
:: build the table, and show the table
CALL :READ_DATA_SEGMENT
IF ERRORLEVEL 1 %@exit% %ERRORLEVEL%
CALL :VALIDATE_CONFIGURATION
IF ERRORLEVEL 1 %@exit% %ERRORLEVEL%
CALL :RESOLVE_SEPARATORS
CALL :BUILD_TABLE
CALL :DISPLAY_TABLE

:: Restore the console Code Page saved at the beginning of the script
IF DEFINED CHCP (
  chcp !CHCP! > NUL 2>&1
  IF ERRORLEVEL 1 (
    ECHO Error: Unable to reset the active codepage
    %@exit% %ErrorLevel%
  )
)

:: Final blank line to match ColorTool.exe -c output and to improve screen readability
ECHO.

:: Exit
%@exit% 0

:: Should never be reached
ECHO Failed to EXIT cleanly
CMD /C EXIT -1073741510


:: Routine to process the data segment of a file, used to build the table
:READ_DATA_SEGMENT
SET "DATA_SEGMENT="
FOR /F "delims=" %%_ IN (!DATA_FILE!) DO (
    SET "DATA=%%_"
    IF /I ["!DATA:~0,12!"] EQU ["__ROWS:END__"] SET "SEGMENT="
    IF /I ["!DATA:~0,12!"] EQU ["__COLS:END__"] SET "SEGMENT="
    IF /I ["!DATA:~0,13!"] EQU ["__TABLE:END__"] SET "SEGMENT="
    IF /I ["!DATA:~0,12!"] EQU ["__DATA:END__"] SET "DATA_SEGMENT="
    IF DEFINED DATA_SEGMENT (
      CALL :PARSE_DATA_SEGMENT !DATA!
      IF ERRORLEVEL 1 %@exit% %ERRORLEVEL%
    )
    IF /I ["!DATA:~0,8!"] EQU ["__ROWS__"] (
      SET "SEGMENT=ROWS"
      :: Initialize the row globals
      SET /A ROW[#]=0
      SET /A ROWS.LEN=0
    )
    IF /I ["!DATA:~0,8!"] EQU ["__COLS__"] (
      SET "SEGMENT=COLS"
      :: Initialize the column globals
      SET /A COL[#]=0
      SET /A COLS.LEN=0
    )
    IF /I ["!DATA:~0,9!"] EQU ["__TABLE__"] (
      SET "SEGMENT=TABLE"
      :: Initialize the table globals
      SET /A TABLE[#]=0
    )
    IF /I ["!DATA:~0,8!"] EQU ["__DATA__"] (
      SET "DATA_SEGMENT=#TRUE#"
      SET "SEGMENT="
    )
)
%@exit%


:: Process each segment type
:PARSE_DATA_SEGMENT
IF NOT DEFINED SEGMENT %@exit%
SET "DATA=%*"
:: We're parsing the data segment, so clean up the data before further processing
%@trim% DATA

:: Skip over any blank lines or comments
IF NOT DEFINED DATA %@exit%
IF /I ["!DATA!"] EQU ["REM"] %@exit%
IF /I ["!DATA!"] EQU ["@REM"] %@exit%
IF /I ["!DATA:~0,2!"] EQU ["::"] %@exit%
IF /I ["!DATA:~0,4!"] EQU ["REM "] %@exit%
IF /I ["!DATA:~0,5!"] EQU ["@REM "] %@exit%

:: Advance and output the spinner animation if not disabled
IF NOT DEFINED SPINNER.DISABLED (
  %@spinner% SPINNER.FRAME
)

:: Dispatch to TABLE, COLS, or ROWS parsing routines
IF /I ["!SEGMENT!"] EQU ["TABLE"] CALL :PARSE_TABLE_DATA !DATA!
IF /I ["!SEGMENT!"] EQU ["COLS"] CALL :PARSE_COLS_DATA !DATA!
IF /I ["!SEGMENT!"] EQU ["ROWS"] CALL :PARSE_ROWS_DATA !DATA!
%@exit%


:PARSE_TABLE_DATA
:: Only eval single line SET or IF statements
SET "DATA=%*"
IF /I ["!DATA:~0,4!"] EQU ["SET "] GOTO :EVAL_TABLE_DATA
IF /I ["!DATA:~0,3!"] EQU ["IF "] GOTO :EVAL_TABLE_DATA
%@exit%


:EVAL_TABLE_DATA
:: Eval the TABLE data directly
%*
:: If there is an error, we want to catch it
IF ERRORLEVEL 1 %@exit% %ERRORLEVEL%
:: Otherwise just exit the routine
%@exit%


:PARSE_COLS_DATA
SET "COL=%*"
:: Set the column header text and track the max width
IF ["!COL:~0,1!"] EQU ["#"] (
  IF ["!COL:~4,1!"] EQU ["#"] (
    :: Special case for #???# tokens
    SET /A "COL.LEN=0"
  ) ELSE (
    %@strlen% COL COL.LEN
  )
) ELSE (
  %@strlen% COL COL.LEN
)
%@maxval% COL.MAX_WIDTH COL.LEN
:: Set the col index and store value
SET /A COL[#]+=1
SET "COL[!COL[#]!]=!COL!"
%@exit%


:PARSE_ROWS_DATA
SET "ROW=%*"
:: Set the row header text
IF ["!ROW:~0,1!"] EQU ["#"] (
  IF ["!ROW:~4,1!"] EQU ["#"] (
    :: Special case for #???# tokens
    SET /A "ROW.LEN=0"
  ) ELSE (
    %@strlen% ROW ROW.LEN
  )
) ELSE (
  %@strlen% ROW ROW.LEN
)
%@maxval% STUB.MAX_WIDTH ROW.LEN
:: Set the row index and store value
SET /A ROW[#]+=1
SET "ROW[!ROW[#]!]=!ROW!"
%@exit%


:: Validate that we can render the definition file
:VALIDATE_CONFIGURATION
:: Does the definition require UTF-8
IF DEFINED UTF8.REQUIRED (
  IF NOT DEFINED SHOW.UTF8 (
    SET "SCRIPT_NAME=%~nx0"
    SET "MSG=Error: UTF-8 console support is required.!LF!       Try ^"!SCRIPT_NAME! /U [^<definition_file^>]^" to enable UTF-8 support!LF!       or change the default configuration in !SCRIPT_NAME! to always use UTF-8."
    CALL :USAGE MSG
    %@exit% 1
  )
)
%@exit%


:RESOLVE_SEPARATORS
:: There's a lot to consider with separators in trying to do what is best
:: for the definition author. Look at the separator.def, ansi-colortool.def,
:: and fgbg.def for examples for how to most effectively manage this.

:: Separators have cascading effects, so if some are not defined this is where
:: they are defined.

:: Define the column separator
::
:: SEPARATOR.COL is the default column separator and it will be overridden
:: by SEPARATOR.BOXHEADERS or SEPARATOR.CELL.
:: If SEPARATOR.COL is not defined, and SEPARATOR.BOXHEADERS is defined
:: but SEPARATOR.CELL is not defined, or vice versa, then the undefined 
:: separator will be defined with a space to preserve proper separation.
IF DEFINED SEPARATOR.COL (
  SET "SEPARATOR.COLUMN=#TRUE#"
  IF NOT DEFINED SEPARATOR.BOXHEADERS (
    SET "SEPARATOR.BOXHEADERS=!SEPARATOR.COL!"
  )
  IF NOT DEFINED SEPARATOR.CELL (
    SET "SEPARATOR.CELL=!SEPARATOR.COL!"
  )
) ELSE (
  IF DEFINED SEPARATOR.BOXHEADERS (
  SET "SEPARATOR.COLUMN=#TRUE#"
    IF NOT DEFINED SEPARATOR.CELL (
      SET "SEPARATOR.CELL= "
    )
  )
  IF DEFINED SEPARATOR.CELL (
  SET "SEPARATOR.COLUMN=#TRUE#"
    IF NOT DEFINED SEPARATOR.BOXHEADERS (
      SET "SEPARATOR.BOXHEADERS= "
    )
  )
)

:: Define the vertical separator
::
:: SEPARATOR.STUB is the default vertical separator and it will be
:: overridden by SEPARATOR.STUBHEAD_BOXHEAD or SEPARATOR.STUB_BODY.
:: If SEPARATOR.STUB is not defined, and SEPARATOR.STUB_BODY is defined
:: but SEPARATOR.STUBHEAD_BOXHEAD is not defined, or vice versa, then
:: the undefined separator will be defined with a space to preserve
:: proper separation.
IF DEFINED SEPARATOR.STUB (
  SET "SEPARATOR.VERTICAL=#TRUE#"
  IF NOT DEFINED SEPARATOR.STUBHEAD_BOXHEAD (
    SET "SEPARATOR.STUBHEAD_BOXHEAD=!SEPARATOR.STUB!"
  )
  IF NOT DEFINED SEPARATOR.STUB_BODY (
    SET "SEPARATOR.STUB_BODY=!SEPARATOR.STUB!"
  )
) ELSE (
  IF DEFINED SEPARATOR.STUB_BODY (
    SET "SEPARATOR.VERTICAL=#TRUE#"
    IF NOT DEFINED SEPARATOR.STUBHEAD_BOXHEAD (
      SET "SEPARATOR.STUBHEAD_BOXHEAD= "
    )
  )
  IF DEFINED SEPARATOR.STUBHEAD_BOXHEAD (
    SET "SEPARATOR.VERTICAL=#TRUE#"
    IF NOT DEFINED SEPARATOR.STUB_BODY (
      SET "SEPARATOR.STUB_BODY= "
    )
  )
)

:: If there is no vertical separator defined, but there is a column separator
:: assume that the column separator should be uniformally applied. This can be
:: forcibly overridden by putting SET "SEPARATOR.VERTICAL=#TRUE#" in the table
:: definition and leaving SEPARATOR.STUBHEAD_BOXHEAD and SEPARATOR.STUB_BODY
:: undefined.
IF NOT DEFINED SEPARATOR.VERTICAL (
  IF DEFINED SEPARATOR.COLUMN (
    SET "SEPARATOR.VERTICAL=#TRUE#"
    SET "SEPARATOR.STUBHEAD_BOXHEAD=!SEPARATOR.BOXHEADERS!"
    SET "SEPARATOR.STUB_BODY=!SEPARATOR.CELL!"
    IF NOT DEFINED SEPARATOR.INTERSECT (
      SET "SEPARATOR.INTERSECT=!SEPARATOR.CELL_INTERSECT!"
    )
    IF NOT DEFINED SEPARATOR.CELL_INTERSECT (
      SET "SEPARATOR.CELL_INTERSECT=!SEPARATOR.INTERSECT!"
    )
  )
)

:: Define the horizontal separator
::
:: SEPARATOR.BOXHEAD is the default horizontal separator and it will be 
:: overridden by SEPARATOR.STUBHEAD_STUB or SEPARATOR.BOXHEAD_BODY.
:: If SEPARATOR.BOXHEAD is not defined and SEPARATOR.BOXHEAD_BODY is defined
:: but SEPARATOR.STUBHEAD_STUB is not defined, SEPARATOR.STUBHEAD_STUB will
:: be defined with a space so that there is proper separation.
:: SEPARATOR.BOXHEAD_BODY does not need this treatment as a newline is an
:: adequate replacement for trailing spaces.
IF DEFINED SEPARATOR.BOXHEAD (
  SET "SEPARATOR.HORIZONTAL=#TRUE#"
  IF NOT DEFINED SEPARATOR.STUBHEAD_STUB (
    SET "SEPARATOR.STUBHEAD_STUB=!SEPARATOR.BOXHEAD!"
  )
  IF NOT DEFINED SEPARATOR.BOXHEAD_BODY (
    SET "SEPARATOR.BOXHEAD_BODY=!SEPARATOR.BOXHEAD!"
  )
) ELSE (
  IF DEFINED SEPARATOR.STUBHEAD_STUB (
    SET "SEPARATOR.HORIZONTAL=#TRUE#"
  )
  IF DEFINED SEPARATOR.BOXHEAD_BODY (
    SET "SEPARATOR.HORIZONTAL=#TRUE#"
    IF NOT DEFINED SEPARATOR.STUBHEAD_STUB (
      SET "SEPARATOR.STUBHEAD_STUB= "
    )
  )
)

:: SEPARATOR.INTERSECT is intended to be a friendlier name of SEPARATOR.STUB_BOXHEAD_INTERSECT
:: Only one should be defined with a non-space character.
IF DEFINED SEPARATOR.INTERSECT (
  IF NOT DEFINED SEPARATOR.STUB_BOXHEAD_INTERSECT (
    SET "SEPARATOR.STUB_BOXHEAD_INTERSECT=!SEPARATOR.INTERSECT!"
  ) ELSE (
    ECHO Warning: SEPARATOR.STUB_BOXHEAD_INTERSECT definition overriding SEPARATOR.INTERSECT
  )
)

:: Determine if we need an intersect and if it wasn't already defined
:: decide what makes the most sense based on other separators, either
:: trying to extend the vertical default or the horizontal default.
:: If default vertical or horizontal separators weren't used, it is an
:: advanced definition and the intersect should have been provided,
:: so with no better guidance we define it as a space.
IF DEFINED SEPARATOR.VERTICAL (
  IF DEFINED SEPARATOR.HORIZONTAL (
    :: Both vertical and horizontal separators are defined
    IF NOT DEFINED SEPARATOR.STUB_BOXHEAD_INTERSECT (
      :: But no intersect was actually defined
      IF DEFINED SEPARATOR.STUB (
        :: A default vertical separator is already defined
        IF NOT DEFINED SEPARATOR.BOXHEAD (
          :: but not a default horizontal, so extend the vertical
          SET "SEPARATOR.STUB_BOXHEAD_INTERSECT=!SEPARATOR.STUB!"
        ) ELSE (
          :: SEPARATOR.INTERSECT should be defined by the definition, just provide a space
          SET "SEPARATOR.STUB_BOXHEAD_INTERSECT= "
        )
      ) ELSE (
        :: There is no default vertical separator
        IF DEFINED SEPARATOR.BOXHEAD (
          :: but there is a default horizontal, so extend the horizontal
          SET "SEPARATOR.STUB_BOXHEAD_INTERSECT=!SEPARATOR.BOXHEAD!"
        ) ELSE (
          :: SEPARATOR.INTERSECT should be defined by the definition, just provide a space
          SET "SEPARATOR.STUB_BOXHEAD_INTERSECT= "
        )
      )
    )
  ) ELSE (
    :: Both a vertical and horizontal separator are needed for a SEPARATOR.INTERSECT, so undefine
    SET "SEPARATOR.STUB_BOXHEAD_INTERSECT="
  )
) ELSE (
  :: Both a vertical and horizontal separator are needed for a SEPARATOR.INTERSECT, so undefine
  SET "SEPARATOR.STUB_BOXHEAD_INTERSECT="
)

:: If SEPARATOR.CELL_INTERSECT is not defined, but SEPARATOR.BOXHEAD_BODY is defined,
:: see if SEPARATOR.COLS, SEPARATOR.CELL, or SEPARATOR.BOXHEADERS is defined and extend 
:: SEPARATOR.BOXHEAD_BODY for SEPARATOR.CELL_INTERSECT as default.
IF NOT DEFINED SEPARATOR.CELL_INTERSECT (
  IF DEFINED SEPARATOR.BOXHEAD_BODY (
    IF DEFINED SEPARATOR.COL (
      SET "SEPARATOR.CELL_INTERSECT=!SEPARATOR.BOXHEAD_BODY!"
    ) ELSE IF DEFINED SEPARATOR.CELL (
      SET "SEPARATOR.CELL_INTERSECT=!SEPARATOR.BOXHEAD_BODY!"
    ) ELSE IF DEFINED SEPARATOR.BOXHEADERS (
      SET "SEPARATOR.CELL_INTERSECT=!SEPARATOR.BOXHEAD_BODY!"
    )
  )
)

:: SEPARATOR.CELL_INTERSECT only applies if SEPARATOR.BOXHEADERS, SEPARATOR.COL, or SEPARATOR.CELL is defined
IF DEFINED SEPARATOR.CELL_INTERSECT (
  IF NOT DEFINED SEPARATOR.BOXHEADERS (
    IF NOT DEFINED SEPARATOR.COL (
      IF NOT DEFINED SEPARATOR.CELL (
        SET "SEPARATOR.CELL_INTERSECT="
      )
    )
  )
)

:: Now with all the separators resolved, determine the maximum vertical width in
:: the stub separators. This is the only category of separators supported to be 
:: more than a character wide because it can be calculated in a column whereas
:: a horizontal separator would require multiple lines and that hasn't been
:: implemented. Boxhead and Cell separators could conceivably be more than a
:: character, but that would quickly take up more space as it is multiplied out.
IF DEFINED SEPARATOR.VERTICAL (
  SET /A "SEPARATOR.VERTICAL.WIDTH=0"
  %@strlen% SEPARATOR.STUBHEAD_BOXHEAD SEPARATOR.WIDTH
  %@maxval% SEPARATOR.VERTICAL.WIDTH SEPARATOR.WIDTH
  %@strlen% SEPARATOR.STUB_BOXHEAD_INTERSECT SEPARATOR.WIDTH
  %@maxval% SEPARATOR.VERTICAL.WIDTH SEPARATOR.WIDTH
  %@strlen% SEPARATOR.STUB_BODY SEPARATOR.WIDTH
  %@maxval% SEPARATOR.VERTICAL.WIDTH SEPARATOR.WIDTH

  IF DEFINED SEPARATOR.STUBHEAD_BOXHEAD ( %@align% SEPARATOR.STUBHEAD_BOXHEAD !SEPARATOR.VERTICAL.WIDTH! C SEPARATOR.STUBHEAD_BOXHEAD )
  IF DEFINED SEPARATOR.STUB_BOXHEAD_INTERSECT ( %@align% SEPARATOR.STUB_BOXHEAD_INTERSECT !SEPARATOR.VERTICAL.WIDTH! C SEPARATOR.STUB_BOXHEAD_INTERSECT )
  IF DEFINED SEPARATOR.STUB_BODY ( %@align% SEPARATOR.STUB_BODY !SEPARATOR.VERTICAL.WIDTH! C SEPARATOR.STUB_BODY )
)
%@exit%


:: At this point, the table is defined only in terms of the Stub and Boxhead
:: BUILD_TABLE takes those definitions and populates the cells
:BUILD_TABLE
:: Build the Stub head
IF NOT DEFINED STUBHEAD (
  %@repeat% #SPC# !STUB.MAX_WIDTH! STUBHEAD
)
%@strlen% STUBHEAD STUBHEAD.WIDTH
%@maxval% STUB.MAX_WIDTH STUBHEAD.WIDTH
IF DEFINED SEPARATOR.HORIZONTAL (
  %@strlen% SEPARATOR.STUBHEAD_STUB STUBHEAD.WIDTH
  %@maxval% STUB.MAX_WIDTH STUBHEAD.WIDTH
)
%@align% STUBHEAD !STUB.MAX_WIDTH! !ALIGN.STUBHEAD! STUBHEAD

SET "LINE=!STUBHEAD!!SEPARATOR.STUBHEAD_BOXHEAD!"

:: Line = Stub head + Stub separator, still missing Boxhead column headers

:: The test text might be wider than the column headers, so check that now
%@strlen% CELL CELL.LEN
%@maxval% COL.MAX_WIDTH CELL.LEN

:: Build the boxheader
:: Append a column header
SET "BOXHEAD="
FOR /L %%c IN (1,1,!COL[#]!) DO (
  SET "COL=!COL[%%c]!"
  SET "COL.VALUE=#SPC#" & %@align% COL !COL.MAX_WIDTH! !ALIGN.BOXHEAD! COL.VALUE
  IF [%%c] EQU [1] (
    SET "BOXHEAD=!COL.VALUE!"
  ) ELSE (
    SET "BOXHEAD=!BOXHEAD!!SEPARATOR.BOXHEADERS!!COL.VALUE!"
  )
)

%@strlen% BOXHEAD BOXHEAD.WIDTH

SET "LINE=!LINE!!BOXHEAD!"
%@strlen% LINE TABLE.WIDTH

:: Save the line to the TABLE
SET /A TABLE[#]+=1
SET "TABLE[!TABLE[#]!]=!LINE!"

:: Add a horizontal separator if defined
IF DEFINED SEPARATOR.HORIZONTAL (
  SET "LINE="
  SET "SEPARATOR="
  %@strlen% SEPARATOR.STUBHEAD_STUB SEPARATOR.WIDTH
  IF [!SEPARATOR.WIDTH!] EQU [1] (
    :: If the SEPARATOR.STUBHEAD_STUB is a single character, repeat it
    IF DEFINED SEPARATOR.STUBHEAD_STUB ( %@repeat% SEPARATOR.STUBHEAD_STUB !STUB.MAX_WIDTH! SEPARATOR)
  ) ELSE (
    :: Otherwise align it based on the defined ALIGN.STUBHEAD_STUB
    IF DEFINED SEPARATOR.STUBHEAD_STUB ( %@align% SEPARATOR.STUBHEAD_STUB !STUB.MAX_WIDTH! !ALIGN.STUBHEAD_STUB! SEPARATOR )
  )
  SET "LINE=!LINE!!SEPARATOR!"

  :: ALIGN.INTERSECT is intended to be a friendlier name of ALIGN.STUB_BOXHEAD_INTERSECT
  IF DEFINED ALIGN.INTERSECT (
    IF NOT DEFINED ALIGN.STUB_BOXHEAD_INTERSECT (
      SET "ALIGN.STUB_BOXHEAD_INTERSECT=!ALIGN.INTERSECT!"
    ) ELSE (
      ECHO Warning: ALIGN.STUB_BOXHEAD_INTERSECT definition overriding ALIGN.INTERSECT
    )
  )

  SET "SEPARATOR="
  %@strlen% SEPARATOR.STUB_BOXHEAD_INTERSECT SEPARATOR.WIDTH
  IF [!SEPARATOR.WIDTH!] EQU [1] (
    :: If the SEPARATOR.STUB_BOXHEAD_INTERSECT is a single character, repeat it
    IF DEFINED SEPARATOR.STUB_BOXHEAD_INTERSECT ( %@repeat% SEPARATOR.STUB_BOXHEAD_INTERSECT !SEPARATOR.VERTICAL.WIDTH! SEPARATOR )
  ) ELSE (
    :: Otherwise align it based on the defined ALIGN.STUBHEAD
    IF DEFINED SEPARATOR.STUB_BOXHEAD_INTERSECT ( %@align% SEPARATOR.STUB_BOXHEAD_INTERSECT !SEPARATOR.VERTICAL.WIDTH! !ALIGN.STUB_BOXHEAD_INTERSECT! SEPARATOR )
  )
  SET "LINE=!LINE!!SEPARATOR!"

  SET "SEPARATOR="
  %@strlen% SEPARATOR.BOXHEAD_BODY SEPARATOR.WIDTH
  IF [!SEPARATOR.WIDTH!] EQU [1] (
    IF DEFINED SEPARATOR.CELL_INTERSECT (
      :: If there is a column intersect, we need to build the line out for each column,
      FOR /L %%c IN (2,1,!COL[#]!) DO (
        :: Loop COL[#]-1 times so that we don't have too many segments
        %@repeat% SEPARATOR.BOXHEAD_BODY !COL.MAX_WIDTH! COLUMN
        SET "LINE=!LINE!!COLUMN!!SEPARATOR.CELL_INTERSECT!"
      )
      :: Append the final column
      SET "LINE=!LINE!!COLUMN!"
    ) ELSE (
      :: otherwise, if the SEPARATOR.BOXHEAD_BODY is a single character, repeat it for the width of the BOXHEAD
      IF DEFINED SEPARATOR.BOXHEAD_BODY ( %@repeat% SEPARATOR.BOXHEAD_BODY !BOXHEAD.WIDTH! SEPARATOR )
    )
  ) ELSE (
    :: Otherwise align it based on the defined ALIGN.STUBHEAD
    IF DEFINED SEPARATOR.BOXHEAD_BODY ( %@align% SEPARATOR.BOXHEAD_BODY !BOXHEAD.WIDTH! !ALIGN.BOXHEAD_BODY! SEPARATOR )
  )
  SET "LINE=!LINE!!SEPARATOR!"

  SET /A TABLE[#]+=1
  SET "TABLE[!TABLE[#]!]=!LINE!"
)

:: If we're going to show an R1C1 reference, we don't need to compute the cell alignment
IF DEFINED SHOW.R1C1_REFERENCE (
  :: Set up the R1C1 row counter
  SET /A "R1C1_REFERENCE.R=0"
) ELSE (
  :: Otherwise figure out the CELL alignment now so we only need to compute this once
  %@align% CELL !COL.MAX_WIDTH! !ALIGN.CELL! CELL
)

:: Build the row
FOR /L %%r IN (1,1,!ROW[#]!) DO (
  :: Advance and output the spinner animation if not disabled
  IF NOT DEFINED SPINNER.DISABLED (
    %@spinner% SPINNER.FRAME
  )

  :: Build the stub for the row
  SET "ROW=!ROW[%%r]!"
  %@align% ROW !STUB.MAX_WIDTH! !ALIGN.STUB! ROW.VALUE

  SET "SEPARATOR="
  IF DEFINED SEPARATOR.VERTICAL (
    %@strlen% SEPARATOR.STUB_BODY SEPARATOR.WIDTH
    IF [!SEPARATOR.WIDTH!] EQU [1] (
      IF DEFINED SEPARATOR.STUB_BODY ( %@repeat% SEPARATOR.STUB_BODY !SEPARATOR.VERTICAL.WIDTH! SEPARATOR )
    ) ELSE (
      IF DEFINED SEPARATOR.STUB_BODY ( %@align% SEPARATOR.STUB_BODY !SEPARATOR.VERTICAL.WIDTH! !ALIGN.STUB_BODY! SEPARATOR )
    )
  )

  IF /I ["!ROW:~0,5!"] EQU ["#SPC#"] (
    :: We want a special case for #SPC# so that we print a blank line
    SET "LINE="
  ) ELSE IF /I ["!ROW:~0,5!"] EQU ["#LBL#"] (
    :: We want a special case for #LBL# so that we print the string which follows and append an SGR RESET
    IF /I ["!ROW:~5,1!"] EQU [" "] (
      :: Allow for a space to follow #LBL#
      SET "LINE=!ROW:~6!!RESET!"
    ) ELSE IF /I ["!ROW:~5,1!"] EQU ["."] (
      :: Allow for a period to follow #LBL# and treat it like space, to mimic ECHO
      SET "LINE=!ROW:~6!!RESET!"
    ) ELSE (
      :: Assume that any other character is part of the label
      SET "LINE=!ROW:~5!!RESET!"
    )
  ) ELSE (
    :: Otherwise process the line
    SET "LINE=!ROW.VALUE!!SEPARATOR!"

    IF /I ["!ROW!"] EQU ["#NUL#"] (
      SET "ROW="
    ) ELSE (
      SET "ROW=!CSI!!ROW!"
    )

    :: Set up the R1C1 column counter
    IF DEFINED SHOW.R1C1_REFERENCE (
      SET /A "R1C1_REFERENCE.C=0"
      %@counter% R1C1_REFERENCE.R
    )

    :: Append a cell to the row, adding separators unless processing the first column
    FOR /L %%c IN (1,1,!COL[#]!) DO (
      :: This is where we actually build the R1C1 reference, replacing the CELL
      IF DEFINED SHOW.R1C1_REFERENCE (
        %@counter% R1C1_REFERENCE.C
        SET "R1C1_REFERENCE=R!R1C1_REFERENCE.R!C!R1C1_REFERENCE.C!"
        %@align% R1C1_REFERENCE !COL.MAX_WIDTH! !ALIGN.CELL.R1C1! CELL
      )

      IF /I ["!COL[%%c]!"] EQU ["#NUL#"] (
        :: Special case for #NUL#
        IF [%%c] EQU [1] (
          SET "LINE=!LINE!!ROW!!CELL!!RESET!"
        ) ELSE (
          SET "LINE=!LINE!!SEPARATOR.CELL!!ROW!!CELL!!RESET!"
        )
      ) ELSE IF /I ["!COL[%%c]!"] EQU ["#SPC#"] (
        :: Special case for #SPC#
        %@repeat% #SPC# !COL.MAX_WIDTH! OUT
        IF [%%c] EQU [1] (
          SET "LINE=!LINE!!OUT!"
        ) ELSE (
          SET "LINE=!LINE!!SEPARATOR.CELL!!OUT!"
        )
        :: Don't count spaces as columns for R1C1_REFERENCE
        SET /A "R1C1_REFERENCE.C-=1"
      ) ELSE (
        :: Normal processing
        SET "COL=!CSI!!COL[%%c]!"
        IF [%%c] EQU [1] (
          SET "LINE=!LINE!!ROW!!COL!!CELL!!RESET!"
        ) ELSE (
          SET "LINE=!LINE!!SEPARATOR.CELL!!ROW!!COL!!CELL!!RESET!"
        )
      )      
    )
  )

  :: Save the line to the TABLE
  SET /A TABLE[#]+=1
  SET "TABLE[!TABLE[#]!]=!LINE!"
)
%@exit%


:DISPLAY_TABLE
:: We use an out buffer to iterate through all the rows of the table
:: This allows us to quickly display the output even though it takes
:: time to figure out the alignment calculations
FOR /L %%r IN (1,1,!TABLE[#]!) DO (
  ECHO.!TABLE[%%r]!
)
%@exit%
%=- End Main Script -=%


%=- Macro Definitions -=%
:DEFINE_MACROS
:: Return from this with GOTO :MAIN to retain macro definitions
SETLOCAL DISABLEDELAYEDEXPANSION
:: -------- Begin macro definitions ----------
(SET LF=^
%= This defines a Line Feed (0x0A) =%
)

(SET \n=^^^
%= This defines an escaped Line Feed (0x5E 0x0A) =%
)

:: @strlen  StrVar  [RtnVar]
::
::   Computes the length of string in variable StrVar
::   and stores the result in variable RtnVar.
::   If StrVar is #SPC#, the return val should be 1.
::   If StrVar is any other Special Token, the return val should be 0.
::   If RtnVar is not specified, then print the length to stdout.
::
SET @strlen=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  FOR /F "tokens=1,2 delims=, " %%1 IN ("!argv!") DO ( ENDLOCAL%\n%
    SET "s=A!%%~1!"%\n%
    IF /I ["!s:~0,5!"] EQU ["#SPC#"] (%\n%
      SET "s= "%\n%
    ) ELSE IF /I ["!s:~0,1!"] EQU ["#"] (%\n%
      IF /I ["!s:~4,1!"] EQU ["#"] (%\n%
        :: Look for Special Tokens of the form #???# %\n%
        SET "s="%\n%
      )%\n%
    )%\n%
    SET "len=0"%\n%
    FOR %%P in (4096 2048 1024 512 256 128 64 32 16 8 4 2 1) DO (%\n%
      IF ["!s:~%%P,1!"] NEQ [""] (%\n%
        SET /A "len+=%%P"%\n%
        SET "s=!s:~%%P!"%\n%
      )%\n%
    )%\n%
    FOR %%V IN (!len!) DO ENDLOCAL^&IF ["%%~2"] NEQ [""] (SET "%%~2=%%V") ELSE (ECHO %%V)%\n%
  )%\n%
) ELSE SETLOCAL ENABLEDELAYEDEXPANSION^&SETLOCAL^&SET argv=,

:: @maxval  NumVar1  NumVar2  [RetVar]
::
::   Compares NumVar1 with NumVar2, and assigns to RetVar.
::   If RtnVar is not specified, then return the largest value
::   back through NumVar1.
::
::   It is recommended that NumVar1 will be accumulating the 
::   Max value passed through multiple subsequent calls, to
::   determine the largest string passed in.
::
SET @maxval=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  FOR /F "tokens=1,2,3 delims=, " %%1 IN ("!argv!") DO ( ENDLOCAL%\n%
    IF [!%%~1!] NEQ [] (SET /a "a=!%%~1!") ELSE (SET /a "a=%%~1")%\n%
    IF [!%%~2!] NEQ [] (SET /a "b=!%%~2!") ELSE (SET /a "b=%%~2")%\n%
    IF !b! GTR !a! (SET /a "a=b")%\n%
    FOR %%V IN (!a!) DO ENDLOCAL^&IF ["%%~3"] NEQ [""] (SET "%%~3=%%V") ELSE (SET "%%~1=%%V")%\n%
  )%\n%
) ELSE SETLOCAL ENABLEDELAYEDEXPANSION^&SETLOCAL^&SET argv=,

:: @repeat  StrVal  Count  [RetVar]
::
::   Repeats StrVal, Count times, and assigns to RetVar.
::   If StrVar is #SPC#, this should empty padding.
::   If StrVar is any other Special Token, this should be empty.
::   If RtnVar is not specified, then print the output string to stdout.
::
SET @repeat=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  FOR /F "tokens=1,2,3 delims=, " %%1 IN ("!argv!") DO ( ENDLOCAL%\n%
    IF [!%%~1!] NEQ [] (SET "s=!%%~1!") ELSE (SET "s=%%~1")%\n%
    IF /I ["!s:~0,5!"] EQU ["#SPC#"] (%\n%
      SET "s= "%\n%
    ) ELSE IF /I ["!s:~0,1!"] EQU ["#"] (%\n%
      IF /I ["!s:~4,1!"] EQU ["#"] (%\n%
        :: Look for Special Tokens of the form #???# %\n%
        SET "s="%\n%
      )%\n%
    )%\n%
    SET "count=%%~2"%\n%
    SET "outstr="%\n%
    FOR /L %%. IN (1,1,!count!) DO SET "outstr=!outstr!!s!"%\n%
    FOR /F "delims=" %%V IN ("!outstr!") DO ENDLOCAL^&IF ["%%~3"] NEQ [""] (SET "%%~3=%%V") ELSE (ECHO %%V)%\n%
  )%\n%
) ELSE SETLOCAL ENABLEDELAYEDEXPANSION^&SETLOCAL^&SET argv=,

:: @rtrim  StrVar  [CharVar]
::
::   Right Trim CharVar surrounding StrVar.
::   If CharVar is not specified, then default to space.
::
::   Technique Source: https://www.dostips.com/forum/viewtopic.php?p=12327#p12327
::
SET @rtrim=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  SET "charVar= "%\n%
  FOR /F "tokens=1,2 delims=, " %%1 IN ("!argv!") DO (%\n%
    SET "strVar=!%%1!"%\n%
    IF [%%~2] NEQ [] IF DEFINED %%~2 SET "charVar=!%%2:~0,1!"%\n%
    FOR /L %%i IN (1 1 12) DO SET "charVar=!charVar!!charVar!"%\n%
    IF DEFINED strVar FOR %%k IN (4096 2048 1024 512 256 128 64 32 16 8 4 2 1) DO (%\n%
      IF ["!strVar:~-%%k!"] EQU ["!charVar:~-%%k!"] SET "strVar=!strVar:~0,-%%k!"%\n%
    )%\n%
    IF DEFINED strVar (%\n%
      IF NOT DEFINED _notDelayed (%\n%
        SET "strVar=!strVar:^=^^!"%\n%
        SET "strVar=!strVar:"=""Q!^"%\n%
        CALL SET "strVar=%%^strVar:^!=""E^!%%" ! %\n%
        SET "strVar=!strVar:""E=^!"%\n%
        SET "strVar=!strVar:""Q="!^"%\n%
      )%\n%
      FOR /F ^^^"eol^^=^^^%LF%%LF%^%LF%%LF%^^ delims^^=^^^" %%k IN ("!strVar!") DO ENDLOCAL^&ENDLOCAL^&SET "%%1=%%k"%\n%
    ) ELSE ENDLOCAL^&ENDLOCAL^&SET "%%1="%\n%
  )%\n%
) ELSE SETLOCAL^&SET "_notDelayed=!"^&SETLOCAL ENABLEDELAYEDEXPANSION^&SET argv=,

:: @ltrim  StrVar  [CharVar]
::
::   Left Trim CharVar surrounding StrVar.
::   If CharVar is not specified, then default to space.
::
::   Technique Source: https://www.dostips.com/forum/viewtopic.php?p=12327#p12327
::
SET @ltrim=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  SET "charVar= "%\n%
  FOR /F "tokens=1,2 delims=, " %%1 IN ("!argv!") DO (%\n%
    SET "strVar=!%%1!"%\n%
    IF [%%~2] NEQ [] IF DEFINED %%~2 SET "charVar=!%%2:~0,1!"%\n%
    FOR /L %%i IN (1 1 12) DO SET "charVar=!charVar!!charVar!"%\n%
    IF DEFINED strVar FOR %%k IN (4096 2048 1024 512 256 128 64 32 16 8 4 2 1) DO (%\n%
      IF ["!strVar:~0,%%k!"] EQU ["!charVar:~-%%k!"] SET "strVar=!strVar:~%%k!"%\n%
    )%\n%
    IF DEFINED strVar (%\n%
      IF NOT DEFINED _notDelayed (%\n%
        SET "strVar=!strVar:^=^^!"%\n%
        SET "strVar=!strVar:"=""Q!^"%\n%
        CALL SET "strVar=%%^strVar:^!=""E^!%%" ! %\n%
        SET "strVar=!strVar:""E=^!"%\n%
        SET "strVar=!strVar:""Q="!^"%\n%
      )%\n%
      FOR /F ^^^"eol^^=^^^%LF%%LF%^%LF%%LF%^^ delims^^=^^^" %%k IN ("!strVar!") DO ENDLOCAL^&ENDLOCAL^&SET "%%1=%%k"%\n%
    ) ELSE ENDLOCAL^&ENDLOCAL^&SET "%%1="%\n%
  )%\n%
) ELSE SETLOCAL^&SET "_notDelayed=!"^&SETLOCAL ENABLEDELAYEDEXPANSION^&SET argv=,

:: @trim  StrVar  [CharVar]
::
::   Trim CharVar surrounding StrVar.
::   If CharVar is not specified, then default to space.
::
::   Technique Source: https://www.dostips.com/forum/viewtopic.php?p=12327#p12327
::
SET @trim=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  SET "charVar= "%\n%
  FOR /F "tokens=1,2 delims=, " %%1 IN ("!argv!") DO (%\n%
    SET "strVar=!%%1!"%\n%
    IF [%%~2] NEQ [] IF DEFINED %%~2 SET "charVar=!%%2:~0,1!"%\n%
    FOR /L %%i IN (1 1 12) DO SET "charVar=!charVar!!charVar!"%\n%
    IF DEFINED strVar FOR %%k IN (4096 2048 1024 512 256 128 64 32 16 8 4 2 1) DO (%\n%
      IF ["!strVar:~-%%k!"] EQU ["!charVar:~-%%k!"] SET "strVar=!strVar:~0,-%%k!"%\n%
      IF ["!strVar:~0,%%k!"] EQU ["!charVar:~-%%k!"] SET "strVar=!strVar:~%%k!"%\n%
    )%\n%
    IF DEFINED strVar (%\n%
      IF NOT DEFINED _notDelayed (%\n%
        SET "strVar=!strVar:^=^^!"%\n%
        SET "strVar=!strVar:"=""Q!^"%\n%
        CALL SET "strVar=%%^strVar:^!=""E^!%%" ! %\n%
        SET "strVar=!strVar:""E=^!"%\n%
        SET "strVar=!strVar:""Q="!^"%\n%
      )%\n%
      FOR /F ^^^"eol^^=^^^%LF%%LF%^%LF%%LF%^^ delims^^=^^^" %%k IN ("!strVar!") DO ENDLOCAL^&ENDLOCAL^&SET "%%1=%%k"%\n%
    ) ELSE ENDLOCAL^&ENDLOCAL^&SET "%%1="%\n%
  )%\n%
) ELSE SETLOCAL^&SET "_notDelayed=!"^&SETLOCAL ENABLEDELAYEDEXPANSION^&SET argv=,

:: @align  StrVar  Width  <L|C|R>  [RtnVar]
::
::   Aligns the string in variable StrVar
::   in the field using the Width and Alignment provided
::   and stores the result in variable RtnVar.
::   If StrVar is a Special Token, it is treated as a space.
::   If RtnVar is not specified, then print the output to stdout.
::
SET @align=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  FOR /F "tokens=1,2,3,4 delims=, " %%1 IN ("!argv!") DO ( ENDLOCAL%\n%
    IF ["!%%~1!"] NEQ [""] (SET "strVar=!%%~1!") ELSE (SET "strVar=%%~1")%\n%
    IF /I ["!strVar:~0,1!"] EQU ["#"] (%\n%
      IF /I ["!strVar:~4,1!"] EQU ["#"] (%\n%
        :: Look for Special Tokens of the form #???# %\n%
        SET "strVar= "%\n%
      )%\n%
    )%\n%
    IF ["!%%~2!"] NEQ [""] (SET "width=!%%~2!") ELSE (SET "width=%%~2")%\n%
    SET "alignment=%%~3"%\n%
    IF ["!%%~4!"] NEQ [""] (SET "%%~4=")%\n%
    SET "len=1"%\n%
    SET "s=!strVar!"%\n%
    FOR %%P in (4096 2048 1024 512 256 128 64 32 16 8 4 2 1) DO (%\n%
      IF ["!s:~%%P,1!"] NEQ [""] (%\n%
        SET /A "len+=%%P"%\n%
        SET "s=!s:~%%P!"%\n%
      )%\n%
    )%\n%
    IF /I ["!alignment!"] EQU ["L"] (%\n%
      SET /A "pre=0"%\n%
      SET /A "post=(!width! - !len!)"%\n%
    )%\n%
    IF /I ["!alignment!"] EQU ["C"] (%\n%
      SET /A "pre=(!width! - !len! + 1) / 2"%\n%
      SET /A "post=(!width! - !pre! - !len!)"%\n%
    )%\n%
    IF /I ["!alignment!"] EQU ["R"] (%\n%
      SET /A "pre=(!width! - !len!)"%\n%
      SET /A "post=0"%\n%
    )%\n%
    SET "wrkstr="%\n%
    FOR /L %%. IN (1,1,!pre!) DO SET "wrkstr=!wrkstr! "%\n%
    SET "wrkstr=!wrkstr!!strVar!"%\n%
    FOR /L %%. IN (1,1,!post!) DO SET "wrkstr=!wrkstr! "%\n%
    FOR /F "delims=" %%V IN ("!wrkstr!") DO ENDLOCAL^&IF ["%%~4"] NEQ [""] (SET "%%~4=%%V") ELSE (ECHO %%V)%\n%
  )%\n%
) ELSE SETLOCAL ENABLEDELAYEDEXPANSION^&SETLOCAL^&SET argv=,

:: @spinner  FrameVar
::
::   When called print a spinning wait cursor using the FrameVar
::   as an accumulator. If the FrameVar variable is not set
::   it is initialized. Advancing one frame the FrameVar is
::   incremented. If it reaches the last frame it is reset
::   back to the first frame.
::
::   Has a dependency on the ANSI Control Sequence,
::   Cursor Previous Line (CPL). This means the initialization
::   needs to be completed before this macro can be used.
::
::   The animation can be changed by adjusting the variables named
::   @spinner[#] and setting the count to match.
::
SET "@spinner[0]= - Processing"
SET "@spinner[1]= \ Processing"
SET "@spinner[2]= ^| Processing"
SET "@spinner[3]= / Processing"
SET /A "@spinner[#]=4"

SET @spinner=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  FOR /f "tokens=1 delims=, " %%1 IN ("!argv!") DO ( ENDLOCAL%\n%
    SET "frameVar=%%~1"%\n%
    IF ["!frameVar!"] EQU [""] (%\n%
      SET /A "frame=0"%\n%
    ) ELSE (%\n%
      SET /A "frame=!frameVar!"%\n%
    )%\n%
    CALL ECHO %%@spinner[!frame!]%%!CPL!%\n%
    SET /A "frame+=1"%\n%
    SET /A "frame%%=!@spinner[#]!"%\n%
    FOR /F "delims=" %%V IN ("!frame!") DO ENDLOCAL^&SET "%%~1=%%V"%\n%
  )%\n%
) ELSE SETLOCAL ENABLEDELAYEDEXPANSION^&SETLOCAL^&SET argv=,

:: @counter  NumVar
::
::   When called it uses the NumVar as an accumulator. If the
::   NumVar variable is not set it is initialized.
::
SET @counter=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  FOR /f "tokens=1 delims=, " %%1 IN ("!argv!") DO ( ENDLOCAL%\n%
    SET "numVar=%%~1"%\n%
    IF ["!numVar!"] EQU [""] (%\n%
      SET /A "count=0"%\n%
    ) ELSE (%\n%
      SET /A "count=!numVar!"%\n%
    )%\n%
    SET /A "count+=1"%\n%
    FOR /F "delims=" %%V IN ("!count!") DO ENDLOCAL^&SET "%%~1=%%V"%\n%
  )%\n%
) ELSE SETLOCAL ENABLEDELAYEDEXPANSION^&SETLOCAL^&SET argv=,

:: @exit  [ErrorLevel]
::
::   Used to exit and optionally sets an error code if provided.
::   This is preferred for exiting a script over GOTO :EOF for consistency
::   and to pass Error Levels if necessary. This tidy's up a call
::   to EXIT /B [ErrorLevel] so the use feels the same as other macros.
::
SET @exit=FOR %%. IN (1 2) DO IF [%%.] EQU [2] (%\n%
  FOR /F "tokens=1 delims=, " %%1 IN ("!argv! 0") DO ( ENDLOCAL%\n%
    IF ["%%~1"] NEQ [""] (IF DEFINED %%~1 (SET "exitCode=!%%1!"%\n%
      ) ELSE (SET "exitCode=%%~1")%\n%
    ) ELSE (SET "exitCode=0")%\n%
    EXIT /B !exitCode!%\n%
  )%\n%
) ELSE SETLOCAL ENABLEDELAYEDEXPANSION^&SETLOCAL^&SET argv=,

GOTO :MAIN
:: -------- End macro definitions ----------
%=- End Macro Definitions -=%


%=- Parse Command Line Arguments -=%
:PARSE_ARGS
SET "SCRIPT=%~dpnx0"
SET "SCRIPT_NAME=%~nx0"
SET "DATA_FILE="

FOR %%a IN (%*) DO (
  SET "ARG=%%~a"
  SET "OPT.FOUND="
  :: Check for options
  IF ["!ARG:~0,1!"] EQU ["/"] (
    SET "OPT.FOUND=#TRUE#"
  ) ELSE IF ["!ARG:~0,1!"] EQU ["-"] (
    :: Makes this more friendly to run in PowerShell where - is used for arguments
    SET "OPT.FOUND=#TRUE#"
  )
  IF DEFINED OPT.FOUND (
    SHIFT /1
    SET "OPT=!ARG:~1,1!"
    :: /H [Help]
    IF /I ["!OPT!"] EQU ["H"] (
      SET "OPT="
      CALL :USAGE
      %@exit% -1
    )
    :: /A [ANSI]
    IF /I ["!OPT!"] EQU ["A"] (
      SET "OPT="
      SET "SHOW.ANSI=#TRUE#"
      SET "SHOW.UTF8=#TRUE#"
    )
    :: /R [R1C1]
    IF /I ["!OPT!"] EQU ["R"] (
      SET "OPT="
      SET "SHOW.R1C1_REFERENCE=#TRUE#"
    )
    :: /U [UTF-8]
    IF /I ["!OPT!"] EQU ["U"] (
      SET "OPT="
      SET "SHOW.UTF8=#TRUE#"
    )
    IF ["!OPT!"] NEQ [""] (
      SET "MSG=Error: Unknown option: !OPT!"
      CALL :USAGE MSG
      %@exit% 1
    )
  )
)

:: If there isn't a definition passed on the command-line
:: default to using this script as the definition file.
IF [%~1] EQU [] (
  SET "DATA_FILE=!SCRIPT!"
) ELSE (
  SET "DATA_FILE=%~1"
)

:: Verify that the definition file exists and exit with
:: error code 1 if it doesn't.
IF NOT EXIST !DATA_FILE! (
  SET "MSG=Error: File does not exist: !DATA_FILE!"
  CALL :USAGE MSG
  %@exit% 1
)

%@exit%


:: CALL :USAGE  MsgVar
::
::   When called with a MsgVar, it will prepend the usage
::   output message with the custom string message assigned
::   to MsgVar. This is used to further explain the error.
::
:USAGE
SET "SCRIPT_NAME=%~nx0"

:: The following ECHO intentionally has 80 spaces to clear the 
:: line any remaining on screen text on call.
ECHO.                                                                                
IF [%1] NEQ [] (
  ECHO !%1!
  ECHO.
)
ECHO Usage: !SCRIPT_NAME! [flags] [^<definition_file^>]
ECHO.
ECHO    This file echoes a bunch of color codes to the terminal to demonstrate
ECHO    how they will render. The `Data Segment` portion of the file defines the
ECHO    table layout and allows the user to configure whatever matrix of ANSI
ECHO    Escape Sequence control characters they wish. This can also be read from
ECHO    an external definition file using the same structure.
ECHO.
ECHO    Flags:
ECHO      /H  :  This message
ECHO      /A  :  Display the ANSI Escape Sequence control characters
ECHO             This requires UTF-8 support and implies the additional /U flag
ECHO      /R  :  Show cell R1C1 reference addressing instead of cell text
ECHO      /U  :  Enable UTF-8 support
ECHO.
ECHO    The script itself only has one external dependency on CHCP if you want
ECHO    to show Unicode text. This just sets the Command Prompt codepage to 65001
ECHO    and will restore it when the script successfully completes.
ECHO.

%@exit%

%=- End Parse Command Line Arguments -=%

::
:: Inspired by Daniel Crisman's BASH script from http://www.tldp.org/HOWTO/Bash-Prompt-HOWTO/x329.html
::
:: #!/bin/bash
:: #
:: #   This file echoes a bunch of color codes to the
:: #   terminal to demonstrate what's available.  Each
:: #   line is the color code of one foreground color,
:: #   out of 17 (default + 16 escapes), followed by a
:: #   test use of that color on all nine background
:: #   colors (default + 8 escapes).
:: #
::
:: T='gYw'   # The test text
::
:: echo -e "\n                 40m     41m     42m     43m\
::      44m     45m     46m     47m";
::
:: for FGs in '    m' '   1m' '  30m' '1;30m' '  31m' '1;31m' '  32m' \
::            '1;32m' '  33m' '1;33m' '  34m' '1;34m' '  35m' '1;35m' \
::            '  36m' '1;36m' '  37m' '1;37m';
::   do FG=${FGs// /}
::   echo -en " $FGs \033[$FG  $T  "
::   for BG in 40m 41m 42m 43m 44m 45m 46m 47m;
::     do echo -en "$EINS \033[$FG\033[$BG  $T  \033[0m";
::   done
::   echo;
:: done
:: echo
::
