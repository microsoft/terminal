#!/bin/sh

if [ "$1" != "" ]; then
    # rm __temp__.txt
    # colortool.exe -x $1 > __temp__.txt
    # cat __temp__.txt
    cmd.exe /c colortool.exe -x $1
else
    colortool.exe --help
fi
