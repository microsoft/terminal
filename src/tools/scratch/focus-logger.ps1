Add-Type @"
  using System;
  using System.Runtime.InteropServices;
  public class Win32 {
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
  }
"@

function Get-ForegroundWindow {
  [System.IntPtr]$handle = [Win32]::GetForegroundWindow()
  return $handle
}

function Get-ProcessFromWindowHandle {
  param(
    [System.IntPtr]$hwnd
  )

  $p = 0
  [Win32]::GetWindowThreadProcessId($hwnd, [ref]$p)
  if ($p -ne 0) {
    $process = Get-Process -Id $p -ErrorAction SilentlyContinue
    if ($process) {
      return $process
    }
  }

  return $null
}

$processId = (Get-Process -Id $pid).Id
$processName = (Get-Process -Id $pid).ProcessName
Write-Host "Current process: $processName"
$hwnd = 0;

while ($true) {

  $hwndNew = Get-ForegroundWindow;


  if ($hwndNew -ne $hwnd) {
    $hwnd = $hwndNew
    $processIdNew = (Get-ProcessFromWindowHandle $hwnd).Id

    if ($processIdNew -ne $processId) {
      $processId = $processIdNew
      $processName = (Get-Process -Id $processId -FileVersionInfo).FileName
      Write-Host "Current process: $processName"
    }
  }
  Start-Sleep -Milliseconds 100
}
