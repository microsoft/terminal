# Copilot Instructions for Windows Terminal

## Build & Test

### Building

**Important:** `Set-MsBuildDevEnvironment` (or `.\tools\razzle.cmd`) must be run first in each new terminal session to put the VS 2022 MSBuild on your PATH.

**Full solution (OpenConsole + Terminal):**
```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment
Invoke-OpenConsoleBuild
```

**Cmd** (after `.\tools\razzle.cmd`):
```cmd
bcz
```

### Building Windows Terminal from the CLI

Use MSBuild solution-level targets to build specific Terminal projects without opening Visual Studio. This is the recommended way to verify compilation from the command line.

```powershell
Import-Module .\tools\OpenConsole.psm1
Set-MsBuildDevEnvironment

# Build TerminalAppLib — compiles TerminalCore, TerminalControl, SettingsModel, SettingsEditor, and TerminalApp
# This is the best single target for verifying most Terminal changes compile (~16s incremental)
msbuild OpenConsole.slnx /t:"Terminal\TerminalAppLib" /p:Configuration=Debug /p:Platform=x64 /p:AppxBundle=false /p:GenerateAppxPackageOnBuild=false /m

# Build the full CascadiaPackage (all Terminal components + packaging, ~27s incremental)
msbuild OpenConsole.slnx /t:"Terminal\CascadiaPackage" /p:Configuration=Debug /p:Platform=x64 /p:AppxBundle=false /p:GenerateAppxPackageOnBuild=false /m

# Build just the SettingsModel library
msbuild OpenConsole.slnx /t:"Terminal\Settings\Microsoft_Terminal_Settings_ModelLib" /p:Configuration=Debug /p:Platform=x64 /p:AppxBundle=false /m
```

Target names are derived from slnx folder paths and project file names (dots become underscores). Common targets:
- `Terminal\TerminalAppLib` — Terminal app library (depends on most other Terminal projects)
- `Terminal\CascadiaPackage` — Full Terminal package (all projects)
- `Terminal\Settings\Microsoft_Terminal_Settings_ModelLib` — Settings model only
- `Terminal\Settings\Microsoft_Terminal_Settings_Editor` — Settings editor only

**Deploy after building** (requires the CascadiaPackage target):
```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\DeployAppRecipe.exe" src\cascadia\CascadiaPackage\bin\x64\Debug\CascadiaPackage.build.appxrecipe
```

### Running Tests

**PowerShell:**
```powershell
Import-Module .\tools\OpenConsole.psm1
Invoke-OpenConsoleTests                          # all unit tests
Invoke-OpenConsoleTests -Test terminalCore       # single test project
Invoke-OpenConsoleTests -Test unitSettingsModel   # settings model tests
Invoke-OpenConsoleTests -Test til                # TIL library tests
```

Valid `-Test` values: `host`, `interactivityWin32`, `terminal`, `adapter`, `feature`, `uia`, `textbuffer`, `til`, `types`, `terminalCore`, `terminalApp`, `localTerminalApp`, `unitSettingsModel`, `unitControl`, `winconpty`.

**Cmd** (after `.\tools\razzle.cmd`):
```cmd
runut.cmd                                              # all unit tests
te.exe MyTests.dll /name:*TestMethodPattern*           # single test by name pattern
runut *Tests.dll /name:TextBufferTests::TestMethod /waitForDebugger  # debug a test
```

### Code Formatting

C++ formatting uses clang-format (config in `.clang-format`, Microsoft style base, no column limit). Run:
```powershell
Invoke-CodeFormat          # PowerShell
runformat                  # Cmd
```

XAML formatting uses XamlStyler (config in `XamlStyler.json`).

## Architecture

This repo contains two products sharing core components:

- **Windows Terminal** (`src/cascadia/`) — modern terminal app using WinUI/XAML, C++/WinRT
- **Windows Console Host** (`src/host/`) — classic `conhost.exe`, built in C/C++ with Win32

### Key shared libraries

