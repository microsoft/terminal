---
author: Dustin Howett @DHowett <duhowett@microsoft.com>
created on: 2020-08-16
last updated: 2023-12-12
issue id: "#7335"
---

# Console Allocation Policy

## Abstract

Due to the design of the console subsystem on Windows as it has existed since Windows 95, every application that is
stamped with the `IMAGE_SUBSYSTEM_WINDOWS_CUI` subsystem in its PE header will be allocated a console by kernel32.

Any application that is stamped `IMAGE_SUBSYSTEM_WINDOWS_GUI` will not automatically be allocated a console.

This has worked fine for many years: when you double-click a console application in your GUI shell, it is allocated a
console.  When you run a GUI application from your console shell, it is **not** allocated a console. The shell will
**not** wait for it to exit before returning you to a prompt.

There is a large class of applications that do not fit neatly into this mold. Take Python, Ruby, Perl, Lua, or even
VBScript: These languages are not relegated to running in a console session; they can be used to write fully-fledged GUI
applications like any other language.

Because their interpreters are console subsystem applications, however, any user double-clicking a shortcut to a Python
or Perl application will be presented with a console window that the language runtime may choose to garbage collect, or
may choose not to.

If the runtime chooses to hide the window, there will still be a brief period during which that window is visible. It is
inescapable.

Likewise, any user running that GUI application from a console shell will see their shell hang until the application
terminates.

All of these scripting languages worked around this by shipping two binaries each, identical in every way expect in
their subsystem fields. python/pythonw, perl/perlw, ruby/rubyw, wscript/cscript.

PowerShell[^1] is waiting to deal with this problem because they don't necessarily want to ship a `pwshw.exe` for all
of their GUI-only authors. Every additional `*w` version of an application is an additional maintenance burden and
source of cognitive overhead[^2] for users.

On the other side, you have mostly-GUI applications that want to print output to a console **if there is one
connected**.

These applications are still primarily GUI-driven, but they might support arguments like `/?` or `--help`. They only
need a console when they need to print out some text. Sometimes they'll allocate their own console (which opens a new
window) to display in, and sometimes they'll reattach to the originating console. VSCode does the latter, and so when
you run `code` from CMD, and then `exit` CMD, your console window sticks around because VSCode is still attached to it.
It will never print anything, and your only option is to close it.

There's another risk in reattaching, too. Given that the shell decides whether to wait based on the subsystem
field, GUI subsystem applications that reattach to their owning consoles *just to print some text* end up stomping on
the output of any shell that doesn't wait for them:

```
C:\> application --help

application - the interesting application
C:\> Usage: application [OPTIONS] ...
```

> _(the prompt is interleaved with the output)_

## Solution Design

I propose that we introduce a fusion manifest field, **consoleAllocationPolicy**, with the following values:

* _absent_
* `detached`

This field allows an application to disable the automatic allocation of a console, regardless of the [process creation flags]
passed to [`CreateProcess`] and its subsystem value.

It would look (roughly) like this:

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <application>
    <windowsSettings>
      <consoleAllocationPolicy xmlns="http://schemas.microsoft.com/SMI/2024/WindowsSettings">detached</consoleAllocationPolicy>
    </windowsSettings>
  </application>
