# Adding profiles for third-party tools

This doc will hopefully provide a useful guide for adding profiles for common
third-party tools to your
[profiles.json](https://github.com/microsoft/terminal/blob/master/doc/user-docs/UsingJsonSettings.md)
file.

All of these profiles are provided _without_ their `guid` set. If you'd like to
set any of these profiles as your _default_ profile, you'll need to make sure to
[generate a unique guid](https://www.guidgenerator.com/) for them manually.

## Anaconda

Assuming that you've installed Anaconda into `%USERPROFILE%\Anaconda3`:

```json
{
    "commandline" : "cmd.exe /K %USERPROFILE%\\Anaconda3\\Scripts\\activate.bat %USERPROFILE%\\Anaconda3",
    "icon" : "%USERPROFILE%/Anaconda3/Menu/anaconda-navigator.ico",
    "name" : "Anaconda3",
    "startingDirectory" : "%USERPROFILE%"
}
```

## cmder

Assuming that you've installed cmder into `%CMDER_ROOT%`:

```json
{
    "commandline" : "cmd.exe /k %CMDER_ROOT%\\vendor\\init.bat",
    "name" : "cmder",
    "startingDirectory" : "%USERPROFILE%"
}
```
