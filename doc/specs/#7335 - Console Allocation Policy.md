---
author: Dustin Howett @DHowett <duhowett@microsoft.com>
created on: 2020-08-16
last updated: 2020-08-21
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

PowerShell\[1\] is waiting to deal with this problem because they don't necessarily want to ship a `pwshw.exe` for all
of their GUI-only authors. Every additional `*w` version of an application is an additional maintenance burden and
source of cognitive overhead\[2\] for users.

On the other side, you have mostly-GUI applications that want to print output to a console **if there is one
connected**.

These applications are still primarily GUI-driven, but they might support arguments like `/?` or `--help`. They only
need a console when they need to print out some text. Sometimes they'll allocate their own console (which opens a new
window) to display in, and sometimes they'll reattach to the originating console. VSCode does the latter, and so when
you run `code` from CMD, and then `exit` CMD, your console window sticks around because VSCode is still attached to it.
It will never print anything, and your only option is to close it.

There's another risk risk in reattaching, too. Given that the shell decides whether to wait based on the subsystem
field, GUI subsystem applications that reattach to their owning consoles *just to print some text* end up stomping on
the output of any shell that doesn't wait for them:

```
C:\> application --help

application - the interesting application
C:\> Usage: application [OPTIONS] ...
```

> _(the prompt is interleaved with the output)_

## Solution Design

I propose that we introduce a fusion manifest field, **consoleWindowPolicy**, with the following values:

* _absent_
* `hidden`
* `detached`

These flags provide a way for an application to specify a default value for the console-related [process creation flags]
supported by [`CreateProcess`] which will take effect when none are provided and there is no existing console session.

It would look (roughly) like this:

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <application>
    <windowsSettings>
      <consoleWindowPolicy xmlns="http://schemas.microsoft.com/SMI/20XX/WindowsSettings">hidden</consoleWindowPolicy>
    </windowsSettings>
  </application>
