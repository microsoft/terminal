# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuiltLayout,
    [Parameter(Mandatory)]
    [string]$RuntimeDirectory,
    [Parameter(Mandatory)]
    [string]$OutputFile,
    [switch]$ExerciseSettings,
    [switch]$ExerciseSettingsEditing,
    [switch]$ExerciseWindowCommands,
    [switch]$ExerciseStartupActions
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($ExerciseStartupActions -and (!$ExerciseSettingsEditing -or !$ExerciseWindowCommands))
{
    throw 'ExerciseStartupActions requires ExerciseSettingsEditing and ExerciseWindowCommands'
}
$layout = (Resolve-Path -LiteralPath $BuiltLayout).Path
$runtime = [IO.Path]::GetFullPath($RuntimeDirectory)
if (Test-Path -LiteralPath $runtime) { throw "Use a new isolated runtime directory: $runtime" }
foreach ($name in @('WindowsTerminal.exe', 'OpenConsole.exe', 'TerminalApp.dll',
    'Microsoft.Terminal.Settings.Model.dll', 'Microsoft.Terminal.Settings.Editor.dll', 'resources.pri'))
{
    if (!(Test-Path -LiteralPath "$layout\$name")) { throw "Incomplete unpackaged layout: $name" }
}
New-Item -ItemType Directory -Path "$runtime\settings" -Force | Out-Null
Get-ChildItem -LiteralPath $layout -Force |
    Where-Object { $_.Extension -ne '.pdb' -and $_.Name -notin @('settings', '.portable') } |
    Copy-Item -Destination $runtime -Recurse
New-Item -ItemType File -Path "$runtime\.portable" -Force | Out-Null
@'
{
    "defaultProfile": "{245ee105-acfd-4f62-a06f-03f092ac9224}",
    "disabledProfileSources": [
        "Windows.Terminal.Wsl", "Windows.Terminal.Azure",
        "Windows.Terminal.PowershellCore", "Windows.Terminal.VisualStudio",
        "Windows.Terminal.SSH"
    ],
    "confirmCloseAllTabs": false,
    "firstWindowPreference": "defaultProfile",
    "profiles": {
        "defaults": { "font": { "face": "Consolas" } },
        "list": [{
            "guid": "{245ee105-acfd-4f62-a06f-03f092ac9224}",
            "name": "Projection PCH smoke",
            "commandline": "cmd.exe",
            "startingDirectory": "%SystemRoot%"
        }]
    }
}
'@ | Set-Content -LiteralPath "$runtime\settings\settings.json" -Encoding UTF8

