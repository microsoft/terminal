# Windows Terminal Architecture

> Comprehensive architectural documentation for Windows Terminal
>
> Generated: 2026-01-26

## Table of Contents

1. [High-Level Architecture Overview](#high-level-architecture-overview)
2. [Architectural Layers](#architectural-layers)
3. [Major Components](#major-components)
4. [Data Flow Patterns](#data-flow-patterns)
5. [Key Interfaces](#key-interfaces)
6. [Rendering Architecture](#rendering-architecture)
7. [Settings Architecture](#settings-architecture)
8. [Design Principles](#design-principles)
9. [Component Interactions](#component-interactions)
10. [File Locations](#file-locations)

---

## High-Level Architecture Overview

Windows Terminal is organized in a layered architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│  WindowsTerminal (EXE) - Win32 Host Application             │
│  - AppHost: Manages native windows and XAML islands          │
│  - IslandWindow: Win32 window wrapper                        │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│  TerminalApp (DLL) - Application Logic & UI                 │
│  - AppLogic: Settings management, lifecycle                 │
│  - TerminalWindow: XAML window container                    │
│  - TerminalPage: Pane/Tab management                        │
│  - ContentManager: Content type resolution                  │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│  TerminalControl (DLL) - Terminal UI Component              │
│  - TermControl: XAML Terminal control                       │
│  - ControlCore: Control logic & terminal lifecycle          │
│  - ControlInteractivity: Input handling & selection         │
│  - Renderer + AtlasEngine: Graphics rendering               │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│  TerminalCore (LIB) - Terminal Engine Core                  │
│  - Terminal: Core terminal instance (270+ methods)          │
│  - TextBuffer: Screen buffer management                     │
│  - StateMachine: VT sequence parsing                        │
│  - TerminalInput: Input sequence generation                 │
│  - RenderSettings & ColorTable: Terminal settings           │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│  TerminalConnection (DLL) - Connection Abstractions         │
│  - ITerminalConnection: Connection interface                │
│  - ConptyConnection: Win32 console process (main)           │
│  - AzureConnection: Cloud shell                             │
│  - EchoConnection: Test/echo connection                     │
└──────────────────────┬──────────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────────┐
│  Low-Level Infrastructure                                   │
│  - Renderer subsystem (base, atlas, gdi, uia)              │
│  - Parser/Adapter (VT sequence handling)                    │
│  - Buffer system (TextBuffer, Cursor, Row, Cell)           │
│  - TSF (Text Services Framework) for IME                    │
└─────────────────────────────────────────────────────────────┘
```

---

## Architectural Layers

### Layer 1: User Interface Layer (Win32/XAML)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    USER INTERFACE LAYER (Win32/XAML)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────┐        │
│  │  WindowsTerminal.exe (Native Host)                            │        │
│  │  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐    │        │
│  │  │ WindowEmperor│ │  AppHost     │  │ IslandWindow     │    │        │
│  │  │ (Multi-win)  │→│ (Main logic) │→│ (Win32 wrapper)  │    │        │
│  │  └─────────────┘  └──────────────┘  └──────────────────┘    │        │
│  │         ↓                ↓                    ↓               │        │
│  │    Multi-window    Native window       XAML Islands          │        │
│  │    orchestration   lifecycle           hosting               │        │
│  └───────────────────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Components:**
- **WindowEmperor**: Multi-window management and coordination
- **AppHost**: Creates and manages IslandWindow, bridges AppLogic to native window
- **IslandWindow/NonClientIslandWindow**: Native Win32 window wrapper with XAML islands

**Responsibilities:**
- Native window creation and management
- XAML island hosting
- Win32 window messages and events
- DPI scaling and display management
- Taskbar integration

### Layer 2: Application Logic Layer

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       APPLICATION LOGIC LAYER                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────┐        │
│  │  TerminalApp.dll (Application & Tab Management)               │        │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │        │
│  │  │  AppLogic    │→│ TerminalPage │→│  Tab/Pane        │   │        │
│  │  │  (Settings   │  │ (Tab/Command │  │  Management      │   │        │
│  │  │   Manager)   │  │  Dispatch)   │  │                  │   │        │
│  │  └──────────────┘  └──────────────┘  └──────────────────┘   │        │
│  │         ↓                  ↓                   ↓              │        │
│  │   Load JSON       Actions/Hotkeys      Split panes           │        │
│  │   Create tabs     Command palette      Pane focus            │        │
│  └───────────────────────────────────────────────────────────────┘        │
│                                 ↓                                          │
│  ┌───────────────────────────────────────────────────────────────┐        │
│  │  TerminalSettingsModel.dll (Configuration)                    │        │
│  │  ┌───────────────┐  ┌─────────────┐  ┌─────────────────┐    │        │
│  │  │CascadiaSettings│ │  Profile    │  │  ActionMap      │    │        │
│  │  │(settings.json) │ │  (colors,   │  │  (keybindings)  │    │        │
│  │  │                │ │   font, etc)│  │                 │    │        │
│  │  └───────────────┘  └─────────────┘  └─────────────────┘    │        │
│  └───────────────────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Classes:**
- **AppLogic**: Application-wide settings management, initialization
- **TerminalWindow**: XAML window container, bridges AppHost and TerminalPage
- **TerminalPage**: Tab/pane management, command dispatch, core UI logic
- **Pane**: Individual terminal pane implementation
- **Tab**: Tab container for panes
- **TerminalPaneContent**: Terminal pane wrapper

**Responsibilities:**
- Load and manage settings (JSON parsing)
- Create/destroy tabs and panes
- Command handling and action execution
- Keyboard shortcut processing
- Multi-pane and split terminal logic

### Layer 3: Terminal Control Layer

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        TERMINAL CONTROL LAYER                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────┐        │
│  │  TerminalControl.dll (UI Component)                           │        │
│  │  ┌───────────┐  ┌───────────────┐  ┌──────────────────┐     │        │
│  │  │ TermControl│→│ ControlCore   │→│ControlInteractivity│     │        │
│  │  │ (XAML UI) │  │ (Orchestrator)│  │(Mouse/Keyboard)  │     │        │
│  │  └───────────┘  └───────────────┘  └──────────────────┘     │        │
│  │        ↓               ↓ ↓ ↓               ↓                 │        │
│  │   SwapChain    ┌──────┘  │  └───────┐   Input Events         │        │
│  │   Present      ↓         ↓          ↓                        │        │
│  │         ┌──────────┐ ┌────────┐ ┌──────────┐                │        │
│  │         │ Renderer │ │Terminal│ │Connection│                │        │
│  │         │          │ │        │ │          │                │        │
│  │         └──────────┘ └────────┘ └──────────┘                │        │
│  └───────────────────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Classes:**
- **TermControl**: XAML user control exposing Terminal functionality
- **ControlCore**: Terminal instance manager and lifecycle coordinator
- **ControlInteractivity**: Input handling, selection, mouse, keyboard
- **Renderer**: Graphics rendering orchestrator
- **AtlasEngine**: DirectX text rendering engine

**Responsibilities:**
- Terminal control XAML wrapper
- Input event routing to Terminal
- Output rendering coordination
- Selection management
- Scrolling and viewport management
- Copy/paste operations
- Font management

### Layer 4: Terminal Engine Layer

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       TERMINAL ENGINE LAYER                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────┐        │
│  │  TerminalCore (Terminal Emulation Engine)                     │        │
│  │  ┌──────────────────────────────────────────────────────┐    │        │
│  │  │           Terminal (270+ methods)                    │    │        │
│  │  │  ┌────────────┐ ┌──────────────┐ ┌──────────────┐  │    │        │
│  │  │  │TextBuffer  │ │ StateMachine │ │TerminalInput │  │    │        │
│  │  │  │(Screen     │ │ (VT Parser)  │ │ (Key→VT)     │  │    │        │
│  │  │  │ Buffer)    │ │              │ │              │  │    │        │
│  │  │  └────────────┘ └──────────────┘ └──────────────┘  │    │        │
│  │  │  ┌────────────┐ ┌──────────────┐ ┌──────────────┐  │    │        │
│  │  │  │   Cursor   │ │AdaptDispatch │ │  Selection   │  │    │        │
│  │  │  │  (State)   │ │ (VT Commands)│ │   Manager    │  │    │        │
│  │  │  └────────────┘ └──────────────┘ └──────────────┘  │    │        │
│  │  └──────────────────────────────────────────────────────┘    │        │
│  │                 Implements: IRenderData,                      │        │
│  │                            ITerminalInput,                    │        │
│  │                            ITerminalApi                       │        │
│  └───────────────────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Classes:**
- **Terminal**: Core terminal emulator (implements IRenderData, ITerminalInput, ITerminalApi)
  - 270+ public/private methods
  - Manages TextBuffer (main and alt screen)
  - Manages cursor position and style
  - Handles viewport scrolling
  - Manages color table and font info
- **TextBuffer**: Screen buffer management with rows and attributes
- **StateMachine**: VT sequence parser
- **TerminalInput**: Converts keyboard/mouse events to VT sequences
- **AdaptDispatch**: Executes VT commands

**Responsibilities:**
- Core terminal state machine and buffer management
- Virtual Terminal sequence parsing
- Terminal input conversion to VT sequences
- Rendering data provision (text, colors, cursor position)
- Selection handling
- Hyperlink detection

### Layer 5: Connection Layer

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        CONNECTION LAYER                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────┐        │
│  │  TerminalConnection.dll (Process Communication)               │        │
│  │                                                                │        │
│  │       ┌────────────────────────────────────────┐              │        │
│  │       │    ITerminalConnection Interface       │              │        │
│  │       │  • Initialize() • Start()              │              │        │
│  │       │  • WriteInput() • Resize() • Close()   │              │        │
│  │       │  • Events: TerminalOutput, StateChanged│              │        │
│  │       └────────────────────────────────────────┘              │        │
│  │                       ↑                                        │        │
│  │         ┌─────────────┼─────────────┬──────────────┐          │        │
│  │         ↓             ↓             ↓              ↓          │        │
│  │  ┌────────────┐ ┌───────────┐ ┌──────────┐ ┌───────────┐    │        │
│  │  │  Conpty    │ │  Azure    │ │  Echo    │ │  Custom   │    │        │
│  │  │ Connection │ │Connection │ │Connection│ │ Connections│    │        │
│  │  └────────────┘ └───────────┘ └──────────┘ └───────────┘    │        │
│  │        ↓                                                      │        │
│  │   Pseudoconsole                                              │        │
│  │   + Process I/O                                              │        │
│  └───────────────────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key Classes:**
- **ITerminalConnection**: Interface for all connection types
  - Events: `TerminalOutput` (char[]), `StateChanged`
  - States: NotConnected → Connecting → Connected → Closing/Failed → Closed
  - Methods: `Initialize()`, `Start()`, `WriteInput()`, `Resize()`, `Close()`
- **ConptyConnection**: Main implementation using Windows Pseudoconsole
- **AzureConnection**: Azure Cloud Shell integration
- **EchoConnection**: Test/debug connection

**Responsibilities:**
- Process spawning and communication
- Output buffering and event firing
- Connection state management
- Terminal resize communication
- Input/output handling with console processes

### Layer 6: Rendering Layer

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         RENDERING LAYER                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────┐        │
│  │  Renderer (Base Rendering System)                             │        │
│  │                                                                │        │
│  │       ┌────────────────────────────────────┐                  │        │
│  │       │   IRenderEngine Interface         │                  │        │
│  │       │  • StartPaint() • EndPaint()      │                  │        │
│  │       │  • PaintBufferLine()              │                  │        │
│  │       │  • PaintCursor() • UpdateFont()   │                  │        │
│  │       └────────────────────────────────────┘                  │        │
│  │                       ↑                                        │        │
│  │         ┌─────────────┼─────────────┬──────────────┐          │        │
│  │         ↓             ↓             ↓              ↓          │        │
│  │  ┌────────────┐ ┌───────────┐ ┌──────────┐ ┌───────────┐    │        │
│  │  │   Atlas    │ │    GDI    │ │   UIA    │ │    VT     │    │        │
│  │  │   Engine   │ │  Engine   │ │  Engine  │ │  Engine   │    │        │
│  │  │ (DirectX)  │ │ (Legacy)  │ │(A11y/UI) │ │ (Headless)│    │        │
│  │  └────────────┘ └───────────┘ └──────────┘ └───────────┘    │        │
│  │        ↓                                                      │        │
│  │    GPU-accelerated                                           │        │
│  │    text rendering                                            │        │
│  └───────────────────────────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Rendering Engines:**
- **AtlasEngine**: DirectX 11 GPU-accelerated rendering (primary)
- **GdiEngine**: Legacy GDI rendering (fallback)
- **UiaEngine**: UI Automation for accessibility
- **VtEngine**: Headless VT sequence output

**Responsibilities:**
- GPU-accelerated text rendering
- Font rasterization and caching
- Color management
- Cursor rendering
- Selection highlighting
- Frame composition

---

## Major Components

### 1. WindowsTerminal.exe

**Location:** `src/cascadia/WindowsTerminal/`

**Key Files:**
- `AppHost.cpp`: Native window management
- `WindowEmperor.cpp`: Multi-window coordination
- `IslandWindow.cpp`: Win32 window wrapper
- `NonClientIslandWindow.cpp`: Custom title bar window

**Role:** Native Win32 host application that creates windows and hosts XAML islands.

### 2. TerminalApp.dll

**Location:** `src/cascadia/TerminalApp/`

**Key Files:**
- `AppLogic.cpp`: Application initialization and settings management
- `TerminalPage.cpp`: Tab/pane management and command dispatch
- `Pane.cpp`: Individual pane implementation
- `Tab.cpp`: Tab container
- `TerminalWindow.cpp`: Window container

**Role:** Application logic layer handling tabs, panes, settings, and commands.

### 3. TerminalControl.dll

**Location:** `src/cascadia/TerminalControl/`

**Key Files:**
- `TermControl.cpp`: XAML terminal control
- `ControlCore.cpp`: Terminal lifecycle management
- `ControlInteractivity.cpp`: Input handling
- `TermControlAutomationPeer.cpp`: Accessibility support

**Role:** Terminal UI component that bridges between TerminalApp and TerminalCore.

### 4. TerminalCore

**Location:** `src/cascadia/TerminalCore/`

**Key Files:**
- `Terminal.cpp`: Core terminal implementation (270+ methods)
- `TerminalApi.cpp`: Terminal API implementation
- `TerminalInput.cpp`: Input sequence generation
- `TerminalSelection.cpp`: Selection management

**Role:** Platform-agnostic terminal emulation engine.

### 5. TerminalConnection.dll

**Location:** `src/cascadia/TerminalConnection/`

**Key Files:**
- `ConptyConnection.cpp`: Pseudoconsole connection
- `AzureConnection.cpp`: Azure Cloud Shell
- `EchoConnection.cpp`: Test connection
- `ITerminalConnection.idl`: Connection interface

**Role:** Abstraction layer for different types of terminal connections.

### 6. TerminalSettingsModel.dll

**Location:** `src/cascadia/TerminalSettingsModel/`

**Key Files:**
- `CascadiaSettings.cpp`: Top-level settings container
- `Profile.cpp`: Profile settings
- `GlobalAppSettings.cpp`: Global settings
- `ActionMap.cpp`: Keybinding management
- `ColorScheme.cpp`: Color scheme definitions

**Role:** Settings serialization, validation, and management.

### 7. Renderer

**Location:** `src/renderer/`

**Key Subdirectories:**
- `base/`: Core rendering framework
- `atlas/`: AtlasEngine (DirectX)
- `gdi/`: GDI rendering engine
- `uia/`: UI Automation engine
- `vt/`: VT sequence output engine

**Role:** Rendering orchestration and graphics output.

---

## Data Flow Patterns

### User Input Flow (Keyboard → Process)

```
┌──────────────────────────────────────────────────────────┐
│ User presses key on keyboard                             │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ IslandWindow::HandleKeyEvent()                           │
│ (Win32 WM_KEYDOWN message)                              │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ TermControl (XAML KeyDown event)                        │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ ControlInteractivity::OnKeyDown()                       │
│ (Handles key modifiers, special keys)                   │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ ControlCore::SendInput()                                │
│ (Validates and forwards input)                          │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Terminal::SendKeyEvent()                                │
│ (Terminal core receives key event)                      │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ TerminalInput::HandleKey()                              │
│ Converts to VT sequence:                                │
│   • Up Arrow    → "ESC[A"                               │
│   • Ctrl+C      → "\x03"                                │
│   • F1          → "ESC[OP"                              │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Connection::WriteInput()                                │
│ (ITerminalConnection interface)                         │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ ConptyConnection::WriteInput()                          │
│ (Writes to pseudoconsole pipe)                          │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Windows ConPTY (Pseudoconsole)                          │
│ (Kernel-level console infrastructure)                   │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Console Process (cmd.exe, PowerShell, bash, etc.)      │
│ Receives input as if from a real terminal               │
└──────────────────────────────────────────────────────────┘
```

### Process Output Flow (Process → Screen)

```
┌──────────────────────────────────────────────────────────┐
│ Console Process writes output                            │
│ (e.g., printf("Hello\n"))                               │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Windows ConPTY captures output                          │
│ (Pseudoconsole translates to VT sequences)              │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ ConptyConnection::_OutputThread()                       │
│ Continuously reads from pipe (overlapped I/O)           │
│ - 128KB buffer                                          │
│ - Non-blocking reads                                    │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ TerminalOutput event fired                              │
│ (array_view<char16_t> with UTF-16 text)                │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ ControlCore::_outputHandler()                           │
│ (Receives output on UI thread)                          │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Terminal::Write()                                       │
│ (Processes character stream)                            │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ StateMachine parses VT sequences                        │
│ Examples:                                               │
│   • "Hello"     → Plain text                            │
│   • "ESC[31m"   → Set foreground red                    │
│   • "ESC[2J"    → Clear screen                          │
│   • "ESC[10;5H" → Move cursor to row 10, col 5         │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ AdaptDispatch executes commands                         │
│ (VT command implementation)                             │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ TextBuffer updated                                      │
│ - Characters written to cells                           │
│ - Attributes (colors, bold, italic) applied             │
│ - Cursor position moved                                 │
│ - Scroll region updated                                 │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Renderer::TriggerRedraw()                               │
│ (Marks regions as dirty)                                │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Renderer polls Terminal (IRenderData interface)         │
│ - GetTextBuffer()                                       │
│ - GetViewport()                                         │
│ - GetCursorPosition()                                   │
│ - GetAttributeColors()                                  │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ AtlasEngine::PaintBufferLine()                          │
│ DirectX rendering:                                      │
│ - Looks up glyphs in texture atlas                      │
│ - Builds vertex buffer with quads                       │
│ - Applies colors and effects                            │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ SwapChain presents to screen                            │
│ (GPU renders final frame)                               │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ User sees output on screen                              │
└──────────────────────────────────────────────────────────┘
```

### Settings Flow

```
┌──────────────────────────────────────────────────────────┐
│ JSON Settings File                                       │
│ (settings.json)                                         │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ CascadiaSettings::LoadAll()                             │
│ - Parses JSON                                           │
│ - Validates schema                                      │
│ - Merges default settings                               │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Creates:                                                │
│ - GlobalAppSettings (keybindings, UI options)           │
│ - Profile objects (per-terminal settings)               │
│ - ColorScheme objects (color palettes)                  │
│ - ActionMap (command mappings)                          │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ AppLogic::Create()                                      │
│ Initializes application with settings                   │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ TerminalPage::SetSettings()                             │
│ Applies settings to UI layer                            │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Profile selected for new Pane/Terminal                  │
│ (Based on default profile or user selection)            │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ TerminalControl::UpdateControlSettings()                │
│ Applies profile settings to control                     │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ ControlCore::UpdateSettings()                           │
│ Updates core control behavior                           │
└────────────────────┬─────────────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────────────┐
│ Terminal::UpdateSettings()                              │
│ Terminal::UpdateAppearance()                            │
│ Applies:                                                │
│ - Font family, size, weight                             │
│ - Color scheme (16 colors + foreground/background)      │
│ - Cursor style (bar, underscore, block)                 │
│ - Scrollback buffer size                                │
│ - Bell style                                            │
└──────────────────────────────────────────────────────────┘
```

---

## Key Interfaces

### ITerminalConnection

**File:** `src/cascadia/TerminalConnection/ITerminalConnection.idl`

```cpp
interface ITerminalConnection
{
    // Lifecycle methods
    void Initialize(Windows.Foundation.Collections.ValueSet settings);
    void Start();
    void Close();

    // I/O methods
    void WriteInput(String data);
    void Resize(UInt32 rows, UInt32 columns);

    // Properties
    Guid SessionId { get; };
    ConnectionState State { get; };

    // Events
    event TerminalOutputHandler TerminalOutput;  // Fires when output received
    event StateChangedHandler StateChanged;      // Fires on state transitions
}

enum ConnectionState
{
    NotConnected,
    Connecting,
    Connected,
    Closing,
    Closed,
    Failed
};
```

**Purpose:** Abstraction for all terminal connection types (ConPTY, Azure, SSH, etc.)

**State Machine:**
```
NotConnected → Connecting → Connected → (Closing) → Closed
                    ↓                        ↓
                  Failed                  Failed
```

### IRenderData

**File:** `src/renderer/inc/IRenderData.hpp`

```cpp
class IRenderData
{
public:
    // Viewport and buffer access
    virtual Microsoft::Console::Types::Viewport GetViewport() = 0;
    virtual const TextBuffer& GetTextBuffer() const = 0;
    virtual const FontInfo& GetFontInfo() const = 0;

    // Cursor information
    virtual til::point GetCursorPosition() const = 0;
    virtual bool IsCursorVisible() const = 0;
    virtual bool IsCursorOn() const = 0;
    virtual ULONG GetCursorHeight() const = 0;
    virtual CursorType GetCursorStyle() const = 0;
    virtual COLORREF GetCursorColor() const = 0;

    // Selection
    virtual bool IsSelectionActive() const = 0;
    virtual const bool IsBlockSelection() const = 0;
    virtual void GetSelectionSpans(std::vector<til::point_span>& out) = 0;

    // Colors and attributes
    virtual COLORREF GetAttributeColors(const TextAttribute& attr) const = 0;
    virtual til::color GetColorTableEntry(size_t index) const = 0;
    virtual bool IsScreenReversed() const = 0;

    // Scrolling
    virtual til::CoordType GetScrollOffset() const = 0;

    // ... 40+ more methods for rendering coordination
};
```

**Purpose:** Provides Terminal data to the Renderer in a read-only, thread-safe manner.

**Implemented by:** Terminal class

### IRenderEngine

**File:** `src/renderer/inc/IRenderEngine.hpp`

```cpp
class IRenderEngine
{
public:
    // Frame lifecycle
    virtual HRESULT StartPaint() = 0;
    virtual HRESULT EndPaint() = 0;

    // Drawing operations
    virtual HRESULT PaintBufferLine(
        gsl::span<const Cluster> clusters,
        til::point coord,
        bool fTrimLeft,
        bool lineWrapped) = 0;

    virtual HRESULT PaintCursor(const CursorOptions& options) = 0;

    virtual HRESULT PaintSelection(const til::rect& rect) = 0;

    // Text rendering
    virtual HRESULT PrepareRenderInfo(RenderFrameInfo& info) = 0;

    // Font management
    virtual HRESULT UpdateFont(
        const FontInfoDesired& desired,
        FontInfo& actual) = 0;

    // Window management
    virtual HRESULT InvalidateAll() = 0;
    virtual HRESULT InvalidateScroll(const til::point delta) = 0;
    virtual HRESULT Invalidate(const til::rect* psrRegion) = 0;

    // Capabilities
    virtual HRESULT GetProposedFont(
        const FontInfoDesired& desired,
        FontInfo& actual,
        int dpi) = 0;

    // ... more rendering methods
};
```

**Purpose:** Abstraction for different rendering backends (DirectX, GDI, UIA, VT).

**Implemented by:**
- AtlasEngine (DirectX 11)
- GdiEngine (GDI)
- UiaEngine (UI Automation)
- VtEngine (VT sequence output)

### ITerminalInput

**File:** `src/cascadia/TerminalCore/ITerminalInput.hpp`

```cpp
class ITerminalInput
{
public:
    // Keyboard input
    virtual bool SendKeyEvent(
        const WORD vkey,
        const WORD scanCode,
        const ControlKeyStates states,
        const bool keyDown) = 0;

    virtual bool SendCharEvent(
        const wchar_t ch,
        const WORD scanCode,
        const ControlKeyStates states) = 0;

    // Mouse input
    virtual bool SendMouseEvent(
        const til::point viewportPos,
        const unsigned int uiButton,
        const ControlKeyStates states,
        const short wheelDelta,
        const TerminalInput::MouseButtonState state) = 0;

    // Focus events
    virtual bool SendFocus(const bool focused) = 0;

    // Manual VT input
    virtual bool SendVTSequence(std::wstring_view sequence) = 0;

    // User interactions
    virtual HRESULT UserResize(const til::size size) = 0;
    virtual void UserScrollViewport(const int viewTop) = 0;
};
```

**Purpose:** Converts user input events into VT sequences for the connected process.

**Implemented by:** Terminal class

---

## Rendering Architecture

### Rendering Pipeline

```
┌─────────────────────────────────────────────────────────┐
│                  ControlCore                            │
│              (Owns Renderer + Terminal)                 │
└────────────────────┬────────────────────────────────────┘
                     │
                     │ has-a
                     ↓
┌─────────────────────────────────────────────────────────┐
│                   Renderer                              │
│           (Orchestrates rendering loop)                 │
│                                                         │
│  ┌──────────────────────────────────────────┐          │
│  │  Rendering Loop (60 FPS target)          │          │
│  │  1. Check if dirty regions exist         │          │
│  │  2. StartPaint() on all engines          │          │
│  │  3. Poll Terminal via IRenderData        │          │
│  │  4. Call Paint*() methods on engines     │          │
│  │  5. EndPaint() on all engines            │          │
│  │  6. Present frame                        │          │
│  └──────────────────────────────────────────┘          │
│                                                         │
│  Manages multiple IRenderEngine instances:             │
└────────────────────┬────────────────────────────────────┘
                     │
        ┌────────────┼────────────┬───────────┐
        ↓            ↓            ↓           ↓
┌──────────────┐ ┌────────┐ ┌────────┐ ┌─────────┐
│ AtlasEngine  │ │  GDI   │ │  UIA   │ │   VT    │
│  (Primary)   │ │ Engine │ │ Engine │ │ Engine  │
└──────────────┘ └────────┘ └────────┘ └─────────┘
```

### AtlasEngine Architecture

**File:** `src/renderer/atlas/AtlasEngine.cpp`

The AtlasEngine is the primary rendering engine using DirectX 11:

```
┌─────────────────────────────────────────────────────────┐
│                    AtlasEngine                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────────────────────────────┐              │
│  │       Texture Atlas                  │              │
│  │  ┌────┬────┬────┬────┬────┬────┐    │              │
│  │  │ A  │ B  │ C  │ D  │ E  │ F  │    │              │
│  │  ├────┼────┼────┼────┼────┼────┤    │              │
│  │  │ G  │ H  │ I  │ J  │ K  │ L  │    │              │
│  │  ├────┼────┼────┼────┼────┼────┤    │              │
│  │  │ ... cached glyphs ...      │    │              │
│  │  └────────────────────────────────┘    │              │
│  └──────────────────────────────────────┘              │
│                                                         │
│  DirectX 11 Pipeline:                                  │
│  1. Rasterize glyphs with DirectWrite                  │
│  2. Cache glyphs in GPU texture atlas                  │
│  3. Build vertex buffer (quads for each character)     │
│  4. Apply colors, backgrounds, underlines              │
│  5. Render with pixel shader                           │
│  6. Present via swap chain                             │
│                                                         │
│  Features:                                             │
│  • GPU-accelerated rendering                           │
│  • Glyph caching for performance                       │
│  • Subpixel antialiasing                               │
│  • Custom pixel shaders for effects                    │
│  • Emoji and unicode support                           │
└─────────────────────────────────────────────────────────┘
```

### Render Data Flow

```
Terminal Buffer State
         ↓
IRenderData interface
         ↓
Renderer queries:
  • GetTextBuffer() → TextBuffer with characters & attributes
  • GetViewport() → Current visible region
  • GetCursorPosition() → Cursor location
  • GetAttributeColors() → Resolved colors for attributes
         ↓
Renderer calculates dirty regions
         ↓
For each dirty region:
  IRenderEngine::PaintBufferLine()
         ↓
AtlasEngine:
  • Looks up/rasterizes glyphs
  • Builds vertex buffer
  • Applies colors & effects
         ↓
DirectX renders frame
         ↓
SwapChain::Present()
         ↓
Display updates
```

---

## Settings Architecture

### Three-Layer Settings Model

```
┌─────────────────────────────────────────────────────────┐
│          Frontend Settings (TerminalApp)                │
│  Application-level settings and UI configuration        │
└────────────────────┬────────────────────────────────────┘
                     │ contains
┌────────────────────▼────────────────────────────────────┐
│     Application Settings (TerminalSettingsModel)        │
│  ┌──────────────────────────────────────────┐          │
│  │      CascadiaSettings                    │          │
│  │  ┌────────────────┐  ┌──────────────┐   │          │
│  │  │ GlobalSettings │  │   Profiles   │   │          │
│  │  │ - Keybindings  │  │   - Colors   │   │          │
│  │  │ - Theme        │  │   - Font     │   │          │
│  │  │ - Actions      │  │   - Command  │   │          │
│  │  └────────────────┘  └──────────────┘   │          │
│  └──────────────────────────────────────────┘          │
└────────────────────┬────────────────────────────────────┘
                     │ contains
┌────────────────────▼────────────────────────────────────┐
│      Component Settings (TerminalControl)               │
│  Settings specific to the terminal control component    │
│  • IControlSettings                                     │
│  • IControlAppearance                                   │
└────────────────────┬────────────────────────────────────┘
                     │ contains
┌────────────────────▼────────────────────────────────────┐
│       Terminal Settings (TerminalCore)                  │
│  Core terminal emulation settings                       │
│  • ICoreSettings                                        │
│  • ICoreAppearance                                      │
└─────────────────────────────────────────────────────────┘
```

### Settings File Structure

**File:** `settings.json` (typically in `%LOCALAPPDATA%\Packages\Microsoft.WindowsTerminal_...\LocalState\`)

```json
{
    "$help": "https://aka.ms/terminal-documentation",
    "$schema": "https://aka.ms/terminal-profiles-schema",

    "defaultProfile": "{guid}",

    "profiles": {
        "defaults": {
            // Default settings for all profiles
        },
        "list": [
            {
                "guid": "{...}",
                "name": "PowerShell",
                "commandline": "powershell.exe",
                "colorScheme": "Campbell",
                "fontSize": 12,
                "fontFace": "Cascadia Code"
            }
        ]
    },

    "schemes": [
        {
            "name": "Campbell",
            "foreground": "#CCCCCC",
            "background": "#0C0C0C",
            "colors": [...]
        }
    ],

    "actions": [
        { "command": "copy", "keys": "ctrl+c" },
        { "command": "paste", "keys": "ctrl+v" }
    ],

    "themes": [...]
}
```

### Key Settings Classes

**CascadiaSettings**
- Top-level settings container
- Manages profile collection
- Handles settings serialization/deserialization
- Validates settings against schema

**Profile**
- Per-terminal profile settings
- Properties: commandline, startingDirectory, colors, font, etc.
- Supports inheritance and layering

**GlobalAppSettings**
- Application-wide settings
- Default profile selection
- Keybinding configuration
- Theme selection

**ActionMap**
- Maps key combinations to actions
- Supports multi-key chords
- Command palette integration

**ColorScheme**
- 16-color palette definition
- Foreground/background colors
- Cursor and selection colors

---

## Design Principles

### 1. Separation of Concerns

Each layer has well-defined responsibilities:
- **UI Layer**: Window management, native platform integration
- **App Layer**: Tab/pane management, settings, commands
- **Control Layer**: Terminal UI component, input handling
- **Core Layer**: Terminal emulation, VT parsing
- **Connection Layer**: Process communication
- **Rendering Layer**: Graphics output

### 2. Platform Agnostic Core

The `TerminalCore` component is intentionally platform-agnostic:
- No Windows-specific APIs in core terminal logic
- No XAML or UI framework dependencies
- Pure C++ terminal emulation
- Enables potential cross-platform reuse

### 3. Interface-Driven Architecture

Components communicate through well-defined interfaces:
- `ITerminalConnection` for connection abstraction
- `IRenderData` for rendering data access
- `IRenderEngine` for rendering backend abstraction
- `ITerminalInput` for input handling
- Enables pluggability and testability

### 4. Event-Driven Communication

Asynchronous communication via typed events:
- `TerminalOutput` event for process output
- `StateChanged` event for connection state
- `TerminalOutputHandler` for async data flow
- Decouples components and enables responsiveness

### 5. Pluggable Architecture

New implementations can be added easily:
- **Connections**: Implement `ITerminalConnection` (ConPTY, Azure, SSH, etc.)
- **Renderers**: Implement `IRenderEngine` (DirectX, GDI, VT, etc.)
- **Settings**: Extend profile and settings classes
- Enables extensibility without modifying core code

### 6. Performance Optimization

Multiple optimizations for smooth 60 FPS rendering:
- **GPU Acceleration**: DirectX 11 rendering via AtlasEngine
- **Overlapped I/O**: Non-blocking pipe operations in ConptyConnection
- **Glyph Caching**: Texture atlas for rendered characters
- **Dirty Region Tracking**: Only redraw changed areas
- **Efficient Buffer Management**: Circular buffer for scrollback

### 7. Accessibility First

Built-in accessibility support:
- **UiaEngine**: UI Automation rendering engine
- **Screen Reader Support**: Full text buffer access
- **Keyboard Navigation**: Complete keyboard control
- **High Contrast**: Theme support for visibility
- **Automation Peers**: XAML accessibility integration

---

## Component Interactions

### ControlCore: The Central Orchestrator

**File:** `src/cascadia/TerminalControl/ControlCore.cpp`

ControlCore is the key component that bridges multiple layers:

```
┌─────────────────────────────────────────────────────────┐
│                    ControlCore                          │
│              (Central Orchestrator)                     │
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │   Terminal   │  │  Renderer    │  │  Connection  │ │
│  │              │  │              │  │              │ │
│  │  (Terminal   │  │  (Graphics)  │  │  (Process    │ │
│  │   emulation) │  │              │  │   I/O)       │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
│         ↕                  ↕                  ↕         │
│  Manages state      Triggers draws     Handles output  │
│  Provides data      Updates display    Sends input     │
└─────────────────────────────────────────────────────────┘
            ↕                                    ↕
┌────────────────────┐              ┌───────────────────┐
│ ControlInteractivity│              │    TermControl    │
│   (Input events)   │              │   (XAML wrapper)  │
└────────────────────┘              └───────────────────┘
```

**Key Responsibilities:**
- Creates and manages Terminal instance
- Creates and manages Renderer instance
- Creates and manages Connection instance
- Coordinates between all three
- Handles settings updates
- Manages lifecycle (creation, updates, destruction)

### Terminal ↔ Connection Output Loop

```
┌─────────────────────────────────────────────────────┐
│  ConptyConnection::_OutputThread()                  │
│  (Background thread continuously reads pipe)        │
└────────────────────┬────────────────────────────────┘
                     │ fires event
                     ↓
┌─────────────────────────────────────────────────────┐
│  TerminalOutput event                               │
│  (contains array_view<char16_t>)                   │
└────────────────────┬────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────┐
│  ControlCore::_outputHandler()                      │
│  (Subscribed to TerminalOutput event)               │
└────────────────────┬────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────┐
│  Terminal::ProcessInput()                           │
│  (Processes character stream)                       │
└────────────────────┬────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────┐
│  StateMachine::ProcessString()                      │
│  (Parses VT sequences character by character)       │
└────────────────────┬────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────┐
│  AdaptDispatch::Execute()                           │
│  (Executes VT commands)                             │
│  • Print() - Regular characters                     │
│  • CursorPosition() - ESC[H                         │
│  • SetGraphicsRendition() - ESC[...m                │
│  • EraseInDisplay() - ESC[2J                        │
└────────────────────┬────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────┐
│  TextBuffer updated                                 │
│  • Write cells with characters                      │
│  • Update cell attributes (colors, styles)          │
│  • Move cursor position                             │
│  • Scroll if needed                                 │
└────────────────────┬────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────┐
│  Renderer::TriggerRedraw()                          │
│  (Marks regions dirty, schedules render)            │
└─────────────────────────────────────────────────────┘
```

### Input Path: User → Process

```
User presses key
      ↓
ControlInteractivity::OnKeyDown()
  ↓ (interprets key + modifiers)
ControlCore::SendInput()
  ↓ (validates)
Terminal::SendKeyEvent()
  ↓ (converts to VT sequence)
TerminalInput::HandleKey()
  ↓ (generates VT string)
Connection::WriteInput()
  ↓ (writes to pipe)
ConptyConnection::WriteInput()
  ↓ (overlapped write)
Process receives input
```

---

## File Locations

### Core Components

| Component | Location |
|-----------|----------|
| WindowsTerminal.exe | `src/cascadia/WindowsTerminal/` |
| TerminalApp.dll | `src/cascadia/TerminalApp/` |
| TerminalControl.dll | `src/cascadia/TerminalControl/` |
| TerminalCore | `src/cascadia/TerminalCore/` |
| TerminalConnection.dll | `src/cascadia/TerminalConnection/` |
| TerminalSettingsModel.dll | `src/cascadia/TerminalSettingsModel/` |
| TerminalSettingsEditor.dll | `src/cascadia/TerminalSettingsEditor/` |

### Rendering Components

| Component | Location |
|-----------|----------|
| Renderer (Base) | `src/renderer/base/` |
| AtlasEngine | `src/renderer/atlas/` |
| GdiEngine | `src/renderer/gdi/` |
| UiaEngine | `src/renderer/uia/` |
| VtEngine | `src/renderer/vt/` |
| DirectX Components | `src/renderer/dx/` |

### Infrastructure

| Component | Location |
|-----------|----------|
| Types Library | `src/types/` |
| Terminal Adapter | `src/terminal/adapter/` |
| Terminal Parser | `src/terminal/parser/` |
| Buffer Library | `src/buffer/` |
| Input Library | `src/terminal/input/` |
| Host (ConPTY) | `src/host/` |
| Terminal Parser | `src/terminal/parser/` |
| Interactivity | `src/interactivity/` |

### Testing

| Component | Location |
|-----------|----------|
| Unit Tests (Control) | `src/cascadia/UnitTests_Control/` |
| Unit Tests (TerminalCore) | `src/cascadia/UnitTests_TerminalCore/` |
| Unit Tests (Settings) | `src/cascadia/UnitTests_SettingsModel/` |
| Feature Tests | `src/winconpty/ft_pty/` |

### Documentation

| Document | Location |
|----------|----------|
| Code Organization | `doc/ORGANIZATION.md` |
| Terminal Settings Spec | `doc/specs/TerminalSettings-spec.md` |
| Adding a Setting | `doc/cascadia/AddASetting.md` |
| Keybindings Spec | `doc/specs/Keybindings-spec.md` |
| Connection Evolution | `doc/specs/#2563 - closeOnExit and TerminalConnection evolution.md` |

---

## Key Architecture Files

### Essential Files to Understand

1. **Terminal Core**
   - `src/cascadia/TerminalCore/Terminal.hpp` - Main terminal class
   - `src/cascadia/TerminalCore/Terminal.cpp` - Terminal implementation (270+ methods)
   - `src/cascadia/TerminalCore/TerminalApi.cpp` - Terminal API
   - `src/cascadia/TerminalCore/terminalrenderdata.cpp` - IRenderData implementation

2. **Terminal Control**
   - `src/cascadia/TerminalControl/ControlCore.h` - Central orchestrator
   - `src/cascadia/TerminalControl/ControlCore.cpp` - Core implementation
   - `src/cascadia/TerminalControl/TermControl.h` - XAML control
   - `src/cascadia/TerminalControl/ControlInteractivity.cpp` - Input handling

3. **Connections**
   - `src/cascadia/TerminalConnection/ConptyConnection.h` - ConPTY header
   - `src/cascadia/TerminalConnection/ConptyConnection.cpp` - ConPTY implementation
   - `src/cascadia/TerminalConnection/ITerminalConnection.idl` - Interface definition

4. **Rendering**
   - `src/renderer/base/renderer.hpp` - Renderer orchestrator
   - `src/renderer/atlas/AtlasEngine.h` - DirectX rendering engine
   - `src/renderer/inc/IRenderEngine.hpp` - Render engine interface
   - `src/renderer/inc/IRenderData.hpp` - Render data interface

5. **Settings**
   - `src/cascadia/TerminalSettingsModel/CascadiaSettings.h` - Top-level settings
   - `src/cascadia/TerminalSettingsModel/Profile.h` - Profile settings
   - `src/cascadia/TerminalSettingsModel/ActionMap.h` - Keybinding management

6. **Application**
   - `src/cascadia/TerminalApp/TerminalPage.h` - Main UI page
   - `src/cascadia/TerminalApp/Pane.h` - Pane implementation
   - `src/cascadia/TerminalApp/AppLogic.h` - Application logic

---

## Conclusion

Windows Terminal is a sophisticated, layered architecture that separates concerns effectively:

- **Modular Design**: Clear separation between UI, application logic, terminal emulation, and process communication
- **Extensible**: New connections, renderers, and features can be added without major refactoring
- **Performant**: GPU-accelerated rendering, efficient I/O, and optimized buffer management
- **Accessible**: Built-in support for screen readers and UI automation
- **Modern**: Uses WinRT, XAML, DirectX 11, and modern C++20 features

The architecture enables Windows Terminal to be:
- Fast and responsive (60 FPS rendering)
- Extensible (custom profiles, themes, connections)
- Embeddable (can be hosted in other applications)
- Maintainable (clean interfaces and separation of concerns)

For more detailed information, refer to:
- `doc/ORGANIZATION.md` - Detailed code organization
- Component source files in `src/cascadia/` and `src/renderer/`
- Specification documents in `doc/specs/`

---

**Document Version:** 1.0
**Last Updated:** 2026-01-26
**Windows Terminal Repository:** https://github.com/microsoft/terminal
