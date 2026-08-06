# This script is a failed attempt to lock the Cascadia Mono/Code font files in order to reproduce an issue with the font
# cache service, where it says a font exists, but then fails to use it (see GH#9375). The script doesn't work because
# for some reason DirectWrite is still able to fully use the fonts. It's left here for reference.

#Requires -RunAsAdministrator

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class Win32LockFile {
    public const uint LOCKFILE_FAIL_IMMEDIATELY = 0x00000001;
    public const uint LOCKFILE_EXCLUSIVE_LOCK = 0x00000002;

    [DllImport("kernel32.dll")]
    public static extern bool LockFileEx(IntPtr hFile, uint dwFlags, uint dwReserved, uint nNumberOfBytesToLockLow, uint nNumberOfBytesToLockHigh, ref OVERLAPPED lpOverlapped);

    [StructLayout(LayoutKind.Sequential)]
    public struct OVERLAPPED {
        public uint Internal;
        public uint InternalHigh;
        public uint Offset;
        public uint OffsetHigh;
        public IntPtr hEvent;
    }
}
"@

function Lock-File {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Path
    )

    $file = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    $overlapped = New-Object Win32LockFile+OVERLAPPED
    $result = [Win32LockFile]::LockFileEx(
        $file.SafeFileHandle.DangerousGetHandle(), # hFile
        [Win32LockFile]::LOCKFILE_EXCLUSIVE_LOCK,  # dwFlags
        0,                                         # dwReserved
        [UInt32]::MaxValue,                        # nNumberOfBytesToLockLow
        [UInt32]::MaxValue,                        # nNumberOfBytesToLockHigh
        [ref]$overlapped                           # lpOverlapped
    )

    if (-not $result) {
        throw "Failed to lock file"
    }

    return $file
}

$fonts = Get-ItemProperty "HKCU:\Software\Microsoft\Windows NT\CurrentVersion\Fonts\*"
    | ForEach-Object { $_.PSobject.Properties }
    | Where-Object { $_.Name.StartsWith("Cascadia") }
    | ForEach-Object { $_.Value }

$fonts += @("CascadiaCode.ttf", "CascadiaCodeItalic.ttf", "CascadiaMono.ttf", "CascadiaMonoItalic.ttf")
    | ForEach-Object { "C:\Windows\Fonts\$_" }
    | Where-Object { Test-Path $_ }

try {
    $handles = $fonts | ForEach-Object {
        try {
            Lock-File $_
        }
        catch {
            Write-Error $_
        }
    }
    Restart-Service FontCache
    Write-Host "Press any key to unlock the font files..."
    $null = $host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
} finally {
    $handles | Where-Object { $_ } | ForEach-Object { $_.Close() }
}
