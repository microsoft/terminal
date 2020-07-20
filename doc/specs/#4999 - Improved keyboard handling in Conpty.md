---
author: Mike Griese @zadjii-msft
created on: 2020-05-07
last updated: 2020-06-03
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
enabling client applications to receive the full range of keys once again.
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

> 👉 NOTE: There are actually 5 types of events that can all be encoded as an
> `INPUT_RECORD`. This spec primarily focuses on the encoding of
> `KEY_EVENT_RECORD`s. It is left as a Future Consideration to add support for
> the other types of `INPUT_RECORD` as other sequences, which could be done
> trivially similarly to the following proposal.

## Solution Design

### Inspiration

The design we've settled upon is one that's highly inspired by a few precedents:
* `Application Cursor Keys (DECCKM)` is a long-supported VT sequence which a
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
  uses "Application Program-Command" (or "APC") sequences , prefixed with `^[_`.
* [iTerm2](https://www.iterm2.com/documentation-escape-codes.html) has a region
  of OSC's that they've carved for themselves all starting with the same initial
  parameter, `1337`. They then have a number of commands that all use the second
  parameter to indicate what command specific to iTerm2 they're actually
  implementing.

### Requesting `win32-input-mode`

An application can request `win32-input-mode` with the following private mode sequence:

```
  ^[ [ ? 9001 h/l
              l: Disable win32-input-mode
              h: Enable win32-input-mode
```

Private mode `9001` seems unused according to the [xterm
documentation](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html). This is
stylistically similar to how `DECKPM`, `DECCKM`, and `kitty`'s ["full mode"] are
enabled.

> 👉 NOTE: an earlier draft of this spec used an OSC sequence for enabling these
> sequences. This was abandoned in favor of the more stylistically consistent
> private mode params proposed above. Additionally, if implemented as a private
> mode, then a client app could query if this setting was set with `DECRQM`

When a terminal receives a `^[[?9001h` sequence, they should switch into
`win32-input-mode`. In `win32-input-mode`, the terminal will send keyboard input
to the connected client application in the following format:

### `win32-input-mode` sequences

The `KEY_EVENT_RECORD` portion of an input record (the part that's important for
us to encode in this feature) is defined as the following:

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

To encode all of this information, I propose the following sequence. This is a
CSI sequence with a final terminator character of `_`. This character appears to
only be used as a terminator for the [SCO input
sequence](https://vt100.net/docs/vt510-rm/chapter6.html) for
<kbd>Ctrl+Shift+F10</kbd>. This conflict isn't a real concern for us
compatibility wise. For more details, see [SCO
Compatibility](#SCO-compatibility) below.

```
  ^[ [ Vk ; Sc ; Uc ; Kd ; Cs ; Rc _

       Vk: the value of wVirtualKeyCode - any number. If omitted, defaults to '0'.

       Sc: the value of wVirtualScanCode - any number. If omitted, defaults to '0'.

       Uc: the decimal value of UnicodeChar - for example, NUL is "0", LF is
           "10", the character 'A' is "65". If omitted, defaults to '0'.

       Kd: the value of bKeyDown - either a '0' or '1'. If omitted, defaults to '0'.

       Cs: the value of dwControlKeyState - any number. If omitted, defaults to '0'.

       Rc: the value of wRepeatCount - any number. If omitted, defaults to '1'.

```

> 👉 NOTE: an earlier draft of this spec used an APC sequence for encoding the
> input sequences. This was changed to a CSI for stylistic reasons. There's not
> a great body of reference anywhere that lists APC sequences in use, so there's
> no way to know if the sequence would collide with another terminal emulator's
> usage. Furthermore, using an APC seems to give a distinct impression that
> this is some "Windows Terminal" specific sequence, which is not intended. This
> is a Windows-specific sequence, but one that any Terminal/application could
> use.

In this way, a terminal can communicate input to a connected client application
as `INPUT_RECORD`s, without any loss of fidelity.

#### Example

When the user presses <kbd>Ctrl+F1</kbd> in the console, the console actually
send 4 input records to the client application:
* A <kbd>Ctrl</kbd> down event
* A <kbd>F1</kbd> down event
* A <kbd>F1</kbd> up event
* A <kbd>Ctrl</kbd> up event

Encoded in `win32-input-mode`, this would look like the following:
```
^[[17;29;0;1;8;1_
^[[112;59;0;1;8;1_
^[[112;59;0;0;8;1_
^[[17;29;0;0;0;1_

Down: 1 Repeat: 1 KeyCode: 0x11 ScanCode: 0x1d Char: \0 (0x0) KeyState: 0x28
Down: 1 Repeat: 1 KeyCode: 0x70 ScanCode: 0x3b Char: \0 (0x0) KeyState: 0x28
Down: 0 Repeat: 1 KeyCode: 0x70 ScanCode: 0x3b Char: \0 (0x0) KeyState: 0x28
Down: 0 Repeat: 1 KeyCode: 0x11 ScanCode: 0x1d Char: \0 (0x0) KeyState: 0x20
```

Similarly, for a keypress like <kbd>Ctrl+Alt+A</kbd>, which is 6 key events:
```
^[[17;29;0;1;8;1_
^[[18;56;0;1;10;1_
^[[65;30;0;1;10;1_
^[[65;30;0;0;10;1_
^[[18;56;0;0;8;1_
^[[17;29;0;0;0;1_

Down: 1 Repeat: 1 KeyCode: 0x11 ScanCode: 0x1d Char: \0 (0x0) KeyState: 0x28
Down: 1 Repeat: 1 KeyCode: 0x12 ScanCode: 0x38 Char: \0 (0x0) KeyState: 0x2a
Down: 1 Repeat: 1 KeyCode: 0x41 ScanCode: 0x1e Char: \0 (0x0) KeyState: 0x2a
Down: 0 Repeat: 1 KeyCode: 0x41 ScanCode: 0x1e Char: \0 (0x0) KeyState: 0x2a
Down: 0 Repeat: 1 KeyCode: 0x12 ScanCode: 0x38 Char: \0 (0x0) KeyState: 0x28
Down: 0 Repeat: 1 KeyCode: 0x11 ScanCode: 0x1d Char: \0 (0x0) KeyState: 0x20
```

Or, for something simple like <kbd>A</kbd> (which is 4 key events):
```
^[[16;42;0;1;16;1_
^[[65;30;65;1;16;1_
^[[16;42;0;0;0;1_
^[[65;30;97;0;0;1_

Down: 1 Repeat: 1 KeyCode: 0x10 ScanCode: 0x2a Char: \0 (0x0) KeyState: 0x30
Down: 1 Repeat: 1 KeyCode: 0x41 ScanCode: 0x1e Char: A  (0x41) KeyState: 0x30
Down: 0 Repeat: 1 KeyCode: 0x10 ScanCode: 0x2a Char: \0 (0x0) KeyState: 0x20
Down: 0 Repeat: 1 KeyCode: 0x41 ScanCode: 0x1e Char: a  (0x61) KeyState: 0x20
```

> 👉 NOTE: In all the above examples, I had my NumLock key off. If I had the
> NumLock key instead pressed, all the KeyState parameters would have bits 0x20
> set. To get these keys with a NumLock, add 32 to the value.

These parameters are ordered based on how likely they are to be used. Most of
the time, the repeat count is not needed (it's almost always `1`), so it can be
left off when not required. Similarly, the control key state is probably going
to be 0 a lot of the time too, so that is second last. Even keydown will be 0 at
least half the time, so that can be omitted some of the time.

Furthermore, considering omitted values in CSI parameters default to the values
specified above, the above sequences could each be shortened to the following.

* <kbd>Ctrl+F1</kbd>
```
^[[17;29;;1;8_
^[[112;59;;1;8_
^[[112;59;;;8_
^[[17;29_
```

* <kbd>Ctrl+Alt+A</kbd>
```
^[[17;29;;1;8_
^[[18;56;;1;10_
^[[65;30;;1;10_
^[[65;30;;;10_
^[[18;56;;;8_
^[[17;29;;_
```

* <kbd>A</kbd> (which is <kbd>shift+a</kbd>)
```
^[[16;42;;1;16_
^[[65;30;65;1;16_
^[[16;42_
^[[65;30;97_
```

* Or even easier, just <kbd>a</kbd>
```
^[[65;30;97;1_
^[[65;30;97_
```

### Scenarios

#### User is typing into WSL from the Windows Terminal

`WT -> conpty[1] -> wsl`

* Conpty[1] will ask for `win32-input-mode` from the Windows Terminal when
  conpty[1] first boots up. Conpty will _always_ ask for win32-input-mode -
  Terminals that _don't_ support this mode will ignore this sequence on startup.
* When the user types keys in Windows Terminal, WT will translate them into
  win32 sequences and send them to conpty[1]
* Conpty[1] will translate those win32 sequences into `INPUT_RECORD`s.
  - When those `INPUT_RECORD`s are written to the input buffer, they'll be
    converted into VT sequences corresponding to whatever input mode the linux
    app is in.
* When WSL reads the input, it'll read (using `ReadConsoleInput`) a stream of
  `INPUT_RECORD`s that contain only character information, which it will then
  pass to the linux application.
  - This is how `wsl.exe` behaves today, before this change.

#### User is typing into `cmd.exe` running in WSL interop

`WT -> conpty[1] -> wsl -> conpty[2] -> cmd.exe`

(presuming you start from the previous scenario, and launch `cmd.exe` inside wsl)

* Conpty[2] will ask for `win32-input-mode` from conpty[1] when conpty[2] first
  boots up.
    - As conpty[1] is just a conhost that knows how to handle
      `win32-input-mode`, it will switch its own VT input handling into
      `win32-input-mode`
* When the user types keys in Windows Terminal, WT will translate them into
  win32 sequences and send them to conpty[1]
* Conpty[1] will translate those win32 sequences into `INPUT_RECORD`s. When
  conpty[1] writes these to its buffer, it will translate the `INPUT_RECORD`s
  into VT sequences for the `win32-input-mode`. This is because it believes the
  client (in this case, the conpty[2] running attached to `wsl`) wants
  `win32-input-mode`.
* When WSL reads the input, it'll read (using `ReadConsoleInput`) a stream of
  `INPUT_RECORD`s that contain only character information, which it will then
  use to pass a stream of characters to conpty[2].
* Conpty[2] will get those sequences, and will translate those win32 sequences
  into `INPUT_RECORD`s
* When `cmd.exe` reads the input, they'll receive the full `INPUT_RECORD`s
  they're expecting


## UI/UX Design

This is not a user-facing feature.

## Capabilities

### Accessibility

_(no change expected)_

### Security

_(no change expected)_

### Reliability

_(no change expected)_

### Compatibility

This isn't expected to break any existing scenarios. The information that we're
passing to conpty from the Terminal should strictly have _more_ information in
them than they used to. Conhost was already capable of translating
`INPUT_RECORD`s back into VT sequences, so this should work the same as before.

There's some hypothetical future where the Terminal isn't connected to conpty.
In that future, the Terminal will still be able to work correctly, even with
this ConPTY change. The Terminal will only switch into sending
`win32-input-mode` sequences when _conpty asks for them_. Otherwise, the
Terminal will still behave like a normal terminal emulator.

#### Terminals that don't support `?9001h`

Traditionally, whenever a terminal emulator doesn't understand a particular VT
sequence, they simply ignore the unknown sequence. This assumption is being
relied upon heavily, as ConPTY will _always_ emit a `^[[?9001h` on
initialization, to request `win32-input-mode`.

#### SCO Compatibility

As mentioned above, the `_` character is used as a terminator for the [SCO input
sequence](https://vt100.net/docs/vt510-rm/chapter6.html) for
<kbd>Ctrl+Shift+F10</kbd>. This conflict would be a problem if a hypothetical
terminal was connected to conpty that sent input to conpty in SCO format.
However, if that terminal was only sending input to conpty in SCO mode, it would
have much worse problems than just <kbd>Ctrl+Shift+F10</kbd> not working. If we
did want to support SCO mode in the future, I'd even go so far as to say we
could maybe treat a `win32-input-mode` sequence with no params as
<kbd>Ctrl+Shift+F10</kbd>, considering that `KEY_EVENT_RECORD{0}` isn't really
valid anyways.

#### Remoting `INPUT_RECORD`s

A potential area of concern is the fact that VT sequences are often used to
remote input from one machine to another. For example, a terminal might be
running on machine A, and the conpty at the end of the pipe (which is running
the client application) might be running on another machine B.

If these two machines have different keyboard layouts, then it's possible that
the `INPUT_RECORD`s synthesized by the terminal on machine A won't really be
valid on machine B. It's possible that machine B has a different mapping of scan
codes \<-> characters. A client that's running on machine B that uses win32 APIs
to try and infer the vkey, scancode, or character from the other information in
the `INPUT_RECORD` might end up synthesizing the wrong values.

At the time of writing, we're not really sure what a good solution to this
problem would be. Client applications that use `win32-input-mode` should be
aware of this, and be written with the understanding that these values are
coming from the terminal's machine, which might not necessarily be the local
machine.

### Performance, Power, and Efficiency

_(no change expected)_

## Potential Issues

_(no change expected)_

## Future considerations

* We could also hypothetically use this same mechanism to send Win32-like mouse
  events to conpty, since similar to VT keyboard events, VT mouse events don't
  have the same fidelity that Win32 mouse events do.
  - We could enable this with a different terminating character, to identify
    which type of `INPUT_RECORD` event we're encoding.
* Client applications that want to be able to read full Win32 keyboard input
  from `conhost` _using VT_ will also be able to use `^[[?9001h` to do this. If
  they emit `^[[?9001h`, then conhost will switch itself into
  `win32-input-mode`, and the client will read `win32-input-mode` encoded
  sequences as input. This could enable other cross-platform applications to
  also use win32-like input in the future.

## Options Considered

_disclaimer: these notes are verbatim from my research notes in [#4999]_.

### Create our own format for `INPUT_RECORD`s

* If we wanted to do this, then we'd probably want to have the Terminal only
  send input as this format, and not use the existing translator to synthesize
  VT sequences
    - Consider sending a ctrl down, '^A', ctrl up. We wouldn't want to send this
      as three sequences, because conpty will take the '^A' and synthesize
      _another_ ctrl down, ctrl up pair.
* With conpty passthrough mode, we'd still need the `InputStateMachineEngine`
  to convert these sequences into INPUT_RECORDs to translate back to VT
* Wouldn't really expect client apps to ever _need_ this format, but it could
  always be possible for them to need it in the future.

#### Pros:
* Definitely gets us all the information that we need.
* Can handle solo modifiers
* Can handle keydown and keyup separately
* We can make the sequence however we want to parse it.

#### Cons:
* No reference implementation, so we'd be flying blind
* We'd be defining our own VT sequences for these, which we've never done
  before. This was _inevitable_, however, this is still the first time we'd be
  doing this.
* By having the Terminal send all input as _this protocol_, VT Input passthrough
  to apps that want VT input won't work anymore for the Terminal. That's _okay_

### kitty extension
[Reference](https://sw.kovidgoyal.net/kitty/protocol-extensions.html#keyboard-handling)

#### Pros:
* Not terribly difficult to decode
* Unique from anything else we'd be processing, as it's an APC sequence
  (`\x1b_`)
* From their docs:
  > All printable key presses without modifier keys are sent
  just as in the normal mode. ... For non printable keys and key combinations
  including one or more modifiers, an escape sequence encoding the key event is
  sent
   - I think I like this. ASCII and other keyboard layout chars (things that would
     hit `SendChar`) would still just come through as the normal char.

#### Cons:
* Their encoding table is _odd_. [Look at
  this](https://sw.kovidgoyal.net/kitty/key-encoding.html). What order is that
  in? Obviously the first column is sorted alphabetically, but the mapping of
  key->char is in a certainly hard to decipher order.
* I can't get it working locally, so hard to test 😐
* They do declare the `fullkbd` terminfo capability to identify that they
  support this mode, but I'm not sure anyone else uses it.
  - I'm also not sure that any _client_ apps are reading this currently.
* This isn't designed to be full `KEY_EVENT`s - where would we put the scancode
  (for apps that think that's important)?
  - We'd have to extend this protocol _anyways_

### `xterm` "Set key modifier options"
Notably looking at
[`modifyOtherKeys`](https://invisible-island.net/xterm/manpage/xterm.html#VT100-Widget-Resources:modifyOtherKeys).

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
* neither `xterm` nor `gnome-terminal` (VTE) seem to implement this. I'm not
  sure if anyone's got a reference implementation for us to work with.
* Unsure how this would work with other keyboard layouts
    - [this doc](https://vt100.net/docs/vt510-rm/chapter8.html#S8.13) seems to
      list the key-down/up codes for all the en-us keyboard keys, but the
      scancodes for these are different for up and down. That would seem to
      imply we couldn't just shove the Win32 scancode in those bits

### `DECKPM`, `DECSMKR`
[DECKPM](https://vt100.net/docs/vt510-rm/DECKPM.html)
[DECSMKR](https://vt100.net/docs/vt510-rm/DECSMKR.html)
[DECEKBD](https://vt100.net/docs/vt510-rm/DECEKBD.html)

#### Pros:
* Enables us to send key-down and key-up keys independently
* Enables us to send modifiers on their own
* Part of the VT 510 standard

#### Cons:
* neither `xterm` nor `gnome-terminal` (VTE) seem to implement this. I'm not
  sure if anyone's got a reference implementation for us to work with.
* not sure that "a three-character ISO key position name, for example C01" is
  super compatible with our Win32 VKEYs.


### `libtickit` encoding
[Source](http://www.leonerd.org.uk/hacks/fixterms)

#### Pros:
* Simple encoding scheme

#### Cons:
* Doesn't differentiate between keydowns and keyups
* Unsure who implements this - not extensively investigated


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
