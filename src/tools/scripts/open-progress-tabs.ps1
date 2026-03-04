# This script is re-entrant. Without a param, it will launch all the tabs. If
# you provide a param (e.g. "1"), it will launch just that tab, which is useful
# for testing the filtering behavior of nextTab/prevTab.

Param(
    [string]$tab
)

if ($tab) {

    # BEL character
    $bel = [char]0x07

    # Tab 1: plain (no indicators)
    # Tab 2: bell
    # Tab 3: progress "set" at 50%
    # Tab 4: progress "error" at 75%
    # Tab 5: progress "indeterminate"
    # Tab 6: progress "paused" at 30%

    switch ($tab) {
        "1" { Write-Host 'Tab 1: plain (no status)' }`
        "2" { Write-Host 'Tab 2: bell'; Start-Sleep -Milliseconds 1500; Write-Host -NoNewline $bel }
        "3" { Write-Host 'Tab 3: progress set 50%'; Start-Sleep -Milliseconds 500; Write-Host -NoNewline ("`e]9;4;1;50`e\") }
        "4" { Write-Host 'Tab 4: progress error 75%'; Start-Sleep -Milliseconds 500; Write-Host -NoNewline ("`e]9;4;2;75`e\") }
        "5" { Write-Host 'Tab 5: progress indeterminate'; Start-Sleep -Milliseconds 500; Write-Host -NoNewline ("`e]9;4;3;0`e\") }
        "6" { Write-Host 'Tab 6: progress paused 30%'; Start-Sleep -Milliseconds 500; Write-Host -NoNewline ("`e]9;4;4;30`e\") }
    }

    exit
}


# OSC 9;4 sequences for taskbar/progress state:
#   state 1 = set, 2 = error, 3 = indeterminate, 4 = paused
# Format: ESC ] 9 ; 4 ; <state> ; <progress> ESC \
# $esc = [char]0x1b

# get the path to us
$scriptPath = $MyInvocation.MyCommand.Path
$pwsh = "C:\Program Files\PowerShell\7\pwsh.exe"

$i = 0
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 1`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 2`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 3`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 4`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 5`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 6`"
sleep 1
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 1`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 2`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 3`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 4`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 5`"
wtd -w test new-tab --tabTitle ${i++} "$pwsh" -NoExit -Command `"$scriptPath 6`"