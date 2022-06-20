# Adding profiles for third-party tools

This doc will hopefully provide a useful guide for adding profiles for common third-party tools to your
[settings.json](https://docs.microsoft.com/en-us/windows/terminal/customize-settings/profile-settings) file.

## Anaconda

Assuming that you've installed Anaconda into `%USERPROFILE%\Anaconda3`:

```json
{
    "commandline": "cmd.exe /k \"%USERPROFILE%\\Anaconda3\\Scripts\\activate.bat %USERPROFILE%\\Anaconda3\"",
    "icon": "%USERPROFILE%\\Anaconda3\\Menu\\anaconda-navigator.ico",
    "name": "Anaconda3",
    "startingDirectory": "%USERPROFILE%"
}
```

## AWS Shell

Assuming that you've installed via pip to python 3.1 into `C:\Python310\Scripts`:

```json
{
    "commandline": "cmd.exe /k \"C:\\Python310\\Scripts\\aws-shell.exe",
    "name": "AWS Shell",
    "icon": "C:\\Python310\\Scripts\\AWS.ico",
    "startingDirectory": "%USERPROFILE%"
}
```


## cmder

Assuming that you've installed cmder into `%CMDER_ROOT%`:

```json
{
    "commandline": "cmd.exe /k \"%CMDER_ROOT%\\vendor\\init.bat\"",
    "name": "cmder",
    "icon": "%CMDER_ROOT%\\icons\\cmder.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## Cygwin

Assuming that you've installed Cygwin into `C:\Cygwin`:

```json
{
    "name": "Cygwin",
    "commandline": "C:\\Cygwin\\bin\\bash --login -i",
    "icon": "C:\\Cygwin\\Cygwin.ico",
    "startingDirectory": "C:\\Cygwin\\bin"
}
```

Note that the starting directory of Cygwin is set as it is to make the path
work. The default directory opened when starting Cygwin will be `$HOME` because
of the `--login` flag.

## Far Manager

Assuming that you've installed Far into `c:\Program Files\Far Manager`:

```json
{
    "name": "Far",
    "commandline": "\"c:\\program files\\far manager\\far.exe\"",
    "startingDirectory": "%USERPROFILE%",
    "useAcrylic": false
},
```

## Git Bash

Assuming that you've installed Git Bash into `C:\\Program Files\\Git`:

```json
{
    "name": "Git Bash",
    "commandline": "C:\\Program Files\\Git\\bin\\bash.exe -li",
    "icon": "C:\\Program Files\\Git\\mingw64\\share\\git\\git-for-windows.ico",
    "startingDirectory": "%USERPROFILE%"
}
````

## Git Bash (WOW64)

Assuming that you've installed Git Bash into `C:\\Program Files (x86)\\Git`:

```json
{
    "name": "Git Bash",
    "commandline": "%ProgramFiles(x86)%\\Git\\bin\\bash.exe -li",
    "icon": "%ProgramFiles(x86)%\\Git\\mingw32\\share\\git\\git-for-windows.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## Google Cloud SDK

Assuming that you've installed Google Cloud SDK into `C:\Program Files (x86)\Google`:

```json
{
    "commandline": "cmd.exe /k \"C:\\Program Files (x86)\\Google\\Cloud SDK\\cloud_env.bat\"",
    "name": "Google Cloud SDK",
    "icon": "C:\\Program Files (x86)\\Google\\Cloud SDK\\supercloud-16x16.ico",
    "startingDirectory": "C:\\Program Files (x86)\\Google\\Cloud SDK"
}
```

## MSYS2

Assuming that you've installed MSYS2 into `C:\\msys64`:

```json
{
    "name": "MSYS2",
    "commandline": "C:\\msys64\\msys2_shell.cmd -defterm -no-start -mingw64",
    "icon": "C:\\msys64\\msys2.ico",
    "startingDirectory": "C:\\msys64\\home\\user"
}
```

For more details, see [this page](https://www.msys2.org/docs/terminals/#windows-terminal) on the MSYS2 documentation.


## Puppet Shell

Assuming that you've installed Puppet into `C:\Program Files\Puppet Labs`:

```json
{
    "commandline": "cmd.exe /k \"C:\\Program Files\\Puppet Labs\\Puppet\\bin\\puppet_shell.bat\"",
    "name": "Puppet Shell",
    "icon": "C:\\Program Files\\Puppet Labs\\Puppet\\misc\\puppet.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## Developer Command Prompt for Visual Studio

Assuming that you've installed VS 2019 Professional:

```json
{
    "name": "Developer Command Prompt for VS 2019",
    "commandline": "cmd.exe /k \"C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/Common7/Tools/VsDevCmd.bat\"",
    "startingDirectory": "C:\\ProgramData\\PuppetLabs"
}
```

<!-- Adding a tool here? Make sure to add it in alphabetical order! -->
