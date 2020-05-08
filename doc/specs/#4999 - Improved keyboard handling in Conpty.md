---
author: Mike Griese @zadjii-msft
created on: 2020-05-07
last updated: 2020-05-07
issue id: 4999
---

# Improved keyboard handling in Conpty

## Abstract

The Windows Console internally uses [`INPUT_RECORD`]s to represent the various
types of input that a user might send to a client application. This includes
things like keypresses, mouse events, window resizes, etc.

However, conpty's keyboard input is fundamentally backed by VT sequences, which
limits the range of keys that a terminal application can actually send relative
to what the console was capable of. This results in a number of keys that were
previously representable in the console as `INPUT_RECORD`s, but are impossible
to send to a client application that's running in conpty mode.

Some of these issues include, but are not limited to:

* Some keybindings used by PSReadLine aren't getting through [#879]
* Bug Report: Control+Space not sent to terminal emulator. [#2865]
* Shift+Enter always submits, breaking PSReadline features [#530]
* Powershell: Ctrl-Alt-? does not work in Windows Terminal [#3079]
* Bug: ctrl+break is not ctrl+c [#1119]
* Something wrong with keyboard modifiers processing? [#1694]
* Numeric input not accepted by choice.exe [#3608]
* Ctrl+Keys that can't be encoded as VT should still fall through as the unmodified character [#3483]
* Modifier keys are not properly propagated to application hosted in Windows Terminal [#4334] / [#4446]

This spec covers a mechanism by which we can add support to ConPTY so that a
terminal application could send `INPUT_RECORD`-like key events to conpty,
enabling client applications to recieve the full range of keys once again.
Included at the bottom of this document is a collection of [options that were
investigated](#options-considered) as a part of preparing this document.

## Considerations

When evaluating existing encoding schemes for viability, the following things
were used to evaluate whether or not a particular encoding would work for our
needs:

* Would a particular encoding be mixable with other normal VT processing easily?
  - How would the Terminal know when it should send a \<chosen_encoding> key vs
    a normally encoded one?
    - For ex, <kbd>Ctrl+space</kbd> - should we send `NUL` or
      \<chosen_encoding's version of ctrl+space>
* If there's a scenario where Windows Terminal might _not_ be connected to a
  conpty, then how does conpty enable \<chosen_encoding>?
* Is the goal "Full `INPUT_RECORD` fidelity" or "Make the above scenarios work"?
  - One could imagine having the Terminal special-case the above keys, and send
    the xterm modifyOtherKeys sequences just for those scenarios.
    - This would _not_ work for <kbd>shift</kbd> all by itself.
  - In my _opinion_, "just making the above work" is a subset of "full
    INPUT_RECORD", and inevitably we're going to want "full INPUT_RECORD"

The goal we're trying to achieve is communicating `INPUT_RECORD`s from the
terminal to the client app via conpty. This isn't supposed to be a \*nix
terminal compatible communication, it's supposed to be fundamentally Win32-like.


Keys that we definitely need to support, that don't have unique VT sequences:
* <kbd>Ctrl+Space</kbd> ([#879], [#2865])
* <kbd>Shift+Enter</kbd> ([#530])
* <kbd>Ctrl+Break</kbd> ([#1119])
* <kbd>Ctrl+Alt+?</kbd> ([#3079])
* <kbd>Ctrl</kbd>, <kbd>Alt</kbd>, <kbd>Shift</kbd>,  (without another keydown/up) ([#3608], [#4334], [#4446])

## Solution Design

### Inspiration

The design we've settled upon is one that's highly inspired by two precedents:
* `Application Cursor Keys (DECCKM)` is a long-supported VT sequences which a
  client application can use to request a different input format from the
  Terminal. This is the DECSET sequence `^[[?1h`/`^[[?1l` (for enable/disable,
  respectively). This changes the sequences sent by keys like the Arrow keys
  from a sequence like `^[[A` to `^[OA` instead.
* The `kitty` terminal emulator uses a similar DECSET sequence for enabling
  their own input format, which they call ["full mode"]. Similar to DECCKM, this
  changes the format of the sequences that the terminal should send for keyboard
  input. Their "full mode" contains much more information when keys are pressed
  or released (though, less than a full `INPUT_RECORD` worth of data). Instead
  of input being sent to the client as a CSI or SS3 sequence, this `kitty` mode
  uses "Application Program-Command" (or "APC) sequences , prefixed with `^[_`.


[ConEmu specific OSCs](#ConEmu-specific-OSCs)

https://conemu.github.io/en/AnsiEscapeCodes.html#ConEmu_specific_OSC

https://www.iterm2.com/documentation-escape-codes.html

```
  ^[ ] 1000 ; 1 ; Ps ST

         Ps: Whether to enable win32-input-mode or not. If omitted, defaults to '0'.
             0: Disable win32-input-mode
             1: Enable win32-input-mode
```

### Requesting `win32-input-mode`

### `win32-input-mode` sequences

```c++
typedef struct _KEY_EVENT_RECORD {
  BOOL  bKeyDown;
  WORD  wRepeatCount;
  WORD  wVirtualKeyCode;
  WORD  wVirtualScanCode;
  union {
    WCHAR UnicodeChar;
    CHAR  AsciiChar;
  } uChar;
  DWORD dwControlKeyState;
} KEY_EVENT_RECORD;
```

```
  ^[ _ 1 ; Kd ; Rc ; Vk ; Sc ; Uc ; Cs ST

         Kd: the value of bKeyDown - either a '0' or '1'. If omitted, defaults to '0'.

         Rc: the value of wRepeatCount - any number. If omitted, defaults to '0'.

         Vk: the value of wVirtualKeyCode - any number. If omitted, defaults to '0'.

         Sc: the value of wVirtualScanCode - any number. If omitted, defaults to '0'.

         Uc: the value of UnicodeChar - TODO I think this should be
             decimal-encoded, because otherwise control chars would be mixed in.

         Cs: the value of dwControlKeyState - any number. If omitted, defaults to '0'.
```

**TODO**: Defaulting `Rc` to `1` makes more sense in my opinion, so
`^[_1;;;Vk;;Uc;;ST` is all that's needed for most keys.



TODO: I don't _love_ using APC 1 here - technically, if anyone else wants to use an
APC, then there's no way to know if this conflicts. Maybe there's a convenient
opening in CSI space?

Also, pretty much no one uses OSC's for input, but conpty generally already does
for window size, so we _could_ use that...

### Scenarios


#### User is typing into WSL from the Windows Terminal

`WT -> conpty[1] -> wsl`

* Conpty[1] will ask for `win32-input-mode` from the Windows Terminal
* When the user types keys in Windows Terminal, WT will translate them into
  win32 sequences and send them to conpty[1]
* Conpty[1] will translate those win32 sequences into `INPUT_RECORD`s
* When WSL reads the input, conpty[1] will translate the `INPUT_RECORD`s into VT
  sequences corresponding to whatever input mode the linux app is in

#### User is typing into `cmd.exe` running in WSL interop

`WT -> conpty[1] -> wsl -> conpty[2] -> cmd.exe`

(presuming you start from the previous scenario, and launch `cmd.exe` inside wsl)

* Conpty[2] will ask for `win32-input-mode` from conpty[1]
* When the user types keys in Windows Terminal, WT will translate them into
  win32 sequences and send them to conpty[1]
* Conpty[1] will translate those win32 sequences into `INPUT_RECORD`s
* When WSL reads the input, conpty[1] will translate the `INPUT_RECORD`s into VT
  sequences for the win32 input that conpty 2 requested
* Conpty[2] will get those sequences, and will translate those win32 sequences
  into `INPUT_RECORD`s
* When `cmd.exe` reads the input, they'll receive the full `INPUT_RECORD`s
  they're expecting


## UI/UX Design

[comment]: # What will this fix/feature look like? How will it affect the end user?

## Capabilities

### Accessibility

_(no change expected)_

### Security

_(no change expected)_

### Reliability

_(no change expected)_

### Compatibility

[comment]: # Will the proposed change break existing code/behaviors? If so, how, and is the breaking change "worth it"?

### Performance, Power, and Efficiency

_(no change expected)_

## Potential Issues

[comment]: # What are some of the things that might cause problems with the fixes/features proposed? Consider how the user might be negatively impacted.

## Future considerations

[comment]: # What are some of the things that the fixes/features might unlock in the future? Does the implementation of this spec enable scenarios?


## Options Considered


### Create our own format for `INPUT_RECORD`s

* If we wanted to do this, then we'd probably want to have the Terminal only send input as this format, and not use the existing translator to synthesize VT sequences
    - Consider sending a ctrl down, '^A', ctrl up. We wouldn't want to send this as three sequences, because conpty will take the '^A' and synthesize _another_ ctrl down, ctrl up pair.
* With conpty passthrough mode, we'd still need the `InpustStateMachineEngine` to convert these sequences into INPUT_RECORDs to translate back to VT
* Wouldn't really expect client apps to ever _need_ this format, but it could always be possible for them to need it in the future.

#### Pros:
* Definitely gets us all the information that we need.
* Can handle solo modifiers
* Can handle keydown and keyup separately
* We can make the sequence however we want to parse it.

#### Cons:
* No reference implementation, so we'd be flying blind
* We'd be defining our own VT sequences for these, which we've never done before. This was _inevitable_, however, this is still the first time we'd be doing this.
* Something about Microsoft extending well-defined protocols being bad 😆
* By having the Terminal send all input as _this protocol_, VT Input passthrough to apps that want VT input won't work anymore for the Terminal. That's _okay_

### kitty extension
[Reference](https://sw.kovidgoyal.net/kitty/protocol-extensions.html#keyboard-handling)

#### Pros:
* Not terribly difficult to decode
* Unique from anything else we'd be processing, as it's an APC sequence (`\x1b_`)
* From their docs:
  > All printable key presses without modifier keys are sent just as in the normal mode. ... For non printable keys and key combinations including one or more modifiers, an escape sequence encoding the key event is sent
   - I think like this. ASCII and other keyboard layout chars (things that would hit `SendChar`) would still just come through as the normal char.

#### Cons:
* Their encoding table is _mad_. [Look at this](https://sw.kovidgoyal.net/kitty/key-encoding.html). What order is that in? Obviously the first column is sorted alphabetically, but the mapping of key->char is in a seemingly bonkers order.
* I can't get it working locally, so hard to test 😐
* They do declare the `fullkbd` terminfo capability to identify that they support this mode, but I'm not sure anyone else uses it.
  - I'm also not sure that any _client_ apps are reading this currently.
* This isn't designed to be full `KEY_EVENT`s - where would we put the scancode (for apps that think that's important)?
  - We'd have  to extend this protocol _anyways_

### `xterm` "Set key modifier options"
Notably looking at [`modifyOtherKeys`](https://invisible-island.net/xterm/manpage/xterm.html#VT100-Widget-Resources:modifyOtherKeys).

#### Pros:
* `xterm` implements this so there's a reference implementation
* relatively easy to parse these sequences. `CSI 27 ; <modifiers> ; <key> ~`

#### Cons:
* Only sends the sequence on key-up
* Doesn't send modifiers all on their own

### `DECPCTERM`
[VT100.net doc](https://vt100.net/docs/vt510-rm/DECPCTERM.html)

#### Pros:
* Enables us to send key-down and key-up keys independently
* Enables us to send modifiers on their own
* Part of the VT 510 standard
#### Cons:
* neither `xterm` nor `gnome-terminal` (VTE) seem to implement this. I'm not sure if anyone's got a reference implementation for us to work with.
* Unsure how this would work with other keyboard layouts
    - [this doc](https://vt100.net/docs/vt510-rm/chapter8.html#S8.13) seems to list the key-down/up codes for all the en-us keyboard keys, but the scancodes for these are different for up and down. That would seem to imply we couldn't just shove the Win32 scancode in those bits


### `DECPKM`, `DECSKMR`
[DECPKM](https://vt100.net/docs/vt510-rm/DECKPM.html)
[DECSKMR](https://vt100.net/docs/vt510-rm/DECSKMR.html)
[DECEKBD](https://vt100.net/docs/vt510-rm/DECEKBD.html)

#### Pros:
* Enables us to send key-down and key-up keys independently
* Enables us to send modifiers on their own
* Part of the VT 510 standard
#### Cons:
* neither `xterm` nor `gnome-terminal` (VTE) seem to implement this. I'm not sure if anyone's got a reference implementation for us to work with.
* not sure that "a three-character ISO key position name, for example C01" is super compatible with our Win32 VKEYs.


## Resources

* The initial discussion for this topic was done in [#879], and much of the
  research of available options is also available as a discussion in [#4999].
* [Why Is It so Hard to Detect Keyup Event on Linux?](https://blog.robertelder.org/detect-keyup-event-linux-terminal/)
  - and the [HackerNews discussion](https://news.ycombinator.com/item?id=19012132)
* [ConEmu specific OSCs](https://conemu.github.io/en/AnsiEscapeCodes.html#ConEmu_specific_OSC)
* [iterm2 specific sequences](https://www.iterm2.com/documentation-escape-codes.html)
* [terminal-wg draft list of OSCs](https://gitlab.freedesktop.org/terminal-wg/specifications/-/issues/10)

<!-- Footnotes -->
[#530]: https://github.com/microsoft/terminal/issues/530
[#879]: https://github.com/microsoft/terminal/issues/879
[#1119]: https://github.com/microsoft/terminal/issues/1119
[#1694]: https://github.com/microsoft/terminal/issues/1694
[#2865]: https://github.com/microsoft/terminal/issues/2865
[#3079]: https://github.com/microsoft/terminal/issues/3079
[#3483]: https://github.com/microsoft/terminal/issues/3483
[#3608]: https://github.com/microsoft/terminal/issues/3608
[#4334]: https://github.com/microsoft/terminal/issues/4334
[#4446]: https://github.com/microsoft/terminal/issues/4446
[#4999]: https://github.com/microsoft/terminal/issues/4999

[`INPUT_RECORD`]: https://docs.microsoft.com/en-us/windows/console/input-record-str

["full mode"]: https://sw.kovidgoyal.net/kitty/protocol-extensions.html#keyboard-handling