</assembly>
```

The effects of this field will only apply to binaries in the `IMAGE_SUBSYSTEM_WINDOWS_CUI` subsystem, as it pertains to
the particulars of their console allocation.

**All console inheritance will proceed as normal.** Since this field takes effect only in the absence of console
inheritance, CUI applications will still be able to run inside an existing console session.

| policy     | behavior                                                                                                            |
| -          | -                                                                                                                   |
| _absent_   | _default behavior_                                                                                                  |
| `detached` | The new process is not attached to a console session (similar to `DETACHED_PROCESS`) unless one was inherited.      |

An application that specifies the `detached` allocation policy will _not_ present a console window when launched by
Explorer, Task Scheduler, etc.

### Interaction with existing APIs

[`CreateProcess`] supports a number of [process creation flags] that dictate how a spawned application will behave with
regards to console allocation:

* `DETACHED_PROCESS`: No console inheritance, no console host spawned for the new process.
* `CREATE_NEW_CONSOLE`: No console inheritance, new console host **is** spawned for the new process.
* `CREATE_NO_WINDOW`: No console inheritance, new console host **is** spawned for the new process.
    * this is the same as `CREATE_NEW_CONSOLE`, except that the first connection packet specifies that the window should
      be invisible

Due to the design of [`CreateProcess`] and `ShellExecute`, this specification recommends that an allocation policy of
`detached` _override_ the inclusion of `CREATE_NEW_CONSOLE` in the `dwFlags` parameter to [`CreateProcess`].

> **Note**
> `ShellExecute` passes `CREATE_NEW_CONSOLE` _by default_ on all invocations. This impacts our ability to resolve the
> conflicts between these two APIs--`detached` policy and `CREATE_NEW_CONSOLE`--without auditing every call site in
> every Windows application that calls `ShellExecute` on a console application. Doing so is infeasible.

### Application impact

An application that opts into the `detached` console allocation policy will **not** be allocated a console unless one is
inherited. This presents an issue for applications like PowerShell that do want a console window when they are launched
directly.

Applications in this category can call `AllocConsole()` early in their startup to get fine-grained control over when a
console is presented.

The call to `AllocConsole()` will fail safely if the application has already inherited a console handle. It will succeed
if the application does not currently have a console handle.

> **Note**
> **Backwards Compatibility**: The behavior of `AllocConsole()` is not changing in response to this specification;
> therefore, applications that intend to run on older versions of Windows that do not support console allocation
> policies, which call `AllocConsole()`, will continue to behave normally.

### New APIs

Because a console-subsystem application may still want fine-grained control over when and how its console window is
spawned, we propose the inclusion of a new API, `AllocConsoleWithOptions(PALLOC_CONSOLE_OPTIONS)`.

#### `AllocConsoleWithOptions`

```c++
// Console Allocation Modes
typedef enum ALLOC_CONSOLE_MODE {
    ALLOC_CONSOLE_MODE_DEFAULT    = 0,
    ALLOC_CONSOLE_MODE_NEW_WINDOW = 1,
    ALLOC_CONSOLE_MODE_NO_WINDOW  = 2
} ALLOC_CONSOLE_MODE;

typedef enum ALLOC_CONSOLE_RESULT {
    ALLOC_CONSOLE_RESULT_NO_CONSOLE       = 0,
    ALLOC_CONSOLE_RESULT_NEW_CONSOLE      = 1,
    ALLOC_CONSOLE_RESULT_EXISTING_CONSOLE = 2
} ALLOC_CONSOLE_RESULT, *PALLOC_CONSOLE_RESULT;

typedef
struct ALLOC_CONSOLE_OPTIONS
{
    ALLOC_CONSOLE_MODE mode;
    BOOL useShowWindow;
    WORD showWindow;
} ALLOC_CONSOLE_OPTIONS, *PALLOC_CONSOLE_OPTIONS;