</assembly>
```

The effects of this field will only apply to binaries in the `IMAGE_SUBSYSTEM_WINDOWS_CUI` subsystem, as it pertains to
the particulars of their console allocation.

**All console inheritance will proceed as normal.** Since these flags take effect only in the absence of console
inheritance, CUI applications will still be able to run inside an existing console session.

| policy     | behavior                                                                                                            |
| -          | -                                                                                                                   |
| _absent_   | _default behavior_                                                                                                  |
| `hidden`   | The new process is attached to a headless console session (similar to `CREATE_NO_WINDOW`) unless one was inherited. |
| `detached` | The new process is not attached to a console session (similar to `DETACHED_PROCESS`) unless one was inherited.      |

An application that uses either of these manifest values will _not_ present a console window when launched by Explorer,
Task Scheduler, etc. The only difference between the two is whether there is a hidden console host that can service
console API requests.

### Interaction with existing APIs

[`CreateProcess`] supports a number of [process creation flags] that dictate how a spawned application will behave with
regards to console allocation:

* `DETACHED_PROCESS`: No console inheritance, no console host spawned for the new process.
* `CREATE_NEW_CONSOLE`: No console inheritance, new console host **is** spawned for the new process.
* `CREATE_NO_WINDOW`: No console inheritance, new console host **is** spawned for the new process.
    * this is the same as `CREATE_NEW_CONSOLE`, except that the first connection packet specifies that the window should
      be invisible

Because this proposal pertains only to applications spawned in their default state, `CreateProcess`' flags will override
the manifested window policy just as they would have overridden the image's subsystem-based default.

As an example, `CreateProcess(...CREATE_NEW_CONSOLE...)` on a **detached**-manifested application will force the
allocation of a new console host.

### Why two values?

**hidden** is intended to be used by console applications that want finer-grained control over the visibility of their
console windows, but that still need a console host to service console APIs. This includes most scripting language
interpreters.

**detached** is intended to be used by primarily graphical applications that would like to operate against a console _if
one is present_ but do not mind its absence. This includes any graphical tool with a `--help` or `/?` argument.

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

An existing application opting into **hidden** may constitute a breaking change, but the scope of the breakage is
restricted to that application and is expected to be managed by the application.

All behavioral changes are opt-in.

> **EXAMPLE**: If Python updates python.exe to specify an allocation policy of **hidden**, graphical python applications
> will become double-click runnable from the graphical shell without spawning a console window. _However_, console-based
> python applications will no longer spawn a console window when double-clicked from the graphical shell.
>
> If python.exe specifies **detached**, Console APIs will fail until a console is allocated.

Python could work around this by calling [`AllocConsole`] if it can be detected that console I/O is required, or
`GetConsoleWindow`/`ShowWindow` if it can detect that the user wants to interact with the hidden console session.

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
them. However, they _can_ use **hidden**. There is a problem, though: if PowerShell becomes **hidden**, double-clicking
`pwsh.exe` will no longer spawn a console. This would almost certainly break PowerShell for all users.

At the same time, PowerShell wants `-WindowStyle Hidden` to suppress the console _before it's created_.

PowerShell, and any other shell that wishes to maintain interactive launch from the graphical shell, can start in
**hidden** mode and then display its window as necessary. Therefore:

* PowerShell will set `<consoleWindowPolicy>hidden</consoleWindowPolicy>`
* On startup, it will process its commandline arguments.
* If `-WindowStyle Hidden` is present, it will do nothing.
* If `-WindowStyle Hidden` is **not** present (the default case), it can:
   * `GetConsoleWindow()`
   * `ShowWindow(consoleWindow, SW_SHOWDEFAULT)`
      * We recommend using `SW_SHOWDEFAULT` to respect any "show command" passed in during process creation.

### ShowWindow and ConPTY

The pseudoconsole creates a hidden window to service `GetConsoleWindow()`, and it can be trivially shown using
`ShowWindow`. If we recommend that applications `ShowWindow` on startup, we will need to guard the pseudoconsole's
pseudo-window from being shown.

## Future considerations

We're introducing a new manifest field today -- what if we want to introduce more? Should we have a `consoleSettings`
manifest block?

Are there other allocation policies we need to consider?

## Resources

### Rejected Solutions

- A new PE subsystem, `IMAGE_SUBSYSTEM_WINDOWS_HYBRID`
    - it would behave like **inheritOnly**
    - relies on shells to update and check for this
    - checking a subsystem doesn't work right with app execution aliases\[3\]
        - This is not a new problem, but it digs the hole a little deeper.
    - requires standardization outside of Microsoft because the PE format is a dependency of the UEFI specification\[4\]
    - requires coordination between tooling teams both within and without Microsoft (regarding any tool that operates on
      or produces PE files)

- An exported symbol that shells can check for to determine whether to wait for the attached process to exit
    - relies on shells to update and check for this
    - cracking an executable to look for symbols is probably the last thing shells want to do
        - we could provide an API to determine whether to wait or return?
    - fragile, somewhat silly, exporting symbols from EXEs is annoying and uncommon

An earlier version of this specification offered the **always** allocation policy, with the following behaviors:

* A GUI subsystem application would always get a console window.
* A command-line shell would not wait for it to exit before returning a prompt.

It was cut because a GUI application that wants a console window can simply attach to an existing console session or
allocate a new one. We found no compelling use case that would require the forced allocation of a console session
outside of the application's code.

An earlier version of this specification offered the **inheritOnly** allocation policy, instead of the finer-grained
**hidden** and **detached** policies. We deemed it insufficient for PowerShell's use case because any application
launched by an **inheritOnly** PowerShell would immediately force the uncontrolled allocation of a console window.

The move to **hidden** allows PowerShell to offer a fully-fledged console connection that can be itself inherited by a
downstream application.

### Links

1. [Powershell -WindowStyle Hidden still shows a window briefly]
2. [StackOverflow: pythonw.exe or python.exe?]
3. [PowerShell: Windows Store applications incorrectly assumed to be console applications]
4. [UEFI spec 2.6 appendix Q.1]

[Powershell -WindowStyle Hidden still shows a window briefly]: https://github.com/PowerShell/PowerShell/issues/3028
[PowerShell: Windows Store applications incorrectly assumed to be console applications]: https://github.com/PowerShell/PowerShell/issues/9970
[StackOverflow: pythonw.exe or python.exe?]: https://stackoverflow.com/questions/9705982/pythonw-exe-or-python-exe
[UEFI spec 2.6 appendix Q.1]: https://www.uefi.org/sites/default/files/resources/UEFI%20Spec%202_6.pdf
[`AllocConsole`]: https://docs.microsoft.com/windows/console/allocconsole
[`CreateProcess`]: https://docs.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
[process creation flags]: https://docs.microsoft.com/en-us/windows/win32/procthread/process-creation-flags