$exe = "$runtime\WindowsTerminal.exe"
$checks = [Collections.Generic.List[object]]::new()
$process = $null
$launchProfile = '{245ee105-acfd-4f62-a06f-03f092ac9224}'
try
{
    $process = Start-Process -FilePath $exe -PassThru -ArgumentList @(
        '-w', 'new', 'new-tab', '--title', '"Projection PCH smoke"',
        'cmd.exe', '/k', '"echo Terminal build smoke"'
    )
    [void]$process.Handle
    $id = $process.Id
    $deadline = [DateTime]::UtcNow.AddSeconds(40)
    do
    {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        if ($process.HasExited) { throw "Window launch exited early with $($process.ExitCode): PID $id" }
    } while ($process.MainWindowHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
    if ($process.MainWindowHandle -eq [IntPtr]::Zero) { throw "No Terminal window appeared: PID $id" }
    $checks.Add([pscustomobject]@{ check = 'terminal window'; pid = $id; title = $process.MainWindowTitle })

    if ($ExerciseSettings -or $ExerciseSettingsEditing)
    {
        if ($ExerciseSettingsEditing -and @($process.Modules | Where-Object ModuleName -EQ 'Microsoft.Terminal.Settings.Editor.dll').Count)
        {
            throw 'The Editor DLL was loaded before opening Settings'
        }
        Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
        $window = [Windows.Automation.AutomationElement]::FromHandle($process.MainWindowHandle)
        if ($window.Current.ProcessId -ne $id) { throw 'Automation attached to a different process' }

        function Find-Control([Windows.Automation.AutomationElement]$parent, [string]$automationId)
        {
            $condition = [Windows.Automation.PropertyCondition]::new(
                [Windows.Automation.AutomationElement]::AutomationIdProperty, $automationId)
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            do
            {
                $element = $parent.FindFirst([Windows.Automation.TreeScope]::Descendants, $condition)
                if ($null -ne $element) { return $element }
                if ($process.HasExited) { throw "Terminal exited while finding $automationId" }
                Start-Sleep -Milliseconds 250
            } while ([DateTime]::UtcNow -lt $deadline)
            throw "Control not found in the isolated Terminal window: $automationId"
        }
        $button = Find-Control $window 'NewTabButton'
        $expand = $button.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern)
        $expand.Expand()
        $ownerCondition = [Windows.Automation.PropertyCondition]::new(
            [Windows.Automation.AutomationElement]::ProcessIdProperty, $id)
        function Find-MenuEntry([string]$name)
        {
            $menuCondition = [Windows.Automation.PropertyCondition]::new(
                [Windows.Automation.AutomationElement]::NameProperty, $name)
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            do
            {
                $menu = $window.FindFirst([Windows.Automation.TreeScope]::Descendants, $menuCondition)
                if ($null -eq $menu)
                {
                    $ownedWindows = [Windows.Automation.AutomationElement]::RootElement.FindAll(
                        [Windows.Automation.TreeScope]::Children, $ownerCondition)
                    foreach ($ownedWindow in $ownedWindows)
                    {
                        $menu = $ownedWindow.FindFirst([Windows.Automation.TreeScope]::Descendants, $menuCondition)
                        if ($null -ne $menu) { break }
                    }
                }
                if ($null -ne $menu) { return $menu }
                if ($process.HasExited) { throw 'Terminal exited while opening its menu' }
                Start-Sleep -Milliseconds 250
            } while ([DateTime]::UtcNow -lt $deadline)
            throw "Menu item not found in the isolated process (English UI required): $name"
        }
        $menu = Find-MenuEntry 'Settings'
        $invoke = $menu.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern)
        $invoke.Invoke()
        $navigation = Find-Control $window 'SettingsNav'
        $startup = Find-Control $navigation 'LaunchNavItem'
        $checks.Add([pscustomobject]@{ check = 'lazy-loaded Settings UI'; pid = $id; startupItem = $startup.Current.Name })

        if ($ExerciseSettingsEditing)
        {
            $process.Refresh()
            if (!@($process.Modules | Where-Object ModuleName -EQ 'Microsoft.Terminal.Settings.Editor.dll').Count)
            {
                throw 'The Editor DLL was not loaded when Settings opened'
            }
            function New-ProfileDraft([string]$name)
            {
                $profiles = Find-Control $window 'ProfilesNavItem'
                $profiles.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select()
                $addProfile = Find-Control $window 'AddProfileButton'
                $addProfile.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
                $nameCard = Find-Control $window 'Name'
                $editCondition = [Windows.Automation.PropertyCondition]::new(
                    [Windows.Automation.AutomationElement]::ControlTypeProperty, [Windows.Automation.ControlType]::Edit)
                $nameBox = $nameCard.FindFirst([Windows.Automation.TreeScope]::Descendants, $editCondition)
                if ($null -eq $nameBox) { throw 'New profile name editor not found' }
                $nameBox.SetFocus()
                $nameBox.GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).SetValue($name)
                (Find-Control $window 'SaveButton').SetFocus()
            }
            New-ProfileDraft 'Native contract discarded draft'
            $reset = Find-Control $window 'ResetButton'
            $reset.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
            $profiles = Find-Control $window 'ProfilesNavItem'
            $profiles.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select()
            Find-Control $window 'AddProfileButton' | Out-Null
            $discardedCondition = [Windows.Automation.PropertyCondition]::new(
                [Windows.Automation.AutomationElement]::NameProperty, 'Native contract discarded draft')
            if ($null -ne $window.FindFirst([Windows.Automation.TreeScope]::Descendants, $discardedCondition))
            {
                throw 'Reset left the unsaved profile in the UI'
            }
            New-ProfileDraft 'Native contract edited profile'
            $beforeSave = Get-Content -LiteralPath "$runtime\settings\settings.json" -Raw | ConvertFrom-Json
            if (@($beforeSave.profiles.list | Where-Object name -EQ 'Native contract edited profile').Count)
            {
                throw 'The profile draft reached the settings file before Save'
            }
            $save = Find-Control $window 'SaveButton'
            $save.SetFocus()
            $save.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
            $deadline = [DateTime]::UtcNow.AddSeconds(20)
            do
            {
                Start-Sleep -Milliseconds 250
                $settings = Get-Content -LiteralPath "$runtime\settings\settings.json" -Raw | ConvertFrom-Json
                $savedProfiles = @($settings.profiles.list | Where-Object name -EQ 'Native contract edited profile')
            } while ($savedProfiles.Count -ne 1 -and [DateTime]::UtcNow -lt $deadline)
            if ($savedProfiles.Count -ne 1) { throw 'Edited profile was not saved exactly once' }
            if (@($settings.profiles.list | Where-Object name -EQ 'Native contract discarded draft').Count)
            {
                throw 'Reset did not discard the original profile draft'
            }
            if (@($settings.profiles.list | Where-Object guid -EQ '{245ee105-acfd-4f62-a06f-03f092ac9224}').Count -ne 1)
            {
                throw 'Adding a profile changed the identity of the existing profile'
            }
            $launchProfile = $savedProfiles[0].guid
            $newTab = Find-Control $window 'NewTabButton'
            $newTab.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
            Find-MenuEntry 'Native contract edited profile' | Out-Null
            $newTab.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Collapse()
            $checks.Add([pscustomobject]@{
                check = 'native settings edit and save through XAML'
                pid = $id
                savedProfileGuid = $savedProfiles[0].guid
                editorLazyLoadVerified = $true
                draftResetVerified = $true
                settingsReloadObserved = $true
            })
        }
    }
    if ($ExerciseWindowCommands)
    {
        Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
        $windowCondition = [Windows.Automation.AndCondition]::new(
            [Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ProcessIdProperty, $id),
            [Windows.Automation.PropertyCondition]::new(
                [Windows.Automation.AutomationElement]::ControlTypeProperty, [Windows.Automation.ControlType]::Window))
        function Get-OwnedWindows
        {
            [Windows.Automation.AutomationElement]::RootElement.FindAll(
                [Windows.Automation.TreeScope]::Children, $windowCondition)
        }
        function Invoke-WindowCommand([string]$title)
        {
            $command = Start-Process -FilePath $exe -PassThru -ArgumentList @(
                '-w', 'native-refactor-smoke', 'new-tab', '--title', "`"$title`"",
                '-p', $launchProfile,
                'cmd.exe', '/k', '"echo Native contract window command"')
            [void]$command.Handle
            try
            {
                if (!$command.WaitForExit(20000)) { throw 'Window command was not handed to the existing process' }
                if ($command.ExitCode -ne 0) { throw "Window command failed: $($command.ExitCode)" }
            }
            finally
            {
                if (!$command.HasExited) { Stop-Process -Id $command.Id -Force }
                $command.Dispose()
            }
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            do
            {
                if ($process.HasExited) { throw 'The existing process exited during window command handoff' }
                $matches = @(Get-OwnedWindows | Where-Object { $_.Current.Name.StartsWith($title, [StringComparison]::Ordinal) })
                if ($matches.Count -eq 1) { return $matches[0] }
                Start-Sleep -Milliseconds 250
            } while ([DateTime]::UtcNow -lt $deadline)
            throw "Expected one owned window titled $title"
        }
        $secondWindow = Invoke-WindowCommand 'Native second window'
        $secondHandle = $secondWindow.Current.NativeWindowHandle
        $forwardedWindow = Invoke-WindowCommand 'Native forwarded tab'
        if ($forwardedWindow.Current.NativeWindowHandle -ne $secondHandle -or @(Get-OwnedWindows).Count -ne 2)
        {
            throw 'Forwarding to the named window created or targeted the wrong window'
        }
        $forwardedWindow.GetCurrentPattern([Windows.Automation.WindowPattern]::Pattern).Close()
        $deadline = [DateTime]::UtcNow.AddSeconds(20)
        do
        {
            Start-Sleep -Milliseconds 250
            $remaining = @(Get-OwnedWindows)
        } while ($remaining.Count -ne 1 -and [DateTime]::UtcNow -lt $deadline)
        if ($remaining.Count -ne 1 -or $process.HasExited) { throw 'Closing the second window did not preserve the first' }
        $checks.Add([pscustomobject]@{
            check = 'named window creation, command handoff and independent close'
            pid = $id
            secondWindowHandle = $secondHandle
        })
        if ($ExerciseStartupActions)
        {
            function Invoke-PaletteAction([Windows.Automation.AutomationElement]$target, [string]$name)
            {
                $newTab = Find-Control $target 'NewTabButton'
                $newTab.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
                (Find-MenuEntry 'Command palette').GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
                $search = Find-Control $target '_searchBox'
                $search.SetFocus()
                $search.GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).SetValue($name)
                $list = Find-Control $target '_filteredActionsView'
                $condition = [Windows.Automation.AndCondition]::new(
                    [Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty, $name),
                    [Windows.Automation.PropertyCondition]::new(
                        [Windows.Automation.AutomationElement]::ControlTypeProperty, [Windows.Automation.ControlType]::ListItem))
                $deadline = [DateTime]::UtcNow.AddSeconds(20)
                do
                {
                    $item = $list.FindFirst([Windows.Automation.TreeScope]::Descendants, $condition)
                    if ($null -ne $item) { break }
                    Start-Sleep -Milliseconds 250
                } while ([DateTime]::UtcNow -lt $deadline)
                if ($null -eq $item) { throw "Palette action not found: $name" }
                $item.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
            }
            $firstHandle = $remaining[0].Current.NativeWindowHandle
            Invoke-PaletteAction $remaining[0] 'New window'
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            do
            {
                Start-Sleep -Milliseconds 250
                $created = @(Get-OwnedWindows | Where-Object { $_.Current.NativeWindowHandle -ne $firstHandle })
            } while ($created.Count -ne 1 -and [DateTime]::UtcNow -lt $deadline)
            if ($created.Count -ne 1) { throw 'New-window action did not create one additional window' }
            $actionHandle = $created[0].Current.NativeWindowHandle
            Invoke-PaletteAction $created[0] 'Move pane to new window'
            $deadline = [DateTime]::UtcNow.AddSeconds(30)
            do
            {
                Start-Sleep -Milliseconds 250
                $owned = @(Get-OwnedWindows)
                $moved = @($owned | Where-Object { $_.Current.NativeWindowHandle -ne $firstHandle -and $_.Current.NativeWindowHandle -ne $actionHandle })
            } while (($owned.Count -ne 2 -or $moved.Count -ne 1) -and [DateTime]::UtcNow -lt $deadline)
            if ($owned.Count -ne 2 -or $moved.Count -ne 1) { throw 'Pane transfer did not replace the source window with a content-restored window' }
            $moved[0].GetCurrentPattern([Windows.Automation.WindowPattern]::Pattern).Close()
            $deadline = [DateTime]::UtcNow.AddSeconds(20)
            do
            {
                Start-Sleep -Milliseconds 250
                $owned = @(Get-OwnedWindows)
            } while ($owned.Count -ne 1 -and [DateTime]::UtcNow -lt $deadline)
            if ($owned.Count -ne 1 -or $process.HasExited) { throw 'Content-window close did not preserve the original window' }
            $checks.Add([pscustomobject]@{
                check = 'startup-action window and content-transfer window'
                pid = $id
            })
        }
    }
    $process.Refresh()
    if (!$process.CloseMainWindow()) { throw "Could not request window shutdown: PID $id" }
    if (!$process.WaitForExit(20000)) { throw "Terminal did not shut down after closing its window: PID $id" }
    if ($process.ExitCode -ne 0) { throw "Terminal window exited with $($process.ExitCode): PID $id" }
    $checks.Add([pscustomobject]@{ check = 'window shutdown'; pid = $id; exitCode = $process.ExitCode })
}
finally
{
    if ($null -ne $process)
    {
        $process.Refresh()
        if (!$process.HasExited)
        {
            Stop-Process -Id $process.Id -Force -ErrorAction Stop
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}
[ordered]@{
    runtimeDirectory = $runtime
    executableSha256 = (Get-FileHash -LiteralPath $exe).Hash
    portableSettings = "$runtime\settings\settings.json"
    checks = $checks.ToArray()
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputFile -Encoding UTF8
$checks | Format-Table -AutoSize
