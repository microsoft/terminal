# Windows Terminal Customization

NOTE: At the time of writing Windows Terminal is still under active development and many things will
change. If you notice an error in the docs, please raise an issue. Or better yet, please file a PR with an appropriate update!

## Title of tab
When Windows Terminal launches, the shell has a special variable to determine the window title. This title is used to set the name of the tab inside Windows Terminal.
There are 2 options for controlling the title of the tab.

1. Set the title from the settings of Windows Terminal
2. Set the title within the shell (varies by shell)

#### 1. Set the title from the settings of Windows Terminal
In the `settings.json` file from Windows Terminal, you can overrule the title of the tab by specifying the property `tabTitle`.
The `tabTitle` overrules the title from the individual shell. Please note that the `tabTitle` is static.

#### 2. Set the title within the shell (varies by shell)
This is a more dynamic solution, but may require you to run a command within the shell itself (depending on the shell).
Below are som example options for controlling the title of your shell.

#### Examples of setting the title within the shell
##### PowerShell
```
$Host.UI.RawUI.WindowTitle = "My PowerShell"
```
###### Documentation: [Get-Host - Microsoft Docs][psgethost] | [PSHostRawUserInterface - Microsoft Docs][pswindowtitle]

##### CMD
```
title My cmd
```
###### Documentation: [title - Microsoft Docs][cmdtitle]

## Customizing the prompt
The prompt is the part of the terminal shown before where your cursor is placed. 

#### Examples of setting the prompt within the shell
##### PowerShell
```
function prompt {"my new prompt"}
```
###### Documentation: [about_Prompts - Microsoft Docs][psprompt]

##### CMD
```
prompt my new prompt
```
###### Documentation: [prompt - Microsoft Docs][cmdprompt]

## Combining prompt and title
Since PowerShell uses a function to evaluate the prompt, you are able to set the title when PowerShell runs the `prompt` function.
An example is the following where the title would update to be the current location of the shell, since the `promp` function is executed every time a statement has been executed.
```
funciton prompt {
  $Host.UI.RawUI.WindowTitle = "PowerShell $(Get-Location)"
  "PS> "
}
```


[psgethost]: https://docs.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/get-host
[pswindowtitle]: https://docs.microsoft.com/en-us/dotnet/api/system.management.automation.host.pshostrawuserinterface.windowtitle
[cmdtitle]: https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/title_1
[psprompt]: https://docs.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_prompts
[cmdprompt]: https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/prompt