| Path | Purpose |
|------|---------|
| `src/buffer/out/` | Text buffer (UTF-16/UTF-8 storage) |
| `src/renderer/` | Rendering abstraction (base, DX, GDI engines) |
| `src/terminal/parser/` | VT sequence state machine & parser |
| `src/terminal/adapter/` | Converts VT verbs to console API calls |
| `src/types/` | Shared types (Viewport, ColorFix, etc.) |
| `src/til/` | Terminal Implementation Library — header-only utilities (math, color, string, enumset, rect) |

### Windows Terminal layers (`src/cascadia/`)

| Layer | Purpose |
|-------|---------|
| `TerminalCore` | Core terminal instance (`Terminal` class) — buffer, VT parsing, input. No UI dependency |
| `TerminalControl` | UWP/XAML control wrapping TerminalCore, DX renderer, input translation |
| `TerminalApp` | Application logic: tabs, panes, command palette, settings UI hosting |
| `TerminalConnection` | Backend connections (ConPTY, Azure Cloud Shell, SSH) |
| `TerminalSettingsModel` | Settings parsing, serialization, profile/scheme/action model |
| `TerminalSettingsEditor` | Settings UI pages (XAML + ViewModel pattern) |
| `WindowsTerminal` | Win32 EXE host: XAML Islands, window chrome, non-client area |
| `Remoting` | Multi-instance coordination (Monarch/Peasant pattern) |
| `CascadiaPackage` | MSIX packaging project |

## Key Conventions

### C++ Style
- Follow existing style in modified code; use Modern C++ and the [C++ Core Guidelines](https://github.com/isocpp/CppCoreGuidelines) for new code
- Use [WIL](https://github.com/Microsoft/wil) smart pointers and result macros (`RETURN_IF_FAILED`, `THROW_IF_FAILED`, `LOG_IF_FAILED`, etc.) for Win32/NT API interaction
- Prefer `HRESULT` over `NTSTATUS`. Functions that always succeed should not return a status code
- Do not let exceptions leak from new code into old (non-exception-safe) code. Encapsulate exception behaviors within classes
- Private members prefixed with `_` (e.g., `_somePrivateMethod()`, `_myField`)

### C++/WinRT & XAML Patterns
- Use `WINRT_PROPERTY(type, name)` for simple getters/setters, `WINRT_OBSERVABLE_PROPERTY` for properties with `PropertyChanged` events (defined in `src/cascadia/inc/cppwinrt_utils.h`)
- Use `TYPED_EVENT` macro for event declarations
- Use `safe_void_coroutine` instead of `winrt::fire_and_forget` — it logs exceptions via `LOG_CAUGHT_EXCEPTION()` instead of silently terminating
- Be mindful of C++/WinRT [strong/weak references](https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/weak-references) and [concurrency](https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/concurrency) in `TerminalApp` code

### Settings Editor UI Pages
Each page consists of coordinated files:
- `.idl` — WinRT interface definition
- `.xaml` — UI layout (uses `x:Bind` to ViewModel)
- `.h` / `.cpp` — Page code-behind (inherits generated `*T<>` base, calls `InitializeComponent()`)
- `*ViewModel.idl` / `.h` / `.cpp` — ViewModel with properties and logic

Navigation uses string tags (defined in `NavConstants.h`) dispatched in `MainPage.cpp`.

### Settings Model (`TerminalSettingsModel`)
- Settings types use an `IInheritable` pattern for layered defaults (profile inherits from defaults)
- IDL files define WinRT projections; C++ headers use macros like `WINRT_PROPERTY` for implementation
- JSON serialization uses `JsonUtils.h` helpers

### Test Organization
- Unit tests: `ut_*` subdirectories (e.g., `src/host/ut_host/`, `src/cascadia/UnitTests_TerminalCore/`)
- Feature tests: `ft_*` subdirectories (e.g., `src/host/ft_api/`)
- Local app tests: `src/cascadia/LocalTests_TerminalApp/` (requires AppContainer)
- Tests use TAEF (Test Authoring and Execution Framework) with `WexTestClass.h`, not Visual Studio Test
- Test classes use `TEST_CLASS()` and `TEST_METHOD()` macros

### Code Organization
- Package new components as libraries with clean interfaces
- Place shared interfaces in `inc/` folders
- Structure related libraries together (e.g., `terminal/parser` + `terminal/adapter`)
