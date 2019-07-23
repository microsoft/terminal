# ColorTool 

ColorTool makes it easy to change the Windows console to your desired scheme. Includes support for iTerm themes!

```
Usage:
    colortool.exe [options] <schemename>
ColorTool is a utility for helping to set the color palette of the Windows Console.
By default, applies the colors in the specified .itermcolors or .ini file to the current console window.
This does NOT save the properties automatically. For that, you'll need to open the properties sheet and hit "Ok".
Included should be a `schemes/` directory with a selection of schemes of both formats for examples.
Feel free to add your own preferred scheme to that directory.
Arguments:
    <schemename>: The name of a color scheme. ct will try to first load it as an .itermcolors color scheme.
                  If that fails, it will look for it as an .ini file color scheme.
Options:
    -?, --help     : Display this help message
    -c, --current  : Print the color table for the currently applied scheme
    -q, --quiet    : Don't print the color table after applying
    -d, --defaults : Apply the scheme to only the defaults in the registry
    -b, --both     : Apply the scheme to both the current console and the defaults.
    -s, --schemes  : Display all available schemes
    -v, --version  : Display the version number
```

## Included Schemes

  Included are two important color schemes in .ini file format - `cmd-legacy` and `campbell`.

  * `cmd-legacy` is the legacy color scheme of the Windows Console, before July 2017
    
  * `campbell` is the new default scheme used by the Windows Console Host, as of the Fall Creator's Update.

  There are a few other schemes in that directory in both .ini format and .itermcolors. 

## Adding Schemes

  You can also add color schemes to the colortool easily. Take any existing scheme in `.itermcolors` format, and paste it in the `schemes/` directory. Or just cd into a directory containing `*.itermcolors` files before running the colortool.

  I recommend the excellent [iTerm2-Color-Schemes](https://github.com/mbadolato/iTerm2-Color-Schemes) repo, which has TONS of schemes to choose from, and previews.
  
  You can also easily visually edit `.itermcolors` color schemes using [terminal.sexy](https://terminal.sexy). Use the **Import** and **Export** tabs with `iTerm2` as the format.

## Installing 

Just [download the latest colortool release](https://github.com/microsoft/terminal/releases/tag/1904.29002) and extract the zip file. 

## Building

  Either build with Visual Studio, or use the included `build.bat` from the command line to try and auto-detect your msbuild version.

## Contributing

  This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