WINBASEAPI
HRESULT
WINAPI
AllocConsoleWithOptions(_In_opt_ PALLOC_CONSOLE_OPTIONS allocOptions, _Out_opt_ PALLOC_CONSOLE_RESULT result);
```

**AllocConsoleWithOptions** affords an application control over how and when it begins a console session.

> [!NOTE]
> Unlike `AllocConsole`, `AllocConsoleWithOptions` without a mode (`ALLOC_CONSOLE_MODE_DEFAULT`) will only allocate a console if one was
> requested during `CreateProcess`.
>
> To override this behavior, pass one of `ALLOC_CONSOLE_MODE_NEW_WINDOW` (which is equivalent to being spawned with
> `CREATE_NEW_WINDOW`) or `ALLOC_CONSOLE_MODE_NO_WINDOW` (which is equivalent to being spawned with `CREATE_NO_CONSOLE`.)

##### Parameters

**allocOptions**: A pointer to a `ALLOC_CONSOLE_OPTIONS`.

**result**: An optional out pointer, which will be populated with a member of the `ALLOC_CONSOLE_RESULT` enum.

##### `ALLOC_CONSOLE_OPTIONS`

###### Members

**mode**: See the table below for the descriptions of the available modes.

**useShowWindow**: Specifies whether the value in `showWindow` should be used.

**showWindow**: If `useShowWindow` is set, specifies the ["show command"] used to display your
console window.

###### Return Value

`AllocConsoleWithOptions` will return `S_OK` and populate `result` to indicate whether--and how--a console session was
created.

`AllocConsoleWithOptions` will return a failing `HRESULT` if the request could not be completed.

###### Modes

|             Mode                | Description                                                                                                                    |
|:-------------------------------:| ------------------------------------------------------------------------------------------------------------------------------ |
| `ALLOC_CONSOLE_MODE_DEFAULT`    | Allocate a console session if (and how) one was requested by the parent process.                                               |
| `ALLOC_CONSOLE_MODE_NEW_WINDOW` | Allocate a console session with a window, even if this process was created with `CREATE_NO_CONSOLE` or `DETACHED_PROCESS`.     |
| `ALLOC_CONSOLE_MODE_NO_WINDOW`  | Allocate a console session _without_ a window, even if this process was created with `CREATE_NEW_WINDOW` or `DETACHED_PROCESS` |

###### Notes

Applications seeking backwards compatibility are encouraged to delay-load `AllocConsoleWithOptions` or check for its presence in
the `api-ms-win-core-console-l1` APISet.

## Inspiration

Fusion manifest entries are used to make application-scoped decisions like this all the time, like `longPathAware` and
`heapType`.

CUI applications that can spawn a UI (or GUI applications that can print to a console) are commonplace on other
platforms because there is no subsystem differentiation.

## UI/UX Design

There is no UI for this feature.

## Capabilities

### Accessibility

This should have no impact on accessibility.

### Security

One reviewer brought up the potential for a malicious actor to spawn an endless stream of headless daemon processes.

This proposal in no way changes the facilities available to malicious people for causing harm: they could have simply
used `IMAGE_SUBSYSTEM_WINDOWS_GUI` and not presented a UI--an option that has been available to them for 35 years.

### Reliability

This should have no impact on reliability.

### Compatibility

An existing application opting into **detached** may constitute a breaking change, but the scope of the breakage is
restricted to that application and is expected to be managed by the application.

All behavioral changes are opt-in.

> **EXAMPLE**: If Python updates python.exe to specify an allocation policy of **detached**, graphical python applications
> will become double-click runnable from the graphical shell without spawning a console window. _However_, console-based
> python applications will no longer spawn a console window when double-clicked from the graphical shell.
>
> In addition, if python.exe specifies **detached**, Console APIs will fail until a console is allocated.

Python could work around this by calling [`AllocConsole`] or [new API `AllocConsoleWithOptions`](#allocconsolewithoptions)
if it can be detected that console I/O is required.

#### Downlevel

On downlevel versions of Windows that do not understand (or expect) this manifest field, applications will allocate
consoles as specified by their image subsystem (described in the [abstract](#abstract) above).

### Performance, Power, and Efficiency

This should have no impact on performance, power or efficiency.

## Potential Issues

### Shell Hang

I am **not** proposing a change in how shells determine whether to wait for an application before returning to a prompt.
This means that a console subsystem application that intends to primarily present a UI but occasionally print text to a
console (therefore choosing the **detached** allocation policy) will cause the shell to "hang" and wait for it to
exit.

The decision to pause/wait is made entirely in the calling shell, and the console subsystem cannot influence that
decision.

Because the vast majority of shells on Windows "hang" by calling `WaitFor...Object` with a HANDLE to the spawned
process, an application that wants to be a "hybrid" CUI/GUI application will be forced to spawn a separate process to
detach from the shell and then terminate its main process.

This is very similar to the forking model seen in many POSIX-compliant operating systems.

### Launching interactively from Explorer, Task Scheduler, etc.

Applications like PowerShell may wish to retain automatic console allocation, and **detached** would be unsuitable for
them. If PowerShell specifies the `detached` console allocation policy, launching `pwsh.exe` from File Explorer it will
no longer spawn a console. This would almost certainly break PowerShell for all users.

Such applications can use `AllocConsole()` early in their startup.

At the same time, PowerShell wants `-WindowStyle Hidden` to suppress the console _before it's created_.

Applications in this category can use `AllocConsoleWithOptions()` to specify additional information about the new console window.

PowerShell, and any other shell that wishes to maintain interactive launch from the graphical shell, can start in
**detached** mode and then allocate a console as necessary. Therefore:

* PowerShell will set `<consoleAllocationPolicy>detached</consoleAllocationPolicy>`
* On startup, it will process its commandline arguments.
* If `-WindowStyle Hidden` is **not** present (the default case), it can:
   * `AllocConsole()` or `AllocConsoleWithOptions(NULL)`
   * Either of these APIs will present a console window (or not) based on the flags passed through `STARTUPINFO` during
     [`CreateProcess`].
* If `-WindowStyle Hidden` is present, it can:
   * `AllocConsoleWithOptions(&alloc)` where `alloc.mode` specifies `ALLOC_CONSOLE_MODE_HIDDEN`

## Future considerations

We're introducing a new manifest field today -- what if we want to introduce more? Should we have a `consoleSettings`
manifest block?

Are there other allocation policies we need to consider?

## Resources

### Rejected Solutions

- A new PE subsystem, `IMAGE_SUBSYSTEM_WINDOWS_HYBRID`
    - it would behave like **inheritOnly**
    - relies on shells to update and check for this
    - checking a subsystem doesn't work right with app execution aliases[^3]
        - This is not a new problem, but it digs the hole a little deeper.
    - requires standardization outside of Microsoft because the PE format is a dependency of the UEFI specification[^4]
    - requires coordination between tooling teams both within and without Microsoft (regarding any tool that operates on
      or produces PE files)

- An exported symbol that shells can check for to determine whether to wait for the attached process to exit
    - relies on shells to update and check for this
    - cracking an executable to look for symbols is probably the last thing shells want to do
        - we could provide an API to determine whether to wait or return?
    - fragile, somewhat silly, exporting symbols from EXEs is annoying and uncommon

An earlier version of this specification offered the **always** allocation policy, with the following behaviors:

> **STRUCK FROM SPECIFICATION**
>
> * A GUI subsystem application would always get a console window.
> * A command-line shell would not wait for it to exit before returning a prompt.

It was cut because a GUI application that wants a console window can simply attach to an existing console session or
allocate a new one. We found no compelling use case that would require the forced allocation of a console session
outside of the application's code.

An earlier version of this specification offered the **inheritOnly** allocation policy, instead of the finer-grained
**hidden** and **detached** policies. We deemed it insufficient for PowerShell's use case because any application
launched by an **inheritOnly** PowerShell would immediately force the uncontrolled allocation of a console window.

> **STRUCK FROM SPECIFICATION**
>
> The move to **hidden** allows PowerShell to offer a fully-fledged console connection that can be itself inherited by a
> downstream application.

#### Additional allocation policies

An earlier revision of this specification suggested two allocation policies:

> **STRUCK FROM SPECIFICATION**
>
> **hidden** is intended to be used by console applications that want finer-grained control over the visibility of their
> console windows, but that still need a console host to service console APIs. This includes most scripting language
> interpreters.
>
> **detached** is intended to be used by primarily graphical applications that would like to operate against a console _if
> one is present_ but do not mind its absence. This includes any graphical tool with a `--help` or `/?` argument.

The `hidden` policy was rejected due to an incompatibility with modern console hosting, as `hidden` would require an
application to interact with the console window via `GetConsoleWindow()` and explicitly show it.

> **STRUCK FROM SPECIFICATION**
>
> ##### ShowWindow and ConPTY
>
> The pseudoconsole creates a hidden window to service `GetConsoleWindow()`, and it can be trivially shown using
> `ShowWindow`. If we recommend that applications `ShowWindow` on startup, we will need to guard the pseudoconsole's
> pseudo-window from being shown.

[^1]: [Powershell -WindowStyle Hidden still shows a window briefly]
[^2]: [StackOverflow: pythonw.exe or python.exe?]
[^3]: [PowerShell: Windows Store applications incorrectly assumed to be console applications]
[^4]: [UEFI spec 2.6 appendix Q.1]

[Powershell -WindowStyle Hidden still shows a window briefly]: https://github.com/PowerShell/PowerShell/issues/3028
[PowerShell: Windows Store applications incorrectly assumed to be console applications]: https://github.com/PowerShell/PowerShell/issues/9970
[StackOverflow: pythonw.exe or python.exe?]: https://stackoverflow.com/questions/9705982/pythonw-exe-or-python-exe
[UEFI spec 2.6 appendix Q.1]: https://www.uefi.org/sites/default/files/resources/UEFI%20Spec%202_6.pdf
[`AllocConsole`]: https://docs.microsoft.com/windows/console/allocconsole
[`CreateProcess`]: https://docs.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
[process creation flags]: https://docs.microsoft.com/en-us/windows/win32/procthread/process-creation-flags
["show command"]: https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-showwindow
