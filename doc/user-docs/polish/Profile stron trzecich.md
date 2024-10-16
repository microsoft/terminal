Dokument ten zawiera przydatne instrukcje dotyczące dodawania profili dla popularnych narzędzi firm trzecich do pliku [settings.json](https://docs.microsoft.com/pl-pl/windows/terminal/customize-settings/profile-settings).

## Anaconda

Zakładając, że zainstalowałeś Anacondę w `%USERPROFILE%\Anaconda3`:

```json
{
    "commandline": "cmd.exe /k \"%USERPROFILE%\\Anaconda3\\Scripts\\activate.bat %USERPROFILE%\\Anaconda3\"",
    "icon": "%USERPROFILE%\\Anaconda3\\Menu\\anaconda-navigator.ico",
    "name": "Anaconda3",
    "startingDirectory": "%USERPROFILE%"
}
```

## cmder

Zakładając, że zainstalowałeś cmder w `%CMDER_ROOT%`:

```json
{
    "commandline": "cmd.exe /k \"%CMDER_ROOT%\\vendor\\init.bat\"",
    "name": "cmder",
    "icon": "%CMDER_ROOT%\\icons\\cmder.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## Cygwin

Zakładając, że zainstalowałeś Cygwin w `C:\Cygwin`:

```json
{
    "name": "Cygwin",
    "commandline": "C:\\Cygwin\\bin\\bash --login -i",
    "icon": "C:\\Cygwin\\Cygwin.ico",
    "startingDirectory": "C:\\Cygwin\\bin"
}
```

Zwróć uwagę, że początkowy katalog Cygwin jest ustawiony w ten sposób, aby ścieżka działała poprawnie. Domyślny katalog otwierany podczas uruchamiania Cygwin to `$HOME` z powodu użycia flagi `--login`.

## Far Manager

Zakładając, że zainstalowałeś Far w `c:\Program Files\Far Manager`:

```json
{
    "name": "Far",
    "commandline": "\"c:\\program files\\far manager\\far.exe\"",
    "startingDirectory": "%USERPROFILE%",
    "useAcrylic": false
}
```

## Git Bash

Zakładając, że zainstalowałeś Git Bash w `C:\\Program Files\\Git`:

```json
{
    "name": "Git Bash",
    "commandline": "C:\\Program Files\\Git\\bin\\bash.exe -li",
    "icon": "C:\\Program Files\\Git\\mingw64\\share\\git\\git-for-windows.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## Git Bash (WOW64)

Zakładając, że zainstalowałeś Git Bash w `C:\\Program Files (x86)\\Git`:

```json
{
    "name": "Git Bash",
    "commandline": "%ProgramFiles(x86)%\\Git\\bin\\bash.exe -li",
    "icon": "%ProgramFiles(x86)%\\Git\\mingw32\\share\\git\\git-for-windows.ico",
    "startingDirectory": "%USERPROFILE%"
}
```

## MSYS2

Zakładając, że zainstalowałeś MSYS2 w `C:\\msys64`:

```json
{
    "name": "MSYS2",
    "commandline": "C:\\msys64\\msys2_shell.cmd -defterm -no-start -mingw64",
    "icon": "C:\\msys64\\msys2.ico",
    "startingDirectory": "C:\\msys64\\home\\user"
}
```

Więcej informacji znajdziesz na [tej stronie](https://www.msys2.org/docs/terminals/#windows-terminal) w dokumentacji MSYS2.

## Developer Command Prompt for Visual Studio

Zakładając, że zainstalowałeś VS 2019 Professional:

```json
{
    "name": "Developer Command Prompt for VS 2019",
    "commandline": "cmd.exe /k \"C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/Common7/Tools/VsDevCmd.bat\"",
    "startingDirectory": "%USERPROFILE%"
}
```

<!-- Dodajesz narzędzie? Upewnij się, że dodajesz je w porządku alfabetycznym! -->