Das Dokument bietet nützliche Anleitungen, um Profile für gängige Drittanbieter-Tools zu deiner [settings.json](https://docs.microsoft.com/de-de/windows/terminal/customize-settings/profile-settings)-Datei hinzuzufügen.

## Anaconda

Angenommen, du hast Anaconda unter `%USERPROFILE%\Anaconda3` installiert:

```json
{
    "commandline": "cmd.exe /k \"%USERPROFILE%\\Anaconda3\\Scripts\\activate.bat %USERPROFILE%\\Anaconda3\"",
    "icon": "%USERPROFILE%\\Anaconda3\\Menu\\anaconda-navigator.ico",
    "name": "Anaconda3",
    "startingDirectory": "%USERPROFILE%"
}
```

## cmder

Angenommen, du hast cmder unter `%CMDER_ROOT%` installiert:

```json
{
    "commandline": "cmd.exe /k \"%CMDER_ROOT%\\vendor\\init.bat\"",
    "name": "cmder",
    "icon": "%CMDER_ROOT%\\icons\\cmder.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## Cygwin

Angenommen, du hast Cygwin unter `C:\Cygwin` installiert:

```json
{
    "name": "Cygwin",
    "commandline": "C:\\Cygwin\\bin\\bash --login -i",
    "icon": "C:\\Cygwin\\Cygwin.ico",
    "startingDirectory": "C:\\Cygwin\\bin"
}
```

Beachte, dass das Startverzeichnis von Cygwin so eingestellt ist, damit der Pfad funktioniert. Das Standardverzeichnis, das beim Start von Cygwin geöffnet wird, ist `$HOME`, da der `--login`-Parameter verwendet wird.

## Far Manager

Angenommen, du hast Far unter `c:\Program Files\Far Manager` installiert:

```json
{
    "name": "Far",
    "commandline": "\"c:\\program files\\far manager\\far.exe\"",
    "startingDirectory": "%USERPROFILE%",
    "useAcrylic": false
}
```

## Git Bash

Angenommen, du hast Git Bash unter `C:\\Program Files\\Git` installiert:

```json
{
    "name": "Git Bash",
    "commandline": "C:\\Program Files\\Git\\bin\\bash.exe -li",
    "icon": "C:\\Program Files\\Git\\mingw64\\share\\git\\git-for-windows.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## Git Bash (WOW64)

Angenommen, du hast Git Bash unter `C:\\Program Files (x86)\\Git` installiert:

```json
{
    "name": "Git Bash",
    "commandline": "%ProgramFiles(x86)%\\Git\\bin\\bash.exe -li",
    "icon": "%ProgramFiles(x86)%\\Git\\mingw32\\share\\git\\git-for-windows.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## MSYS2

Angenommen, du hast MSYS2 unter `C:\\msys64` installiert:

```json
{
    "name": "MSYS2",
    "commandline": "C:\\msys64\\msys2_shell.cmd -defterm -no-start -mingw64",
    "icon": "C:\\msys64\\msys2.ico",
    "startingDirectory": "C:\\msys64\\home\\user"
}
```

Weitere Informationen findest du auf [dieser Seite](https://www.msys2.org/docs/terminals/#windows-terminal) der MSYS2-Dokumentation.

## Developer Command Prompt for Visual Studio

Angenommen, du hast VS 2019 Professional installiert:

```json
{
    "name": "Developer Command Prompt for VS 2019",
    "commandline": "cmd.exe /k \"C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/Common7/Tools/VsDevCmd.bat\"",
    "startingDirectory": "%USERPROFILE%"
}
```

<!-- Fügst du hier ein Tool hinzu? Stelle sicher, dass es in alphabetischer Reihenfolge eingefügt wird! -->