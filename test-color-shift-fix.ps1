# Test script for color shift fix
# This script displays ANSI colors to test if they remain stable when window loses focus

Write-Host "`n=== Windows Terminal Color Shift Fix Test ===" -ForegroundColor Cyan
Write-Host "Instructions:" -ForegroundColor Yellow
Write-Host "1. This script will display various ANSI colors"
Write-Host "2. Unfocus/minimize the terminal window"
Write-Host "3. Colors should remain STABLE (no flickering/shifting)"
Write-Host "4. Focus the window again - colors should stay consistent"
Write-Host "`nPress any key to start the test..." -ForegroundColor Green
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

Clear-Host

# Display 256 color palette
Write-Host "`n256-Color Palette Test:" -ForegroundColor Cyan
Write-Host "========================`n"

for ($i = 0; $i -lt 256; $i++) {
    $esc = [char]27
    Write-Host -NoNewline "${esc}[38;5;${i}m█${esc}[0m"
    if (($i + 1) % 16 -eq 0) {
        Write-Host ""
    }
}

Write-Host "`n"

# Grey scale test (where the issue is most visible)
Write-Host "Grey Scale Test (232-255):" -ForegroundColor Cyan
Write-Host "===========================`n"

$esc = [char]27
for ($i = 232; $i -le 255; $i++) {
    Write-Host -NoNewline "${esc}[48;5;${i}m   ${esc}[0m "
}
Write-Host "`n"

# Display some colored text
Write-Host "`nColored Text Test:" -ForegroundColor Cyan
Write-Host "==================`n"

$colors = @(
    @{code=30; name="Black"},
    @{code=31; name="Red"},
    @{code=32; name="Green"},
    @{code=33; name="Yellow"},
    @{code=34; name="Blue"},
    @{code=35; name="Magenta"},
    @{code=36; name="Cyan"},
    @{code=37; name="White"},
    @{code=90; name="Bright Black (Grey)"},
    @{code=91; name="Bright Red"},
    @{code=92; name="Bright Green"},
    @{code=93; name="Bright Yellow"},
    @{code=94; name="Bright Blue"},
    @{code=95; name="Bright Magenta"},
    @{code=96; name="Bright Cyan"},
    @{code=97; name="Bright White"}
)

foreach ($color in $colors) {
    $code = $color.code
    $name = $color.name
    Write-Host "${esc}[${code}m$name - The quick brown fox jumps over the lazy dog${esc}[0m"
}

Write-Host "`n`n=== TEST INSTRUCTIONS ===" -ForegroundColor Yellow
Write-Host "1. Keep this window visible"
Write-Host "2. Click on another window to UNFOCUS this terminal"
Write-Host "3. Watch the colors - they should NOT shift or flicker"
Write-Host "4. Click back to focus this window"
Write-Host "5. Minimize and restore the window"
Write-Host "6. Colors should remain STABLE throughout"
Write-Host "`nWith the fix applied: Colors stay stable ✓" -ForegroundColor Green
Write-Host "Without the fix: Grey colors would flicker ✗" -ForegroundColor Red
Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
