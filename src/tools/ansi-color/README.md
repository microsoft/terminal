# ANSI-COLOR

Ansi-Color.cmd makes it easy to render SGR attributes, foreground colors, and background colors, in a table. This complements ColorTool for diagnostics purposes and seeing all the colors of the applied color scheme.

```plain
Usage: ansi-color.cmd [flags] [<definition_file>]

   This file echoes a bunch of color codes to the terminal to demonstrate
   how they will render. The `Data Segment` portion of the file defines the
   table layout and allows the user to configure whatever matrix of ANSI
   Escape Sequence control characters they wish. This can also be read from
   an external definition file using the same structure.

   Flags:
     /H  :  This message
     /A  :  Display the ANSI Escape Sequence control characters
     /R  :  Show cell R1C1 reference addressing instead of cell text
     /U  :  Enable UTF-8 support

   The script itself only has one external dependency on CHCP if you want
   to show Unicode text. This just sets the Command Prompt codepage to 65001
   and will restore it when the script successfully completes.
   ```

   The entire tool is written as a Windows Command script and the only dependency is on the stock CHCP tool which can be used to change the command prompt code page. The script makes heavy use of Batch "macros," a concept originally explored by Ed Dyreen, Jeb, and Dave Benham on [DosTips.com](https://www.dostips.com/forum/viewtopic.php?f=3&t=1827). The use of macros in Ansi-Color allow complex results and capable of generating tables defined as separate files.
   
   Of notable interest, the script itself is its own definition file and doesn't require external definition files to work. In fact, the script file itself is just a UTF-8 text file which can easily be edited, doesn't need to be recompiled, and has configuration settings embedded in it, or it can use flags passed as arguments. Additionally, several definition files are included which replicate the output of similar tools.
   
   Lastly, there are two diagnostic modes which might be useful when writing color schemes. `/A` is a flag which instead of showing the output table, it replaces the actual ESC code in the output string with the Unicode codepoint representation of an ESC character, and when redirected from standard out to a file like `ansi-color /a > out.txt` it will create a text file which shows what would have been generated. This makes it easier to find errors in how something rendered. Additionally the `/R` flag borrows the Excel R1C1 reference address scheme to replace the Cell text with row/column IDs. In this way you can generate a table and then identify a combination of attributes and colors by that corresponding reference.

   Under PowerShell, you can run all the definitions in a folder and subfolders using `gci -r .\*.def | %{write ($_ | rvpa -r) && .\ansi-color.cmd $_}`. This is convenient for finding a specific definition.